#ifdef ROVER_TEST_VULKAN_SYNC

    #include "drivers/vulkan/vk_sync.h"

    #include <doctest/doctest.h>

using namespace rover;

namespace
{
    struct TrackedResource
    {
        ResourceAccess       access      = ResourceAccess::Undefined;
        VkPipelineStageFlags sync_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkAccessFlags        sync_access = 0;
    };
} // namespace

TEST_CASE("Vulkan sync: last writer remains in scope for readers at different stages")
{
    TrackedResource buffer;
    vk_track_access(buffer, ResourceAccess::TransferWrite);
    vk_track_access(buffer, ResourceAccess::VertexRead);
    vk_track_access(buffer, ResourceAccess::UniformRead);
    CHECK(buffer.access == ResourceAccess::UniformRead);
    CHECK((buffer.sync_access & VK_ACCESS_TRANSFER_WRITE_BIT) != 0);
    CHECK((buffer.sync_access & VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT) != 0);
    CHECK((buffer.sync_access & VK_ACCESS_UNIFORM_READ_BIT) != 0);
    CHECK((buffer.sync_stages & VK_PIPELINE_STAGE_TRANSFER_BIT) != 0);
    CHECK((buffer.sync_stages & VK_PIPELINE_STAGE_VERTEX_INPUT_BIT) != 0);
    CHECK((buffer.sync_stages & VK_PIPELINE_STAGE_ALL_COMMANDS_BIT) != 0);

    // A new write consumes the accumulated dependency, starting a fresh scope.
    vk_track_access(buffer, ResourceAccess::StorageWrite);
    CHECK(buffer.sync_access == VK_ACCESS_SHADER_WRITE_BIT);
    CHECK(buffer.sync_stages == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
}

TEST_CASE("Vulkan sync: readback retains transfer writes and host reads before next frame overwrite")
{
    TrackedResource buffer;
    vk_track_access(buffer, ResourceAccess::TransferWrite);
    vk_track_access(buffer, ResourceAccess::HostRead);
    CHECK((buffer.sync_stages & VK_PIPELINE_STAGE_TRANSFER_BIT) != 0);
    CHECK((buffer.sync_stages & VK_PIPELINE_STAGE_HOST_BIT) != 0);
    CHECK((buffer.sync_access & VK_ACCESS_TRANSFER_WRITE_BIT) != 0);
    CHECK((buffer.sync_access & VK_ACCESS_HOST_READ_BIT) != 0);
    vk_track_access(buffer, ResourceAccess::TransferWrite);
    CHECK(buffer.sync_stages == VK_PIPELINE_STAGE_TRANSFER_BIT);
    CHECK(buffer.sync_access == VK_ACCESS_TRANSFER_WRITE_BIT);
}

TEST_CASE("Vulkan sync: offscreen sampling layout is independent of retained attachment writes")
{
    TrackedResource image;
    vk_track_access(image, ResourceAccess::ColorAttachment);
    vk_track_access(image, ResourceAccess::TransferRead);
    vk_track_access(image, ResourceAccess::ShaderRead);
    CHECK(vk_access_scope(image.access).layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    CHECK((image.sync_access & VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT) != 0);
    CHECK((image.sync_access & VK_ACCESS_TRANSFER_READ_BIT) != 0);
    CHECK((image.sync_access & VK_ACCESS_SHADER_READ_BIT) != 0);
    CHECK(vk_access_scope(ResourceAccess::Present).layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    CHECK(vk_access_scope(ResourceAccess::DepthStencilAttachment).stages ==
          (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT));
    CHECK(vk_image_aspect(VK_FORMAT_D32_SFLOAT) == VK_IMAGE_ASPECT_DEPTH_BIT);
    CHECK(vk_image_aspect(VK_FORMAT_D24_UNORM_S8_UINT) == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT));
}

#endif
