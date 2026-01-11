#include <doctest/doctest.h>
#include <keystone.h>
#include <string.h>
#include <string>
#include <vector>

struct TestDataEvent {
    int x, y;
    char name[32];
};

struct TestPrimitiveEvent {
    int id;
    float value;
    const char* message;
};


void register_test_reflection() {
    if (!ks_reflection_get_type(ks_type_id(TestPrimitiveEvent))) {
        ks_reflect_struct(TestPrimitiveEvent,
            ks_reflect_field(int, id),
            ks_reflect_field(float, value),
            ks_reflect_field(const char*, message)
        );
    }

    if (!ks_reflection_get_type(ks_type_id(TestDataEvent))) {
        ks_reflect_struct(TestDataEvent,
            ks_reflect_field(int, x),
            ks_reflect_field(int, y),
            ks_reflect_field(char, name, [32])
        );
    }
}

static int g_callback_count = 0;
static int g_last_int_val = 0;
static float g_last_float_val = 0.0f;
static std::string g_last_string_val = "";
static TestDataEvent g_received_data = {};

void reset_test_globals() {
    g_callback_count = 0;
    g_last_int_val = 0;
    g_last_float_val = 0.0f;
    g_last_string_val = "";
    memset(&g_received_data, 0, sizeof(TestDataEvent));
}

void on_primitive_event(Ks_EventData data, void* user_data) {
    g_callback_count++;
    const TestPrimitiveEvent* evt = (const TestPrimitiveEvent*)data;

    g_last_int_val = evt->id;
    g_last_float_val = evt->value;
    if (evt->message) g_last_string_val = evt->message;
}

void on_data_event(Ks_EventData data, void* user_data) {
    g_callback_count++;
    const TestDataEvent* evt = (const TestDataEvent*)data;
    g_received_data = *evt;
}

void on_signal_event(Ks_EventData data, void* user_data) {
    g_callback_count++;
}

void on_user_data_check(Ks_EventData data, void* user_data) {
    int* val = (int*)user_data;
    (*val)++;
}

TEST_CASE("C API: Event Manager") {
    ks_memory_init();
    ks_reflection_init();
    register_test_reflection();
    reset_test_globals();

    Ks_EventManager em = ks_event_manager_create();
    REQUIRE(em != nullptr);

    SUBCASE("Registration") {
        CHECK(ks_event_manager_register_type(em, ks_type_id(TestPrimitiveEvent)) != KS_INVALID_HANDLE);
        CHECK(ks_event_manager_register_signal(em, "TestSignal") != KS_INVALID_HANDLE);
        CHECK(ks_event_manager_register_type(em, "NonExistentType") == KS_INVALID_HANDLE);
    }

    SUBCASE("Publish & Subscribe (Primitives)") {
        Ks_Handle test_primitive_e = ks_event_manager_register_type(em, ks_type_id(TestPrimitiveEvent));

        Ks_Handle sub = ks_event_manager_subscribe(em, test_primitive_e, on_primitive_event, nullptr);
        CHECK(sub != KS_INVALID_HANDLE);

        TestPrimitiveEvent evt_data = { 42, 3.14f, "Keystone" };
        ks_event_manager_publish(em, test_primitive_e, &evt_data);

        CHECK(g_callback_count == 1);
        CHECK(g_last_int_val == 42);
        CHECK(g_last_float_val == doctest::Approx(3.14f));
        CHECK(g_last_string_val == "Keystone");
    }

    SUBCASE("Publish & Subscribe (Struct Data)") {
        Ks_Handle test_data_e = ks_event_manager_register_type(em, ks_type_id(TestDataEvent));

        ks_event_manager_subscribe(em, test_data_e, on_data_event, nullptr);

        TestDataEvent send_data;
        send_data.x = 100;
        send_data.y = 200;

        ks_event_manager_publish(em, test_data_e, &send_data);

        CHECK(g_callback_count == 1);
        CHECK(g_received_data.x == 100);
        CHECK(g_received_data.y == 200);
    }

    SUBCASE("Signals (No Payload)") {
        Ks_Handle stop_engine_s = ks_event_manager_register_signal(em, "StopEngine");

        ks_event_manager_subscribe(em, stop_engine_s, on_signal_event, nullptr);

        ks_event_manager_emit(em, stop_engine_s);

        CHECK(g_callback_count == 1);
    }

    SUBCASE("User Data Passing") {
        Ks_Handle ping_e  =ks_event_manager_register_signal(em, "Ping");

        int my_counter = 0;
        ks_event_manager_subscribe(em, ping_e, on_user_data_check, &my_counter);

        ks_event_manager_emit(em, ping_e);

        CHECK(my_counter == 1);
    }

    SUBCASE("Unsubscribe Logic") {
        Ks_Handle update_e =  ks_event_manager_register_signal(em, "Update");

        Ks_Handle sub = ks_event_manager_subscribe(em, update_e, on_signal_event, nullptr);

        ks_event_manager_emit(em, update_e);
        CHECK(g_callback_count == 1);

        ks_event_manager_unsubscribe(em, sub);

        ks_event_manager_emit(em, update_e);
        CHECK(g_callback_count == 1);
    }

    SUBCASE("Multiple Subscribers") {
        Ks_Handle tick_e = ks_event_manager_register_signal(em, "Tick");

        Ks_Handle sub1 = ks_event_manager_subscribe(em, tick_e, on_signal_event, nullptr);
        Ks_Handle sub2 = ks_event_manager_subscribe(em, tick_e, on_signal_event, nullptr);

        ks_event_manager_emit(em, tick_e);

        CHECK(g_callback_count == 2);

        ks_event_manager_unsubscribe(em, sub1);

        ks_event_manager_emit(em, tick_e);
        CHECK(g_callback_count == 3);
    }

    ks_event_manager_destroy(em);
    ks_reflection_shutdown();
    ks_memory_shutdown();
}