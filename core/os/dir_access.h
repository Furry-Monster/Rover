#pragma once

#include "core/typedefs.h"

#include <memory>
#include <string>
#include <vector>

namespace rover
{

    // ---------------------------------------------------------------------------
    // DirAccess: abstract directory-system access (per ADR-0006).
    //
    // Implementations live under `platform/<os>/`. Path semantics are
    // platform-native. VFS schemes (`res://`, `user://`) will be added in a
    // future serialization-layer wrapper.
    // ---------------------------------------------------------------------------

    class DirAccess
    {
    public:
        virtual ~DirAccess() = default;

        // ---- Queries ----
        [[nodiscard]] virtual bool file_exists(const std::string& path) const = 0;
        [[nodiscard]] virtual bool dir_exists(const std::string& path) const  = 0;

        // ---- Mutations ----
        virtual bool make_dir(const std::string& path)           = 0;
        virtual bool make_dir_recursive(const std::string& path) = 0;
        virtual bool remove_file(const std::string& path)        = 0;
        virtual bool remove_dir(const std::string& path)         = 0;

        // ---- Listing ----
        // Returns immediate children (file or directory names, no recursion). On
        // failure returns empty vector.
        [[nodiscard]] virtual std::vector<std::string> list_dir(const std::string& path) const = 0;

        // ---- CWD ----
        [[nodiscard]] virtual std::string get_current_dir() const = 0;

        // ---- Factory ----
        using Factory = std::unique_ptr<DirAccess> (*)();
        static void                                     set_factory(Factory factory);
        [[nodiscard]] static std::unique_ptr<DirAccess> create();

    private:
        static Factory& factory_slot();
    };

} // namespace rover
