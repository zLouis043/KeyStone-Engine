#include "../include/core/error.h"
#include "../include/memory/memory.h"

#include <string.h>
#include <stdlib.h>
#include <varargs.h>
#include <mutex>
#include <stack>
#include <unordered_map>
#include <chrono>

struct CodeMetadata {
	std::string title;
	std::string description;
};

static std::recursive_mutex sg_error_mutex;
static std::stack<Ks_Error_Info> sg_error_stack;

static std::unordered_map<std::string, ks_uint16> sg_modules;
static std::unordered_map<ks_uint16, std::string> sg_module_names;
static std::unordered_map<ks_uint64, CodeMetadata> sg_code_registry;
static ks_uint16 sg_module_counter = 1;

#define KS_ERROR_LOCAL_MASK 0x00FFFFFFULL

static char* internal_strdup(const char* s) {
	if (!s) return nullptr;
	size_t len = strlen(s) + 1;
	char* p = (char*)malloc(len);
	if (p) memcpy(p, s, len);
	return p;
}

ks_uint16 ks_error_make_module_prefix(ks_str module_name) {
	std::lock_guard<std::recursive_mutex> lock(sg_error_mutex);
	if (sg_modules.find(module_name) == sg_modules.end()) {
		sg_modules[module_name] = sg_module_counter;
		sg_module_names[sg_module_counter++] = module_name;
	}
	return sg_modules[module_name];
}

ks_str ks_error_get_module_prefix_str(ks_uint16 id)
{
	std::lock_guard<std::recursive_mutex> lock(sg_error_mutex);
	if (sg_module_names.find(id) == sg_module_names.end()) {
		return nullptr;
	}
	return sg_module_names[id].c_str();
}

ks_uint16 ks_error_get_module_prefix(ks_str module_name) {
	std::lock_guard<std::recursive_mutex> lock(sg_error_mutex);
	auto it = sg_modules.find(module_name);
	if (it == sg_modules.end()) {
		ks_uint16 pref = sg_module_counter++;
		sg_modules[module_name] = pref;
		sg_module_names[pref] = module_name;
		return pref;
	}
	return it->second;
}

ks_no_ret ks_error_set_code_info(ks_str module_name, ks_uint64 local_code, ks_str title, ks_str description) {
	ks_uint16 prefix = ks_error_make_module_prefix(module_name);
	ks_uint64 registry_key = ((ks_uint64)prefix << 48) | (local_code & KS_ERROR_LOCAL_MASK);
	std::lock_guard<std::recursive_mutex> lock(sg_error_mutex);
	sg_code_registry[registry_key] = { title, description };
}

ks_str ks_error_get_code_info_desc(ks_uint64 full_code) {
	ks_uint16 owner = KS_ERR_GET_OWNER(full_code);
	ks_uint64 local = KS_ERR_GET_LOCAL(full_code);
	ks_uint64 lookup_key = ((ks_uint64)owner << 48) | (local & KS_ERROR_LOCAL_MASK);

	std::lock_guard<std::recursive_mutex> lock(sg_error_mutex);
	auto it = sg_code_registry.find(lookup_key);
	if (it != sg_code_registry.end()) {
		return it->second.description.c_str();
	}
	return "Unknown Error Code";
}

ks_str ks_error_get_code_info_name(ks_uint64 full_code) {
	ks_uint16 owner = KS_ERR_GET_OWNER(full_code);
	ks_uint32 local = KS_ERR_GET_LOCAL(full_code);
	ks_uint64 lookup_key = ((ks_uint64)owner << 48) | ((ks_uint64)local & KS_ERROR_LOCAL_MASK);

	std::lock_guard<std::recursive_mutex> lock(sg_error_mutex);
	auto it = sg_code_registry.find(lookup_key);
	if (it != sg_code_registry.end()) {
		return it->second.title.c_str();
	}
	return "Unknown Error Code";
}

KS_API ks_no_ret ks_error_push(Ks_Error_Level lvl, ks_str file, ks_size line, ks_str owner_prefix, ks_str source_prefix, ks_uint64 local_code, ks_str message) {
	ks_uint16 owner_id = ks_error_get_module_prefix(owner_prefix);
	ks_uint16 source_id = ks_error_get_module_prefix(source_prefix);
	
	Ks_Error_Info info = { 0 };

	info.code = ((ks_uint64)owner_id << 48) |
		((ks_uint64)source_id << 32) |
		((ks_uint64)lvl << 24) |
		(local_code & KS_ERROR_LOCAL_MASK);

	info.line = line;

	info.message = internal_strdup(message);
	info.file = internal_strdup(file);

	auto now = std::chrono::high_resolution_clock::now();
	info.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();

	std::lock_guard<std::recursive_mutex> lock(sg_error_mutex);
	sg_error_stack.push(info);
}

KS_API ks_no_ret ks_error_push_fmt(Ks_Error_Level lvl, ks_str file, ks_size line, ks_str owner_prefix, ks_str source_prefix, ks_uint64 local_code, ks_str message, ...) {
	ks_uint16 owner_id = ks_error_get_module_prefix(owner_prefix);
	ks_uint16 source_id = ks_error_get_module_prefix(source_prefix);
	
	Ks_Error_Info info = { 0 };
	info.line = line;
	info.file = internal_strdup(file);
	info.code = ((ks_uint64)owner_id << 48) |
		((ks_uint64)source_id << 32) |
		((ks_uint64)lvl << 24) |
		(local_code & 0x00FFFFFFULL);
	
	va_list args;
	va_start(args, message);

	va_list args_copy;
	va_copy(args_copy, args);
	int size = vsnprintf(nullptr, 0, message, args_copy);
	va_end(args_copy);

	if (size < 0) {
		va_end(args);
		return;
	}

	ks_str buffer = (ks_str)malloc(size + 1);
	vsnprintf((char*)buffer, size+1, message, args);
	va_end(args);

	info.message = buffer;
	auto now = std::chrono::high_resolution_clock::now();
	info.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();

	std::lock_guard<std::recursive_mutex> lock(sg_error_mutex);
	sg_error_stack.push(info);
}

KS_API ks_no_ret ks_error_pop_last() {
	std::lock_guard<std::recursive_mutex> lock(sg_error_mutex);
	Ks_Error_Info info = sg_error_stack.top();
	free((void*)info.file);
	free((void*)info.message);
	sg_error_stack.pop();
}

KS_API ks_size ks_error_count() {
	std::lock_guard<std::recursive_mutex> lock(sg_error_mutex);
	return sg_error_stack.size();
}

KS_API Ks_Error_Info ks_error_get_last_error() {
	std::lock_guard<std::recursive_mutex> lock(sg_error_mutex);
	return sg_error_stack.top();
}