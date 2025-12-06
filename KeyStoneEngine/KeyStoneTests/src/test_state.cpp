#include <doctest/doctest.h>
#include <keystone.h>
#include <string.h>
#include <string>

struct TestVec3 {
    float x, y, z;
};

TEST_CASE("C API: State Manager Core") {
    ks_memory_init();
    Ks_StateManager sm = ks_state_manager_create();

    REQUIRE(sm != nullptr);

    SUBCASE("Primitives: Creation and Retrieval") {
        Ks_Handle h_int = ks_state_manager_new_int(sm, "score", 100);
        CHECK(h_int != KS_INVALID_HANDLE);
        CHECK(ks_state_get_type(sm, h_int) == KS_TYPE_INT);
        CHECK(ks_state_get_int(sm, h_int) == 100);

        Ks_Handle h_float = ks_state_manager_new_float(sm, "gravity", 9.81);
        CHECK(ks_state_get_float(sm, h_float) == doctest::Approx(9.81));

        Ks_Handle h_bool = ks_state_manager_new_bool(sm, "is_alive", ks_true);
        CHECK(ks_state_get_bool(sm, h_bool) == ks_true);

        {
            std::string temp_str = "Hello World";
            ks_state_manager_new_string(sm, "message", temp_str.c_str());
        }

        Ks_Handle h_str = ks_state_manager_get_handle(sm, "message");
        const char* saved_str = ks_state_get_string(sm, h_str);
        CHECK(strcmp(saved_str, "Hello World") == 0);
    }

    SUBCASE("Updates and Type Safety") {
        Ks_Handle h = ks_state_manager_new_int(sm, "level", 1);

        bool ok = ks_state_set_int(sm, h, 2);
        CHECK(ok == ks_true);
        CHECK(ks_state_get_int(sm, h) == 2);

        bool fail = ks_state_set_float(sm, h, 5.5);
        CHECK(fail == ks_false);
        CHECK(ks_state_get_int(sm, h) == 2);
    }

    SUBCASE("UserTypes: Deep Copy") {
        TestVec3 vec = { 1.0f, 2.0f, 3.0f };

        Ks_Handle h = ks_state_manager_new_usertype(sm, "player_pos", KS_USERDATA(vec), "TestVec3");
        CHECK(h != KS_INVALID_HANDLE);

        vec.x = 999.0f;

        const char* type_name = nullptr;
        size_t size = 0;
        void* internal_ptr = ks_state_get_usertype_info(sm, h, &type_name, &size);

        REQUIRE(internal_ptr != nullptr);
        CHECK(strcmp(type_name, "TestVec3") == 0);
        CHECK(size == sizeof(TestVec3));

        TestVec3* stored_vec = (TestVec3*)internal_ptr;
        CHECK(stored_vec->x == 1.0f);
        CHECK(stored_vec->y == 2.0f);

        stored_vec->z = 50.0f;

        TestVec3* stored_vec2 = (TestVec3*)ks_state_get_ptr(sm, h);
        CHECK(stored_vec2->z == 50.0f);
    }

    ks_state_manager_destroy(sm);
    ks_memory_shutdown();
}