#pragma once

#include "core/event/signal.h"

#include <typeindex>
#include <unordered_map>

namespace rover {

/// Centralized, type-keyed event bus for decoupled communication.
/// Pass by reference — this is NOT a global singleton.
/// NOT thread-safe — external synchronization is required for concurrent access.
class EventBus {
public:
    EventBus();
    ~EventBus();

    EventBus(EventBus&&) = default;
    EventBus& operator=(EventBus&&) = default;

    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    /// Subscribe to events of a given type. Returns an ID for later unsubscription.
    template<typename EventType>
    SlotId subscribe(Delegate<void(const EventType&)> slot) {
        return get_or_create_signal<EventType>().connect(std::move(slot));
    }

    /// Unsubscribe a previously registered slot by its ID.
    template<typename EventType>
    void unsubscribe(SlotId id) {
        auto it = signals_.find(std::type_index(typeid(EventType)));
        if (it != signals_.end()) {
            static_cast<Signal<const EventType&>*>(it->second.ptr)->disconnect(id);
        }
    }

    /// Publish an event to all subscribers of that event type.
    template<typename EventType>
    void publish(const EventType& event) {
        auto it = signals_.find(std::type_index(typeid(EventType)));
        if (it != signals_.end()) {
            static_cast<Signal<const EventType&>*>(it->second.ptr)->emit(event);
        }
    }

private:
    struct ErasedSignal {
        void* ptr = nullptr;
        void (*deleter)(void*) = nullptr;

        ErasedSignal() = default;

        template<typename T>
        explicit ErasedSignal(T* signal)
            : ptr(signal)
            , deleter([](void* p) { delete static_cast<T*>(p); })
        {}

        ~ErasedSignal() {
            if (deleter) { deleter(ptr); }
        }

        ErasedSignal(ErasedSignal&& o) noexcept
            : ptr(o.ptr), deleter(o.deleter) {
            o.ptr     = nullptr;
            o.deleter = nullptr;
        }

        ErasedSignal& operator=(ErasedSignal&& o) noexcept {
            if (this != &o) {
                if (deleter) { deleter(ptr); }
                ptr     = o.ptr;
                deleter = o.deleter;
                o.ptr     = nullptr;
                o.deleter = nullptr;
            }
            return *this;
        }

        ErasedSignal(const ErasedSignal&) = delete;
        ErasedSignal& operator=(const ErasedSignal&) = delete;
    };

    template<typename EventType>
    Signal<const EventType&>& get_or_create_signal() {
        auto key = std::type_index(typeid(EventType));
        auto it = signals_.find(key);
        if (it == signals_.end()) {
            it = signals_.emplace(key, ErasedSignal(new Signal<const EventType&>())).first;
        }
        return *static_cast<Signal<const EventType&>*>(it->second.ptr);
    }

    std::unordered_map<std::type_index, ErasedSignal> signals_;
};

} // namespace rover
