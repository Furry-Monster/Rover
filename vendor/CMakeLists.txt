message(STATUS "[Rover] Configuring vendor...")

# Each engine layer links vendor targets as needed (SDL3, spdlog, etc.).
# request only what they depend on so transitive includes stay tight.

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

set(ROVER_VENDOR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

# ---------------------------------------------------------------------------
# SDL3
# ---------------------------------------------------------------------------
set(SDL_SHARED       OFF CACHE BOOL "" FORCE)
set(SDL_STATIC       ON  CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL      OFF CACHE BOOL "" FORCE)

add_subdirectory("${ROVER_VENDOR_ROOT}/SDL" EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
find_package(Vulkan REQUIRED)
message(STATUS "[Rover] Vulkan SDK: ${Vulkan_INCLUDE_DIRS}")

# ---------------------------------------------------------------------------
# volk  (dynamic Vulkan loader)
# ---------------------------------------------------------------------------
if (WIN32)
    set(VOLK_STATIC_DEFINES "VK_USE_PLATFORM_WIN32_KHR"
        CACHE STRING "volk platform defines" FORCE)
elseif (UNIX AND NOT APPLE)
    set(VOLK_STATIC_DEFINES
        "VK_USE_PLATFORM_XLIB_KHR;VK_USE_PLATFORM_XCB_KHR;VK_USE_PLATFORM_WAYLAND_KHR"
        CACHE STRING "volk platform defines" FORCE)
elseif (APPLE)
    set(VOLK_STATIC_DEFINES "VK_USE_PLATFORM_METAL_EXT"
        CACHE STRING "volk platform defines" FORCE)
endif ()
set(VOLK_PULL_IN_VULKAN ON  CACHE BOOL "volk finds Vulkan headers" FORCE)
set(VOLK_INSTALL        OFF CACHE BOOL "" FORCE)

add_subdirectory("${ROVER_VENDOR_ROOT}/volk" EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------
# VulkanMemoryAllocator  (header-only GPU allocator)
# ---------------------------------------------------------------------------
set(VMA_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory("${ROVER_VENDOR_ROOT}/VulkanMemoryAllocator" EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------
# Rover::Vulkan  (aggregate target)
# ---------------------------------------------------------------------------
add_library(rover_vulkan INTERFACE)
add_library(Rover::Vulkan ALIAS rover_vulkan)

target_link_libraries(rover_vulkan INTERFACE
    volk::volk
    VulkanMemoryAllocator
    Vulkan::Headers
)

target_compile_definitions(rover_vulkan INTERFACE
    VK_NO_PROTOTYPES
    VULKAN_HPP_NO_EXCEPTIONS
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
    VMA_STATIC_VULKAN_FUNCTIONS=0
    VMA_DYNAMIC_VULKAN_FUNCTIONS=1
)

# ---------------------------------------------------------------------------
# Dear ImGui  (SDL3 + Vulkan backends)
# ---------------------------------------------------------------------------
set(IMGUI_DIR "${ROVER_VENDOR_ROOT}/imgui")
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui SYSTEM PUBLIC "${IMGUI_DIR}" "${IMGUI_DIR}/backends")
target_link_libraries(imgui PUBLIC SDL3::SDL3 Rover::Vulkan)
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_VULKAN_NO_PROTOTYPES)

# ---------------------------------------------------------------------------
# spdlog
# ---------------------------------------------------------------------------
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH   OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL       OFF CACHE BOOL "" FORCE)

add_subdirectory("${ROVER_VENDOR_ROOT}/spdlog")

# ---------------------------------------------------------------------------
# EnTT  (header-only — manual INTERFACE target)
# ---------------------------------------------------------------------------
add_library(entt INTERFACE)
target_include_directories(entt SYSTEM INTERFACE "${ROVER_VENDOR_ROOT}/entt/src")

# ---------------------------------------------------------------------------
# doctest
# ---------------------------------------------------------------------------
add_library(rover_doctest INTERFACE)
add_library(Rover::Doctest ALIAS rover_doctest)
target_include_directories(rover_doctest SYSTEM INTERFACE
    "${ROVER_VENDOR_ROOT}/doctest")

# ---------------------------------------------------------------------------
# Platform system libraries
# ---------------------------------------------------------------------------
add_library(rover_platform_libs INTERFACE)
if (UNIX AND NOT APPLE)
    target_link_libraries(rover_platform_libs INTERFACE dl pthread)
elseif (WIN32)
    target_link_libraries(rover_platform_libs INTERFACE user32 ws2_32)
endif ()
