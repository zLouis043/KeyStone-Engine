#include "../include/ecs/ecs_binding.h"
#include "../include/core/log.h"
#include "../include/core/reflection.h"
#include "../include/memory/memory.h"
#include <string.h>
#include <vector>
#include <string>
#include <string.h>
#include <mutex>
#include <unordered_set>
#include <map>

typedef struct ScriptComponent {
    int ref;
} ScriptComponent;

struct EntityHandle {
    Ks_Ecs_World world;
    Ks_Entity id;
};

struct ScriptCleanupCtx {
    Ks_Script_Ctx ctx;
    char type_name[64];
};

static std::mutex s_script_types_mutex;
static std::vector<std::string> s_script_component_types;
static std::unordered_set<std::string> g_registered_observers;
static std::vector<ScriptCleanupCtx*> s_cleanup_contexts;

static void on_script_component_remove(Ks_Ecs_World w, Ks_Entity e, void* user_data) {
    ScriptCleanupCtx* clean_ctx = (ScriptCleanupCtx*)user_data;

    const void* data = ks_ecs_get_component(w, e, clean_ctx->type_name);

    if (data) {
        auto* wrapper = (ScriptComponent*)data;
        if (wrapper->ref != KS_SCRIPT_NO_REF) {

            Ks_Script_Object obj;
            obj.state = KS_SCRIPT_OBJECT_VALID;
            obj.type = KS_TYPE_SCRIPT_TABLE;
            obj.val.table_ref = wrapper->ref;
            ks_script_free_obj(clean_ctx->ctx, obj);
            wrapper->ref = KS_SCRIPT_NO_REF;
        }
    }
}

KS_API ks_no_ret ks_ecs_lua_shutdown(Ks_Ecs_World world) {
    std::lock_guard<std::mutex> lock(s_script_types_mutex);

    for (auto* ctx_ptr : s_cleanup_contexts) {
        ks_dealloc(ctx_ptr);
    }
    s_cleanup_contexts.clear();

    s_script_component_types.clear();
    g_registered_observers.clear();
}

static void apply_components_from_table(Ks_Script_Ctx ctx, Ks_Ecs_World world, Ks_Entity entity, Ks_Script_Object list_obj) {
    if (!ks_script_obj_is(ctx, list_obj, KS_TYPE_SCRIPT_TABLE)) return;

    ks_size len = ks_script_table_array_size(ctx, list_obj);

    for (ks_size i = 1; i <= len; ++i) {
        ks_script_begin_scope(ctx);
        Ks_Script_Object index = ks_script_create_integer(ctx, (ks_int64)i);
        Ks_Script_Object item = ks_script_table_get(ctx, list_obj, index);

        if (ks_script_obj_is(ctx, item, KS_TYPE_USERDATA)) {
            ks_str type_name = ks_script_obj_get_usertype_name(ctx, item);
            void* ptr = ks_script_usertype_get_ptr(ctx, item);

            if (type_name && ptr) {
                ks_ecs_set_component(world, entity, type_name, ptr);
            }
            else {
                KS_LOG_WARN("Lua ECS: Invalid UserData component passed in definition");
            }
        }
        
        else if (ks_script_obj_is(ctx, item, KS_TYPE_SCRIPT_TABLE)) {
            
            Ks_Script_Object key_type = ks_script_create_cstring(ctx, "_type");
            Ks_Script_Object val_type = ks_script_table_get(ctx, item, key_type);

            if (ks_script_obj_is_valid(ctx, val_type)) {
                ks_str type_name = ks_script_obj_as_cstring(ctx, val_type);
                Ks_Script_Object ref_obj = ks_script_ref_obj(ctx, item);
                ks_script_promote(ctx, ref_obj);

                ScriptComponent wrapper;
                wrapper.ref = ref_obj.val.table_ref;
                ks_ecs_set_component(world, entity, type_name, &wrapper);
            }
            else {
                KS_LOG_WARN("Lua ECS: Table in definition missing '_type'");
            }
            ;
        }
        ks_script_end_scope(ctx);
       
    }
}

static int clone_script_component_data(Ks_Script_Ctx ctx, int old_ref) {
    if (old_ref == KS_SCRIPT_NO_REF) return KS_SCRIPT_NO_REF;

    Ks_Script_Object old_tbl;
    old_tbl.type = KS_TYPE_SCRIPT_TABLE;
    old_tbl.state = KS_SCRIPT_OBJECT_VALID;
    old_tbl.val.table_ref = old_ref;

    ks_script_begin_scope(ctx);

    Ks_Script_Object new_tbl = ks_script_create_table(ctx);

    Ks_Script_Table_Iterator it = ks_script_table_iterate(ctx, old_tbl);

    while (ks_script_iterator_has_next(ctx, &it)) {
        Ks_Script_Object key, val;
        if (ks_script_iterator_next(ctx, &it, &key, &val)) {
            ks_script_table_set(ctx, new_tbl, key, val);
        }
    }

    ks_script_iterator_destroy(ctx, &it);

    Ks_Script_Object ref_obj = ks_script_ref_obj(ctx, new_tbl);
    ks_script_promote(ctx, ref_obj);

    auto result = ref_obj.val.table_ref;

    ks_script_end_scope(ctx);

    return result;
}

static ks_returns_count l_component_ctor(Ks_Script_Ctx ctx) {
    Ks_Script_Object cls = ks_script_get_arg(ctx, 1);
    Ks_Script_Object instance = ks_script_get_arg(ctx, 2);

    if (!ks_script_obj_is(ctx, instance, KS_TYPE_SCRIPT_TABLE)) {
        instance = ks_script_create_table(ctx);
    }

    Ks_Script_Object key_type = ks_script_create_cstring(ctx, "_type");
    Ks_Script_Object type_name = ks_script_table_get(ctx, cls, key_type);

    ks_script_table_set(ctx, instance, key_type, type_name);

    ks_script_stack_push_obj(ctx, instance);
    return 1;
}


static ks_returns_count l_ecs_Component(Ks_Script_Ctx ctx) {
    Ks_Script_Object upval = ks_script_get_upvalue(ctx, 1);
    Ks_Ecs_World world = (Ks_Ecs_World)ks_script_lightuserdata_get_ptr(ctx, upval);

    Ks_Script_Object name_obj = ks_script_get_arg(ctx, 1);
    const char* name = ks_script_obj_as_cstring(ctx, name_obj);

    if (g_registered_observers.find(name) == g_registered_observers.end()) {
        ScriptCleanupCtx* clean_ctx = (ScriptCleanupCtx*)ks_alloc_debug(
            sizeof(ScriptCleanupCtx),
            KS_LT_USER_MANAGED,
            KS_TAG_SCRIPT,
            "ScriptCleanupCtx"
        );
        clean_ctx->ctx = ctx;
        memcpy(clean_ctx->type_name, name, 63);
        clean_ctx->type_name[63] = '\0';

        ks_ecs_create_observer(world, KS_EVENT_ON_REMOVE, name,
            on_script_component_remove, clean_ctx);

        g_registered_observers.insert(name);
        s_cleanup_contexts.push_back(clean_ctx);
    }

    Ks_Script_Table cls = ks_script_create_table(ctx);

    {
        ks_script_begin_scope(ctx);
        Ks_Script_Object key_type = ks_script_create_cstring(ctx, "_type");
        ks_script_table_set(ctx, cls, key_type, name_obj);
        ks_script_end_scope(ctx);
    }

    Ks_Script_Table mt = ks_script_create_table(ctx);


    Ks_Script_Sig_Def sig = KS_SCRIPT_SIG_DEF(l_component_ctor, KS_TYPE_SCRIPT_TABLE, KS_TYPE_SCRIPT_TABLE);
    Ks_Script_Function ctor = ks_script_create_cfunc(ctx, &sig, 1);

    {
        ks_script_begin_scope(ctx);
        Ks_Script_Object key_call = ks_script_create_cstring(ctx, "__call");
        ks_script_table_set(ctx, mt, key_call, ctor);
        ks_script_end_scope(ctx);
    }

    {
        std::lock_guard<std::mutex> lock(s_script_types_mutex);
        s_script_component_types.push_back(name);
    }

    ks_script_obj_set_metatable(ctx, cls, mt);

    ks_script_stack_push_obj(ctx, cls);
    return 1;
}

static void push_entity_handle(Ks_Script_Ctx ctx, Ks_Ecs_World w, Ks_Entity id) {
    Ks_Script_Userdata ud = ks_script_create_usertype_instance(ctx, "EntityHandle");
    EntityHandle* handle = (EntityHandle*)ks_script_usertype_get_ptr(ctx, ud);

    handle->world = w;
    handle->id = id;

    ks_script_stack_push_obj(ctx, ud);
}

static ks_returns_count l_ecs_Entity(Ks_Script_Ctx ctx) {
    Ks_Script_Object upval = ks_script_get_upvalue(ctx, 1);
    Ks_Ecs_World world = (Ks_Ecs_World)ks_script_lightuserdata_get_ptr(ctx, upval);

    const char* name = ks_script_obj_as_cstring(ctx, ks_script_get_arg(ctx, 1));
    Ks_Script_Object components = ks_script_get_arg(ctx, 2);

    Ks_Entity e = ks_ecs_create_entity(world, name);

    if (ks_script_obj_is(ctx, components, KS_TYPE_SCRIPT_TABLE)) {
        apply_components_from_table(ctx, world, e, components);
    }

    push_entity_handle(ctx, world, e);
    return 1;
}

static ks_returns_count l_ecs_Prefab(Ks_Script_Ctx ctx) {
    Ks_Script_Object upval = ks_script_get_upvalue(ctx, 1);
    Ks_Ecs_World world = (Ks_Ecs_World)ks_script_lightuserdata_get_ptr(ctx, upval);

    const char* name = ks_script_obj_as_cstring(ctx, ks_script_get_arg(ctx, 1));
    Ks_Script_Object components = ks_script_get_arg(ctx, 2);

    Ks_Entity e = ks_ecs_create_prefab(world, name);

    if (ks_script_obj_is(ctx, components, KS_TYPE_SCRIPT_TABLE)) {
        apply_components_from_table(ctx, world, e, components);
    }

    push_entity_handle(ctx, world, e);
    return 1;
}

static ks_returns_count l_ecs_create_instance(Ks_Script_Ctx ctx) {
    Ks_Script_Object upval = ks_script_get_upvalue(ctx, 1);
    Ks_Ecs_World world = (Ks_Ecs_World)ks_script_lightuserdata_get_ptr(ctx, upval);

    Ks_Script_Object arg1 = ks_script_get_arg(ctx, 1);
    Ks_Entity prefab_id = 0;

    if (ks_script_obj_is(ctx, arg1, KS_TYPE_CSTRING)) {
        prefab_id = ks_ecs_lookup(world, ks_script_obj_as_cstring(ctx, arg1));
    }
    else if (ks_script_obj_is(ctx, arg1, KS_TYPE_USERDATA)) {
        EntityHandle* h = (EntityHandle*)ks_script_usertype_get_ptr(ctx, arg1);
        if (h) prefab_id = h->id;
    }

    if (prefab_id) {
        Ks_Entity inst = ks_ecs_instantiate(world, prefab_id);

        {
            std::vector<std::string> types_to_check;
            {
                std::lock_guard<std::mutex> lock(s_script_types_mutex);
                types_to_check = s_script_component_types;
            }

            for (const auto& type_name : types_to_check) {
                if (ks_ecs_has_component(world, inst, type_name.c_str())) {
                    ScriptComponent* data = (ScriptComponent*)ks_ecs_get_component_mut(world, inst, type_name.c_str());

                    if (data && data->ref != KS_SCRIPT_NO_REF) {
                        int new_ref = clone_script_component_data(ctx, data->ref);
                        data->ref = new_ref;
                    }
                }
            }
        }

        push_entity_handle(ctx, world, inst);
        return 1;
    }

    ks_script_stack_push_obj(ctx, ks_script_create_nil(ctx));
    return 1;
}

static ks_returns_count l_entity_add(Ks_Script_Ctx ctx) {
    EntityHandle* ent = (EntityHandle*)ks_script_get_self(ctx);

    Ks_Script_Object item = ks_script_get_arg(ctx, 1);

    if (ks_script_obj_is(ctx, item, KS_TYPE_USERDATA)) {
        ks_str type = ks_script_obj_get_usertype_name(ctx, item);
        KS_LOG_INFO("type = %s", type);
        void* ptr = ks_script_usertype_get_ptr(ctx, item);
        if (type && ptr) ks_ecs_set_component(ent->world, ent->id, type, ptr);
    }
    else if (ks_script_obj_is(ctx, item, KS_TYPE_SCRIPT_TABLE)) {
        Ks_Script_Object key_type = ks_script_create_cstring(ctx, "_type");
        Ks_Script_Object val_type = ks_script_table_get(ctx, item, key_type);

        if (ks_script_obj_is_valid(ctx, val_type)) {
            ks_str type = ks_script_obj_as_cstring(ctx, val_type);

            Ks_Script_Object ref_obj = ks_script_ref_obj(ctx, item);

            ScriptComponent wrapper;
            wrapper.ref = ref_obj.val.table_ref;

            ks_ecs_set_component(ent->world, ent->id, type, &wrapper);
        }
    }
    Ks_Script_Object self = ks_script_create_usertype_ref(ctx, "EntityHandle", ent);

    ks_script_stack_push_obj(ctx, self);
    return 1;
}

static ks_returns_count l_entity_get(Ks_Script_Ctx ctx) {
    EntityHandle* ent = (EntityHandle*)ks_script_get_self(ctx);

    Ks_Script_Object name_obj = ks_script_get_arg(ctx, 1);
    const char* type_name = ks_script_obj_as_cstring(ctx, name_obj);

    void* ptr = ks_ecs_get_component_mut(ent->world, ent->id, type_name);

    if (!ptr) {
        ks_script_stack_push_obj(ctx, ks_script_create_nil(ctx));
        return 1;
    }

    const Ks_Type_Info* info = ks_reflection_get_type(type_name);
    if (info) {
        Ks_Script_Object ref = ks_script_create_usertype_ref(ctx, type_name, ptr);
        ks_script_stack_push_obj(ctx, ref);
    }
    else {
        auto* wrapper = (ScriptComponent*)ptr;

        Ks_Script_Object tbl;
        tbl.type = KS_TYPE_SCRIPT_TABLE;
        tbl.state = KS_SCRIPT_OBJECT_VALID;
        tbl.val.table_ref = wrapper->ref;

        ks_script_stack_push_obj(ctx, tbl);
    }
    return 1;
}

static ks_returns_count l_entity_has(Ks_Script_Ctx ctx) {
    EntityHandle* ent = (EntityHandle*)ks_script_get_self(ctx);

    const char* type_name = ks_script_obj_as_cstring(ctx, ks_script_get_arg(ctx, 1));

    ks_bool has = ks_ecs_has_component(ent->world, ent->id, type_name);
    ks_script_stack_push_boolean(ctx, has);
    return 1;
}

static ks_returns_count l_entity_destroy(Ks_Script_Ctx ctx) {
    EntityHandle* ent = (EntityHandle*)ks_script_get_self(ctx);

    if (ks_ecs_is_alive(ent->world, ent->id)) {
        {
            std::lock_guard<std::mutex> lock(s_script_types_mutex);

            for (const auto& type_name : s_script_component_types) {
                if (ks_ecs_has_component(ent->world, ent->id, type_name.c_str())) {
                    ScriptComponent* data = (ScriptComponent*)ks_ecs_get_component_mut(ent->world, ent->id, type_name.c_str());

                    if (data && data->ref != KS_SCRIPT_NO_REF) {
                        Ks_Script_Object obj;
                        obj.type = KS_TYPE_SCRIPT_TABLE;
                        obj.state = KS_SCRIPT_OBJECT_VALID;
                        obj.val.table_ref = data->ref;

                        ks_script_free_obj(ctx, obj);
                        data->ref = KS_SCRIPT_NO_REF;
                    }
                }
            }
        }
        ks_ecs_destroy_entity(ent->world, ent->id);
        ent->id = 0;
    }
    return 0;
}


KS_API ks_no_ret ks_ecs_lua_bind(Ks_Ecs_World world, Ks_Script_Ctx ctx) {
    Ks_Script_Usertype_Builder b = ks_script_usertype_begin(ctx, "EntityHandle", sizeof(EntityHandle));

    ks_script_usertype_add_method(b, "add", KS_SCRIPT_OVERLOAD(
        KS_SCRIPT_SIG_DEF(l_entity_add, KS_TYPE_USERDATA),
        KS_SCRIPT_SIG_DEF(l_entity_add, KS_TYPE_SCRIPT_TABLE)
    ));

    ks_script_usertype_add_method(b, "get", KS_SCRIPT_FUNC(l_entity_get, KS_TYPE_CSTRING));
    ks_script_usertype_add_method(b, "has", KS_SCRIPT_FUNC(l_entity_has, KS_TYPE_CSTRING));
    ks_script_usertype_add_method(b, "destroy", KS_SCRIPT_FUNC_VOID(l_entity_destroy));

    ks_script_usertype_end(b);

    Ks_Script_Table ecs_table = ks_script_create_named_table(ctx, "ecs");

    auto register_ecs_func = [&](const char* name, ks_script_cfunc f) {
        ks_script_stack_push_obj(ctx, ks_script_create_lightuserdata(ctx, world));

        Ks_Script_Sig_Def sig = { f, nullptr, 0 };

        Ks_Script_Function func = ks_script_create_cfunc_with_upvalues(ctx, &sig, 1, 1);

        Ks_Script_Object key = ks_script_create_cstring(ctx, name);
        ks_script_table_set(ctx, ecs_table, key, func);
    };

    register_ecs_func("Entity", l_ecs_Entity);
    register_ecs_func("Prefab", l_ecs_Prefab);
    register_ecs_func("Component", l_ecs_Component);
    register_ecs_func("create_instance", l_ecs_create_instance);
}