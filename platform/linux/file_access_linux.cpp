#include "platform/linux/file_access_linux.h"

#include <sys/stat.h>

namespace rover
{

    FileAccessLinux::~FileAccessLinux()
    {
        close();
    }

    bool FileAccessLinux::open(const std::string& path, FileMode mode)
    {
        close();
        const char* mstr = "rb";
        switch (mode)
        {
            case FileMode::Read:
                mstr = "rb";
                break;
            case FileMode::Write:
                mstr = "wb";
                break;
            case FileMode::ReadWrite:
                mstr = "wb+";
                break;
            case FileMode::Append:
                mstr = "ab";
                break;
        }
        file_ = std::fopen(path.c_str(), mstr);
        return file_ != nullptr;
    }

    void FileAccessLinux::close()
    {
        if (file_)
        {
            std::fclose(file_);
            file_ = nullptr;
        }
    }

    usize FileAccessLinux::read(void* buffer, usize size)
    {
        if (!file_)
        {
            return 0;
        }
        return std::fread(buffer, 1, size, file_);
    }

    usize FileAccessLinux::write(const void* buffer, usize size)
    {
        if (!file_)
        {
            return 0;
        }
        return std::fwrite(buffer, 1, size, file_);
    }

    bool FileAccessLinux::seek(i64 offset, SeekOrigin origin)
    {
        if (!file_)
        {
            return false;
        }
        int whence = SEEK_SET;
        switch (origin)
        {
            case SeekOrigin::Begin:
                whence = SEEK_SET;
                break;
            case SeekOrigin::Current:
                whence = SEEK_CUR;
                break;
            case SeekOrigin::End:
                whence = SEEK_END;
                break;
        }
        return std::fseek(file_, static_cast<long>(offset), whence) == 0;
    }

    u64 FileAccessLinux::tell() const
    {
        if (!file_)
        {
            return 0;
        }
        long pos = std::ftell(file_);
        return pos < 0 ? 0 : static_cast<u64>(pos);
    }

    bool FileAccessLinux::eof() const
    {
        return file_ ? (std::feof(file_) != 0) : true;
    }

    u64 FileAccessLinux::length() const
    {
        if (!file_)
        {
            return 0;
        }
        const long cur = std::ftell(file_);
        if (cur < 0)
        {
            return 0;
        }
        if (std::fseek(file_, 0, SEEK_END) != 0)
        {
            return 0;
        }
        const long end = std::ftell(file_);
        std::fseek(file_, cur, SEEK_SET);
        return end < 0 ? 0 : static_cast<u64>(end);
    }

    std::unique_ptr<FileAccess> create_file_access_linux()
    {
        return std::make_unique<FileAccessLinux>();
    }

} // namespace rover
