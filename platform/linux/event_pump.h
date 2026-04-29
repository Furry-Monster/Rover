#pragma once

#include "core/event/event.h"
#include "core/typedefs.h"
#include "platform/linux/input_events.h"

namespace rover
{

    class Window;

    // ---------------------------------------------------------------------------
    // EventPump -- drains SDL's event queue once per frame and republishes
    // translated POD payloads on the engine's EventBus.
    // ---------------------------------------------------------------------------
    class EventPump
    {
    public:
        EventPump(EventBus& bus, Window& window);

        // Drain all pending SDL events. Call once per frame.
        void poll();

    private:
        EventBus& bus_;
        Window&   window_;
        f32       last_mouse_x_    = 0.0f;
        f32       last_mouse_y_    = 0.0f;
        bool      have_last_mouse_ = false;

        [[nodiscard]] KeyCode translate_scancode(int scancode) const;
        [[nodiscard]] u32     translate_modifiers(u16 sdl_mod) const;
    };

} // namespace rover
