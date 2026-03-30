#pragma once

#include <spdlog/spdlog.h>

void
rover_log_init();
void
rover_log_shutdown();

// clang-format off
#define LOG_TRACE(...) SPDLOG_TRACE(__VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__)
#define LOG_INFO(...)  SPDLOG_INFO(__VA_ARGS__)
#define LOG_WARN(...)  SPDLOG_WARN(__VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_ERROR(__VA_ARGS__)
#define LOG_FATAL(...) do { spdlog::critical(__VA_ARGS__); std::abort(); } while (0)
// clang-format on
