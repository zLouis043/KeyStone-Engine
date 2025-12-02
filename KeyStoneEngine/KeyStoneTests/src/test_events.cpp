#include <doctest/doctest.h>
#include <keystone.h>
#include <string.h>
#include <string>
#include <vector>

static int g_callback_count = 0;
static int g_last_int_val = 0;
static float g_last_float_val = 0.0f;
static std::string g_last_string_val = "";

void reset_test_globals() {
    g_callback_count = 0;
    g_last_int_val = 0;
    g_last_float_val = 0.0f;
    g_last_string_val = "";
}

ks_bool on_primitive_event(Ks_Event_Payload payload, ks_ptr user_data) {
    g_callback_count++;

    g_last_int_val = ks_event_get_int(payload, 0);
    g_last_float_val = ks_event_get_float(payload, 1);
    const char* str = ks_event_get_cstring(payload, 2);
    if (str) g_last_string_val = str;

    return ks_true;
}

struct TestData {
    int x, y;
    char name[32];
};

static TestData g_received_data = {};

ks_bool on_userdata_event(Ks_Event_Payload payload, ks_ptr user_data) {
    g_callback_count++;

    Ks_UserData ud = ks_event_get_userdata(payload, 0);
    if (ud.data) {
        memcpy(&g_received_data, ud.data, ud.size);
    }
    return ks_true;
}

TEST_CASE("C API: Event Manager") {
    ks_memory_init();
    reset_test_globals();

    Ks_EventManager em = ks_event_manager_create();
    REQUIRE(em != nullptr);

    SUBCASE("Registration & Handles") {
        Ks_Handle evt_handle = ks_event_manager_register(em, "TestEvent", KS_TYPE_INT);

        CHECK(evt_handle != KS_INVALID_HANDLE);

        Ks_Handle retrieved = ks_event_manager_get_event_handle(em, "TestEvent");
        CHECK(retrieved == evt_handle);

        CHECK(ks_event_manager_get_event_handle(em, "NonExistent") == KS_INVALID_HANDLE);
    }

    SUBCASE("Publish & Subscribe (Primitives)") {
        Ks_Handle evt = ks_event_manager_register(em, "PrimEvent", 
                KS_TYPE_INT, KS_TYPE_FLOAT, KS_TYPE_CSTRING);

        Ks_Handle sub = ks_event_manager_subscribe(em, evt, on_primitive_event, NULL);
        CHECK(sub != KS_INVALID_HANDLE);

        ks_event_manager_publish(em, evt, 42, 3.14f, "Keystone");

        CHECK(g_callback_count == 1);
        CHECK(g_last_int_val == 42);
        CHECK(g_last_float_val == doctest::Approx(3.14f));
        CHECK(g_last_string_val == "Keystone");
    }

    SUBCASE("Userdata Passing") {
        Ks_Handle evt = ks_event_manager_register(em, "DataEvent", KS_TYPE_USERDATA);

        ks_event_manager_subscribe(em, evt, on_userdata_event, NULL);

        TestData data;
        data.x = 100;
        data.y = 200;
        strcpy(data.name, "Player");
        ks_event_manager_publish(em, evt, KS_USERDATA(data));

        CHECK(g_callback_count == 1);
        CHECK(g_received_data.x == 100);
        CHECK(g_received_data.y == 200);
        CHECK(strcmp(g_received_data.name, "Player") == 0);
    }

    SUBCASE("Unsubscribe Logic") {
        Ks_Handle evt = ks_event_manager_register(em, "UnsubTest", KS_TYPE_INT);

        Ks_Handle sub = ks_event_manager_subscribe(em, evt, on_primitive_event, NULL);

        ks_event_manager_publish(em, evt, 1, 0.0f, "");
        CHECK(g_callback_count == 1);

        ks_event_manager_unsubscribe(em, sub);

        ks_event_manager_publish(em, evt, 2, 0.0f, "");
        CHECK(g_callback_count == 1);
    }

    SUBCASE("Multiple Subscribers") {
        Ks_Handle evt = ks_event_manager_register(em, "MultiTest", KS_TYPE_INT);

        Ks_Handle sub1 = ks_event_manager_subscribe(em, evt, on_primitive_event, NULL);
        Ks_Handle sub2 = ks_event_manager_subscribe(em, evt, on_primitive_event, NULL);

        ks_event_manager_publish(em, evt, 10, 0.0f, "");

        CHECK(g_callback_count == 2);

        ks_event_manager_unsubscribe(em, sub1);

        ks_event_manager_publish(em, evt, 20, 0.0f, "");
        CHECK(g_callback_count == 3);
    }

    ks_event_manager_destroy(em);
    ks_memory_shutdown();
}