// Unit tests for services/graphics/frame_graph.
//
// FrameGraph itself is decoupled from the GPU; the tests verify resource
// import + pass declaration + execute order without instantiating a real
// GraphicsDevice. A null-op mock device is used to exercise execute().

#include "services/graphics/frame_graph.h"
#include "tests/mocks/mock_graphics_device.h"

#include <doctest/doctest.h>

#include <vector>

using namespace rover;

TEST_CASE("FrameGraph: imported texture is recorded with metadata")
{
    FrameGraph fg;
    auto       id = fg.import_texture("color", 0xCAFE, Format::R8G8B8A8_SRGB, 800, 600);
    CHECK(id != INVALID_RESOURCE_ID);
    CHECK(fg.resource(id).name == "color");
    CHECK(fg.resource(id).texture == 0xCAFE);
    CHECK(fg.resource(id).width == 800);
    CHECK(fg.resource(id).format == Format::R8G8B8A8_SRGB);
}

TEST_CASE("FrameGraph: passes execute in declaration order")
{
    FrameGraph fg;
    auto       color = fg.import_texture("color", 1, Format::B8G8R8A8_UNORM, 1, 1);

    std::vector<int> order;
    fg.add_pass("a", [&](PassBuilder& b) { b.write(color); }, [&](PassExecuteContext&) { order.push_back(1); });
    fg.add_pass("b", [&](PassBuilder& b) { b.read(color); }, [&](PassExecuteContext&) { order.push_back(2); });

    REQUIRE(fg.compile());

    MockGraphicsDevice dev;
    fg.execute(dev);

    CHECK(order == std::vector<int>{1, 2});
    // Each pass: begin_cmd + execute + end_cmd (no begin_pass since
    // framebuffer wasn't set on either pass).
    CHECK(dev.events.size() == 4);
}

TEST_CASE("FrameGraph: set_color_attachment wraps execute with render pass")
{
    FrameGraph fg;
    auto       color = fg.import_texture("color", 1, Format::B8G8R8A8_UNORM, 1, 1, /*fb*/ 0xF00);

    fg.add_pass("a", [&](PassBuilder& b) { b.write(color); }, [&](PassExecuteContext&) {});
    ClearValue clear{};
    fg.set_color_attachment(0, color, clear);

    REQUIRE(fg.compile());

    MockGraphicsDevice dev;
    fg.execute(dev);

    REQUIRE(dev.events.size() == 4);
    CHECK(dev.events[0] == "begin_cmd");
    CHECK(dev.events[1] == "begin_pass");
    CHECK(dev.events[2] == "end_pass");
    CHECK(dev.events[3] == "end_cmd");
}

TEST_CASE("FrameGraph: compile fails when a pass references a bad resource id")
{
    FrameGraph fg;
    fg.add_pass("bad", [](PassBuilder& b) { b.write(static_cast<RenderResourceId>(99)); }, [](PassExecuteContext&) {});
    CHECK_FALSE(fg.compile());
}

TEST_CASE("FrameGraph: reset clears resources and passes")
{
    FrameGraph fg;
    fg.import_texture("a", 1, Format::R8G8B8A8_UNORM, 1, 1);
    fg.add_pass("p", [](PassBuilder&) {}, [](PassExecuteContext&) {});
    fg.reset();
    CHECK(fg.pass_count() == 0);
    CHECK(fg.resource_count() == 0);
}
