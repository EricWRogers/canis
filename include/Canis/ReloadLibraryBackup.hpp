#pragma once

#include <filesystem>
#include <string>

namespace Canis::ReloadBuild
{
    // Leave the build output and its timestamp intact so an unchanged build
    // remains a no-op. Preserve the timestamp in the backup for failed builds.
    inline bool CopyLibrary(const std::filesystem::path& source,
                            const std::filesystem::path& destination,
                            std::string& error)
    {
        std::error_code ec;
        const auto timestamp = std::filesystem::last_write_time(source, ec);
        if (!ec)
            std::filesystem::copy_file(source, destination,
                std::filesystem::copy_options::overwrite_existing, ec);
        if (!ec) std::filesystem::last_write_time(destination, timestamp, ec);
        if (ec)
        {
            error = "Cannot copy game library from " + source.string() + " to " +
                destination.string() + ": " + ec.message();
            return false;
        }
        return true;
    }
}
