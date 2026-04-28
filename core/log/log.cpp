#include "core/log/log.h"

#include <spdlog/sinks/stdout_color_sinks.h>

namespace rover {

std::shared_ptr<spdlog::logger> Log::s_core_logger;
std::shared_ptr<spdlog::logger> Log::s_app_logger;

void Log::init() {
    spdlog::set_pattern("[%T.%e] [%n] [%^%l%$] %v");

    s_core_logger = spdlog::stdout_color_mt("Rover");
    s_core_logger->set_level(spdlog::level::trace);

    s_app_logger = spdlog::stdout_color_mt("App");
    s_app_logger->set_level(spdlog::level::trace);
}

void Log::shutdown() {
    s_app_logger.reset();
    s_core_logger.reset();
    spdlog::shutdown();
}

} // namespace rover
