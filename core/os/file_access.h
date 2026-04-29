#pragma once

#include "core/typedefs.h"

#include <memory>
#include <string>

namespace rover
{

    // ---------------------------------------------------------------------------
    // FileAccess: abstract file-system access (per ADR-0006).
    //
    // Implementations live under `platform/<os>/`. The active implementation is
    // selected at startup by the platform layer via `FileAccess::set_factory()`.
    // Higher-level code (modules/serialization, asset loaders) should never
    // hard-code POSIX/Win32 calls.
    // ---------------------------------------------------------------------------

    enum class FileMode : u8
    {
        Read,
        Write,
        ReadWrite,
        Append,
    };

    enum class SeekOrigin : u8
    {
        Begin,
        Current,
        End,
    };

    class FileAccess
    {
    public:
        virtual ~FileAccess() = default;

        // Open the file at `path` in `mode`. Returns true on success.
        virtual bool               open(const std::string& path, FileMode mode) = 0;
        virtual void               close()                                      = 0;
        [[nodiscard]] virtual bool is_open() const                              = 0;

        // ---- Reading ----
        // Returns the number of bytes actually read (may be less than `size` on EOF).
        virtual usize read(void* buffer, usize size) = 0;

        // Convenience: reads the full content into a heap buffer. Returns empty
        // string on failure.
        virtual std::string read_all_text();

        // ---- Writing ----
        // Returns the number of bytes actually written.
        virtual usize write(const void* buffer, usize size) = 0;

        // Convenience: writes a string in full.
        virtual bool write_all_text(const std::string& contents);

        // ---- Position ----
        virtual bool               seek(i64 offset, SeekOrigin origin) = 0;
        [[nodiscard]] virtual u64  tell() const                        = 0;
        [[nodiscard]] virtual bool eof() const                         = 0;
        [[nodiscard]] virtual u64  length() const                      = 0;

        // ---- Factory ----
        using Factory = std::unique_ptr<FileAccess> (*)();
        static void                                      set_factory(Factory factory);
        [[nodiscard]] static std::unique_ptr<FileAccess> create();

        // Convenience: open and read full text in one call. Returns empty string
        // on any failure (open / read).
        [[nodiscard]] static std::string read_text_file(const std::string& path);

        // Convenience: open in write mode and dump the string. Returns true on
        // success.
        static bool write_text_file(const std::string& path, const std::string& contents);

    private:
        static Factory& factory_slot();
    };

} // namespace rover
