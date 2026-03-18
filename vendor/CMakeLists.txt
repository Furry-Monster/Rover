message(STATUS "[Rover] Configuring vendor...")

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

set(ROVER_VENDOR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(ROVER_VENDOR_LIBS "")

# --- SDL3 ---
if (EXISTS "${ROVER_VENDOR_ROOT}/SDL/CMakeLists.txt" AND NOT TARGET SDL3::SDL3)
    set(SDL_SHARED       OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC       ON  CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL      OFF CACHE BOOL "" FORCE)
    add_subdirectory("${ROVER_VENDOR_ROOT}/SDL" "${CMAKE_BINARY_DIR}/SDL" EXCLUDE_FROM_ALL)
endif ()
if (TARGET SDL3::SDL3)
    list(APPEND ROVER_VENDOR_LIBS SDL3::SDL3)
endif ()

# --- glm ---
if (EXISTS "${ROVER_VENDOR_ROOT}/glm/glm" AND NOT TARGET glm)
    add_library(glm INTERFACE)
    add_library(glm::glm ALIAS glm)
    target_include_directories(glm SYSTEM INTERFACE "${ROVER_VENDOR_ROOT}/glm")
    target_compile_definitions(glm INTERFACE
        GLM_FORCE_DEPTH_ZERO_TO_ONE  # Vulkan clip space: depth [0, 1]
        GLM_FORCE_RADIANS
    )
endif ()
if (TARGET glm)
    list(APPEND ROVER_VENDOR_LIBS glm)
endif ()

# --- Vulkan SDK ---
# find_package(Vulkan) locates the installed SDK. We only want headers and
# validation layer paths; actual function loading is handled by volk.
find_package(Vulkan REQUIRED)
message(STATUS "[Rover] Vulkan SDK: ${Vulkan_INCLUDE_DIRS}")

# --- volk (submodule) ---
# volk replaces the Vulkan loader at link time: libvolk.a is statically linked,
# and volkInitialize() opens the Vulkan loader at runtime via dlopen/LoadLibrary.
# This avoids depending on a platform-specific libvulkan.so at link time.
if (EXISTS "${ROVER_VENDOR_ROOT}/volk/volk.h" AND NOT TARGET volk)
    # Forward platform surface extension types into the volk static library.
    # SDL3 handles surface creation but the types must exist in volk's headers.
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
    add_subdirectory("${ROVER_VENDOR_ROOT}/volk" "${CMAKE_BINARY_DIR}/volk" EXCLUDE_FROM_ALL)
endif ()

if (NOT TARGET volk::volk)
    message(FATAL_ERROR "[Rover] vendor/volk submodule is missing. Run: git submodule update --init")
endif ()

# --- VMA (submodule) ---
# VMA is a header-only allocator.
# DYNAMIC_VULKAN_FUNCTIONS=1 tells it to fetch Vulkan function
# pointers at runtime via vkGet*ProcAddr, which is exactly what
# volk provides after volkInitialize().
if (EXISTS "${ROVER_VENDOR_ROOT}/VulkanMemoryAllocator/include/vk_mem_alloc.h"
    AND NOT TARGET VulkanMemoryAllocator)
    set(VMA_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory("${ROVER_VENDOR_ROOT}/VulkanMemoryAllocator"
                     "${CMAKE_BINARY_DIR}/VulkanMemoryAllocator" EXCLUDE_FROM_ALL)
endif ()

if (NOT TARGET VulkanMemoryAllocator)
    message(FATAL_ERROR "[Rover] vendor/VulkanMemoryAllocator submodule is missing. Run: git submodule update --init")
endif ()

# --- Rover::Vulkan aggregate target ---
# Consumers link against this single target to get:
#   - volk (dynamic function loading, replaces Vulkan loader)
#   - VulkanMemoryAllocator (GPU memory management)
#   - vulkan.hpp C++ wrapper headers (via Vulkan::Headers)
#   - All required compile definitions for volk + VMA + vulkan.hpp
add_library(rover_vulkan INTERFACE)
add_library(Rover::Vulkan ALIAS rover_vulkan)

target_link_libraries(rover_vulkan INTERFACE
    volk::volk
    VulkanMemoryAllocator
    Vulkan::Headers  # vulkan.hpp C++ wrapper (CMake 3.21+), no loader linkage
)

target_compile_definitions(rover_vulkan INTERFACE
    VK_NO_PROTOTYPES              # all consumers must use volk, never raw vulkan.h prototypes
    VULKAN_HPP_NO_EXCEPTIONS
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
    VMA_STATIC_VULKAN_FUNCTIONS=0 # VMA does not use static Vulkan function pointers
    VMA_DYNAMIC_VULKAN_FUNCTIONS=1 # VMA fetches via vkGetInstanceProcAddr at runtime
)

list(APPEND ROVER_VENDOR_LIBS rover_vulkan)

# --- imgui (SDL3 + Vulkan backends) ---
if (EXISTS "${ROVER_VENDOR_ROOT}/imgui/imgui.cpp" AND NOT TARGET imgui)
    set(IMGUI_DIR "${ROVER_VENDOR_ROOT}/imgui")
    set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
    )
    if (EXISTS "${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp")
        list(APPEND IMGUI_SOURCES "${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp")
    endif ()
    if (EXISTS "${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp")
        list(APPEND IMGUI_SOURCES "${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp")
    endif ()
    add_library(imgui STATIC ${IMGUI_SOURCES})
    target_include_directories(imgui SYSTEM PUBLIC ${IMGUI_DIR} "${IMGUI_DIR}/backends")
    if (TARGET SDL3::SDL3)
        target_link_libraries(imgui PUBLIC SDL3::SDL3)
    endif ()
    # imgui_impl_vulkan: IMGUI_IMPL_VULKAN_NO_PROTOTYPES tells it to skip its own
    # Vulkan function declarations; volk supplies them instead.
    target_link_libraries(imgui PUBLIC Rover::Vulkan)
    target_compile_definitions(imgui PUBLIC IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
endif ()
if (TARGET imgui)
    list(APPEND ROVER_VENDOR_LIBS imgui)
endif ()

# --- spdlog ---
if (EXISTS "${ROVER_VENDOR_ROOT}/spdlog/CMakeLists.txt" AND NOT TARGET spdlog)
    set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_BENCH   OFF CACHE BOOL "" FORCE)
    set(SPDLOG_INSTALL       OFF CACHE BOOL "" FORCE)
    add_subdirectory("${ROVER_VENDOR_ROOT}/spdlog" "${CMAKE_BINARY_DIR}/spdlog")
endif ()
if (TARGET spdlog)
    list(APPEND ROVER_VENDOR_LIBS spdlog)
endif ()

# --- EnTT ---
if (EXISTS "${ROVER_VENDOR_ROOT}/entt/src/entt" AND NOT TARGET entt)
    add_library(entt INTERFACE)
    target_include_directories(entt SYSTEM INTERFACE "${ROVER_VENDOR_ROOT}/entt/src")
endif ()
if (TARGET entt)
    list(APPEND ROVER_VENDOR_LIBS entt)
endif ()

# --- Platform system libraries ---
add_library(rover_platform_libs INTERFACE)
if (UNIX AND NOT APPLE)
    target_link_libraries(rover_platform_libs INTERFACE dl pthread)
elseif (WIN32)
    target_link_libraries(rover_platform_libs INTERFACE user32 ws2_32)
endif ()
list(APPEND ROVER_VENDOR_LIBS rover_platform_libs)

# --- Aggregate target ---
add_library(rover_vendor INTERFACE)
add_library(Rover::Vendor ALIAS rover_vendor)
target_include_directories(rover_vendor INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rover_vendor INTERFACE Rover::Core ${ROVER_VENDOR_LIBS})

set_target_properties(rover_vendor PROPERTIES FOLDER "Vendor")
