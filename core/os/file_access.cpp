#include "core/os/file_access.h"

#include <cstring>
#include <vector>

namespace rover
{

    FileAccess::Factory& FileAccess::factory_slot()
    {
        static Factory slot = nullptr;
        return slot;
    }

    void FileAccess::set_factory(Factory factory)
    {
        factory_slot() = factory;
    }

    std::unique_ptr<FileAccess> FileAccess::create()
    {
        auto factory = factory_slot();
        if (!factory)
        {
            return nullptr;
        }
        return factory();
    }

    std::string FileAccess::read_all_text()
    {
        if (!is_open())
        {
            return {};
        }
        const u64 len = length();
        if (len == 0)
        {
            return {};
        }
        std::string out;
        out.resize(static_cast<usize>(len));
        seek(0, SeekOrigin::Begin);
        const usize got = read(out.data(), out.size());
        out.resize(got);
        return out;
    }

    bool FileAccess::write_all_text(const std::string& contents)
    {
        if (!is_open())
        {
            return false;
        }
        const usize wrote = write(contents.data(), contents.size());
        return wrote == contents.size();
    }

    std::string FileAccess::read_text_file(const std::string& path)
    {
        auto fa = create();
        if (!fa)
        {
            return {};
        }
        if (!fa->open(path, FileMode::Read))
        {
            return {};
        }
        auto data = fa->read_all_text();
        fa->close();
        return data;
    }

    bool FileAccess::write_text_file(const std::string& path, const std::string& contents)
    {
        auto fa = create();
        if (!fa)
        {
            return false;
        }
        if (!fa->open(path, FileMode::Write))
        {
            return false;
        }
        const bool ok = fa->write_all_text(contents);
        fa->close();
        return ok;
    }

} // namespace rover
