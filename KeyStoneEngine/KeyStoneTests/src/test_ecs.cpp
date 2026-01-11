#include <doctest/doctest.h>
#include <keystone.h>
#include <string.h>

struct Position { float x, y; };
struct Velocity { float x, y; };
struct Health { int hp; };
struct GameConfig { float gravity; int max_players; };

void reflect_components() {
    if (!ks_reflection_get_type(ks_type_id(Position))) {
        ks_reflect_struct(Position,
            ks_reflect_field(float, x),
            ks_reflect_field(float, y)
        );
    }

    if (!ks_reflection_get_type(ks_type_id(Velocity))) {
        ks_reflect_struct(Velocity,
            ks_reflect_field(float, x),
            ks_reflect_field(float, y)
        );
    }

    if (!ks_reflection_get_type(ks_type_id(Health))) {
        ks_reflect_struct(Health,
            ks_reflect_field(int, hp)
        );
    }

    if (!ks_reflection_get_type(ks_type_id(GameConfig))) {
        ks_reflect_struct(GameConfig,
            ks_reflect_field(float, gravity),
            ks_reflect_field(int, max_players)
        );
    }
}

struct SystemTestData {
    int entity_count;
    float sum_x;
};

void TestSystemCallback(Ks_Ecs_World world, Ks_Entity entity, void* user_data) {
    SystemTestData* data = (SystemTestData*)user_data;
    data->entity_count++;

    const Position* pos = (const Position*)ks_ecs_get_component(world, entity, ks_type_id(Position));
    if (pos) {
        data->sum_x += pos->x;
    }
}

void ObserverCallback(Ks_Ecs_World world, Ks_Entity entity, void* user_data) {
    int* counter = (int*)user_data;
    (*counter)++;
}

void QueryCallback(Ks_Ecs_World world, Ks_Entity entity, void* user_data) {
    int* counter = (int*)user_data;
    (*counter)++;
}

#define ks_str(...) #__VA_ARGS__


TEST_CASE("ECS System Advanced Features") {
    ks_memory_init();
    ks_reflection_init();
    reflect_components();

    SUBCASE("Entity & Component Lifecycle") {
        Ks_Ecs_World world = ks_ecs_create_world();
        Ks_Entity e = ks_ecs_create_entity(world, "Hero");

        Position p = { 10, 20 };
        ks_ecs_set_component(world, e, ks_type_id(Position), &p);

        CHECK(ks_ecs_has_component(world, e, ks_type_id(Position)) == true);

        const Position* get_p = (const Position*)ks_ecs_get_component(world, e, ks_type_id(Position));
        CHECK(get_p != nullptr);
        CHECK(get_p->x == 10);

        Position* mut_p = (Position*)ks_ecs_get_component_mut(world, e, ks_type_id(Position));
        CHECK(mut_p != nullptr);
        mut_p->x = 50;

        const Position* check_p = (const Position*)ks_ecs_get_component(world, e, ks_type_id(Position));
        CHECK(check_p->x == 50);

        ks_ecs_destroy_world(world);
    }

    SUBCASE("System Execution with Phases") {
        Ks_Ecs_World world = ks_ecs_create_world();
        Ks_Entity e1 = ks_ecs_create_entity(world, "E1");
        Ks_Entity e2 = ks_ecs_create_entity(world, "E2");

        Position p1 = { 1.0f, 0.0f };
        Position p2 = { 2.0f, 0.0f };
        ks_ecs_set_component(world, e1, ks_type_id(Position), &p1);
        ks_ecs_set_component(world, e2, ks_type_id(Position), &p2);

        SystemTestData stats = { 0, 0.0f };

        ks_ecs_create_system(world, "TestSys", ks_type_id(Position), KS_PHASE_ON_UPDATE, TestSystemCallback, &stats);

        ks_ecs_progress(world, 0.16f);

        CHECK(stats.entity_count == 2);
        CHECK(stats.sum_x == doctest::Approx(3.0f));

        Ks_Entity sys = ks_ecs_lookup(world, "TestSys");
        CHECK(sys != 0);

        ks_ecs_enable_system(world, sys, false);
        ks_ecs_progress(world, 0.16f);
        CHECK(stats.entity_count == 2);

        ks_ecs_enable_system(world, sys, true);
        ks_ecs_progress(world, 0.16f);
        CHECK(stats.entity_count == 4);

        ks_ecs_destroy_world(world);
    }

    SUBCASE("Hierarchy System") {
        Ks_Ecs_World world = ks_ecs_create_world();

        Ks_Entity parent = ks_ecs_create_entity(world, "Parent");
        Ks_Entity child = ks_ecs_create_entity(world, "Child");

        ks_ecs_add_child(world, parent, child);

        Ks_Entity actual_parent = ks_ecs_get_parent(world, child);
        CHECK(actual_parent == parent);

        ks_ecs_remove_child(world, parent, child);
        CHECK(ks_ecs_get_parent(world, child) == 0);

        ks_ecs_destroy_world(world);
    }

    SUBCASE("Prefab Instantiation & Copy-on-Write") {
        Ks_Ecs_World world = ks_ecs_create_world();

        Ks_Entity prefab = ks_ecs_create_prefab(world, "OrcTemplate");
        Health default_hp = { 100 };
        ks_ecs_set_component(world, prefab, ks_type_id(Health), &default_hp);

        Ks_Entity orc1 = ks_ecs_instantiate(world, prefab);
        Ks_Entity orc2 = ks_ecs_instantiate(world, prefab);

        const Health* hp1 = (const Health*)ks_ecs_get_component(world, orc1, ks_type_id(Health));
        CHECK(hp1 != nullptr);
        CHECK(hp1->hp == 100);

        Health* hp1_mut = (Health*)ks_ecs_get_component_mut(world, orc1, ks_type_id(Health));
        CHECK(hp1_mut != nullptr);
        hp1_mut->hp = 50;

        const Health* h1_final = (const Health*)ks_ecs_get_component(world, orc1, ks_type_id(Health));
        CHECK(h1_final->hp == 50); 

        const Health* h2_final = (const Health*)ks_ecs_get_component(world, orc2, ks_type_id(Health));
        CHECK(h2_final->hp == 100);

        CHECK(ks_ecs_get_prefab(world, "OrcTemplate") == prefab);

        ks_ecs_destroy_world(world);
    }

    SUBCASE("Singleton Components") {
        Ks_Ecs_World world = ks_ecs_create_world();

        GameConfig cfg = { 9.81f, 4 };
        ks_ecs_set_global(world, ks_type_id(GameConfig), &cfg);

        GameConfig* g_cfg = (GameConfig*)ks_ecs_get_global(world, ks_type_id(GameConfig));

        CHECK(g_cfg != nullptr);
        CHECK(g_cfg->gravity == doctest::Approx(9.81f));
        CHECK(g_cfg->max_players == 4);

        g_cfg->gravity = 5.0f;

        GameConfig* g_cfg2 = (GameConfig*)ks_ecs_get_global(world, ks_type_id(GameConfig));
        CHECK(g_cfg2->gravity == doctest::Approx(5.0f));

        ks_ecs_destroy_world(world);
    }

    SUBCASE("Observers (Reactive System)") {
        Ks_Ecs_World world = ks_ecs_create_world();

        int add_calls = 0;
        int remove_calls = 0;

        ks_ecs_create_observer(world, KS_EVENT_ON_ADD, ks_type_id(Position), ObserverCallback, &add_calls);
        ks_ecs_create_observer(world, KS_EVENT_ON_REMOVE, ks_type_id(Position), ObserverCallback, &remove_calls);

        Ks_Entity e = ks_ecs_create_entity(world, "ObserverTest");

        Position p = { 0,0 };
        ks_ecs_set_component(world, e, ks_type_id(Position), &p);
        CHECK(add_calls == 1);
        CHECK(remove_calls == 0);

        ks_ecs_remove_component(world, e, ks_type_id(Position));
        CHECK(add_calls == 1);
        CHECK(remove_calls == 1);

        ks_ecs_destroy_world(world);
    }

    SUBCASE("Entity Lookup & State") {
        Ks_Ecs_World world = ks_ecs_create_world();
        Ks_Entity e = ks_ecs_create_entity(world, "MyEntity");

        CHECK(ks_ecs_lookup(world, "MyEntity") == e);
        CHECK(ks_ecs_lookup(world, "NonExistent") == 0);

        CHECK(ks_ecs_is_alive(world, e));

        ks_ecs_enable_entity(world, e, false);

        ks_ecs_enable_entity(world, e, true);

        ks_ecs_destroy_world(world);
    }

    SUBCASE("Ad-Hoc Query") {
        Ks_Ecs_World world = ks_ecs_create_world();

        Ks_Entity e1 = ks_ecs_create_entity(world, "Q1");
        Ks_Entity e2 = ks_ecs_create_entity(world, "Q2");

        Position p = { 10, 10 };
        ks_ecs_set_component(world, e1, ks_type_id(Position), &p);

        int count = 0;
        ks_ecs_run_query(world, ks_type_id(Position), QueryCallback, &count);

        CHECK(count == 1);

        ks_ecs_destroy_world(world);
    }

    SUBCASE("DSL Query Syntax (AND, NOT)") {
        Ks_Ecs_World world = ks_ecs_create_world();

        Ks_Entity e1 = ks_ecs_create_entity(world, "E1");
        Ks_Entity e2 = ks_ecs_create_entity(world, "E2");
        Ks_Entity e3 = ks_ecs_create_entity(world, "E3");

        Position p = { 0,0 }; Velocity v = { 0,0 };

        ks_ecs_set_component(world, e1, ks_type_id(Position), &p);
        ks_ecs_set_component(world, e1, ks_type_id(Velocity), &v);

        ks_ecs_set_component(world, e2, ks_type_id(Position), &p);

        ks_ecs_set_component(world, e3, ks_type_id(Velocity), &v);

        int count = 0;
        ks_ecs_run_query(world, "Position, Velocity", QueryCallback, &count);
        CHECK(count == 1);

        count = 0;
        ks_ecs_run_query(world, "Position, !Velocity", QueryCallback, &count);
        CHECK(count == 1);

        count = 0;
        ks_ecs_run_query(world, "Position || Velocity", QueryCallback, &count);
        CHECK(count == 3);

        ks_ecs_destroy_world(world);
    }

    ks_reflection_shutdown();
    ks_memory_shutdown();
}