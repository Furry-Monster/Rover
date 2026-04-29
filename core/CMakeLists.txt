message(STATUS "[Rover] Configuring core...")

# ---------------------------------------------------------------------------
# rover_core -- Layer 1 foundation library.
#
# Glob sources from all core subsystems. Engine subsystems live in nested
# folders (allocator, event, math, object, task, time, variant) and add
# files independently; CONFIGURE_DEPENDS picks them up on rebuild.
# ---------------------------------------------------------------------------
rover_glob_sources(_core_sources "${CMAKE_CURRENT_SOURCE_DIR}")

rover_add_library(rover_core
    SOURCES ${_core_sources}
    PUBLIC_DEPS
        spdlog
        glm
        entt
    INCLUDE_DIRS
        "${ROVER_GENERATED_DIR}"
    FOLDER "Rover/Core"
)

add_library(Rover::Core ALIAS rover_core)

# GLM defaults follow OpenGL NDC Z in [-1, 1]; Vulkan uses [0, 1]. Without this,
# depth testing compares nonsense values and meshes look inside-out / wrongly
# layered despite a correct depth attachment.
target_compile_definitions(rover_core PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)
