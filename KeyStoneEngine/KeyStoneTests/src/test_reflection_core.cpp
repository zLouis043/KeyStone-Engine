#include <doctest/doctest.h>
#include <keystone.h>
#include <cstring>

typedef enum TestColor {
    COLOR_RED = 10,
    COLOR_GREEN = 20
} TestColor;

typedef void (*ComplexCallback)(int code, const char* msg);

struct ComplexData {
    uint32_t flags : 3;
    TestColor color;
    ComplexCallback cb;
};

void RegisterComplexTypes() {
    ks_reflection_init();

    ks_reflect_enum(TestColor,
        ks_enum_value(COLOR_RED),
        ks_enum_value(COLOR_GREEN)
    );

    ks_reflect_function(ComplexCallback, void,
        ks_args(
            ks_arg(int, code),
            ks_arg(const char*, msg)
        )
    );

    ks_reflect_struct(ComplexData,
        ks_reflect_bitfield(uint32_t, flags, 0, 3),
        ks_reflect_field(TestColor, color),
        ks_reflect_func_ptr(cb, void,
            ks_args(
                ks_arg(int, code),
                ks_arg(const char*, msg)
            )
        )
    );
}

TEST_CASE("Core: Reflection Metadata") {
    RegisterComplexTypes();

    SUBCASE("Enums") {
        const Ks_Type_Info* info = ks_reflection_get_type("TestColor");
        REQUIRE(info != nullptr);
        CHECK(info->kind == KS_META_ENUM);
        CHECK(info->enum_count == 2);
        CHECK(info->enum_items[0].value == 10);
    }

    SUBCASE("Bitfields") {
        const Ks_Type_Info* info = ks_reflection_get_type("ComplexData");
        REQUIRE(info != nullptr);

        bool found = false;
        for (size_t i = 0; i < info->field_count; ++i) {
            if (strcmp(info->fields[i].name, "flags") == 0) {
                CHECK(info->fields[i].is_bitfield);
                CHECK(info->fields[i].bit_width == 3);
                found = true;
            }
        }
        CHECK(found);
    }

    SUBCASE("Function Pointers") {
        const Ks_Type_Info* info = ks_reflection_get_type("ComplexData");
        bool found = false;
        for (size_t i = 0; i < info->field_count; ++i) {
            if (strcmp(info->fields[i].name, "cb") == 0) {
                CHECK(info->fields[i].is_function_ptr);
                CHECK(info->fields[i].arg_count == 2);
                if (info->fields[i].arg_count >= 2) {
                    CHECK(strcmp(info->fields[i].args[1].name, "msg") == 0);
                }
                found = true;
            }
        }
        CHECK(found);
    }

}