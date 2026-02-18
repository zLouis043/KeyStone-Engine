#include "../include/ecs/ecs.h"
#include "../include/core/reflection.h"
#include "../include/core/log.h"
#include "../include/core/error.h"
#include "../include/memory/memory.h"

#include <flecs.h>

#include <unordered_map>
#include <string>
#include <string.h>

extern "C" {
    const Ks_Entity KS_PHASE_ON_LOAD = (Ks_Entity)EcsOnLoad;
    const Ks_Entity KS_PHASE_POST_LOAD = (Ks_Entity)EcsPostLoad;
    const Ks_Entity KS_PHASE_PRE_UPDATE = (Ks_Entity)EcsPreUpdate;
    const Ks_Entity KS_PHASE_ON_UPDATE = (Ks_Entity)EcsOnUpdate;
    const Ks_Entity KS_PHASE_POST_UPDATE = (Ks_Entity)EcsPostUpdate;
    const Ks_Entity KS_PHASE_PRE_STORE = (Ks_Entity)EcsPreStore;
    const Ks_Entity KS_PHASE_ON_STORE = (Ks_Entity)EcsOnStore;
}

enum ECSErros {
    SYSTEM_CREATION_FAIL,
    QUERY_CREATION_FAIL
};

struct Ks_Ecs_World_Impl {
    ecs_world_t* ecs;
    std::unordered_map<std::string, ecs_entity_t> component_ids;
    std::unordered_map<ecs_entity_t, const Ks_Type_Info*> ids_to_type_info;

    ~Ks_Ecs_World_Impl() {
        if (ecs) ecs_fini(ecs);
    }
};

Ks_Ecs_World_Impl* get(Ks_Ecs_World world) {
    return static_cast<Ks_Ecs_World_Impl*>(world);
}

static ecs_entity_t get_component_id(Ks_Ecs_World world, const char* type_name) {
    auto w = get(world);
    auto it = w->component_ids.find(type_name);
    if (it != w->component_ids.end()) return it->second;

    ecs_entity_t id = 0;
    const Ks_Type_Info* info = ks_reflection_get_type(type_name);

    ecs_component_desc_t component_desc = { 0 };
    ecs_entity_desc_t entity_desc = { 0 };

    entity_desc.name = info ? info->name : type_name;
    entity_desc.symbol = type_name;

    component_desc.entity = ecs_entity_init(w->ecs, &entity_desc);
    if (info) {
        component_desc.type.size = (ecs_size_t)info->size;
        component_desc.type.alignment = (ecs_size_t)info->alignment;
    }
    else {
        component_desc.type.size = (ecs_size_t)sizeof(ks_int);
        component_desc.type.alignment = (ecs_size_t)alignof(ks_int);
    }

    id = ecs_component_init(w->ecs, &component_desc);

    w->component_ids[type_name] = id;
    w->ids_to_type_info[id] = info;

    return id;
}

Ks_Ecs_World ks_ecs_create_world() {
    void* mem = ks_alloc_debug(sizeof(Ks_Ecs_World_Impl), KS_LT_USER_MANAGED, KS_TAG_INTERNAL_DATA, "KsEcsWorld");
    Ks_Ecs_World_Impl* w = new(mem) Ks_Ecs_World_Impl();
    w->ecs = ecs_init();
    return w;
}

void ks_ecs_destroy_world(Ks_Ecs_World world) {
    if (world) {
        auto w = get(world);
        w->~Ks_Ecs_World_Impl();
        ks_dealloc(w);
    }
}

void ks_ecs_progress(Ks_Ecs_World world, float delta_time) {
    if (world) ecs_progress(get(world)->ecs, delta_time);
}

Ks_Entity ks_ecs_create_entity(Ks_Ecs_World world, const char* name) {
    ecs_entity_desc_t ent_desc = { 0 };
    ent_desc.name = name;
    return (Ks_Entity)ecs_entity_init(get(world)->ecs, &ent_desc);
}

void ks_ecs_destroy_entity(Ks_Ecs_World world, Ks_Entity entity) {
    ecs_delete(get(world)->ecs, (ecs_entity_t)entity);
}

void ks_ecs_enable_entity(Ks_Ecs_World world, Ks_Entity entity, bool enabled){
    ecs_enable(get(world)->ecs, (ecs_entity_t)entity, enabled);
}

bool ks_ecs_is_alive(Ks_Ecs_World world, Ks_Entity entity) {
    return ecs_is_alive(get(world)->ecs, (ecs_entity_t)entity);
}

const char* ks_ecs_get_name(Ks_Ecs_World world, Ks_Entity entity) {
    return ecs_get_name(get(world)->ecs, (ecs_entity_t)entity);
}

Ks_Entity ks_ecs_lookup(Ks_Ecs_World world, const char* name){
    return (Ks_Entity)ecs_lookup(get(world)->ecs, name);
}

void ks_ecs_set_component(Ks_Ecs_World world, Ks_Entity entity, const char* type_name, const void* data) {
    ecs_entity_t id = get_component_id(world, type_name);
    const Ks_Type_Info* info = get(world)->ids_to_type_info[id];
    size_t size = info ? info->size : sizeof(ks_int);

    ecs_set_id(get(world)->ecs, (ecs_entity_t)entity, id, size, data);
}

const void* ks_ecs_get_component(Ks_Ecs_World world, Ks_Entity entity, const char* type_name) {
    ecs_entity_t id = get_component_id(world, type_name);
    return ecs_get_id(get(world)->ecs, (ecs_entity_t)entity, id);
}

void* ks_ecs_get_component_mut(Ks_Ecs_World world, Ks_Entity entity, const char* type_name) {
    ecs_entity_t id = get_component_id(world, type_name);
    return ecs_get_mut_id(get(world)->ecs, (ecs_entity_t)entity, id);
}

void ks_ecs_remove_component(Ks_Ecs_World world, Ks_Entity entity, const char* type_name) {
    auto w = get(world);
    auto it = w->component_ids.find(type_name);
    if (it != w->component_ids.end()) {
        ecs_remove_id(w->ecs, (ecs_entity_t)entity, it->second);
    }
}

bool ks_ecs_has_component(Ks_Ecs_World world, Ks_Entity entity, const char* type_name) {
    ecs_entity_t id = get_component_id(world, type_name);
    if (!id) return false;
    const void* ptr = ecs_get_id(get(world)->ecs, (ecs_entity_t)entity, id);
    return ptr != nullptr;
}

void ks_ecs_add_child(Ks_Ecs_World world, Ks_Entity parent, Ks_Entity child) {
    ecs_add_pair(get(world)->ecs, (ecs_entity_t)child, EcsChildOf, (ecs_entity_t)parent);
}

void ks_ecs_remove_child(Ks_Ecs_World world, Ks_Entity parent, Ks_Entity child) {
    ecs_remove_pair(get(world)->ecs, (ecs_entity_t)child, EcsChildOf, (ecs_entity_t)parent);
}

Ks_Entity ks_ecs_get_parent(Ks_Ecs_World world, Ks_Entity child) {
    return (Ks_Entity)ecs_get_target(get(world)->ecs, (ecs_entity_t)child, EcsChildOf, 0);
}

Ks_Entity ks_ecs_create_prefab(Ks_Ecs_World world, const char* name) {
    ecs_entity_desc_t desc = { 0 };
    desc.name = name;
    ecs_id_t add[] = { EcsPrefab, 0 };
    desc.add = add;
    return (Ks_Entity)ecs_entity_init(get(world)->ecs, &desc);
}

Ks_Entity ks_ecs_get_prefab(Ks_Ecs_World world, const char* name){
    ecs_entity_t e = ecs_lookup(get(world)->ecs, name);
    if (e && ecs_has_id(get(world)->ecs, e, EcsPrefab)) {
        return (Ks_Entity)e;
    }
    return 0;
}

Ks_Entity ks_ecs_instantiate(Ks_Ecs_World world, Ks_Entity prefab) {
    auto w = get(world);
    ecs_id_t relation_pair = ecs_pair(EcsIsA, (ecs_entity_t)prefab);
    return (Ks_Entity)ecs_new_w_id(w->ecs, relation_pair);
}

void ks_ecs_set_global(Ks_Ecs_World world, const char* type_name, const void* data) {
    ecs_entity_t id = get_component_id(world, type_name);

    size_t size = sizeof(int);
    auto it = get(world)->ids_to_type_info.find(id);
    if (it != get(world)->ids_to_type_info.end() && it->second) {
        size = it->second->size;
    }

    ecs_set_id(get(world)->ecs, id, id, size, data);
}

void* ks_ecs_get_global(Ks_Ecs_World world, const char* type_name) {
    ecs_entity_t id = get_component_id(world, type_name);
    return ecs_get_mut_id(get(world)->ecs, id, id);
}

struct SysCtx { Ks_System_Func cb; void* ud; Ks_Ecs_World w; };

static void sys_trampoline(ecs_iter_t* it) {
    SysCtx* ctx = (SysCtx*)it->ctx;
    for (int i = 0; i < it->count; ++i) {
        ctx->cb(ctx->w, (Ks_Entity)it->entities[i], ctx->ud);
    }
}

void ks_ecs_create_system(Ks_Ecs_World world, const char* name, const char* filter, Ks_Entity phase_id, Ks_System_Func func, void* user_data) {
    auto w = get(world);
    SysCtx* ctx = new SysCtx{ func, user_data, world };

    ecs_system_desc_t sys_desc = { 0 };
    ecs_entity_desc_t ent_desc = { 0 };
    ent_desc.name = name;
    sys_desc.entity = ecs_entity_init(w->ecs, &ent_desc);;
    sys_desc.query.expr = filter;
    sys_desc.query.flags = EcsQueryAllowUnresolvedByName;
    sys_desc.callback = sys_trampoline;
    sys_desc.ctx = ctx;

    ecs_entity_t sys_entity = ecs_system_init(w->ecs, &sys_desc);

    if (sys_entity) {
        if (phase_id != 0) {
            ecs_add_pair(w->ecs, sys_entity, EcsDependsOn, (ecs_entity_t)phase_id);
        }
        else {
            ecs_add_pair(w->ecs, sys_entity, EcsDependsOn, EcsOnUpdate);
        }
        ecs_enable(w->ecs, sys_entity, true);
    }
    else {
        ks_epush_s_fmt(KS_ERROR_LEVEL_BASE, "ECS", ECSErros::SYSTEM_CREATION_FAIL, "Failed to create system '%s'", name);
    }
}

void ks_ecs_run_query(Ks_Ecs_World world, const char* filter, Ks_System_Func func, void* user_data){
    auto w = get(world);
    ecs_query_desc_t desc = { 0 };
    desc.expr = filter;
    desc.flags = EcsQueryAllowUnresolvedByName;

    ecs_query_t* q = ecs_query_init(w->ecs, &desc);
    if (!q) {
        ks_epush_s_fmt(KS_ERROR_LEVEL_BASE, "ECS", ECSErros::QUERY_CREATION_FAIL, "Failed to create query for filter '%s'", filter);
        return;
    }

    SysCtx ctx = { func, user_data, world };
    ecs_iter_t it = ecs_query_iter(w->ecs, q);
    while (ecs_query_next(&it)) {
        for (int i = 0; i < it.count; ++i) {
            ctx.cb(ctx.w, (Ks_Entity)it.entities[i], ctx.ud);
        }
    }

    ecs_query_fini(q);
}

void ks_ecs_enable_system(Ks_Ecs_World world, Ks_Entity system, bool enabled){
    ecs_enable(get(world)->ecs, (ecs_entity_t)system, enabled);
}

void ks_ecs_create_observer(Ks_Ecs_World world, Ks_Ecs_Event trigger, const char* component, Ks_System_Func func, void* user_data) {
    auto w = get(world);
    
    get_component_id(world, component);

    SysCtx* ctx = new SysCtx{ func, user_data, world };

    ecs_observer_desc_t desc = { 0 };
    desc.query.expr = component;
    desc.callback = sys_trampoline;
    desc.ctx = ctx;

    if (trigger == KS_EVENT_ON_ADD) desc.events[0] = EcsOnAdd;
    else if (trigger == KS_EVENT_ON_REMOVE) desc.events[0] = EcsOnRemove;
    else if (trigger == KS_EVENT_ON_SET) desc.events[0] = EcsOnSet;

    ecs_observer_init(w->ecs, &desc);
}