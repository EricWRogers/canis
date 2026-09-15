#pragma once
#include <string>
#include <vector>

namespace Canis::ScriptEditing
{
    struct SyncResult
    {
        std::string source;
        std::string error;
        std::vector<std::string> notes;
        int properties = 0;
        int methods = 0;
    };

    // Conservatively synchronizes a Canis template header and its paired source.
    // No file I/O; callers retain ownership of unsaved buffers and conflict checks.
    SyncResult SynchronizeHeader(const std::string& header, const std::string& source);
}
