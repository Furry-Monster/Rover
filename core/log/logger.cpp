#include "core/log/logger.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

void
rover_log_init()
{
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::trace);
    console_sink->set_pattern("[%^%l%$] %v");

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("rover.log", true);
    file_sink->set_level(spdlog::level::trace);
    file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%s:%#] %v");

    auto logger = std::make_shared<spdlog::logger>("rover", spdlog::sinks_init_list{console_sink, file_sink});
    logger->set_level(spdlog::level::trace);
    logger->flush_on(spdlog::level::warn);

    spdlog::set_default_logger(std::move(logger));
    LOG_INFO("Logger initialized.");
}

void
rover_log_shutdown()
{
    LOG_INFO("Logger shutting down.");
    spdlog::shutdown();
}
