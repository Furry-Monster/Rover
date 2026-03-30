#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <mutex>
#include <typeindex>
#include <unordered_map>
#include <vector>

/**
 * @brief
 *
 * Type-erased publish/subscribe event bus.
 * Ported from Realm engine — thread-safe for subscribe/unsubscribe;
 * publish copies handler list under the lock, then invokes outside it.
 */
class EventBus
{
public:
    using HandlerId = uint64_t;

    EventBus()           = default;
    ~EventBus() noexcept = default;

    EventBus(const EventBus&)            = delete;
    EventBus& operator=(const EventBus&) = delete;
    EventBus(EventBus&&)                 = delete;
    EventBus& operator=(EventBus&&)      = delete;

    template <typename E>
    [[nodiscard]] HandlerId subscribe(std::function<void(const E&)> p_handler)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        HandlerId                   id       = _next_id++;
        auto                        type_key = std::type_index(typeid(E));

        _handlers[type_key].push_back({id, [handler = std::move(p_handler)](const void* raw) {
                                           handler(*static_cast<const E*>(raw));
                                       }});
        return id;
    }

    void unsubscribe(HandlerId p_id)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        for (auto& [type, entries] : _handlers)
        {
            auto it =
                std::remove_if(entries.begin(), entries.end(), [p_id](const HandlerEntry& e) { return e.id == p_id; });
            if (it != entries.end())
            {
                entries.erase(it, entries.end());
                return;
            }
        }
    }

    template <typename E>
    void publish(const E& p_event) const
    {
        std::vector<ErasedHandler> snapshot;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            auto                        type_key = std::type_index(typeid(E));
            auto                        it       = _handlers.find(type_key);
            if (it == _handlers.end())
            {
                return;
            }
            snapshot.reserve(it->second.size());
            for (const auto& entry : it->second)
            {
                snapshot.push_back(entry.func);
            }
        }
        for (const auto& func : snapshot)
        {
            func(&p_event);
        }
    }

private:
    using ErasedHandler = std::function<void(const void*)>;

    struct HandlerEntry
    {
        HandlerId     id;
        ErasedHandler func;
    };

    std::unordered_map<std::type_index, std::vector<HandlerEntry>> _handlers;
    HandlerId                                                      _next_id{0};
    mutable std::mutex                                             _mutex;
};

extern EventBus* g_event_bus;
