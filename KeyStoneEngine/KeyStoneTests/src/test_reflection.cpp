#include <doctest/doctest.h>
#include "core/reflection.h"
#include <cstring>
#include <string>

typedef float TestFloat;
typedef TestFloat MyTime;

typedef enum TestColor {
    COLOR_RED = 10,
    COLOR_GREEN = 20,
    COLOR_BLUE = 30
} TestColor;

typedef void (*OnEventCallback)(int code, const char* msg);

typedef struct ComplexStruct {
    const int id;
    volatile float value;

    const char* name;
    int* numbers;
    float matrix[4][4];

    uint32_t flags_a : 1;
    uint32_t flags_b : 3;
    uint32_t flags_c : 4;

    TestColor color;
    MyTime timestamp;

    OnEventCallback callback_typedef;
    int (*callback_inline)(float x, float y);

} ComplexStruct;

void RegisterReflectionTests() {
    ks_reflection_init();

    ks_reflect_typedef(float, TestFloat);
    ks_reflect_typedef(TestFloat, MyTime);

    ks_reflect_enum(TestColor,
        ks_enum_value(COLOR_RED),
        ks_enum_value(COLOR_GREEN),
        ks_enum_value(COLOR_BLUE)
    );

    ks_reflect_function(OnEventCallback, void,
        ks_reflect_arg(int, code),
        ks_reflect_arg(const char*, msg)
    );

    ks_reflect_struct(ComplexStruct,
        ks_reflect_field(const int, id),
        ks_reflect_field(volatile float, value),

        ks_reflect_field(const char*, name),
        ks_reflect_field(int*, numbers),

        ks_reflect_field(float, matrix, [4][4]),

        ks_reflect_bitfield(uint32_t, flags_a, 0, 1),
        ks_reflect_bitfield(uint32_t, flags_b, 1, 3),
        ks_reflect_bitfield(uint32_t, flags_c, 4, 4),

        ks_reflect_field(TestColor, color),
        ks_reflect_field(MyTime, timestamp),

        ks_reflect_field(OnEventCallback, callback_typedef),

        ks_reflect_func_ptr(callback_inline, int,
            ks_reflect_arg(float, x),
            ks_reflect_arg(float, y)
        )
    );
}

TEST_CASE("Reflection System Tests") {
    RegisterReflectionTests();

    SUBCASE("Verify Enum Registration") {
        const Ks_Type_Info* info = ks_reflection_get_type("TestColor");

        REQUIRE(info != nullptr);
        CHECK(info->kind == KS_META_ENUM);
        CHECK(info->enum_count == 3);

        CHECK(strcmp(info->enum_items[0].name, "COLOR_RED") == 0);
        CHECK(info->enum_items[0].value == 10);

        CHECK(strcmp(info->enum_items[1].name, "COLOR_GREEN") == 0);
        CHECK(info->enum_items[1].value == 20);

        CHECK(strcmp(info->enum_items[2].name, "COLOR_BLUE") == 0);
        CHECK(info->enum_items[2].value == 30);
    }

    SUBCASE("Verify Typedef Resolution") {
        const Ks_Type_Info* info = ks_reflection_get_type("ComplexStruct");
        REQUIRE(info != nullptr);

        bool found = false;
        for (size_t i = 0; i < info->field_count; i++) {
            if (strcmp(info->fields[i].name, "timestamp") == 0) {
                CHECK(info->fields[i].type == KS_TYPE_FLOAT);
                found = true;
                break;
            }
        }
        CHECK(found == true);
    }

    SUBCASE("Verify Modifiers and Pointers") {
        const Ks_Type_Info* info = ks_reflection_get_type("ComplexStruct");
        REQUIRE(info != nullptr);

        for (size_t i = 0; i < info->field_count; i++) {
            const Ks_Field_Info* f = &info->fields[i];

            if (strcmp(f->name, "id") == 0) {
                CHECK((f->modifiers & KS_MOD_CONST) != 0);
                CHECK(f->type == KS_TYPE_INT);
            }
            else if (strcmp(f->name, "value") == 0) {
                CHECK((f->modifiers & KS_MOD_VOLATILE) != 0);
                CHECK(f->type == KS_TYPE_FLOAT);
            }
            else if (strcmp(f->name, "name") == 0) {
                CHECK(f->type == KS_TYPE_CHAR);
                CHECK(f->ptr_depth == 1);
                CHECK((f->modifiers & KS_MOD_CONST) != 0);
            }
        }
    }

    SUBCASE("Verify Multidimensional Arrays") {
        const Ks_Type_Info* info = ks_reflection_get_type("ComplexStruct");
        REQUIRE(info != nullptr);

        bool found = false;
        for (size_t i = 0; i < info->field_count; i++) {
            const Ks_Field_Info* f = &info->fields[i];
            if (strcmp(f->name, "matrix") == 0) {
                CHECK(f->is_array == true);
                CHECK(f->dim_count == 2);
                CHECK(f->dims[0] == 4);
                CHECK(f->dims[1] == 4);
                CHECK(f->total_element_count == 16);
                CHECK(f->type == KS_TYPE_FLOAT);
                found = true;
                break;
            }
        }
        CHECK(found == true);
    }

    SUBCASE("Verify Bitfields") {
        const Ks_Type_Info* info = ks_reflection_get_type("ComplexStruct");
        REQUIRE(info != nullptr);

        bool found = false;
        for (size_t i = 0; i < info->field_count; i++) {
            const Ks_Field_Info* f = &info->fields[i];
            if (strcmp(f->name, "flags_b") == 0) {
                CHECK(f->is_bitfield == true);
                CHECK(f->bit_offset == 1);
                CHECK(f->bit_width == 3);
                found = true;
                break;
            }
        }
        CHECK(found == true);
    }

    SUBCASE("Verify Function Pointers") {
        const Ks_Type_Info* info = ks_reflection_get_type("ComplexStruct");
        REQUIRE(info != nullptr);

        int checked_count = 0;
        for (size_t i = 0; i < info->field_count; i++) {
            const Ks_Field_Info* f = &info->fields[i];

            if (strcmp(f->name, "callback_typedef") == 0) {
                const Ks_Type_Info* funcInfo = ks_reflection_get_type(f->type_str);
                REQUIRE(funcInfo != nullptr);
                CHECK(funcInfo->kind == KS_META_FUNCTION);
                CHECK(funcInfo->arg_count == 2);
                checked_count++;
            }

            if (strcmp(f->name, "callback_inline") == 0) {
                CHECK(f->is_function_ptr == true);
                CHECK(f->return_type == KS_TYPE_INT);
                CHECK(f->arg_count == 2);

                if (f->arg_count >= 2) {
                    CHECK(f->args[0].type == KS_TYPE_FLOAT);
                    CHECK(strcmp(f->args[0].name, "x") == 0);
                    CHECK(f->args[1].type == KS_TYPE_FLOAT);
                    CHECK(strcmp(f->args[1].name, "y") == 0);
                }
                checked_count++;
            }
        }
        CHECK(checked_count == 2);
    }

    ks_reflection_shutdown();
}