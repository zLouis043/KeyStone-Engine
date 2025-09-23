#include "draw.h"

#include "../old/raywrap.h"
#include "../old/jsonwrap.h"

void draw_rect(float x, float y, float w, float h, Color c){
    DrawRectangle((int)x, (int)y, (int)w, (int)h, c);
}

void draw_text(const char* s, float x, float y, float fs, Color c){
    DrawText(s, (int)x, (int)y, (int)fs, c);
}

Result<void> DrawSystem::on_configs_load(EngineContext *ctx)
{
    return Result<void>::Ok();
}

Result<void> DrawSystem::on_lua_register(EngineContext *ctx)
{
    auto view = ctx->get_lua().view();

    ctx->get_lua().register_type<Color>("Color", 
        "r", &Color::r,
        "g", &Color::g,
        "b", &Color::b,
        "a", &Color::a,
        "hex", &GetColor,
        "rgb", [](float r, float g, float b) {
            Color c = {(uint8_t)r, (uint8_t)g, (uint8_t)b, 255};
            return c;
        },
        "rgba", [](float r, float g, float b, float a) {
            Color c = {(uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a};
            return c;
        },
        "to_json", [](const Color& self) -> json {
            json j = { 
                {"r", self.r},
                {"g", self.g},
                {"b", self.b},
                {"a", self.a},
            };
            return j;
        },
        "from_json", [](const json& data) -> Color {
            Color c;
            c.r = data["r"];
            c.g = data["g"];
            c.b = data["b"];
            c.a = data["a"];
            return c;
        }
    );

    sol::table draw_tb = view.create_named_table("draw");
    draw_tb.set_function("rectangle", &draw_rect);
    draw_tb.set_function("text", &draw_text);

    return Result<void>::Ok();
}

Result<void> DrawSystem::on_start(EngineContext *ctx)
{
    return Result<void>::Ok();
}

Result<void> DrawSystem::on_end(EngineContext *ctx)
{
    return Result<void>::Ok();
}

Result<void> DrawSystem::on_begin_loop(EngineContext *ctx)
{   
    BeginDrawing();
    ClearBackground(WHITE);
    return Result<void>::Ok();
}

Result<void> DrawSystem::on_end_loop(EngineContext *ctx)
{
    EndDrawing();
    return Result<void>::Ok();
}
