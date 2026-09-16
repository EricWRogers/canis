#pragma once
#include <Canis/ScriptLanguageClient.hpp>
#include <Canis/ScriptDocument.hpp>
#include <memory>
#include <map>
#include <unordered_map>
namespace Canis::ScriptEditing
{
    struct Workspace
    {
        struct Suggestion { std::string label, insertion, detail; };
        std::vector<Suggestion> suggestions;
        std::string completionPath, completionSnapshot, observedPath, observedText;
        int completionCursor = -1, suggestionIndex = 0, observedCursor = -1;
        double completionChanged = 0;
        bool completionOpen = false, completionPending = false, completionQueued = false;
        bool suggestionScroll = false;
        std::shared_ptr<ScriptLanguageClient> language;
        std::unordered_map<std::string,std::string> managedSynced;
        double lastSync = 0, lastRecovery = 0;
        float zoom = 1.0f;
        bool tools = false, selectionOnly = false, recoveryWritable = true;
        std::string lastSession, buildLog;
        std::vector<Diagnostic> buildDiagnostics;
        std::string replace, filter, panel = "Search", results, resultPath, resultSnapshot;
        std::string renameName;
        bool renameApply = false;
        std::string navigatePath;
        int navigateLine = 1, navigateColumn = 1;
        int searchBegin = 0, searchEnd = 0;
        std::string searchPath;
        std::map<std::string, std::vector<Diagnostic>> diagnostics;
    };
}
