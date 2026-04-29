#pragma once

#include "core/event/delegate.h"
#include "core/typedefs.h"

#include <utility>
#include <vector>

namespace rover
{

    using SlotId = u32;

    /// Signal-slot observer pattern. Listeners are stored as Delegates.
    /// NOT thread-safe — external synchronization is required for concurrent access.
    template <typename... Args>
    class Signal
    {
    public:
        /// Connect a slot. Returns an ID that can be used to disconnect later.
        SlotId connect(Delegate<void(Args...)> slot)
        {
            SlotId id = next_id_++;
            slots_.emplace_back(id, std::move(slot));
            return id;
        }

        /// Disconnect a single slot by its ID.
        void disconnect(SlotId id)
        {
            for (auto it = slots_.begin(); it != slots_.end(); ++it)
            {
                if (it->first == id)
                {
                    slots_.erase(it);
                    return;
                }
            }
        }

        /// Remove all connected slots.
        void disconnect_all() { slots_.clear(); }

        /// Emit the signal, invoking every connected slot with the given arguments.
        /// Re-entrant connect/disconnect during emit is undefined behavior.
        void emit(Args... args)
        {
            for (auto& [id, slot] : slots_)
            {
                slot(args...);
            }
        }

        [[nodiscard]] usize slot_count() const { return slots_.size(); }

    private:
        std::vector<std::pair<SlotId, Delegate<void(Args...)>>> slots_;
        SlotId                                                  next_id_ = 1;
    };

} // namespace rover
