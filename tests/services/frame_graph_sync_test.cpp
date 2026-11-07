#include "services/graphics/frame_graph.h"
#include "tests/mocks/mock_graphics_device.h"

#include <doctest/doctest.h>

using namespace rover;

TEST_CASE("FrameGraph sync: color producer transitions before sampling and inside the correct command lists")
{
    FrameGraph graph;
    const auto texture = graph.import_texture("offscreen", 11, Format::R8G8B8A8_UNORM, 16, 16, 22);
    graph.add_pass(
        "sample",
        [&](PassBuilder& b) { b.read(texture); },
        [](PassExecuteContext& ctx) { static_cast<MockGraphicsDevice&>(ctx.device).events.emplace_back("sample"); });
    const auto producer = graph.add_pass(
        "render",
        [&](PassBuilder& b) { b.write(texture); },
        [](PassExecuteContext& ctx) { static_cast<MockGraphicsDevice&>(ctx.device).events.emplace_back("render"); });
    graph.set_color_attachment(producer, texture, {});
    REQUIRE(graph.compile());
    REQUIRE(graph.barriers().size() == 2);
    REQUIRE(graph.barriers()[producer].size() == 1);
    CHECK(graph.barriers()[producer][0].before == ResourceAccess::Undefined);
    CHECK(graph.barriers()[producer][0].after == ResourceAccess::ColorAttachment);
    REQUIRE(graph.barriers()[0].size() == 1);
    CHECK(graph.barriers()[0][0].resource == texture);
    CHECK(graph.barriers()[0][0].before == ResourceAccess::ColorAttachment);
    CHECK(graph.barriers()[0][0].after == ResourceAccess::ShaderRead);

    MockGraphicsDevice device;
    REQUIRE(graph.execute(device));
    CHECK(device.events == std::vector<std::string>{"begin_cmd",
                                                    "barriers",
                                                    "begin_pass",
                                                    "render",
                                                    "end_pass",
                                                    "end_cmd",
                                                    "begin_cmd",
                                                    "barriers",
                                                    "sample",
                                                    "end_cmd"});
    REQUIRE(device.barrier_batches.size() == 2);
    REQUIRE(device.barrier_batches[0].textures.size() == 1);
    CHECK(device.barrier_batches[0].textures[0].texture == 11);
    CHECK(device.barrier_batches[0].textures[0].access == ResourceAccess::ColorAttachment);
    CHECK(device.barrier_batches[1].textures[0].access == ResourceAccess::ShaderRead);
}

TEST_CASE("FrameGraph sync: transfer readback batches mixed resources and exports host visibility last")
{
    FrameGraph graph;
    const auto image    = graph.import_texture("offscreen", 11, Format::R8G8B8A8_UNORM, 16, 16, 22);
    const auto readback = graph.import_buffer("readback", 33);
    graph.set_final_access(readback, ResourceAccess::HostRead);
    graph.add_pass(
        "copy",
        [&](PassBuilder& b) {
            b.read(image, ResourceAccess::TransferRead);
            b.write(readback, ResourceAccess::TransferWrite);
        },
        [&](PassExecuteContext& ctx) { CHECK(ctx.device.cmd_copy_texture_to_buffer(ctx.cmd, 11, 33)); });
    const auto producer = graph.add_pass("clear", {}, {});
    graph.set_color_attachment(producer, image, {});
    REQUIRE(graph.compile());
    REQUIRE(graph.barriers()[0].size() == 2);
    REQUIRE(graph.final_barriers().size() == 1);
    CHECK(graph.final_barriers()[0].resource == readback);
    CHECK(graph.final_barriers()[0].before == ResourceAccess::TransferWrite);
    CHECK(graph.final_barriers()[0].after == ResourceAccess::HostRead);
    MockGraphicsDevice device;
    REQUIRE(graph.execute(device));
    CHECK(device.events == std::vector<std::string>{"begin_cmd",
                                                    "barriers",
                                                    "begin_pass",
                                                    "end_pass",
                                                    "end_cmd",
                                                    "begin_cmd",
                                                    "barriers",
                                                    "copy_texture_to_buffer",
                                                    "end_cmd",
                                                    "begin_cmd",
                                                    "barriers",
                                                    "end_cmd"});
    REQUIRE(device.barrier_batches.size() == 3);
    const auto& copy = device.barrier_batches[1];
    REQUIRE(copy.buffers.size() == 1);
    REQUIRE(copy.textures.size() == 1);
    CHECK(copy.buffers[0].buffer == 33);
    CHECK(copy.buffers[0].access == ResourceAccess::TransferWrite);
    CHECK(copy.textures[0].texture == 11);
    CHECK(copy.textures[0].access == ResourceAccess::TransferRead);
    CHECK(device.barrier_batches[2].buffers[0].access == ResourceAccess::HostRead);
}

TEST_CASE("FrameGraph sync: repeated read scopes are reused and different read usages transition")
{
    FrameGraph graph;
    const auto buffer = graph.import_buffer("mesh", 1);
    graph.add_pass("upload", [&](PassBuilder& b) { b.write(buffer, ResourceAccess::TransferWrite); }, {});
    graph.add_pass("vertex", [&](PassBuilder& b) { b.read(buffer, ResourceAccess::VertexRead); }, {});
    graph.add_pass("vertex again", [&](PassBuilder& b) { b.read(buffer, ResourceAccess::VertexRead); }, {});
    graph.add_pass("index", [&](PassBuilder& b) { b.read(buffer, ResourceAccess::IndexRead); }, {});
    for (int i = 0; i < 2; ++i)
    {
        REQUIRE(graph.compile());
        REQUIRE(graph.barriers()[1].size() == 1);
        CHECK(graph.barriers()[1][0].before == ResourceAccess::TransferWrite);
        CHECK(graph.barriers()[1][0].after == ResourceAccess::VertexRead);
        CHECK(graph.barriers()[2].empty());
        REQUIRE(graph.barriers()[3].size() == 1);
        CHECK(graph.barriers()[3][0].before == ResourceAccess::VertexRead);
        CHECK(graph.barriers()[3][0].after == ResourceAccess::IndexRead);
        MockGraphicsDevice device;
        REQUIRE(graph.execute(device));
        CHECK(device.barrier_batches.size() == 3);
    }
}

TEST_CASE("FrameGraph sync: external first reads are prepared using driver state")
{
    FrameGraph graph;
    const auto buffer = graph.import_buffer("camera", 1);
    graph.add_pass("use camera", [&](PassBuilder& b) { b.read(buffer, ResourceAccess::UniformRead); }, {});
    REQUIRE(graph.compile());
    REQUIRE(graph.barriers()[0].size() == 1);
    CHECK(graph.barriers()[0][0].before == ResourceAccess::Undefined);
    CHECK(graph.barriers()[0][0].after == ResourceAccess::UniformRead);
    CHECK(graph.dependencies()[0].empty());
}

TEST_CASE("FrameGraph sync: depth writes are declared independently of the framebuffer's color")
{
    FrameGraph graph;
    const auto depth = graph.import_texture("depth", 1, Format::D32_SFLOAT, 16, 16);
    graph.add_pass("depth reader", [&](PassBuilder& b) { b.read(depth); }, {});
    graph.add_pass("depth writer", [&](PassBuilder& b) { b.write(depth, ResourceAccess::DepthStencilAttachment); }, {});
    REQUIRE(graph.compile());
    CHECK(graph.execution_order() == std::vector<u32>{1, 0});
    CHECK(graph.barriers()[1][0].after == ResourceAccess::DepthStencilAttachment);
    CHECK(graph.barriers()[0][0].before == ResourceAccess::DepthStencilAttachment);
}

TEST_CASE("FrameGraph sync: incompatible usages fail compilation and expose no partial barrier plan")
{
    FrameGraph graph;
    const auto buffer = graph.import_buffer("buffer", 1);
    const auto color  = graph.import_texture("color", 2, Format::R8G8B8A8_UNORM, 16, 16, 3);
    SUBCASE("texture used as vertex input")
    {
        graph.add_pass("bad", [&](PassBuilder& b) { b.read(color, ResourceAccess::VertexRead); }, {});
    }
    SUBCASE("buffer used as color attachment")
    {
        graph.add_pass("bad", [&](PassBuilder& b) { b.write(buffer, ResourceAccess::ColorAttachment); }, {});
    }
    SUBCASE("color used as depth")
    {
        graph.add_pass("bad", [&](PassBuilder& b) { b.write(color, ResourceAccess::DepthStencilAttachment); }, {});
    }
    SUBCASE("read declared with a write usage")
    {
        graph.add_pass("bad", [&](PassBuilder& b) { b.read(buffer, ResourceAccess::StorageWrite); }, {});
    }
    SUBCASE("write declared with a read usage")
    {
        graph.add_pass("bad", [&](PassBuilder& b) { b.write(buffer, ResourceAccess::ShaderRead); }, {});
    }
    SUBCASE("undefined read")
    {
        graph.add_pass("bad", [&](PassBuilder& b) { b.read(buffer, ResourceAccess::Undefined); }, {});
    }
    SUBCASE("host access belongs to exports")
    {
        graph.add_pass("bad", [&](PassBuilder& b) { b.read(buffer, ResourceAccess::HostRead); }, {});
    }
    SUBCASE("one resource cannot have two usages in the same pass")
    {
        graph.add_pass("bad",
                       [&](PassBuilder& b) {
                           b.read(buffer, ResourceAccess::UniformRead);
                           b.read(buffer, ResourceAccess::VertexRead);
                       },
                       {});
    }
    SUBCASE("explicit storage conflicts with implicit color attachment")
    {
        const auto pass =
            graph.add_pass("bad", [&](PassBuilder& b) { b.write(color, ResourceAccess::StorageWrite); }, {});
        graph.set_color_attachment(pass, color, {});
    }
    SUBCASE("texture host export")
    {
        graph.set_final_access(color, ResourceAccess::HostRead);
    }
    SUBCASE("buffer present export")
    {
        graph.set_final_access(buffer, ResourceAccess::Present);
    }
    SUBCASE("automatic export is not a target state")
    {
        graph.set_final_access(buffer, ResourceAccess::Automatic);
    }
    SUBCASE("invalid export resource")
    {
        graph.set_final_access(INVALID_RESOURCE_ID, ResourceAccess::HostRead);
    }
    CHECK_FALSE(graph.compile());
    CHECK(graph.barriers().empty());
    CHECK(graph.final_barriers().empty());
    CHECK(graph.execution_order().empty());
}

TEST_CASE("FrameGraph sync: graph mutations clear export and per-pass plans")
{
    FrameGraph graph;
    const auto buffer = graph.import_buffer("buffer", 1);
    graph.add_pass("write", [&](PassBuilder& b) { b.write(buffer, ResourceAccess::TransferWrite); }, {});
    graph.set_final_access(buffer, ResourceAccess::HostRead);
    REQUIRE(graph.compile());
    REQUIRE_FALSE(graph.final_barriers().empty());
    SUBCASE("export change")
    {
        graph.set_final_access(buffer, ResourceAccess::Undefined);
    }
    SUBCASE("reset")
    {
        graph.reset();
    }
    SUBCASE("new usage")
    {
        PassBuilder(graph, 0).read(buffer);
    }
    CHECK(graph.barriers().empty());
    CHECK(graph.final_barriers().empty());
    MockGraphicsDevice device;
    CHECK_FALSE(graph.execute(device));
    CHECK(device.events.empty());
}

TEST_CASE("FrameGraph sync: failed barriers stop callbacks downstream passes and exports")
{
    FrameGraph graph;
    const auto buffer   = graph.import_buffer("buffer", 1);
    int        executed = 0;
    graph.add_pass("write", [&](PassBuilder& b) { b.write(buffer); }, [&](PassExecuteContext&) { ++executed; });
    graph.add_pass("read", [&](PassBuilder& b) { b.read(buffer); }, [&](PassExecuteContext&) { ++executed; });
    graph.set_final_access(buffer, ResourceAccess::HostRead);
    REQUIRE(graph.compile());
    MockGraphicsDevice device;
    device.barriers_succeed = false;
    CHECK_FALSE(graph.execute(device));
    CHECK(executed == 0);
    CHECK(device.events == std::vector<std::string>{"begin_cmd", "barriers", "end_cmd"});
    device.barriers_succeed = true;
    CHECK(graph.execute(device));
    CHECK(executed == 2);
}

TEST_CASE("FrameGraph sync: failed export reports failure after completed passes")
{
    class ExportFailure : public MockGraphicsDevice
    {
        bool cmd_resource_barriers(CommandListHandle               cmd,
                                   std::span<const BufferBarrier>  buffers,
                                   std::span<const TextureBarrier> textures) override
        {
            barriers_succeed = barrier_batches.empty();
            return MockGraphicsDevice::cmd_resource_barriers(cmd, buffers, textures);
        }
    } device;

    FrameGraph graph;
    const auto buffer   = graph.import_buffer("buffer", 1);
    bool       executed = false;
    graph.add_pass("write", [&](PassBuilder& b) { b.write(buffer); }, [&](PassExecuteContext&) { executed = true; });
    graph.set_final_access(buffer, ResourceAccess::HostRead);
    REQUIRE(graph.compile());
    CHECK_FALSE(graph.execute(device));
    CHECK(executed);
    CHECK(device.events ==
          std::vector<std::string>{"begin_cmd", "barriers", "end_cmd", "begin_cmd", "barriers", "end_cmd"});
}
