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
#include "../memory/memory.h"

class KsScriptEngineCtx {
public:
	KsScriptEngineCtx(lua_State* state) : 
		p_state(state) ,
		p_error_info{
			.error = KS_SCRIPT_ERROR_NONE,
			.message = NULL
		}
	{}

	~KsScriptEngineCtx() {
		cleanup_registry();
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

	Ks_Script_Ref store_in_registry() {
		Ks_Script_Ref ref = luaL_ref(p_state, LUA_REGISTRYINDEX);
		p_registry_refs.push_back(ref);
		return ref;
	}

	ks_no_ret get_from_registry(Ks_Script_Ref ref) {
		lua_rawgeti(p_state, LUA_REGISTRYINDEX, ref);
	}

	ks_no_ret release_from_registry(Ks_Script_Ref ref) {
		luaL_unref(p_state, LUA_REGISTRYINDEX, ref);
	}

	ks_no_ret cleanup_registry() {
		for (Ks_Script_Ref ref : p_registry_refs) {
			if (ref != LUA_REFNIL && ref != LUA_NOREF) {
				luaL_unref(p_state, LUA_REGISTRYINDEX, ref);
			}
		}
		p_registry_refs.clear();
	}

	const Ks_Script_Error_Info& get_error_info() const { return p_error_info;  }

private:
	lua_State* p_state = nullptr;
	Ks_Script_Error_Info p_error_info;
	std::vector<Ks_Script_Ref> p_registry_refs;
};