#pragma once

#include "./script_engine.h"
#ifdef __cplusplus
extern "C" {
#endif
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#ifdef __cplusplus
}
#endif
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <string.h>
#include "../memory/memory.h"
#include "../core/log.h"

struct CallFrame {
	int arg_offset = 0;
	int upval_offset = 0;
};

struct UsertypeInfo {
	ks_size size;
	std::string name;
};

class KsScriptEngineCtx {
public:
	using Scope = std::vector<Ks_Script_Ref>;

	KsScriptEngineCtx(lua_State* state) : 
		p_state(state) ,
		p_error_info{
			.error = KS_SCRIPT_ERROR_NONE,
			.message = NULL
		}
	{
		p_scopes.emplace_back();
	}

	~KsScriptEngineCtx() {
		while (!p_scopes.empty()) {
			force_close_top_scope();
		}

		if (p_error_info.message != NULL) {
			ks_dealloc((void*)p_error_info.message);
		}

		lua_close(p_state);
	}

	ks_no_ret set_internal_error(Ks_Script_Error code, const std::string& msg) {
		p_error_info.error = code;
		if (p_error_info.message) ks_dealloc((void*)p_error_info.message);
		ks_size len = msg.length();
		p_error_info.message = (ks_str)ks_alloc_debug(
			(len + 1) * sizeof(char),
			KS_LT_USER_MANAGED,
			KS_TAG_GARBAGE,
			"ScriptCtxInternalErrorMessage"
		);
		memcpy((void*)p_error_info.message, msg.c_str(), len);
		char* data = (char*)p_error_info.message;
		data[len] = '\0';
	}

	ks_no_ret clear_error() {
		p_error_info.error = KS_SCRIPT_ERROR_NONE;
		if (p_error_info.message) ks_dealloc((void*)p_error_info.message);
		p_error_info.message = NULL;
	}

	lua_State* get_raw_state() { return p_state;  }

	ks_no_ret set_raw_state(lua_State* state) {
		p_state = state;
	}

	void begin_scope() {
		p_scopes.emplace_back();
	}

	void end_scope() {
		if (p_scopes.size() > 1) {
			force_close_top_scope();
		}
	}

	Ks_Script_Ref store_in_registry() {
		Ks_Script_Ref ref = luaL_ref(p_state, LUA_REGISTRYINDEX);
		p_scopes.back().push_back(ref);
		return ref;
	}

	ks_no_ret get_from_registry(Ks_Script_Ref ref) {
		lua_rawgeti(p_state, LUA_REGISTRYINDEX, ref);
	}

	ks_no_ret release_from_registry(Ks_Script_Ref ref) {
		for (auto it = p_scopes.rbegin(); it != p_scopes.rend(); ++it) {
			auto& scope = *it;
			auto found = std::find(scope.begin(), scope.end(), ref);
			if (found != scope.end()) {
				scope.erase(found);
				luaL_unref(p_state, LUA_REGISTRYINDEX, ref);
				return;
			}
		}
	}

	void promote_to_parent(Ks_Script_Ref ref) {

		if (p_scopes.size() <= 1) {
			return;
		}

		auto& current = p_scopes.back();
		auto it = std::find(current.begin(), current.end(), ref);
		if (it != current.end()) {
			current.erase(it);
			p_scopes[p_scopes.size() - 2].push_back(ref);
		}
	}

	void push_call_frame(int arg_off, int upval_off) {
		p_call_stack.push_back({ arg_off, upval_off });
	}

	void pop_call_frame() {
		if (!p_call_stack.empty()) p_call_stack.pop_back();
	}

	CallFrame current_frame() const {
		if (p_call_stack.empty()) return { 0, 0 };
		return p_call_stack.back();
	}

	void register_usertype_info(const std::string& name, UsertypeInfo info) {
		usertype_registry[name] = info;
	}

	UsertypeInfo* get_usertype_info(const std::string& name) {
		auto it = usertype_registry.find(name);
		if (it == usertype_registry.end()) return nullptr;
		return &it->second;
	}

	const Ks_Script_Error_Info& get_error_info() const { return p_error_info;  }

private:
	void force_close_top_scope() {
		if (p_scopes.empty()) return;
		Scope& current = p_scopes.back();for (Ks_Script_Ref ref : current) {
			if (ref != LUA_NOREF && ref != LUA_REFNIL) {
				luaL_unref(p_state, LUA_REGISTRYINDEX, ref);
			}
		}
		p_scopes.pop_back();
	}

private:
	lua_State* p_state = nullptr;
	Ks_Script_Error_Info p_error_info;
	std::vector<Scope> p_scopes;
	std::vector<CallFrame> p_call_stack;
	std::map<std::string, UsertypeInfo> usertype_registry;
};

struct MethodInfo {
	std::string name;
	ks_script_cfunc func;
	std::vector<Ks_Type> signature;
};

struct PropertyInfo {
	std::string name;
	ks_script_cfunc getter;
	ks_script_cfunc setter;
};

struct FieldInfo {
	std::string name;
	Ks_Type type;
	ks_size offset;
	std::string type_name;
};

struct KsUsertypeBuilder {
	Ks_Script_Ctx ctx;
	std::string type_name;
	std::string base_type_name;
	
	size_t instance_size = 0;

	std::vector<MethodInfo> constructors;
	ks_script_deallocator destructor = nullptr;

	std::map<std::string, std::vector<MethodInfo>> methods;
	std::map<std::string, std::vector<MethodInfo>> static_methods;
	std::vector<PropertyInfo> properties;
	std::vector<FieldInfo> fields;
	std::map<Ks_Script_Metamethod, ks_script_cfunc> metamethods;

	KsUsertypeBuilder(Ks_Script_Ctx c, const char* name, size_t instance_size) : 
		ctx(c), type_name(name) , instance_size(instance_size)
	{}
};