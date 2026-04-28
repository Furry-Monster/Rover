#include "platform/linux/event_pump.h"

#include "platform/linux/window.h"

#include <SDL3/SDL.h>

namespace rover {

EventPump::EventPump(EventBus& bus, Window& window)
    : bus_(bus)
    , window_(window)
{}

void EventPump::poll() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_EVENT_QUIT: {
                bus_.publish(WindowCloseEvent{});
                window_.mark_should_close();
                break;
            }

            case SDL_EVENT_WINDOW_RESIZED: {
                const u32 w = static_cast<u32>(e.window.data1 > 0 ? e.window.data1 : 0);
                const u32 h = static_cast<u32>(e.window.data2 > 0 ? e.window.data2 : 0);
                window_.set_size(w, h);
                bus_.publish(WindowResizeEvent{w, h});
                break;
            }

            case SDL_EVENT_WINDOW_FOCUS_GAINED: {
                bus_.publish(WindowFocusEvent{true});
                break;
            }
            case SDL_EVENT_WINDOW_FOCUS_LOST: {
                bus_.publish(WindowFocusEvent{false});
                break;
            }

            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP: {
                KeyEvent ke{};
                ke.key       = translate_scancode(static_cast<int>(e.key.scancode));
                ke.pressed   = e.key.down;
                ke.repeat    = e.key.repeat;
                ke.modifiers = translate_modifiers(static_cast<u16>(e.key.mod));
                bus_.publish(ke);
                break;
            }

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP: {
                MouseButtonEvent mb{};
                switch (e.button.button) {
                    case SDL_BUTTON_LEFT:   mb.button = MouseButton::Left;   break;
                    case SDL_BUTTON_RIGHT:  mb.button = MouseButton::Right;  break;
                    case SDL_BUTTON_MIDDLE: mb.button = MouseButton::Middle; break;
                    case SDL_BUTTON_X1:     mb.button = MouseButton::X1;     break;
                    case SDL_BUTTON_X2:     mb.button = MouseButton::X2;     break;
                    default: continue;
                }
                mb.pressed = e.button.down;
                mb.x       = e.button.x;
                mb.y       = e.button.y;
                bus_.publish(mb);
                break;
            }

            case SDL_EVENT_MOUSE_MOTION: {
                MouseMoveEvent mm{};
                mm.x = e.motion.x;
                mm.y = e.motion.y;
                if (have_last_mouse_) {
                    mm.dx = mm.x - last_mouse_x_;
                    mm.dy = mm.y - last_mouse_y_;
                } else {
                    // SDL also exposes xrel/yrel; prefer those on the very
                    // first event so dx/dy are sensible from frame zero.
                    mm.dx = e.motion.xrel;
                    mm.dy = e.motion.yrel;
                }
                last_mouse_x_    = mm.x;
                last_mouse_y_    = mm.y;
                have_last_mouse_ = true;
                bus_.publish(mm);
                break;
            }

            case SDL_EVENT_MOUSE_WHEEL: {
                MouseWheelEvent mw{};
                mw.dx = e.wheel.x;
                mw.dy = e.wheel.y;
                bus_.publish(mw);
                break;
            }

            default:
                break;
        }
    }
}

u32 EventPump::translate_modifiers(u16 sdl_mod) const {
    u32 out = 0;
    if (sdl_mod & SDL_KMOD_SHIFT) { out |= KeyMod_Shift; }
    if (sdl_mod & SDL_KMOD_CTRL)  { out |= KeyMod_Ctrl;  }
    if (sdl_mod & SDL_KMOD_ALT)   { out |= KeyMod_Alt;   }
    if (sdl_mod & SDL_KMOD_GUI)   { out |= KeyMod_Super; }
    return out;
}

KeyCode EventPump::translate_scancode(int scancode) const {
    switch (static_cast<SDL_Scancode>(scancode)) {
        // Letters
        case SDL_SCANCODE_A: return KeyCode::A;
        case SDL_SCANCODE_B: return KeyCode::B;
        case SDL_SCANCODE_C: return KeyCode::C;
        case SDL_SCANCODE_D: return KeyCode::D;
        case SDL_SCANCODE_E: return KeyCode::E;
        case SDL_SCANCODE_F: return KeyCode::F;
        case SDL_SCANCODE_G: return KeyCode::G;
        case SDL_SCANCODE_H: return KeyCode::H;
        case SDL_SCANCODE_I: return KeyCode::I;
        case SDL_SCANCODE_J: return KeyCode::J;
        case SDL_SCANCODE_K: return KeyCode::K;
        case SDL_SCANCODE_L: return KeyCode::L;
        case SDL_SCANCODE_M: return KeyCode::M;
        case SDL_SCANCODE_N: return KeyCode::N;
        case SDL_SCANCODE_O: return KeyCode::O;
        case SDL_SCANCODE_P: return KeyCode::P;
        case SDL_SCANCODE_Q: return KeyCode::Q;
        case SDL_SCANCODE_R: return KeyCode::R;
        case SDL_SCANCODE_S: return KeyCode::S;
        case SDL_SCANCODE_T: return KeyCode::T;
        case SDL_SCANCODE_U: return KeyCode::U;
        case SDL_SCANCODE_V: return KeyCode::V;
        case SDL_SCANCODE_W: return KeyCode::W;
        case SDL_SCANCODE_X: return KeyCode::X;
        case SDL_SCANCODE_Y: return KeyCode::Y;
        case SDL_SCANCODE_Z: return KeyCode::Z;

        // Top-row digits
        case SDL_SCANCODE_0: return KeyCode::Num0;
        case SDL_SCANCODE_1: return KeyCode::Num1;
        case SDL_SCANCODE_2: return KeyCode::Num2;
        case SDL_SCANCODE_3: return KeyCode::Num3;
        case SDL_SCANCODE_4: return KeyCode::Num4;
        case SDL_SCANCODE_5: return KeyCode::Num5;
        case SDL_SCANCODE_6: return KeyCode::Num6;
        case SDL_SCANCODE_7: return KeyCode::Num7;
        case SDL_SCANCODE_8: return KeyCode::Num8;
        case SDL_SCANCODE_9: return KeyCode::Num9;

        // Function keys
        case SDL_SCANCODE_F1:  return KeyCode::F1;
        case SDL_SCANCODE_F2:  return KeyCode::F2;
        case SDL_SCANCODE_F3:  return KeyCode::F3;
        case SDL_SCANCODE_F4:  return KeyCode::F4;
        case SDL_SCANCODE_F5:  return KeyCode::F5;
        case SDL_SCANCODE_F6:  return KeyCode::F6;
        case SDL_SCANCODE_F7:  return KeyCode::F7;
        case SDL_SCANCODE_F8:  return KeyCode::F8;
        case SDL_SCANCODE_F9:  return KeyCode::F9;
        case SDL_SCANCODE_F10: return KeyCode::F10;
        case SDL_SCANCODE_F11: return KeyCode::F11;
        case SDL_SCANCODE_F12: return KeyCode::F12;

        // Arrows
        case SDL_SCANCODE_LEFT:  return KeyCode::Left;
        case SDL_SCANCODE_RIGHT: return KeyCode::Right;
        case SDL_SCANCODE_UP:    return KeyCode::Up;
        case SDL_SCANCODE_DOWN:  return KeyCode::Down;

        // Whitespace / control
        case SDL_SCANCODE_SPACE:     return KeyCode::Space;
        case SDL_SCANCODE_RETURN:    return KeyCode::Enter;
        case SDL_SCANCODE_RETURN2:   return KeyCode::Enter;
        case SDL_SCANCODE_KP_ENTER:  return KeyCode::Enter;
        case SDL_SCANCODE_ESCAPE:    return KeyCode::Escape;
        case SDL_SCANCODE_TAB:       return KeyCode::Tab;
        case SDL_SCANCODE_BACKSPACE: return KeyCode::Backspace;
        case SDL_SCANCODE_INSERT:    return KeyCode::Insert;
        case SDL_SCANCODE_DELETE:    return KeyCode::Delete;
        case SDL_SCANCODE_HOME:      return KeyCode::Home;
        case SDL_SCANCODE_END:       return KeyCode::End;
        case SDL_SCANCODE_PAGEUP:    return KeyCode::PageUp;
        case SDL_SCANCODE_PAGEDOWN:  return KeyCode::PageDown;

        // Modifiers
        case SDL_SCANCODE_LSHIFT: return KeyCode::LeftShift;
        case SDL_SCANCODE_RSHIFT: return KeyCode::RightShift;
        case SDL_SCANCODE_LCTRL:  return KeyCode::LeftCtrl;
        case SDL_SCANCODE_RCTRL:  return KeyCode::RightCtrl;
        case SDL_SCANCODE_LALT:   return KeyCode::LeftAlt;
        case SDL_SCANCODE_RALT:   return KeyCode::RightAlt;
        case SDL_SCANCODE_LGUI:   return KeyCode::LeftSuper;
        case SDL_SCANCODE_RGUI:   return KeyCode::RightSuper;

        // Punctuation
        case SDL_SCANCODE_MINUS:        return KeyCode::Minus;
        case SDL_SCANCODE_EQUALS:       return KeyCode::Equals;
        case SDL_SCANCODE_LEFTBRACKET:  return KeyCode::LeftBracket;
        case SDL_SCANCODE_RIGHTBRACKET: return KeyCode::RightBracket;
        case SDL_SCANCODE_BACKSLASH:    return KeyCode::Backslash;
        case SDL_SCANCODE_SEMICOLON:    return KeyCode::Semicolon;
        case SDL_SCANCODE_APOSTROPHE:   return KeyCode::Apostrophe;
        case SDL_SCANCODE_GRAVE:        return KeyCode::Grave;
        case SDL_SCANCODE_COMMA:        return KeyCode::Comma;
        case SDL_SCANCODE_PERIOD:       return KeyCode::Period;
        case SDL_SCANCODE_SLASH:        return KeyCode::Slash;

        default:
            return KeyCode::Unknown;
    }
}

} // namespace rover
