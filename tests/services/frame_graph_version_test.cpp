#include "services/graphics/frame_graph.h"
#include "tests/mocks/mock_graphics_device.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>

using namespace rover;

TEST_CASE("FrameGraph versions: preserve metadata and share one physical resource")
{
    FrameGraph graph;
    const auto texture   = graph.import_texture("color", 17, Format::R8G8B8A8_UNORM, 80, 60, 29);
    const auto v1        = graph.create_version(texture, "color.v1");
    const auto v2        = graph.create_version(v1);
    const auto buffer    = graph.import_buffer("buffer", 17); // Separate handle namespace.
    const auto buffer_v1 = graph.create_version(buffer);
    CHECK(v1 != texture);
    CHECK(v2 != v1);
    CHECK(graph.resource(v2).physical_resource == texture);
    CHECK(graph.resource(v2).previous_version == v1);
    CHECK(graph.resource(v2).texture == 17);
    CHECK(graph.resource(v2).framebuffer == 29);
    CHECK(graph.resource(v2).width == 80);
    CHECK(graph.resource(v2).height == 60);
    CHECK(graph.resource(v2).format == Format::R8G8B8A8_UNORM);
    CHECK(graph.resource(v1).name == "color.v1");
    CHECK(graph.resource(v2).name != graph.resource(v1).name);
    CHECK(graph.resource(buffer_v1).kind == ResourceKind::Buffer);
    CHECK(graph.resource(buffer_v1).buffer == 17);
    CHECK(graph.resource(buffer_v1).physical_resource == buffer);
    for (auto id : {v1, v2})
    {
        graph.add_pass("texture write", [=](PassBuilder& b) { b.write(id); }, {});
    }
    graph.add_pass("buffer write", [=](PassBuilder& b) { b.write(buffer_v1); }, {});
    REQUIRE(graph.compile());
}

TEST_CASE("FrameGraph versions: three writes wait for all previous readers")
{
    FrameGraph       graph;
    const auto       a     = graph.import_buffer("a", 77);
    const auto       b     = graph.create_version(a);
    const auto       c     = graph.create_version(b);
    int              value = 0;
    std::vector<int> observed;
    graph.add_pass(
        "read c",
        [=](PassBuilder& p) { p.read(c); },
        [&](PassExecuteContext& ctx) {
            CHECK(ctx.graph.resource(c).buffer == 77);
            observed.push_back(value);
        });
    graph.add_pass("write c", [=](PassBuilder& p) { p.write(c); }, [&](PassExecuteContext&) { value = 30; });
    graph.add_pass(
        "read a", [=](PassBuilder& p) { p.read(a); }, [&](PassExecuteContext&) { observed.push_back(value); });
    graph.add_pass("write b", [=](PassBuilder& p) { p.write(b); }, [&](PassExecuteContext&) { value = 20; });
    graph.add_pass(
        "read b", [=](PassBuilder& p) { p.read(b); }, [&](PassExecuteContext&) { observed.push_back(value); });
    graph.add_pass("write a", [=](PassBuilder& p) { p.write(a); }, [&](PassExecuteContext&) { value = 10; });
    graph.add_pass(
        "another a reader",
        [=](PassBuilder& p) {
            p.read(a);
            p.read(a);
        },
        [&](PassExecuteContext&) { observed.push_back(value); });
    REQUIRE(graph.compile());
    CHECK(graph.execution_order() == std::vector<u32>{5, 2, 6, 3, 4, 1, 0});
    CHECK(graph.dependencies()[3] == std::vector<u32>{2, 5, 6});
    CHECK(graph.dependencies()[1] == std::vector<u32>{3, 4});
    MockGraphicsDevice device;
    REQUIRE(graph.execute(device));
    CHECK(observed == std::vector<int>{10, 10, 20, 30});
}

TEST_CASE("FrameGraph versions: every registration permutation preserves overwritten contents")
{
    std::array<int, 5> registration{0, 1, 2, 3, 4};
    do
    {
        FrameGraph         graph;
        const auto         original    = graph.import_buffer("value", 1);
        const auto         overwritten = graph.create_version(original);
        int                value       = 0;
        std::array<int, 3> observed{};
        for (const int operation : registration)
        {
            graph.add_pass(
                "operation",
                [=](PassBuilder& p) {
                    if (operation == 0)
                    {
                        p.write(original);
                    }
                    else if (operation == 3)
                    {
                        p.write(overwritten);
                    }
                    else
                    {
                        p.read(operation == 4 ? overwritten : original);
                    }
                },
                [&, operation](PassExecuteContext&) {
                    if (operation == 0)
                    {
                        value = 7;
                    }
                    else if (operation == 3)
                    {
                        value = 9;
                    }
                    else
                    {
                        observed[operation == 4 ? 2 : operation - 1] = value;
                    }
                });
        }
        REQUIRE(graph.compile());
        MockGraphicsDevice device;
        REQUIRE(graph.execute(device));
        CHECK(observed == std::array<int, 3>{7, 7, 9});
    } while (std::next_permutation(registration.begin(), registration.end()));
}

TEST_CASE("FrameGraph versions: writes without readers still follow the physical overwrite chain")
{
    FrameGraph graph;
    const auto a = graph.import_buffer("a", 1);
    const auto b = graph.create_version(a);
    const auto c = graph.create_version(b);
    for (auto id : {c, b, a})
    {
        graph.add_pass("write", [=](PassBuilder& p) { p.write(id); }, {});
    }
    graph.set_final_access(c, ResourceAccess::HostRead);
    REQUIRE(graph.compile());
    CHECK(graph.execution_order() == std::vector<u32>{2, 1, 0});
    CHECK(graph.barriers()[2][0].before == ResourceAccess::Undefined);
    CHECK(graph.barriers()[1][0].resource == b);
    CHECK(graph.barriers()[1][0].before == ResourceAccess::StorageWrite);
    CHECK(graph.barriers()[1][0].after == ResourceAccess::StorageWrite);
    CHECK(graph.barriers()[0][0].resource == c);
    CHECK(graph.barriers()[0][0].before == ResourceAccess::StorageWrite);
    REQUIRE(graph.final_barriers().size() == 1);
    CHECK(graph.final_barriers()[0].resource == c);
    CHECK(graph.final_barriers()[0].before == ResourceAccess::StorageWrite);
    CHECK(graph.final_barriers()[0].after == ResourceAccess::HostRead);
}

TEST_CASE("FrameGraph versions: external contents are read before the first overwrite")
{
    FrameGraph graph;
    const auto external    = graph.import_buffer("external", 1);
    const auto overwritten = graph.create_version(external);
    graph.add_pass("overwrite", [=](PassBuilder& p) { p.write(overwritten); }, {});
    graph.add_pass("read external", [=](PassBuilder& p) { p.read(external, ResourceAccess::UniformRead); }, {});
    REQUIRE(graph.compile());
    CHECK(graph.execution_order() == std::vector<u32>{1, 0});
    CHECK(graph.dependencies()[0] == std::vector<u32>{1});
    CHECK(graph.barriers()[0][0].before == ResourceAccess::UniformRead);
}

TEST_CASE("FrameGraph versions: attachments synchronize through one continuous texture state")
{
    FrameGraph graph;
    const auto original    = graph.import_texture("color", 11, Format::R8G8B8A8_UNORM, 16, 16, 22);
    const auto overwritten = graph.create_version(original);
    graph.add_pass("sample new", [=](PassBuilder& p) { p.read(overwritten); }, {});
    const auto overwrite = graph.add_pass("write new", [=](PassBuilder& p) { p.write(overwritten); }, {});
    graph.set_color_attachment(overwrite, overwritten, {});
    graph.add_pass("copy old", [=](PassBuilder& p) { p.read(original, ResourceAccess::TransferRead); }, {});
    const auto clear = graph.add_pass("write old", {}, {});
    graph.set_color_attachment(clear, original, {});
    graph.set_final_access(overwritten, ResourceAccess::TransferRead);
    REQUIRE(graph.compile());
    CHECK(graph.execution_order() == std::vector<u32>{3, 2, 1, 0});
    REQUIRE(graph.barriers()[overwrite].size() == 1);
    CHECK(graph.barriers()[overwrite][0].resource == overwritten);
    CHECK(graph.barriers()[overwrite][0].before == ResourceAccess::TransferRead);
    CHECK(graph.barriers()[overwrite][0].after == ResourceAccess::ColorAttachment);
    CHECK(graph.barriers()[0][0].before == ResourceAccess::ColorAttachment);
    CHECK(graph.barriers()[0][0].after == ResourceAccess::ShaderRead);
    MockGraphicsDevice device;
    REQUIRE(graph.execute(device));
    REQUIRE(device.barrier_batches.size() == 5);
    const std::array expected{ResourceAccess::ColorAttachment,
                              ResourceAccess::TransferRead,
                              ResourceAccess::ColorAttachment,
                              ResourceAccess::ShaderRead,
                              ResourceAccess::TransferRead};
    for (usize i = 0; i < expected.size(); ++i)
    {
        REQUIRE(device.barrier_batches[i].textures.size() == 1);
        CHECK(device.barrier_batches[i].textures[0].texture == 11);
        CHECK(device.barrier_batches[i].textures[0].access == expected[i]);
    }
}

TEST_CASE("FrameGraph versions: invalid chains and aliased imports are rejected")
{
    FrameGraph graph;
    const auto a = graph.import_buffer("a", 1);
    const auto b = graph.create_version(a);
    graph.add_pass("write b", [=](PassBuilder& p) { p.write(b); }, {});
    SUBCASE("branch")
    {
        const auto fork = graph.create_version(a);
        graph.add_pass("write fork", [=](PassBuilder& p) { p.write(fork); }, {});
    }
    SUBCASE("version without producer")
    {
        graph.create_version(b);
    }
    SUBCASE("invalid predecessor")
    {
        CHECK(graph.create_version(99) == INVALID_RESOURCE_ID);
    }
    SUBCASE("duplicate import")
    {
        graph.import_buffer("duplicate", 1);
    }
    SUBCASE("multiple writers for one version")
    {
        graph.add_pass("another b writer", [=](PassBuilder& p) { p.write(b); }, {});
    }
    CHECK_FALSE(graph.compile());
    CHECK(graph.execution_order().empty());
    CHECK(graph.barriers().empty());
    MockGraphicsDevice device;
    CHECK_FALSE(graph.execute(device));
    CHECK(device.events.empty());
}

TEST_CASE("FrameGraph versions: one pass cannot access different versions of one physical resource")
{
    FrameGraph graph;
    const auto a = graph.import_texture("a", 11, Format::R8G8B8A8_UNORM, 16, 16, 22);
    const auto b = graph.create_version(a);
    SUBCASE("feedback through different IDs")
    {
        graph.add_pass("feedback",
                       [=](PassBuilder& p) {
                           p.read(a);
                           p.write(b);
                       },
                       {});
    }
    SUBCASE("two writes")
    {
        graph.add_pass("two writes",
                       [=](PassBuilder& p) {
                           p.write(a);
                           p.write(b);
                       },
                       {});
    }
    SUBCASE("two reads")
    {
        graph.add_pass("write", [=](PassBuilder& p) { p.write(b); }, {});
        graph.add_pass("two reads",
                       [=](PassBuilder& p) {
                           p.read(a);
                           p.read(b);
                       },
                       {});
    }
    SUBCASE("explicit write conflicts with another version attachment")
    {
        const auto pass = graph.add_pass("write", [=](PassBuilder& p) { p.write(a); }, {});
        graph.set_color_attachment(pass, b, {});
    }
    CHECK_FALSE(graph.compile());
}

TEST_CASE("FrameGraph versions: export belongs to the last version")
{
    FrameGraph graph;
    const auto a = graph.import_buffer("a", 1);
    graph.set_final_access(a, ResourceAccess::HostRead);
    const auto b = graph.create_version(a);
    CHECK(graph.resource(b).final_access == ResourceAccess::Undefined);
    graph.add_pass("write", [=](PassBuilder& p) { p.write(b); }, {});
    CHECK_FALSE(graph.compile());
    graph.set_final_access(a, ResourceAccess::Undefined);
    graph.set_final_access(b, ResourceAccess::HostRead);
    REQUIRE(graph.compile());
    CHECK(graph.final_barriers()[0].resource == b);
}

TEST_CASE("FrameGraph versions: overwrite dependency cycles fail and clear a previous plan")
{
    FrameGraph graph;
    const auto a = graph.import_buffer("a", 1);
    graph.add_pass("read a", [=](PassBuilder& p) { p.read(a); }, {});
    REQUIRE(graph.compile());
    const auto b      = graph.create_version(a);
    const auto output = graph.import_buffer("output", 2);
    graph.add_pass("write b and output",
                   [=](PassBuilder& p) {
                       p.write(b);
                       p.write(output);
                   },
                   {});
    graph.add_pass("read old after output",
                   [=](PassBuilder& p) {
                       p.read(a);
                       p.read(output);
                   },
                   {});
    CHECK_FALSE(graph.compile());
    CHECK(graph.execution_order().empty());
    CHECK(graph.dependencies().empty());
    CHECK(graph.barriers().empty());
    CHECK(graph.final_barriers().empty());
}

TEST_CASE("FrameGraph versions: creation invalidates plans and is refused during execution")
{
    FrameGraph graph;
    const auto a = graph.import_buffer("a", 1);
    graph.add_pass(
        "read",
        [=](PassBuilder& p) { p.read(a); },
        [&](PassExecuteContext& ctx) { CHECK(ctx.graph.create_version(a) == INVALID_RESOURCE_ID); });
    REQUIRE(graph.compile());
    MockGraphicsDevice device;
    REQUIRE(graph.execute(device));
    CHECK(graph.resource_count() == 1);
    graph.create_version(a);
    CHECK(graph.execution_order().empty());
    CHECK_FALSE(graph.execute(device));
    CHECK_FALSE(graph.compile());
    graph.reset();
    CHECK(graph.resource_count() == 0);
    const auto root = graph.import_buffer("reset", 1);
    CHECK(graph.create_version(root) != INVALID_RESOURCE_ID);
}
