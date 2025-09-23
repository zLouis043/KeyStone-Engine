#include "input.h"

#include "../old/raywrap.h"

Result<void> InputSystem::on_configs_load(EngineContext* ctx){
    return Result<void>::Ok();
}

Result<void> InputSystem::on_lua_register(EngineContext *ctx)
{
    auto view = ctx->get_lua().view();

    sol::table input_tb = view.create_named_table("input");

    sol::table mouse_tb = input_tb.create_named("mouse");

    mouse_tb["btn_left"] = MOUSE_BUTTON_LEFT;
    mouse_tb["btn_middle"] = MOUSE_BUTTON_MIDDLE;
    mouse_tb["btn_right"] = MOUSE_BUTTON_RIGHT;
    
    mouse_tb.set_function("btn_pressed", &IsMouseButtonPressed);
    mouse_tb.set_function("btn_released", &IsMouseButtonReleased);
    mouse_tb.set_function("btn_down", &IsMouseButtonDown);
    mouse_tb.set_function("btn_up", &IsMouseButtonUp);

    sol::table keyboard_tb = input_tb.create_named("keyboard");

    keyboard_tb["KEY_APOSTROPHE"] = KEY_APOSTROPHE;
    keyboard_tb["KEY_COMMA"] = KEY_COMMA;
    keyboard_tb["KEY_MINUS"] = KEY_MINUS;
    keyboard_tb["KEY_PERIOD"] = KEY_PERIOD;
    keyboard_tb["KEY_SLASH"] = KEY_SLASH;
    keyboard_tb["KEY_ZERO"] = KEY_ZERO;
    keyboard_tb["KEY_ONE"] = KEY_ONE;
    keyboard_tb["KEY_TWO"] = KEY_TWO;
    keyboard_tb["KEY_THREE"] = KEY_THREE;
    keyboard_tb["KEY_FOUR"] = KEY_FOUR;
    keyboard_tb["KEY_FIVE"] = KEY_FIVE;
    keyboard_tb["KEY_SIX"] = KEY_SIX;
    keyboard_tb["KEY_SEVEN"] = KEY_SEVEN;
    keyboard_tb["KEY_EIGHT"] = KEY_EIGHT;
    keyboard_tb["KEY_SEMICOLON"] = KEY_SEMICOLON;
    keyboard_tb["KEY_EQUAL"] = KEY_EQUAL;
    keyboard_tb["KEY_A"] = KEY_A;
    keyboard_tb["KEY_B"] = KEY_B;
    keyboard_tb["KEY_C"] = KEY_C;
    keyboard_tb["KEY_D"] = KEY_D;
    keyboard_tb["KEY_E"] = KEY_E;
    keyboard_tb["KEY_F"] = KEY_F;
    keyboard_tb["KEY_G"] = KEY_G;
    keyboard_tb["KEY_H"] = KEY_H;
    keyboard_tb["KEY_I"] = KEY_I;
    keyboard_tb["KEY_J"] = KEY_J;
    keyboard_tb["KEY_K"] = KEY_K;
    keyboard_tb["KEY_L"] = KEY_L;
    keyboard_tb["KEY_M"] = KEY_M;
    keyboard_tb["KEY_N"] = KEY_N;
    keyboard_tb["KEY_O"] = KEY_O;
    keyboard_tb["KEY_P"] = KEY_P;
    keyboard_tb["KEY_Q"] = KEY_Q;
    keyboard_tb["KEY_R"] = KEY_R;
    keyboard_tb["KEY_S"] = KEY_S;
    keyboard_tb["KEY_T"] = KEY_T;
    keyboard_tb["KEY_U"] = KEY_U;
    keyboard_tb["KEY_V"] = KEY_V;
    keyboard_tb["KEY_W"] = KEY_W;
    keyboard_tb["KEY_X"] = KEY_X;
    keyboard_tb["KEY_Y"] = KEY_Y;
    keyboard_tb["KEY_Z"] = KEY_Z;
    keyboard_tb["KEY_LEFT_BRACKET"] = KEY_LEFT_BRACKET;
    keyboard_tb["KEY_BACKSLASH"] = KEY_BACKSLASH;
    keyboard_tb["KEY_RIGHT_BRACKET"] = KEY_RIGHT_BRACKET;
    keyboard_tb["KEY_GRAVE"] = KEY_GRAVE;
    keyboard_tb["KEY_SPACE"] = KEY_SPACE;
    keyboard_tb["KEY_ESCAPE"] = KEY_ESCAPE;
    keyboard_tb["KEY_ENTER"] = KEY_ENTER;
    keyboard_tb["KEY_TAB"] = KEY_TAB;
    keyboard_tb["KEY_BACKSPACE"] = KEY_BACKSPACE;
    keyboard_tb["KEY_INSERT"] = KEY_INSERT;
    keyboard_tb["KEY_RIGHT"] = KEY_RIGHT;
    keyboard_tb["KEY_LEFT"] = KEY_LEFT;
    keyboard_tb["KEY_DOWN"] = KEY_DOWN;
    keyboard_tb["KEY_DOWN"] = KEY_DOWN;
    keyboard_tb["KEY_PAGE_UP"] = KEY_PAGE_UP;
    keyboard_tb["KEY_PAGE_DOWN"] = KEY_PAGE_DOWN;
    keyboard_tb["KEY_HOME"] = KEY_HOME;
    keyboard_tb["KEY_END"] = KEY_END;
    keyboard_tb["KEY_CAPS_LOCK"] = KEY_CAPS_LOCK;
    keyboard_tb["KEY_SCROLL_LOCK"] = KEY_SCROLL_LOCK;
    keyboard_tb["KEY_NUM_LOCK"] = KEY_NUM_LOCK;
    keyboard_tb["KEY_PRINT_SCREEN"] = KEY_PRINT_SCREEN;
    keyboard_tb["KEY_PAUSE"] = KEY_PAUSE;
    keyboard_tb["KEY_F1"] = KEY_F1;
    keyboard_tb["KEY_F2"] = KEY_F2;
    keyboard_tb["KEY_F3"] = KEY_F3;
    keyboard_tb["KEY_F4"] = KEY_F4;
    keyboard_tb["KEY_F5"] = KEY_F5;
    keyboard_tb["KEY_F6"] = KEY_F6;
    keyboard_tb["KEY_F7"] = KEY_F7;
    keyboard_tb["KEY_F8"] = KEY_F8;
    keyboard_tb["KEY_F9"] = KEY_F9;
    keyboard_tb["KEY_F10"] = KEY_F10;
    keyboard_tb["KEY_F11"] = KEY_F11;
    keyboard_tb["KEY_F12"] = KEY_F12;
    keyboard_tb["KEY_LEFT_SHIFT"] = KEY_LEFT_SHIFT;
    keyboard_tb["KEY_LEFT_CONTROL"] = KEY_LEFT_CONTROL;
    keyboard_tb["KEY_LEFT_ALT"] = KEY_LEFT_ALT;
    keyboard_tb["KEY_LEFT_SUPER"] = KEY_LEFT_SUPER;
    keyboard_tb["KEY_RIGHT_SHIFT"] = KEY_RIGHT_SHIFT;
    keyboard_tb["KEY_RIGHT_CONTROL"] = KEY_RIGHT_CONTROL;
    keyboard_tb["KEY_RIGHT_ALT"] = KEY_RIGHT_ALT;
    keyboard_tb["KEY_RIGHT_SUPER"] = KEY_RIGHT_SUPER;
    keyboard_tb["KEY_KB_MENU"] = KEY_KB_MENU;
    keyboard_tb["KEY_KP_0"] = KEY_KP_0;
    keyboard_tb["KEY_KP_1"] = KEY_KP_1;
    keyboard_tb["KEY_KP_2"] = KEY_KP_2;
    keyboard_tb["KEY_KP_3"] = KEY_KP_3;
    keyboard_tb["KEY_KP_4"] = KEY_KP_4;
    keyboard_tb["KEY_KP_5"] = KEY_KP_5;
    keyboard_tb["KEY_KP_6"] = KEY_KP_6;
    keyboard_tb["KEY_KP_7"] = KEY_KP_7;
    keyboard_tb["KEY_KP_8"] = KEY_KP_8;
    keyboard_tb["KEY_KP_9"] = KEY_KP_9;
    keyboard_tb["KEY_KP_DECIMAL"] = KEY_KP_DECIMAL;
    keyboard_tb["KEY_KP_DIVIDE"] = KEY_KP_DIVIDE;
    keyboard_tb["KEY_KP_MULTIPLY"] = KEY_KP_MULTIPLY;
    keyboard_tb["KEY_KP_SUBTRACT"] = KEY_KP_SUBTRACT;
    keyboard_tb["KEY_KP_ADD"] = KEY_KP_ADD;
    keyboard_tb["KEY_KP_ENTER"] = KEY_KP_ENTER;
    keyboard_tb["KEY_KP_EQUAL"] = KEY_KP_EQUAL;
    keyboard_tb["KEY_BACK"] = KEY_BACK;
    keyboard_tb["KEY_MENU"] = KEY_MENU;
    keyboard_tb["KEY_VOLUME_UP"] = KEY_VOLUME_UP;
    keyboard_tb["KEY_VOLUME_DOWN"] = KEY_VOLUME_DOWN;

    keyboard_tb.set_function("key_pressed", &IsKeyPressed);
    keyboard_tb.set_function("key_released", &IsKeyReleased);
    keyboard_tb.set_function("key_down", &IsKeyDown);
    keyboard_tb.set_function("key_up", &IsKeyUp);
    keyboard_tb.set_function("key_pressed_repeated", &IsKeyPressedRepeat);
    return Result<void>::Ok();
}

Result<void> InputSystem::on_start(EngineContext *ctx)
{
    return Result<void>::Ok();
}

Result<void> InputSystem::on_end(EngineContext *ctx)
{
    return Result<void>::Ok();
}

Result<void> InputSystem::on_begin_loop(EngineContext *ctx)
{
    return Result<void>::Ok();
}

Result<void> InputSystem::on_end_loop(EngineContext *ctx)
{
    return Result<void>::Ok();
}
