#pragma once
#include <Canis/ScriptLanguageClient.hpp>
#include <Canis/ScriptDocument.hpp>
#include <memory>
#include <map>
namespace Canis::ScriptEditing
{
    struct Workspace
    {
        std::shared_ptr<ScriptLanguageClient> language;
        double lastSync = 0, lastRecovery = 0;
        float zoom = 1.0f;
        bool tools = false, selectionOnly = false, recoveryWritable = true;
        std::string lastSession, buildLog;
        std::vector<Diagnostic> buildDiagnostics;
        std::string replace, filter, panel = "Search", results, resultPath, resultSnapshot;
        std::string navigatePath;
        int navigateLine = 1, navigateColumn = 1;
        int searchBegin = 0, searchEnd = 0;
        std::string searchPath;
        std::map<std::string, std::vector<Diagnostic>> diagnostics;
    };
}
