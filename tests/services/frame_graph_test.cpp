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

TEST_CASE("FrameGraph: producer executes before its consumer")
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
    CHECK(dev.events ==
          std::vector<std::string>{"begin_cmd", "barriers", "end_cmd", "begin_cmd", "barriers", "end_cmd"});
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

    CHECK(dev.events == std::vector<std::string>{"begin_cmd", "barriers", "begin_pass", "end_pass", "end_cmd"});
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
    REQUIRE(fg.compile());
    fg.reset();
    CHECK(fg.pass_count() == 0);
    CHECK(fg.resource_count() == 0);
    CHECK(fg.execution_order().empty());
    CHECK(fg.dependencies().empty());
    CHECK(fg.barriers().empty());
    CHECK(fg.final_barriers().empty());
    REQUIRE(fg.compile());
    MockGraphicsDevice dev;
    fg.execute(dev);
    CHECK(dev.events.empty());
}

TEST_CASE("FrameGraph: reverse registered chain executes producer data through all passes")
{
    FrameGraph       fg;
    const auto       input        = fg.import_buffer("external input", 1);
    const auto       intermediate = fg.import_buffer("intermediate", 2);
    const auto       output       = fg.import_buffer("output", 3);
    int              value        = 0;
    std::vector<int> order;
    const auto       consume = fg.add_pass(
        "consume",
        [&](PassBuilder& b) { b.read(output); },
        [&](PassExecuteContext& ctx) {
            CHECK(ctx.graph.resource(output).buffer == 3);
            CHECK(value == 42);
            order.push_back(3);
        });
    const auto transform = fg.add_pass(
        "transform",
        [&](PassBuilder& b) {
            b.read(intermediate);
            b.write(output);
        },
        [&](PassExecuteContext&) {
            value *= 2;
            order.push_back(2);
        });
    const auto produce = fg.add_pass(
        "produce",
        [&](PassBuilder& b) {
            b.read(input);
            b.write(intermediate);
        },
        [&](PassExecuteContext&) {
            value = 21;
            order.push_back(1);
        });

    REQUIRE(fg.compile());
    CHECK(fg.execution_order() == std::vector<u32>{produce, transform, consume});
    REQUIRE(fg.dependencies().size() == 3);
    CHECK(fg.dependencies()[consume] == std::vector<u32>{transform});
    CHECK(fg.dependencies()[produce].empty());
    CHECK(fg.passes()[consume].name == "consume");
    MockGraphicsDevice dev;
    fg.execute(dev);
    CHECK(order == std::vector<int>{1, 2, 3});
    CHECK(dev.events == std::vector<std::string>{"begin_cmd",
                                                 "barriers",
                                                 "end_cmd",
                                                 "begin_cmd",
                                                 "barriers",
                                                 "end_cmd",
                                                 "begin_cmd",
                                                 "barriers",
                                                 "end_cmd"});
}

TEST_CASE("FrameGraph: diamond dependencies and shared reads are deterministic")
{
    FrameGraph fg;
    const auto root  = fg.import_buffer("root", 1);
    const auto left  = fg.import_buffer("left", 2);
    const auto right = fg.import_buffer("right", 3);
    fg.add_pass("join",
                [&](PassBuilder& b) {
                    b.read(left);
                    b.read(right);
                },
                {});
    fg.add_pass("right",
                [&](PassBuilder& b) {
                    b.read(root);
                    b.write(right);
                },
                {});
    fg.add_pass("left",
                [&](PassBuilder& b) {
                    b.read(root);
                    b.write(left);
                },
                {});
    fg.add_pass("root", [&](PassBuilder& b) { b.write(root); }, {});

    for (int iteration = 0; iteration < 3; ++iteration)
    {
        REQUIRE(fg.compile());
        CHECK(fg.execution_order() == std::vector<u32>{3, 1, 2, 0});
        CHECK(fg.dependencies()[0] == std::vector<u32>{1, 2});
        CHECK(fg.dependencies()[1] == std::vector<u32>{3});
        CHECK(fg.dependencies()[2] == std::vector<u32>{3});
    }
}

TEST_CASE("FrameGraph: lowest registration ID wins among all ready passes")
{
    FrameGraph fg;
    const auto data = fg.import_buffer("data", 1);
    fg.add_pass("consumer", [&](PassBuilder& b) { b.read(data); }, {});
    fg.add_pass("independent", {}, {});
    fg.add_pass("producer", [&](PassBuilder& b) { b.write(data); }, {});
    fg.add_pass("unconnected side effect", {}, {});
    REQUIRE(fg.compile());
    CHECK(fg.execution_order() == std::vector<u32>{1, 2, 0, 3});
}

TEST_CASE("FrameGraph: duplicate declarations and edges do not inflate indegrees")
{
    FrameGraph fg;
    const auto a = fg.import_buffer("a", 1);
    const auto b = fg.import_buffer("b", 2);
    fg.add_pass("consumer",
                [&](PassBuilder& builder) {
                    builder.read(a);
                    builder.read(a);
                    builder.read(b);
                },
                {});
    fg.add_pass("producer",
                [&](PassBuilder& builder) {
                    builder.write(a);
                    builder.write(a);
                    builder.write(b);
                },
                {});
    REQUIRE(fg.compile());
    CHECK(fg.execution_order() == std::vector<u32>{1, 0});
    CHECK(fg.dependencies()[0] == std::vector<u32>{1});
    CHECK(fg.passes()[0].reads.size() == 2);
    CHECK(fg.passes()[1].writes.size() == 2);
}

TEST_CASE("FrameGraph: invalid read and write declarations fail compilation")
{
    FrameGraph fg;
    SUBCASE("out of bounds read")
    {
        fg.add_pass("bad", [](PassBuilder& b) { b.read(99); }, {});
    }
    SUBCASE("sentinel read")
    {
        fg.add_pass("bad", [](PassBuilder& b) { b.read(INVALID_RESOURCE_ID); }, {});
    }
    SUBCASE("out of bounds write")
    {
        fg.add_pass("bad", [](PassBuilder& b) { b.write(99); }, {});
    }
    SUBCASE("sentinel write")
    {
        fg.add_pass("bad", [](PassBuilder& b) { b.write(INVALID_RESOURCE_ID); }, {});
    }
    CHECK_FALSE(fg.compile());
    CHECK(fg.execution_order().empty());
    CHECK(fg.dependencies().empty());
    MockGraphicsDevice dev;
    fg.execute(dev);
    CHECK(dev.events.empty());
}

TEST_CASE("FrameGraph: multiple writers and same-pass feedback are rejected")
{
    FrameGraph fg;
    const auto data = fg.import_buffer("data", 1);
    SUBCASE("multiple writers")
    {
        fg.add_pass("first", [&](PassBuilder& b) { b.write(data); }, {});
        fg.add_pass("second", [&](PassBuilder& b) { b.write(data); }, {});
    }
    SUBCASE("feedback")
    {
        fg.add_pass("feedback",
                    [&](PassBuilder& b) {
                        b.read(data);
                        b.write(data);
                    },
                    {});
    }
    CHECK_FALSE(fg.compile());
}

TEST_CASE("FrameGraph: cyclic graphs never execute a partial plan")
{
    FrameGraph fg;
    const auto a        = fg.import_buffer("a", 1);
    const auto b        = fg.import_buffer("b", 2);
    const auto c        = fg.import_buffer("c", 3);
    int        executed = 0;
    fg.add_pass("independent", {}, [&](PassExecuteContext&) { ++executed; });
    SUBCASE("two-pass cycle")
    {
        fg.add_pass("a",
                    [&](PassBuilder& p) {
                        p.read(b);
                        p.write(a);
                    },
                    {});
        fg.add_pass("b",
                    [&](PassBuilder& p) {
                        p.read(a);
                        p.write(b);
                    },
                    {});
    }
    SUBCASE("three-pass cycle with blocked downstream")
    {
        fg.add_pass("a",
                    [&](PassBuilder& p) {
                        p.read(c);
                        p.write(a);
                    },
                    {});
        fg.add_pass("b",
                    [&](PassBuilder& p) {
                        p.read(a);
                        p.write(b);
                    },
                    {});
        fg.add_pass("c",
                    [&](PassBuilder& p) {
                        p.read(b);
                        p.write(c);
                    },
                    {});
        fg.add_pass("downstream", [&](PassBuilder& p) { p.read(c); }, {});
    }
    CHECK_FALSE(fg.compile());
    CHECK(fg.execution_order().empty());
    CHECK(fg.dependencies().empty());
    MockGraphicsDevice dev;
    fg.execute(dev);
    CHECK(dev.events.empty());
    CHECK(executed == 0);
}

TEST_CASE("FrameGraph: every authoring mutation invalidates the execution plan")
{
    FrameGraph fg;
    const auto color = fg.import_texture("color", 1, Format::R8G8B8A8_UNORM, 1, 1, 2);
    fg.add_pass("pass", {}, {});
    REQUIRE(fg.compile());
    REQUIRE(fg.execution_order() == std::vector<u32>{0});

    SUBCASE("buffer import")
    {
        fg.import_buffer("buffer", 1);
    }
    SUBCASE("texture import")
    {
        fg.import_texture("another", 2, Format::R8G8B8A8_UNORM, 1, 1);
    }
    SUBCASE("pass registration")
    {
        fg.add_pass("second", {}, {});
    }
    SUBCASE("read declaration")
    {
        PassBuilder(fg, 0).read(color);
    }
    SUBCASE("write declaration")
    {
        PassBuilder(fg, 0).write(color);
    }
    SUBCASE("attachment declaration")
    {
        fg.set_color_attachment(0, color, {});
    }
    SUBCASE("final access declaration")
    {
        fg.set_final_access(color, ResourceAccess::ShaderRead);
    }
    CHECK(fg.execution_order().empty());
    CHECK(fg.dependencies().empty());
    MockGraphicsDevice dev;
    fg.execute(dev);
    CHECK(dev.events.empty());
    REQUIRE(fg.compile());
    fg.execute(dev);
    CHECK_FALSE(dev.events.empty());
}

TEST_CASE("FrameGraph: failed recompilation cannot reuse an earlier successful plan")
{
    FrameGraph fg;
    fg.add_pass("valid", {}, {});
    REQUIRE(fg.compile());
    PassBuilder(fg, 0).read(INVALID_RESOURCE_ID);
    CHECK_FALSE(fg.compile());
    CHECK(fg.execution_order().empty());
    CHECK(fg.dependencies().empty());
    MockGraphicsDevice dev;
    fg.execute(dev);
    CHECK(dev.events.empty());
}

TEST_CASE("FrameGraph: attachment writes create dependencies without explicit write declarations")
{
    FrameGraph fg;
    const auto color = fg.import_texture("color", 1, Format::R8G8B8A8_UNORM, 1, 1, 2);
    fg.add_pass("consumer", [&](PassBuilder& b) { b.read(color); }, {});
    const auto producer = fg.add_pass("producer", {}, {});
    fg.set_color_attachment(producer, color, {});
    REQUIRE(fg.compile());
    CHECK(fg.execution_order() == std::vector<u32>{1, 0});
    CHECK(fg.dependencies()[0] == std::vector<u32>{1});
    MockGraphicsDevice dev;
    fg.execute(dev);
    CHECK(dev.events ==
          std::vector<std::string>{
              "begin_cmd", "barriers", "begin_pass", "end_pass", "end_cmd", "begin_cmd", "barriers", "end_cmd"});
}

TEST_CASE("FrameGraph: replacing an attachment removes only its implicit write")
{
    FrameGraph fg;
    const auto old_color = fg.import_texture("old", 1, Format::R8G8B8A8_UNORM, 1, 1, 3);
    const auto new_color = fg.import_texture("new", 2, Format::R8G8B8A8_UNORM, 1, 1, 4);
    fg.add_pass("old reader", [&](PassBuilder& b) { b.read(old_color); }, {});
    fg.add_pass("new reader", [&](PassBuilder& b) { b.read(new_color); }, {});
    fg.add_pass("producer", {}, {});
    fg.set_color_attachment(2, old_color, {});
    REQUIRE(fg.compile());
    CHECK(fg.dependencies()[0] == std::vector<u32>{2});
    fg.set_color_attachment(2, new_color, {});
    SUBCASE("old implicit write removed")
    {
        REQUIRE(fg.compile());
        CHECK(fg.execution_order() == std::vector<u32>{0, 2, 1});
        CHECK(fg.dependencies()[0].empty());
    }
    SUBCASE("old explicit write retained")
    {
        PassBuilder(fg, 2).write(old_color);
        REQUIRE(fg.compile());
        CHECK(fg.execution_order() == std::vector<u32>{2, 0, 1});
        CHECK(fg.dependencies()[0] == std::vector<u32>{2});
    }
    CHECK(fg.dependencies()[1] == std::vector<u32>{2});
}

TEST_CASE("FrameGraph: invalid attachments and attachment conflicts fail compilation")
{
    FrameGraph fg;
    const auto color = fg.import_texture("color", 1, Format::R8G8B8A8_UNORM, 1, 1, 2);
    fg.add_pass("pass", {}, {});
    SUBCASE("out of range")
    {
        fg.set_color_attachment(0, 99, {});
    }
    SUBCASE("sentinel")
    {
        fg.set_color_attachment(0, INVALID_RESOURCE_ID, {});
    }
    SUBCASE("buffer target")
    {
        fg.set_color_attachment(0, fg.import_buffer("buffer", 1), {});
    }
    SUBCASE("missing framebuffer")
    {
        fg.set_color_attachment(0, fg.import_texture("no framebuffer", 2, Format::R8G8B8A8_UNORM, 1, 1), {});
    }
    SUBCASE("implicit writer conflicts with another writer")
    {
        fg.set_color_attachment(0, color, {});
        fg.add_pass("another writer", [&](PassBuilder& b) { b.write(color); }, {});
    }
    SUBCASE("implicit feedback")
    {
        fg.set_color_attachment(0, color, {});
        PassBuilder(fg, 0).read(color);
    }
    CHECK_FALSE(fg.compile());
}

TEST_CASE("FrameGraph: invalid pass declarations require reset and cannot silently compile")
{
    FrameGraph fg;
    SUBCASE("read")
    {
        PassBuilder(fg, INVALID_PASS_ID).read(0);
    }
    SUBCASE("write")
    {
        PassBuilder(fg, 0).write(0);
    }
    SUBCASE("attachment")
    {
        fg.set_color_attachment(0, 0, {});
    }
    CHECK_FALSE(fg.compile());
    fg.reset();
    CHECK(fg.compile());
}

TEST_CASE("FrameGraph: imported handles must be valid and unique within their resource type")
{
    FrameGraph fg;
    SUBCASE("invalid texture")
    {
        fg.import_texture("invalid", INVALID_HANDLE, Format::R8G8B8A8_UNORM, 1, 1);
    }
    SUBCASE("invalid buffer")
    {
        fg.import_buffer("invalid", INVALID_HANDLE);
    }
    SUBCASE("aliased texture")
    {
        fg.import_texture("first", 1, Format::R8G8B8A8_UNORM, 1, 1);
        fg.import_texture("second", 1, Format::R8G8B8A8_UNORM, 1, 1);
    }
    SUBCASE("aliased buffer")
    {
        fg.import_buffer("first", 1);
        fg.import_buffer("second", 1);
    }
    CHECK_FALSE(fg.compile());
}

TEST_CASE("FrameGraph: texture and buffer handles have separate namespaces")
{
    FrameGraph fg;
    fg.import_texture("texture", 1, Format::R8G8B8A8_UNORM, 1, 1);
    fg.import_buffer("buffer", 1);
    CHECK(fg.compile());
}

TEST_CASE("FrameGraph: mutation and reentry during execution are rejected without corrupting the plan")
{
    FrameGraph fg;
    const auto color    = fg.import_texture("color", 1, Format::R8G8B8A8_UNORM, 1, 1, 2);
    int        executed = 0;
    fg.add_pass("guarded", {}, [&](PassExecuteContext& ctx) {
        ++executed;
        fg.reset();
        CHECK(fg.import_buffer("rejected", 2) == INVALID_RESOURCE_ID);
        CHECK(fg.import_texture("rejected", 2, Format::R8G8B8A8_UNORM, 1, 1) == INVALID_RESOURCE_ID);
        CHECK(fg.add_pass("rejected", {}, {}) == INVALID_PASS_ID);
        PassBuilder(fg, 0).read(color);
        PassBuilder(fg, 0).write(color);
        fg.set_color_attachment(0, color, {});
        CHECK_FALSE(fg.compile());
        fg.execute(ctx.device);
        fg.set_final_access(color, ResourceAccess::ShaderRead);
    });
    fg.add_pass("following", {}, [&](PassExecuteContext&) { ++executed; });
    REQUIRE(fg.compile());
    MockGraphicsDevice dev;
    fg.execute(dev);
    CHECK(executed == 2);
    CHECK(fg.pass_count() == 2);
    CHECK(fg.resource_count() == 1);
    CHECK(fg.execution_order() == std::vector<u32>{0, 1});
    CHECK(fg.passes()[0].reads.empty());
    CHECK(fg.passes()[0].writes.empty());
    CHECK_FALSE(fg.passes()[0].has_clear);
    CHECK(fg.resource(color).final_access == ResourceAccess::Undefined);
    CHECK(dev.events == std::vector<std::string>{"begin_cmd", "end_cmd", "begin_cmd", "end_cmd"});
    fg.reset();
    CHECK(fg.pass_count() == 0);
}

TEST_CASE("FrameGraph: command list failure stops downstream execution and permits later execution")
{
    class FailingDevice : public MockGraphicsDevice
    {
    public:
        int begins = 0;

        CommandListHandle begin_command_list() override
        {
            if (++begins == 2)
            {
                events.push_back("failed_cmd");
                return INVALID_HANDLE;
            }
            return MockGraphicsDevice::begin_command_list();
        }
    } dev;

    FrameGraph       fg;
    const auto       a = fg.import_buffer("a", 1);
    const auto       b = fg.import_buffer("b", 2);
    std::vector<int> executed;
    fg.add_pass("produce", [&](PassBuilder& p) { p.write(a); }, [&](PassExecuteContext&) { executed.push_back(1); });
    fg.add_pass(
        "transform",
        [&](PassBuilder& p) {
            p.read(a);
            p.write(b);
        },
        [&](PassExecuteContext&) { executed.push_back(2); });
    fg.add_pass("consume", [&](PassBuilder& p) { p.read(b); }, [&](PassExecuteContext&) { executed.push_back(3); });
    REQUIRE(fg.compile());
    fg.execute(dev);
    CHECK(executed == std::vector<int>{1});
    CHECK(dev.events == std::vector<std::string>{"begin_cmd", "barriers", "end_cmd", "failed_cmd"});
    executed.clear();
    fg.execute(dev);
    CHECK(executed == std::vector<int>{1, 2, 3});
}

TEST_CASE("FrameGraph: nested setup registration keeps callbacks and pass IDs valid")
{
    FrameGraph fg;
    const auto output = fg.import_buffer("output", 1);
    const auto outer  = fg.add_pass("outer",
                                    [&](PassBuilder& b) {
                                       fg.add_pass("inner", [&](PassBuilder& inner) { inner.write(output); }, {});
                                       b.read(output);
                                    },
                                    {});
    CHECK(outer == 0);
    REQUIRE(fg.compile());
    CHECK(fg.execution_order() == std::vector<u32>{1, 0});
}
