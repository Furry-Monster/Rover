#pragma once

#include "core/os/file_access.h"

#include <cstdio>

namespace rover
{

    // POSIX (FILE*) implementation of FileAccess. Selected at startup by the
    // linux platform layer.
    class FileAccessLinux final : public FileAccess
    {
    public:
        FileAccessLinux() = default;
        ~FileAccessLinux() override;

        bool open(const std::string& path, FileMode mode) override;
        void close() override;

        [[nodiscard]] bool is_open() const override { return file_ != nullptr; }

        usize read(void* buffer, usize size) override;
        usize write(const void* buffer, usize size) override;

        bool               seek(i64 offset, SeekOrigin origin) override;
        [[nodiscard]] u64  tell() const override;
        [[nodiscard]] bool eof() const override;
        [[nodiscard]] u64  length() const override;

    private:
        std::FILE* file_ = nullptr;
    };

    // Static factory; passed to `FileAccess::set_factory()` at platform init.
    std::unique_ptr<FileAccess> create_file_access_linux();

} // namespace rover
