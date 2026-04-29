#include "platform/linux/dir_access_linux.h"

#include <filesystem>
#include <system_error>

namespace rover
{

    namespace fs = std::filesystem;

    bool DirAccessLinux::file_exists(const std::string& path) const
    {
        std::error_code ec;
        return fs::is_regular_file(path, ec);
    }

    bool DirAccessLinux::dir_exists(const std::string& path) const
    {
        std::error_code ec;
        return fs::is_directory(path, ec);
    }

    bool DirAccessLinux::make_dir(const std::string& path)
    {
        std::error_code ec;
        return fs::create_directory(path, ec);
    }

    bool DirAccessLinux::make_dir_recursive(const std::string& path)
    {
        std::error_code ec;
        fs::create_directories(path, ec);
        return !ec;
    }

    bool DirAccessLinux::remove_file(const std::string& path)
    {
        std::error_code ec;
        return fs::remove(path, ec);
    }

    bool DirAccessLinux::remove_dir(const std::string& path)
    {
        std::error_code ec;
        fs::remove_all(path, ec);
        return !ec;
    }

    std::vector<std::string> DirAccessLinux::list_dir(const std::string& path) const
    {
        std::vector<std::string> out;
        std::error_code          ec;
        fs::directory_iterator   it(path, ec);
        if (ec)
        {
            return out;
        }
        for (const auto& entry : it)
        {
            out.push_back(entry.path().filename().string());
        }
        return out;
    }

    std::string DirAccessLinux::get_current_dir() const
    {
        std::error_code ec;
        auto            p = fs::current_path(ec);
        return ec ? std::string{} : p.string();
    }

    std::unique_ptr<DirAccess> create_dir_access_linux()
    {
        return std::make_unique<DirAccessLinux>();
    }

} // namespace rover
