#include <doctest/doctest.h>
#include <keystone.h>
#include <string.h>
#include <string>
#include <vector>
#include <cctype>
#include "../include/common.h"

std::string remove_spaces(std::string s) {
    s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());
    return s;
}

static ks_bool mock_state_def(const Ks_Preproc_Ctx* ctx, Ks_StringBuilder* out) {
    ks_sb_appendf(out, "%s = state(\"%s\", %s)", ctx->symbol_name, ctx->symbol_name, ctx->assignment_value ? ctx->assignment_value : "nil");
    return ks_true;
}

static ks_bool mock_state_set(const Ks_Preproc_Ctx* ctx, Ks_StringBuilder* out) {
    ks_sb_appendf(out,"%s:set(%s)", ctx->symbol_name, ctx->assignment_value);
    return ks_true;
}

static ks_bool mock_state_get(const Ks_Preproc_Ctx* ctx, Ks_StringBuilder* out) {
    ks_sb_appendf(out, "%s:get()", ctx->symbol_name);
    return ks_true;
}

static ks_bool mock_system_def(const Ks_Preproc_Ctx* ctx, Ks_StringBuilder* out){
    if (!ctx->is_func_def) return ks_false;
    ks_sb_appendf(out, "ecs.System(\"%s\", \"",
        ctx->symbol_name);
    for (int i = 0; i < ctx->decorator_args_count; i++) {
        ks_sb_appendf(out, "%s", ctx->decorator_args[i]);
        if (i < ctx->decorator_args_count - 1) {
            ks_sb_append(out, ", ");
        }
    }
    ks_sb_appendf(out, "\", function(");
    for (int i = 0; i < ctx->function_args_count; i++) {
        ks_sb_appendf(out, "%s", ctx->function_args[i]);
        if (i < ctx->function_args_count - 1) {
            ks_sb_append(out, ", ");
        }
    }
    ks_sb_appendf(out, ") %s end)", ctx->function_body);
    return ks_true;
}

static ks_bool mock_proxy_set(const Ks_Preproc_Ctx* ctx, Ks_StringBuilder* out) {
    if (ctx->access_type == KS_ACCESS_DOT) {
        ks_sb_appendf(out, "%s:set_prop(\"%s\", %s)", ctx->symbol_name, ctx->member_key, ctx->assignment_value);
        return ks_true;
    }
    ks_sb_appendf(out, "%s:set(%s)", ctx->symbol_name, ctx->assignment_value);
    return ks_true;
}

static ks_bool mock_proxy_get(const Ks_Preproc_Ctx* ctx, Ks_StringBuilder* out) {
    if (ctx->access_type == KS_ACCESS_DOT) {
        ks_sb_appendf(out, "%s:get_prop(\"%s\")", ctx->symbol_name, ctx->member_key);
        return ks_true;
    }
    if (ctx->access_type == KS_ACCESS_COLON) {
        ks_sb_appendf(out, "%s:get_rpc(\"%s\")", ctx->symbol_name, ctx->member_key);
        return ks_true;
    }
    ks_sb_appendf(out, "%s:get()", ctx->symbol_name);
    return ks_true;
}

static ks_bool mock_class_def(const Ks_Preproc_Ctx* ctx, Ks_StringBuilder* out) {
    if (ctx->is_table_def) {
        ks_sb_appendf(out, "%s = class(\"%s\", {", ctx->symbol_name, ctx->symbol_name);
        for (size_t i = 0; i < ctx->table_fields_count; ++i) {
            ks_sb_appendf(out, "%s", ctx->table_fields[i]);
            if (i < ctx->table_fields_count - 1) ks_sb_append(out, ", ");
        }
        ks_sb_append(out, "})");
        return ks_true;
    }
    return ks_false;
}

static ks_bool mock_named_state(const Ks_Preproc_Ctx* ctx, Ks_StringBuilder* out) {
    ks_sb_appendf(out, "%s = state(\"%s\", %s, {", ctx->symbol_name, ctx->symbol_name, ctx->assignment_value);
    for (int i = 0; i < ctx->decorator_args_count; ++i) {
        if (ctx->decorator_arg_keys[i]) {
            ks_sb_appendf(out, "%s=\"%s\"", ctx->decorator_arg_keys[i], ctx->decorator_args[i]);
        }
        if (i < ctx->decorator_args_count - 1) ks_sb_append(out, ", ");
    }
    ks_sb_append(out, "})");
    return ks_true;
}

TEST_CASE("Scripting: Preprocessor & Decorators") {
    ks_memory_init();
    Ks_Preprocessor pp = ks_preprocessor_create(NULL);
    REQUIRE(pp != nullptr);

    ks_preprocessor_register(pp, "state", mock_state_def, mock_state_set, mock_state_get, nullptr);
    ks_preprocessor_register(pp, "system", mock_system_def, nullptr, nullptr, nullptr);
    ks_preprocessor_register(pp, "proxy", nullptr, mock_proxy_set, mock_proxy_get, nullptr);
    ks_preprocessor_register(pp, "class", mock_class_def, nullptr, nullptr, nullptr);
    ks_preprocessor_register(pp, "nstate", mock_named_state, nullptr, nullptr, nullptr);

    SUBCASE("State Manager: Definition & Access") {
        const char* src = R"(
            @state local hp = 100
            hp = hp - amount
            if hp < 0 then hp = 0 end
        )";

        ks_str processed = ks_preprocessor_process(pp, src);
        KS_LOG_INFO("From : '%s' to '%s'", src, processed);
        std::string res = remove_spaces(processed);

        CHECK(res.find(remove_spaces("local hp = state(\"hp\", 100)")) != std::string::npos);
        CHECK(res.find(remove_spaces("hp:set(hp:get() - amount)")) != std::string::npos);
        CHECK(res.find(remove_spaces("if hp:get() < 0")) != std::string::npos);
        CHECK(res.find(remove_spaces("then hp:set(0)")) != std::string::npos);
        ks_dealloc((ks_ptr)processed);
    }

    SUBCASE("Scope Safety: Shadowing") {
        const char* src = R"(
            @state local x = 10
            function inner()
                local x = 5 
                x = x + 1 
            end
            x = 20
        )";

        ks_str processed = ks_preprocessor_process(pp, src);
        KS_LOG_INFO("From : '%s' to '%s'", src, processed);
        std::string res = remove_spaces(processed);

        CHECK(res.find(remove_spaces("x:set(20)")) != std::string::npos);
        CHECK(res.find(remove_spaces("local x = 5")) != std::string::npos);
        CHECK(res.find(remove_spaces("x = x + 1")) != std::string::npos);
        ks_dealloc((ks_ptr)processed);
    }

    SUBCASE("ECS: System Decoration") {
        const char* src = R"(
            @system(Position, Velocity) 
            function MoveSystem(e)
                print("Update")
            end
        )";
        ks_str processed = ks_preprocessor_process(pp, src);
        KS_LOG_INFO("From : '%s' to '%s'", src, processed);

        std::string res = remove_spaces(processed);

        CHECK(res.find(remove_spaces("ecs.System(\"MoveSystem\", \"Position, Velocity\", function(e)")) != std::string::npos);
        CHECK(res.find(remove_spaces("end)")) != std::string::npos);
        ks_dealloc((ks_ptr)processed);
    }

    SUBCASE("Member Access Interception") {
        const char* src = R"(
            @proxy local player = Entity()
            player.hp = 100
            local x = player.pos
        )";

        ks_str processed = ks_preprocessor_process(pp, src);
        KS_LOG_INFO("From : '%s' to '%s'", src, processed);
        std::string res = remove_spaces(processed);

        CHECK(res.find(remove_spaces("player:set_prop(\"hp\", 100)")) != std::string::npos);

        CHECK(res.find(remove_spaces("player:get_prop(\"pos\")")) != std::string::npos);
    }

    SUBCASE("Method Call Interception") {
        const char* src = R"(
            @proxy local net = NetObj()
            net:Sync() 
        )";

        ks_str processed = ks_preprocessor_process(pp, src);
        KS_LOG_INFO("From : '%s' to '%s'", src, processed);
        std::string res = remove_spaces(processed);

        CHECK(res.find(remove_spaces("net:get_rpc(\"Sync\")")) != std::string::npos);
    }

    SUBCASE("Table Decoration") {
        const char* src = R"(
            @class Player { name = "Hero", hp = 100 }
        )";
        ks_str processed = ks_preprocessor_process(pp, src);
        KS_LOG_INFO("From : '%s' to '%s'", src, processed);
        std::string res = remove_spaces(processed);
        CHECK(res.find(remove_spaces("Player = class(\"Player\", {")) != std::string::npos);
    }

    SUBCASE("Named Parameters") {
        const char* src = R"(
            @nstate(type: int, sync: true) local x = 10
        )";
        ks_str processed = ks_preprocessor_process(pp, src);
        KS_LOG_INFO("From : '%s' to '%s'", src, processed);
        std::string res = remove_spaces(processed);
        CHECK(res.find(remove_spaces("type=\"int\"")) != std::string::npos);
    }

    ks_preprocessor_destroy(pp);
    ks_memory_shutdown();
}