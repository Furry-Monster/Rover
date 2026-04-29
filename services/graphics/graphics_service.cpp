#include "services/graphics/graphics_service.h"

#include "core/log/log.h"

namespace rover
{

    GraphicsService& GraphicsService::get()
    {
        static GraphicsService instance;
        return instance;
    }

    void GraphicsService::bind_device(GraphicsDevice* device)
    {
        device_ = device;
        if (device != nullptr)
        {
            ROVER_LOG_INFO("GraphicsService bound to device '{}'", device->get_device_name());
        }
    }

    void GraphicsService::unbind_device()
    {
        device_ = nullptr;
    }

    void GraphicsService::render_frame()
    {
        if (device_ == nullptr)
        {
            ROVER_LOG_WARN("GraphicsService::render_frame called without bound device");
            return;
        }
        if (!frame_graph_.compile())
        {
            ROVER_LOG_WARN("GraphicsService: frame graph compile failed; skipping execute");
            return;
        }
        frame_graph_.execute(*device_);
    }

} // namespace rover
