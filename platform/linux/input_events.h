#pragma once

#include "core/typedefs.h"

namespace rover {

// ---------------------------------------------------------------------------
// Modifier bitflag constants for KeyEvent::modifiers.
// ---------------------------------------------------------------------------
inline constexpr u32 KeyMod_Shift = 1u << 0;
inline constexpr u32 KeyMod_Ctrl  = 1u << 1;
inline constexpr u32 KeyMod_Alt   = 1u << 2;
inline constexpr u32 KeyMod_Super = 1u << 3;

// ---------------------------------------------------------------------------
// Engine-level key codes. Subset of physical scancodes; values are stable
// and platform-independent (do not depend on SDL_Scancode numerics).
// ---------------------------------------------------------------------------
enum class KeyCode : u32 {
    Unknown = 0,

    // Letters
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    // Top-row digits
    Num0, Num1, Num2, Num3, Num4,
    Num5, Num6, Num7, Num8, Num9,

    // Function keys
    F1,  F2,  F3,  F4,  F5,  F6,
    F7,  F8,  F9,  F10, F11, F12,

    // Arrows
    Left, Right, Up, Down,

    // Whitespace / control
    Space,
    Enter,
    Escape,
    Tab,
    Backspace,
    Insert,
    Delete,
    Home,
    End,
    PageUp,
    PageDown,

    // Modifiers (left/right collapsed)
    LeftShift,  RightShift,
    LeftCtrl,   RightCtrl,
    LeftAlt,    RightAlt,
    LeftSuper,  RightSuper,

    // Punctuation (sparse but useful)
    Minus,
    Equals,
    LeftBracket,
    RightBracket,
    Backslash,
    Semicolon,
    Apostrophe,
    Grave,
    Comma,
    Period,
    Slash,
};

enum class MouseButton : u8 {
    Left   = 0,
    Right  = 1,
    Middle = 2,
    X1     = 3,
    X2     = 4,
};

// ---------------------------------------------------------------------------
// Event payloads published on EventBus by the platform's EventPump.
// ---------------------------------------------------------------------------

struct KeyEvent {
    KeyCode key;
    bool    pressed;     // false = released
    bool    repeat;
    u32     modifiers;   // bitmask of KeyMod_*
};

struct MouseButtonEvent {
    MouseButton button;
    bool        pressed;
    f32         x;
    f32         y;
};

struct MouseMoveEvent {
    f32 x;
    f32 y;
    f32 dx;
    f32 dy;
};

struct MouseWheelEvent {
    f32 dx;
    f32 dy;
};

struct WindowResizeEvent {
    u32 width;
    u32 height;
};

struct WindowCloseEvent {};

struct WindowFocusEvent {
    bool focused;
};

} // namespace rover
