#pragma once

#include <spdlog/spdlog.h>

#include <memory>

namespace rover
{

    class Log
    {
    public:
        Log() = delete;

        static void init();
        static void shutdown();

        static std::shared_ptr<spdlog::logger>& get_core_logger() { return s_core_logger; }

        static std::shared_ptr<spdlog::logger>& get_app_logger() { return s_app_logger; }

    private:
        static std::shared_ptr<spdlog::logger> s_core_logger;
        static std::shared_ptr<spdlog::logger> s_app_logger;
    };

} // namespace rover

// --- Core (engine) logging macros -------------------------------------------

#ifndef ROVER_RELEASE
    #define ROVER_LOG_TRACE(...) ::rover::Log::get_core_logger()->trace(__VA_ARGS__)
    #define ROVER_LOG_DEBUG(...) ::rover::Log::get_core_logger()->debug(__VA_ARGS__)
#else
    #define ROVER_LOG_TRACE(...) (void)0
    #define ROVER_LOG_DEBUG(...) (void)0
#endif

#define ROVER_LOG_INFO(...)  ::rover::Log::get_core_logger()->info(__VA_ARGS__)
#define ROVER_LOG_WARN(...)  ::rover::Log::get_core_logger()->warn(__VA_ARGS__)
#define ROVER_LOG_ERROR(...) ::rover::Log::get_core_logger()->error(__VA_ARGS__)
#define ROVER_LOG_FATAL(...) ::rover::Log::get_core_logger()->critical(__VA_ARGS__)

// --- App (game) logging macros ----------------------------------------------

#ifndef ROVER_RELEASE
    #define ROVER_APP_TRACE(...) ::rover::Log::get_app_logger()->trace(__VA_ARGS__)
    #define ROVER_APP_DEBUG(...) ::rover::Log::get_app_logger()->debug(__VA_ARGS__)
#else
    #define ROVER_APP_TRACE(...) (void)0
    #define ROVER_APP_DEBUG(...) (void)0
#endif

#define ROVER_APP_INFO(...)  ::rover::Log::get_app_logger()->info(__VA_ARGS__)
#define ROVER_APP_WARN(...)  ::rover::Log::get_app_logger()->warn(__VA_ARGS__)
#define ROVER_APP_ERROR(...) ::rover::Log::get_app_logger()->error(__VA_ARGS__)
#define ROVER_APP_FATAL(...) ::rover::Log::get_app_logger()->critical(__VA_ARGS__)
