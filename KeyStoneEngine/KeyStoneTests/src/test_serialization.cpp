#include <doctest/doctest.h>
#include <keystone.h>
#include <string>

TEST_CASE("C API: Serializer System") {
    ks_memory_init();

    SUBCASE("Lifecycle & Root Object") {
        Ks_Serializer ser = ks_serializer_create();
        REQUIRE(ser != nullptr);

        Ks_Json root = ks_serializer_get_root(ser);
        CHECK(root != nullptr);
        CHECK(ks_json_get_type(root) == KS_JSON_OBJECT);

        ks_serializer_destroy(ser);
    }

    SUBCASE("Factory Methods & Types") {
        Ks_Serializer ser = ks_serializer_create();

        Ks_Json n = ks_json_create_number(ser, 123.456);
        CHECK(ks_json_get_type(n) == KS_JSON_NUMBER);
        CHECK(ks_json_get_number(n) == doctest::Approx(123.456));

        Ks_Json b = ks_json_create_bool(ser, ks_true);
        CHECK(ks_json_get_type(b) == KS_JSON_BOOLEAN);
        CHECK(ks_json_get_bool(b) == ks_true);

        Ks_Json s = ks_json_create_string(ser, "Hello JSON");
        CHECK(ks_json_get_type(s) == KS_JSON_STRING);
        CHECK(std::string(ks_json_get_string(s)) == "Hello JSON");

        ks_serializer_destroy(ser);
    }

    SUBCASE("Object Manipulation (Ownership Transfer)") {
        Ks_Serializer ser = ks_serializer_create();
        Ks_Json root = ks_serializer_get_root(ser);

        Ks_Json name = ks_json_create_string(ser, "Keystone");
        Ks_Json ver = ks_json_create_number(ser, 0.9);

        ks_json_object_add(ser, root, "engine", name);
        ks_json_object_add(ser, root, "version", ver);

        CHECK(ks_json_object_has(root, "engine") == ks_true);
        CHECK(ks_json_object_has(root, "version") == ks_true);
        CHECK(ks_json_object_has(root, "missing") == ks_false);

        Ks_Json retrieved_name = ks_json_object_get(root, "engine");
        CHECK(std::string(ks_json_get_string(retrieved_name)) == "Keystone");

        ks_serializer_destroy(ser);
    }

    SUBCASE("Array Manipulation") {
        Ks_Serializer ser = ks_serializer_create();
        Ks_Json arr = ks_json_create_array(ser);

        ks_json_array_push(ser, arr, ks_json_create_number(ser, 10));
        ks_json_array_push(ser, arr, ks_json_create_number(ser, 20));
        ks_json_array_push(ser, arr, ks_json_create_number(ser, 30));

        CHECK(ks_json_array_size(arr) == 3);

        Ks_Json el2 = ks_json_array_get(arr, 1);
        CHECK(ks_json_get_number(el2) == 20.0);

        ks_serializer_destroy(ser);
    }

    SUBCASE("Dump to String & Load") {
        Ks_Serializer ser = ks_serializer_create();
        Ks_Json root = ks_serializer_get_root(ser);

        ks_json_object_add(ser, root, "id", ks_json_create_number(ser, 1));
        ks_json_object_add(ser, root, "active", ks_json_create_bool(ser, ks_true));

        ks_str json_output = ks_serializer_dump_to_string(ser);
        std::string s_out = json_output;
        CHECK(s_out.find("\"id\"") != std::string::npos);
        CHECK(s_out.find("\"active\"") != std::string::npos);

        Ks_Serializer ser2 = ks_serializer_create();
        bool load_ok = ks_serializer_load_from_string(ser2, json_output);
        CHECK(load_ok == ks_true);

        Ks_Json root2 = ks_serializer_get_root(ser2);
        CHECK(ks_json_object_has(root2, "id"));
        CHECK(ks_json_get_number(ks_json_object_get(root2, "id")) == 1.0);

        ks_serializer_destroy(ser);
        ks_serializer_destroy(ser2);
    }

    SUBCASE("Object Composition & Move Semantics") {
        Ks_Serializer ser = ks_serializer_create();
        Ks_Json root = ks_serializer_get_root(ser);

        Ks_Json meta = ks_json_create_object(ser);
        Ks_Json ver = ks_json_create_number(ser, 1.5);

        ks_json_object_add(ser, meta, "version", ver);

        ks_json_object_add(ser, root, "metadata", meta);

        CHECK(ks_json_object_has(root, "metadata"));

        Ks_Json fetched_meta = ks_json_object_get(root, "metadata");
        CHECK(ks_json_get_type(fetched_meta) == KS_JSON_OBJECT);

        Ks_Json fetched_ver = ks_json_object_get(fetched_meta, "version");
        CHECK(ks_json_get_number(fetched_ver) == doctest::Approx(1.5));

        ks_serializer_destroy(ser);
    }

    SUBCASE("Arrays & Iteration") {
        Ks_Serializer ser = ks_serializer_create();
        Ks_Json arr = ks_json_create_array(ser);

        for (int i = 0; i < 5; ++i) {
            ks_json_array_push(ser, arr, ks_json_create_number(ser, i * 10));
        }

        CHECK(ks_json_array_size(arr) == 5);
        CHECK(ks_json_get_number(ks_json_array_get(arr, 4)) == 40.0);
        CHECK(ks_json_array_get(arr, 100) == nullptr);

        ks_serializer_destroy(ser);
    }

    SUBCASE("IO: Dump & Load") {
        Ks_Serializer ser = ks_serializer_create();
        Ks_Json root = ks_serializer_get_root(ser);
        ks_json_object_add(ser, root, "test", ks_json_create_bool(ser, ks_true));

        const char* json_str = ks_serializer_dump_to_string(ser);
        CHECK(std::string(json_str).find("\"test\"") != std::string::npos);

        Ks_Serializer ser2 = ks_serializer_create();
        CHECK(ks_serializer_load_from_string(ser2, json_str) == ks_true);

        Ks_Json root2 = ks_serializer_get_root(ser2);
        CHECK(ks_json_get_bool(ks_json_object_get(root2, "test")) == ks_true);

        ks_serializer_destroy(ser);
        ks_serializer_destroy(ser2);
    }

    SUBCASE("Error Handling: Bad JSON") {
        Ks_Serializer ser = ks_serializer_create();
        bool ok = ks_serializer_load_from_string(ser, "{\"key\": 1");
        CHECK(ok == ks_false);
        ks_serializer_destroy(ser);
    }

    ks_memory_shutdown();
}