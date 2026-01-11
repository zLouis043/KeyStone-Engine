#pragma once 

#include "core/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef ks_ptr Ks_Ecs_World;
typedef ks_uint64 Ks_Entity;

KS_API extern const Ks_Entity KS_PHASE_ON_LOAD;
KS_API extern const Ks_Entity KS_PHASE_POST_LOAD;
KS_API extern const Ks_Entity KS_PHASE_PRE_UPDATE;
KS_API extern const Ks_Entity KS_PHASE_ON_UPDATE;
KS_API extern const Ks_Entity KS_PHASE_POST_UPDATE;
KS_API extern const Ks_Entity KS_PHASE_PRE_STORE;
KS_API extern const Ks_Entity KS_PHASE_ON_STORE;

typedef enum {
    KS_EVENT_ON_ADD,
    KS_EVENT_ON_REMOVE,
    KS_EVENT_ON_SET
} Ks_Ecs_Event;

typedef void (*Ks_System_Func)(Ks_Ecs_World world, Ks_Entity entity, void* user_data);

KS_API Ks_Ecs_World ks_ecs_create_world(void);
KS_API void     ks_ecs_destroy_world(Ks_Ecs_World world);
KS_API void     ks_ecs_progress(Ks_Ecs_World world, float delta_time);

KS_API Ks_Entity ks_ecs_create_entity(Ks_Ecs_World world, const char* name);
KS_API void      ks_ecs_destroy_entity(Ks_Ecs_World world, Ks_Entity entity);
KS_API void      ks_ecs_enable_entity(Ks_Ecs_World world, Ks_Entity entity, bool enabled);
KS_API bool      ks_ecs_is_alive(Ks_Ecs_World world, Ks_Entity entity);
KS_API const char* ks_ecs_get_name(Ks_Ecs_World world, Ks_Entity entity);
KS_API Ks_Entity ks_ecs_lookup(Ks_Ecs_World world, const char* name);

KS_API void ks_ecs_set_component(Ks_Ecs_World world, Ks_Entity entity, const char* type_name, const void* data);
KS_API const void* ks_ecs_get_component(Ks_Ecs_World world, Ks_Entity entity, const char* type_name);
KS_API void* ks_ecs_get_component_mut(Ks_Ecs_World world, Ks_Entity entity, const char* type_name);
KS_API void ks_ecs_remove_component(Ks_Ecs_World world, Ks_Entity entity, const char* type_name);
KS_API bool ks_ecs_has_component(Ks_Ecs_World world, Ks_Entity entity, const char* type_name);

KS_API void ks_ecs_add_child(Ks_Ecs_World world, Ks_Entity parent, Ks_Entity child);
KS_API void ks_ecs_remove_child(Ks_Ecs_World world, Ks_Entity parent, Ks_Entity child);
KS_API Ks_Entity ks_ecs_get_parent(Ks_Ecs_World world, Ks_Entity child);

KS_API Ks_Entity ks_ecs_create_prefab(Ks_Ecs_World world, const char* name);
KS_API Ks_Entity ks_ecs_get_prefab(Ks_Ecs_World world, const char* name);
KS_API Ks_Entity ks_ecs_instantiate(Ks_Ecs_World world, Ks_Entity prefab);

KS_API void  ks_ecs_set_global(Ks_Ecs_World world, const char* type_name, const void* data);
KS_API void* ks_ecs_get_global(Ks_Ecs_World world, const char* type_name);

KS_API void ks_ecs_create_system(Ks_Ecs_World world, const char* name, const char* filter, Ks_Entity phase_id, Ks_System_Func func, void* user_data);
KS_API void ks_ecs_run_query(Ks_Ecs_World world, const char* filter, Ks_System_Func func, void* user_data);
KS_API void ks_ecs_enable_system(Ks_Ecs_World world, Ks_Entity system, bool enabled);
KS_API void ks_ecs_create_observer(Ks_Ecs_World world, Ks_Ecs_Event trigger, const char* component, Ks_System_Func func, void* user_data);

#ifdef __cplusplus
}
#endif