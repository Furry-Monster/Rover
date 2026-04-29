// Unit tests for core/os/. The factory selection is normally done by the
// platform layer; the tests install a minimal in-memory factory so they
// don't depend on linux_platform initialization.

#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "platform/linux/dir_access_linux.h"
#include "platform/linux/file_access_linux.h"

#include <doctest/doctest.h>

#include <cstdio>
#include <filesystem>
#include <string>

using namespace rover;

namespace
{

    // Test fixture: register the linux factories on first use. Calling
    // set_factory repeatedly is a no-op overwrite, so doing it from each test
    // case is safe.
    void install_factories()
    {
        FileAccess::set_factory(&create_file_access_linux);
        DirAccess::set_factory(&create_dir_access_linux);
    }

    std::string tmp_path(const char* name)
    {
        std::filesystem::path p = std::filesystem::temp_directory_path() / name;
        return p.string();
    }

} // namespace

// ---------------------------------------------------------------------------
// FileAccess
// ---------------------------------------------------------------------------
TEST_CASE("FileAccess: write and read text round-trip")
{
    install_factories();
    const std::string path = tmp_path("rover_fa_roundtrip.txt");

    REQUIRE(FileAccess::write_text_file(path, "hello rover"));
    auto data = FileAccess::read_text_file(path);
    CHECK(data == "hello rover");

    std::filesystem::remove(path);
}

TEST_CASE("FileAccess: tell + seek + length report position correctly")
{
    install_factories();
    const std::string path = tmp_path("rover_fa_seek.txt");
    REQUIRE(FileAccess::write_text_file(path, "0123456789"));

    auto fa = FileAccess::create();
    REQUIRE(fa);
    REQUIRE(fa->open(path, FileMode::Read));
    CHECK(fa->length() == 10);

    char buf[4]{};
    fa->read(buf, 4);
    CHECK(fa->tell() == 4);

    fa->seek(0, SeekOrigin::Begin);
    CHECK(fa->tell() == 0);

    fa->seek(2, SeekOrigin::Current);
    CHECK(fa->tell() == 2);

    fa->seek(0, SeekOrigin::End);
    CHECK(fa->tell() == 10);

    fa->close();
    std::filesystem::remove(path);
}

TEST_CASE("FileAccess: opening a non-existent path fails cleanly")
{
    install_factories();
    auto fa = FileAccess::create();
    REQUIRE(fa);
    CHECK_FALSE(fa->open("/nonexistent/rover/path/abc.txt", FileMode::Read));
    CHECK_FALSE(fa->is_open());
}

// ---------------------------------------------------------------------------
// DirAccess
// ---------------------------------------------------------------------------
TEST_CASE("DirAccess: file/dir existence checks match the underlying FS")
{
    install_factories();
    auto da = DirAccess::create();
    REQUIRE(da);

    const std::string tmp = std::filesystem::temp_directory_path().string();
    CHECK(da->dir_exists(tmp));
    CHECK_FALSE(da->dir_exists("/nonexistent/path"));
    CHECK_FALSE(da->file_exists("/nonexistent/path/foo.txt"));
}

TEST_CASE("DirAccess: make_dir + remove_dir round-trip")
{
    install_factories();
    auto da = DirAccess::create();
    REQUIRE(da);
    const std::string path = tmp_path("rover_da_test_dir");
    da->remove_dir(path);

    CHECK(da->make_dir(path));
    CHECK(da->dir_exists(path));
    CHECK(da->remove_dir(path));
    CHECK_FALSE(da->dir_exists(path));
}

TEST_CASE("DirAccess: list_dir returns immediate children only")
{
    install_factories();
    auto da = DirAccess::create();
    REQUIRE(da);

    const std::string root = tmp_path("rover_da_list_root");
    da->remove_dir(root);
    REQUIRE(da->make_dir_recursive(root));
    REQUIRE(FileAccess::write_text_file(root + "/a.txt", "a"));
    REQUIRE(FileAccess::write_text_file(root + "/b.txt", "b"));
    REQUIRE(da->make_dir(root + "/sub"));

    auto items = da->list_dir(root);
    CHECK(items.size() == 3);

    da->remove_dir(root);
}
