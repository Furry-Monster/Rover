#pragma once

#include "core/os/dir_access.h"

namespace rover
{

    // std::filesystem-backed DirAccess for Linux (POSIX). Selected at startup by
    // the linux platform layer.
    class DirAccessLinux final : public DirAccess
    {
    public:
        [[nodiscard]] bool file_exists(const std::string& path) const override;
        [[nodiscard]] bool dir_exists(const std::string& path) const override;

        bool make_dir(const std::string& path) override;
        bool make_dir_recursive(const std::string& path) override;
        bool remove_file(const std::string& path) override;
        bool remove_dir(const std::string& path) override;

        [[nodiscard]] std::vector<std::string> list_dir(const std::string& path) const override;
        [[nodiscard]] std::string              get_current_dir() const override;
    };

    // Static factory; passed to `DirAccess::set_factory()` at platform init.
    std::unique_ptr<DirAccess> create_dir_access_linux();

} // namespace rover
