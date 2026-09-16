#include <Canis/Editor.hpp>
#include <Canis/ScriptSync.hpp>
#include <Canis/External/tinygltf/json.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <SDL3/SDL.h>
#include <fstream>
#include <sstream>
#include <functional>
#include <cmath>
#include <cfloat>
#include <cstring>
namespace Canis
{
using Json = nlohmann::json;
namespace
{
    std::filesystem::path SessionPath()
    {
        const auto* base = SDL_GetBasePath();
        return (base ? std::filesystem::path(base) : std::filesystem::current_path()) / "user_settings" / "script-session.json";
    }
    int Offset(const std::string& text, const Json& point)
    { return ScriptEditing::LineOffset(text, point.value("line", 0) + 1, point.value("character", 0) + 1); }
}
void Editor::SaveScriptSession()
{
    if (!m_scriptWorkspace.recoveryWritable) return;
    Json state = {{"version", 1}, {"root", CANIS_GAME_BUILD_DIR}, {"active", m_activeScriptDocument},
                  {"tabs", m_scriptTabFiles}, {"zoom", m_scriptWorkspace.zoom}, {"visible", m_showScriptEditor}, {"csharpReloadOnSave", m_csharpReloadOnSave}, {"documents", Json::array()}};
    for (const auto& doc : m_scriptDocuments)
        state["documents"].push_back({{"path", doc.path.string()}, {"text", doc.text}, {"saved", doc.savedText},
            {"cursor", doc.cursor}, {"folds", doc.foldedLines}});
    const auto serialized = state.dump();
    if (serialized == m_scriptWorkspace.lastSession) return;
    const auto path = SessionPath();
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    const auto temporary = path.string() + ".tmp";
    { std::ofstream file(temporary, std::ios::binary); file << serialized; file.close(); if (!file) return; }
    std::filesystem::rename(temporary, path, error);
    if (error) m_scriptEditorMessage = "Could not update script recovery: " + error.message();
    else m_scriptWorkspace.lastSession = serialized;
}
void Editor::RestoreScriptSession()
{
    try
    {
        std::ifstream file(SessionPath()); if (!file) return;
        auto state = Json::parse(file);
        if (state.value("version", 0) != 1 || state.value("root", "") != CANIS_GAME_BUILD_DIR) return;
        for (const auto& entry : state.at("documents"))
        {
            ScriptEditing::Document doc;
            const std::string path = entry.at("path");
            const std::string text = entry.at("text"), saved = entry.at("saved");
            if (text == saved) { if (!doc.Load(path)) continue; }
            else
            {
                doc.path = path; doc.text = text; doc.savedText = saved;
                std::string disk;
                if (!ScriptEditing::Document::Read(path, disk) || disk != saved)
                    m_scriptEditorMessage = "Recovered unsaved edits; disk changes will be protected when saving.";
            }
            doc.cursor = std::clamp(entry.value("cursor", 0), 0, (int)doc.text.size());
            doc.jump = doc.cursor;
            doc.foldedLines = entry.value("folds", std::vector<int>{});
            m_scriptDocuments.push_back(std::move(doc));
        }
        m_activeScriptDocument = state.value("active", "");
        m_scriptTabFiles = state.value("tabs", std::unordered_map<std::string, std::string>{});
        m_scriptWorkspace.zoom = std::clamp(state.value("zoom", 1.0f), 0.7f, 2.0f);
        m_showScriptEditor = state.value("visible", false);
        m_csharpReloadOnSave = state.value("csharpReloadOnSave", true);
        m_focusScriptDocument = m_showScriptEditor;
    }
    catch (const std::exception& error) { m_scriptWorkspace.recoveryWritable = false; m_scriptEditorMessage = "Script recovery could not be read: " + std::string(error.what()); }
}
void Editor::TickScriptWorkspace()
{
    auto& work = m_scriptWorkspace;
    if (work.renameApply)
    {
        work.renameApply = false;
        try
        {
            const auto changes = Json::parse(work.results).at("result").at("changes");
            auto staged = m_scriptDocuments;
            // Validate every snapshot before changing any document. Disk writes still use Save.
            for (const auto& change : changes)
            {
                const std::filesystem::path path = change.at("path").get<std::string>();
                auto doc = std::find_if(staged.begin(), staged.end(), [&](const auto& d) { return d.path == path; });
                if (doc == staged.end())
                {
                    ScriptEditing::Document added;
                    if (!added.Load(path)) throw std::runtime_error("Cannot open " + path.string());
                    staged.push_back(std::move(added)); doc = std::prev(staged.end());
                }
                if (doc->text != change.at("before").get<std::string>() || doc->pendingEdit)
                    throw std::runtime_error("Rename preview expired: " + path.string());
                doc->text = change.at("text").get<std::string>();
                ++doc->revision;
                doc->foldedLines.clear(); doc->jump = 0;
            }
            m_scriptDocuments = std::move(staged);
            work.results.clear(); work.tools = false;
            m_scriptEditorMessage = "Rename applied to buffers. Save All to write changes.";
        }
        catch (const std::exception& error) { m_scriptEditorMessage = error.what(); }
    }
    if (!work.language && m_scriptDocuments.empty()) return;
    if (!work.navigatePath.empty())
    {
        OpenScriptDocument(work.navigatePath, work.navigateLine, work.navigateColumn);
        work.navigatePath.clear();
    }
    if (!work.language)
        work.language = std::make_shared<ScriptEditing::ScriptLanguageClient>(
            std::filesystem::path(CANIS_GAME_SOURCE_DIR).parent_path().string(), CANIS_GAME_BUILD_DIR);
    auto replies=work.language->Poll();
#if CANIS_CSHARP
    if(m_csharp){auto managed=m_csharp->PollLanguage();replies.insert(replies.end(),managed.begin(),managed.end());}
#endif
    for (const auto& reply : replies)
    {
        try
        {
            if(std::filesystem::path(reply.path).extension()==".cs") {
                auto doc=std::find_if(m_scriptDocuments.begin(),m_scriptDocuments.end(),[&](const auto& d){return d.path.string()==reply.path;});
                if(doc==m_scriptDocuments.end() || doc->text!=reply.snapshot)continue;
            }
            auto json = Json::parse(reply.json);
            if (reply.method == "diagnostics")
            {
                auto& diagnostics = work.diagnostics[reply.path]; diagnostics.clear();
                for (const auto& item : json.at("diagnostics"))
                    diagnostics.push_back({reply.path, item["range"]["start"].value("line", 0) + 1,
                        item["range"]["start"].value("character", 0) + 1, item.value("message", ""), item.value("severity", 1)});
            }
            else if (reply.method == "textDocument/completion" && std::filesystem::path(reply.path).extension() == ".cs")
            {
                if (!work.completionPending || work.completionPath != reply.path || work.completionSnapshot != reply.snapshot) continue;
                if (json.contains("error")) { work.completionPending = false; m_scriptEditorMessage = json["error"].get<std::string>(); continue; }
                const auto& result = json.at("result");
                if (result.value("cursor", -1) != work.completionCursor) continue;
                work.completionPending = false; work.suggestions.clear(); work.suggestionIndex = 0;
                for (const auto& item : result.value("items", Json::array()))
                    work.suggestions.push_back({item.value("label", ""), item.value("insertText", item.value("label", "")), item.value("detail", "")});
                work.completionOpen = !work.suggestions.empty();
                work.suggestionScroll = true;
            }
            else if (reply.method != "initialize")
            {
                work.results = json.dump(); work.resultPath = reply.path; work.resultSnapshot = reply.snapshot;
                work.panel = reply.method; work.tools = true;
            }
        }
        catch (...) { m_scriptEditorMessage = "Invalid response from script language service."; }
    }
    const auto now = ImGui::GetTime();
    if (now - work.lastSync > 0.8)
    {
        for (const auto& doc : m_scriptDocuments) if (doc.path.extension() != ".cs") work.language->Sync(doc.path.string(), doc.text);
#if CANIS_CSHARP
        if(m_csharp) {
            std::map<std::string,std::string> overlays;for(const auto& doc:m_scriptDocuments)if(doc.path.extension()==".cs")overlays[doc.path.string()]=doc.text;
            for(const auto& doc:m_scriptDocuments)if(doc.path.extension()==".cs" && (!work.managedSynced.contains(doc.path.string()) || work.managedSynced[doc.path.string()]!=doc.text)) {
                work.managedSynced[doc.path.string()]=doc.text;m_csharp->RequestLanguage("diagnostics",doc.path.string(),doc.text,doc.cursor,overlays);
            }
        }
#endif
        std::string output;
        { std::scoped_lock lock(m_reloadBuildMutex); output = m_reloadBuildOutput; }
#if CANIS_CSHARP
        if (m_csharp) output += "\n" + m_csharp->BuildOutput();
#endif
        if (output != work.buildLog)
        {
            work.buildLog = output; work.buildDiagnostics.clear();
            std::istringstream lines(output); std::string line;
            while (std::getline(lines, line))
            {
                ScriptEditing::Diagnostic diagnostic;
                if (ScriptEditing::ParseDiagnostic(line, diagnostic))
                {
                    std::filesystem::path path = diagnostic.path;
                    if (path.is_relative()) path = std::filesystem::path(CANIS_GAME_BUILD_DIR) / path;
                    diagnostic.path = path.lexically_normal().string();
                    diagnostic.severity = diagnostic.message.find("error") == std::string::npos ? 2 : 1;
                    work.buildDiagnostics.push_back(std::move(diagnostic));
                }
            }
        }
        work.lastSync = now;
    }
    if (now - work.lastRecovery > 2.0) { SaveScriptSession(); work.lastRecovery = now; }
}
void Editor::RequestScriptCompletion(ScriptEditing::Document& doc)
{
#if CANIS_CSHARP
    if (!m_csharp || doc.path.extension() != ".cs") return;
    auto& work = m_scriptWorkspace;
    work.completionPath = doc.path.string(); work.completionSnapshot = doc.text;
    work.completionCursor = doc.cursor; work.completionPending = true;
    work.completionOpen = false; work.completionQueued = false;
    work.observedPath = doc.path.string(); work.observedText = doc.text; work.observedCursor = doc.cursor;
    std::map<std::string,std::string> overlays;
    for (const auto& document : m_scriptDocuments) if (document.path.extension() == ".cs") overlays[document.path.string()] = document.text;
    m_csharp->RequestLanguage("textDocument/completion", doc.path.string(), doc.text, doc.cursor, overlays);
#endif
}
void Editor::AcceptScriptCompletion(ScriptEditing::Document& doc)
{
    auto& work = m_scriptWorkspace;
    if (work.completionPath == doc.path.string() && work.completionSnapshot == doc.text && work.completionCursor == doc.cursor &&
        work.suggestionIndex >= 0 && work.suggestionIndex < (int)work.suggestions.size())
    {
        auto edit = ScriptEditing::CompleteIdentifier(doc.text, doc.cursor, work.suggestions[work.suggestionIndex].insertion);
        doc.pendingEdit = edit;
        work.observedText = doc.text; work.observedText.replace(edit.begin, edit.end - edit.begin, edit.text);
        work.observedCursor = edit.cursor;
        m_focusScriptDocument = true;
    }
    work.completionOpen = work.completionPending = work.completionQueued = false;
}
void Editor::DrawScriptIntellisense(ScriptEditing::Document& doc, unsigned int codeId)
{
    auto& work = m_scriptWorkspace;
    if (doc.path.extension() != ".cs") { work.completionOpen = work.completionPending = work.completionQueued = false; return; }
    ImGuiWindow* child = nullptr;
    for (auto* window : ImGui::GetCurrentWindow()->DC.ChildWindows)
        if (window->ChildId == codeId && window->LastFrameActive == ImGui::GetFrameCount()) child = window;
    if (!child) return;
    auto* state = ImGui::GetInputTextState(codeId);
    const auto& padding = ImGui::GetStyle().FramePadding;
    const ImVec2 origin(child->Pos.x + child->WindowPadding.x + child->DecoOuterSizeX1 + padding.x - child->Scroll.x - (state ? state->Scroll.x : 0),
        child->Pos.y + child->WindowPadding.y + child->DecoOuterSizeY1 + padding.y - child->Scroll.y);
    auto& io = ImGui::GetIO();
    const auto* suggestionWindow = ImGui::FindWindowByName("C# Suggestions##Script");
    const bool overSuggestions = suggestionWindow && suggestionWindow->WasActive && suggestionWindow->Rect().Contains(io.MousePos);
    if (ImGui::GetActiveID() != codeId && !overSuggestions && !m_focusScriptDocument)
        work.completionOpen = work.completionPending = work.completionQueued = false;
    if (work.completionPending && ImGui::IsMouseClicked(0)) work.completionPending = work.completionQueued = false;
    const float height = ImGui::GetFontSize();
    auto* font = ImGui::GetFont();
    if (child->InnerClipRect.Contains(io.MousePos) && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        const int line = std::max(0, (int)std::floor((io.MousePos.y - origin.y) / height));
        int at = ScriptEditing::LineOffset(doc.text, line + 1);
        float x = origin.x;
        while (at < (int)doc.text.size() && doc.text[at] != '\n')
        {
            int next = at + 1;
            while (next < (int)doc.text.size() && ((unsigned char)doc.text[next] & 0xc0) == 0x80) ++next;
            float width = font->CalcTextSizeA(height, FLT_MAX, 0, doc.text.data() + at, doc.text.data() + next).x;
            if (io.MousePos.x < x + width) break;
            x += width; at = next;
        }
        auto range = ScriptEditing::IdentifierSelection(doc.text, at, std::min(at + 1, (int)doc.text.size()));
        const int begin = std::min(doc.selectionStart, doc.selectionFinish), end = std::max(doc.selectionStart, doc.selectionFinish);
        if (at < begin || at >= end)
        {
            doc.selectionStart = doc.selectionEnd = range.first;
            doc.selectionFinish = doc.jump = range.second;
            doc.cursor = range.second;
        }
        work.completionOpen = work.completionPending = work.completionQueued = false;
        ImGui::OpenPopup("CSharpContext");
    }
    if (ImGui::BeginPopup("CSharpContext"))
    {
        auto request = [&](const char* method) {
#if CANIS_CSHARP
            if (m_csharp) {
                std::map<std::string,std::string> overlays;
                for (const auto& document : m_scriptDocuments) if (document.path.extension() == ".cs") overlays[document.path.string()] = document.text;
                work.panel = method; work.results.clear(); work.tools = true;
                m_csharp->RequestLanguage(method, doc.path.string(), doc.text, doc.cursor, overlays);
            }
#endif
        };
        if (ImGui::MenuItem("Go to Definition", "F12")) request("textDocument/definition");
        if (ImGui::MenuItem("Find References", "Shift+F12")) request("textDocument/references");
        if (ImGui::MenuItem("Rename Symbol", "F2")) { work.panel = "textDocument/rename"; work.results.clear(); work.tools = true; }
        if (ImGui::MenuItem("Suggest Completions", "Ctrl+Space")) { RequestScriptCompletion(doc); m_focusScriptDocument = true; }
        ImGui::Separator();
        const int begin = std::clamp(std::min(doc.selectionStart, doc.selectionFinish), 0, (int)doc.text.size());
        const int end = std::clamp(std::max(doc.selectionStart, doc.selectionFinish), begin, (int)doc.text.size());
        if (ImGui::MenuItem("Cut", "Ctrl+X", false, begin != end)) { ImGui::SetClipboardText(doc.text.substr(begin, end - begin).c_str()); doc.pendingEdit = ScriptEditing::TextEdit{begin, end, "", begin}; }
        if (ImGui::MenuItem("Copy", "Ctrl+C", false, begin != end)) ImGui::SetClipboardText(doc.text.substr(begin, end - begin).c_str());
        if (ImGui::MenuItem("Paste", "Ctrl+V")) if (const char* text = ImGui::GetClipboardText()) doc.pendingEdit = ScriptEditing::TextEdit{begin, end, text, begin + (int)std::strlen(text)};
        ImGui::Separator();
        if (ImGui::MenuItem("Select All", "Ctrl+A")) { doc.jump = 0; doc.selectionEnd = doc.text.size(); m_focusScriptDocument = true; }
        ImGui::EndPopup();
    }
    const bool changed = work.observedPath != doc.path.string() || work.observedText != doc.text || work.observedCursor != doc.cursor;
    if (changed)
    {
        const bool typed = work.observedPath == doc.path.string() && work.observedText != doc.text;
        work.completionOpen = work.completionPending = work.completionQueued = false;
        work.observedPath = doc.path.string(); work.observedText = doc.text; work.observedCursor = doc.cursor;
        if (typed && ImGui::GetActiveID() == codeId && doc.selectionStart == doc.selectionFinish && doc.cursor > 0)
        {
            auto edit = ScriptEditing::CompleteIdentifier(doc.text, doc.cursor, "");
            const char previous = doc.text[doc.cursor - 1];
            work.completionQueued = previous == '.' || doc.cursor - edit.begin >= 2;
            work.completionChanged = ImGui::GetTime();
        }
    }
    if (work.completionQueued && ImGui::GetActiveID() == codeId && ImGui::GetTime() - work.completionChanged > .22)
        RequestScriptCompletion(doc);
    if (!work.completionOpen || work.completionPath != doc.path.string() || work.completionSnapshot != doc.text || work.completionCursor != doc.cursor) return;
    const int cursor = std::clamp(doc.cursor, 0, (int)doc.text.size());
    const auto newline = cursor ? doc.text.rfind('\n', cursor - 1) : std::string::npos;
    const int lineStart = newline == std::string::npos ? 0 : (int)newline + 1;
    const int line = std::count(doc.text.begin(), doc.text.begin() + cursor, '\n');
    ImVec2 position(origin.x + font->CalcTextSizeA(height, FLT_MAX, 0, doc.text.data() + lineStart, doc.text.data() + cursor).x, origin.y + (line + 1) * height);
    const auto* viewport = ImGui::GetWindowViewport();
    const float width = std::min(460.f, viewport->WorkSize.x);
    const float popupHeight = std::min({250.f, viewport->WorkSize.y, 75.f + (float)work.suggestions.size() * ImGui::GetTextLineHeightWithSpacing()});
    if (position.y + popupHeight > viewport->WorkPos.y + viewport->WorkSize.y) position.y -= popupHeight + height;
    position.x = std::clamp(position.x, viewport->WorkPos.x, viewport->WorkPos.x + viewport->WorkSize.x - width);
    position.y = std::clamp(position.y, viewport->WorkPos.y, viewport->WorkPos.y + viewport->WorkSize.y - popupHeight);
    ImGui::SetNextWindowPos(position); ImGui::SetNextWindowSize(ImVec2(width, popupHeight));
    ImGui::Begin("C# Suggestions##Script", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavFocus);
    ImGui::BeginChild("SuggestionList", ImVec2(0, popupHeight - 75));
    for (int i = 0; i < (int)work.suggestions.size(); ++i)
    {
        ImGui::PushID(i);
        if (ImGui::Selectable(work.suggestions[i].label.c_str(), i == work.suggestionIndex, ImGuiSelectableFlags_SelectOnClick)) { work.suggestionIndex = i; AcceptScriptCompletion(doc); }
        if (i == work.suggestionIndex && work.suggestionScroll) ImGui::SetScrollHereY();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", work.suggestions[i].detail.c_str());
        ImGui::PopID();
    }
    work.suggestionScroll = false;
    ImGui::EndChild(); ImGui::Separator();
    if (work.suggestionIndex >= 0 && work.suggestionIndex < (int)work.suggestions.size()) ImGui::TextWrapped("%s", work.suggestions[work.suggestionIndex].detail.c_str());
    const bool hovered = ImGui::GetCurrentWindow()->Rect().Contains(io.MousePos);
    ImGui::End();
    if (ImGui::IsMouseClicked(0) && !hovered) work.completionOpen = work.completionPending = work.completionQueued = false;
}
void Editor::DrawScriptTools(ScriptEditing::Document* active)
{
    auto& work = m_scriptWorkspace;
    if (!work.tools) return;
    ImGui::BeginChild("ScriptTools", ImVec2(0, 205), ImGuiChildFlags_Borders);
    if (ImGui::SmallButton("Close tools")) work.tools = false;
    std::string title = work.panel;
    if (title == "textDocument/completion") title = "Completion";
    else if (title == "textDocument/rename") title = "Rename symbol";
    else if (title == "textDocument/definition") title = "Definitions";
    else if (title == "textDocument/references") title = "References";
    else if (title == "textDocument/signatureHelp") title = "Parameter hints";
    else if (title == "textDocument/documentSymbol") title = "Methods and symbols";
    ImGui::SameLine(); ImGui::TextUnformatted(title.c_str());
    if (work.panel == "Search")
    {
        ImGui::SetNextItemWidth(210); ImGui::InputTextWithHint("##replace", "Replace with", &work.replace);
        ImGui::SameLine();
        if (ImGui::Checkbox("In selection", &work.selectionOnly) && active)
        {
            work.searchPath = active->path.string();
            work.searchBegin = std::min(active->selectionStart, active->selectionFinish);
            work.searchEnd = std::max(active->selectionStart, active->selectionFinish);
        }
        if (active && !m_scriptSearch.empty())
        {
            int begin = work.selectionOnly ? work.searchBegin : 0;
            int end = work.selectionOnly ? work.searchEnd : (int)active->text.size();
            bool valid = !work.selectionOnly || (work.searchPath == active->path.string() && end > begin && end <= (int)active->text.size());
            ImGui::BeginDisabled(!valid);
            if (ImGui::Button("Replace next"))
            {
                const auto found = active->text.find(m_scriptSearch, std::max(begin, active->cursor));
                if (found != std::string::npos && found + m_scriptSearch.size() <= (size_t)end)
                    active->pendingEdit = ScriptEditing::TextEdit{(int)found, (int)(found + m_scriptSearch.size()), work.replace, (int)found + (int)work.replace.size()};
                else m_scriptEditorMessage = "No further matches in this range.";
            }
            ImGui::SameLine();
            if (ImGui::Button("Replace all"))
            {
                active->pendingEdit = ScriptEditing::ReplaceMatches(active->text, m_scriptSearch, work.replace, begin, end, true);
                if (work.selectionOnly) work.searchEnd = begin + active->pendingEdit->text.size();
            }
            ImGui::EndDisabled();
        }
        ImGui::SameLine();
        if (ImGui::Button("Search all scripts") && !m_scriptSearch.empty())
        {
            Json matches = Json::array();
            for (const auto& root : {std::filesystem::path(CANIS_GAME_SOURCE_DIR), std::filesystem::current_path() / "assets"}) {
            std::error_code error;
            for (auto it = std::filesystem::recursive_directory_iterator(root, error); !error && it != std::filesystem::recursive_directory_iterator(); it.increment(error))
            {
                const auto path = it->path(); auto extension = path.extension();
                if (extension != ".cpp" && extension != ".hpp" && extension != ".h" && extension != ".cs") continue;
                std::string text;
                const auto doc = std::find_if(m_scriptDocuments.begin(), m_scriptDocuments.end(), [&](const auto& d) { return d.path == path; });
                if (doc != m_scriptDocuments.end()) text = doc->text;
                else if (!ScriptEditing::Document::Read(path, text)) continue;
                for (size_t p = 0; (p = text.find(m_scriptSearch, p)) != std::string::npos; p += m_scriptSearch.size())
                {
                    matches.push_back({{"path", path.string()}, {"line", 1 + std::count(text.begin(), text.begin() + p, '\n')}});
                    if (matches.size() >= 1000) break;
                }
                if (matches.size() >= 1000) break;
            }
            if (matches.size() >= 1000) break;
            }
            work.results = matches.dump();
        }
        if (!work.results.empty())
        {
            auto results = Json::parse(work.results, nullptr, false);
            if (results.is_array()) for (size_t i = 0; i < results.size(); ++i)
            {
                const auto& item = results[i]; const std::string path = item.value("path", ""); int line = item.value("line", 1);
                const std::string label = path + ":" + std::to_string(line) + "##search" + std::to_string(i);
                if (ImGui::Selectable(label.c_str())) { work.navigatePath = path; work.navigateLine = line; work.navigateColumn = 1; }
            }
        }
    }
    else if (work.panel == "textDocument/rename")
    {
        ImGui::SetNextItemWidth(240); ImGui::InputText("New name", &work.renameName);
#if CANIS_CSHARP
        if (active && m_csharp && active->path.extension() == ".cs")
        {
            ImGui::SameLine();
            if (ImGui::Button("Preview rename"))
            {
                std::map<std::string,std::string> overlays;
                for (const auto& doc : m_scriptDocuments) if (doc.path.extension() == ".cs") overlays[doc.path.string()] = doc.text;
                work.results.clear();
                m_csharp->RequestLanguage(work.panel, active->path.string(), active->text, active->cursor, overlays, work.renameName);
            }
        }
#endif
        if (!work.results.empty()) try
        {
            const auto reply = Json::parse(work.results);
            if (reply.contains("error")) ImGui::TextWrapped("%s", reply["error"].dump().c_str());
            else if (reply.contains("result"))
            {
                const auto& result = reply.at("result");
                const auto& changes = result.at("changes");
                ImGui::BeginDisabled(changes.empty() || result.value("name", "") != work.renameName);
                if (ImGui::Button("Apply rename")) work.renameApply = true;
                ImGui::EndDisabled();
                for (const auto& change : changes) ImGui::TextUnformatted(change.at("path").get<std::string>().c_str());
            }
        } catch (const std::exception& error) { ImGui::TextWrapped("%s", error.what()); }
    }
    else if (work.panel == "Sync preview")
    {
        if (!active) { ImGui::TextDisabled("Open a script header to preview synchronization."); ImGui::EndChild(); return; }
        auto path = active->path;
        auto root = path.parent_path();
        while (!root.empty() && root != root.root_path() && root.filename() != "include") root = root.parent_path();
        if (root.filename() != "include") ImGui::TextWrapped("Open a script header to preview synchronization.");
        else
        {
            auto source = root.parent_path() / "src" / path.lexically_relative(root); source.replace_extension(".cpp");
            std::string text; ScriptEditing::Document::Read(source, text);
            for (const auto& doc : m_scriptDocuments) if (doc.path == source) text = doc.text;
            const auto sync = ScriptEditing::SynchronizeHeader(active->text, text);
            ImGui::Text("%d public properties; %d new method stubs", sync.properties, sync.methods);
            if (!sync.error.empty()) ImGui::TextWrapped("%s", sync.error.c_str());
            for (const auto& note : sync.notes) ImGui::TextWrapped("%s", note.c_str());
            if (sync.source == text) ImGui::TextDisabled("No source changes needed.");
            else { ImGui::TextDisabled("Proposed source (Save applies sync):"); ImGui::TextUnformatted(sync.source.c_str()); }
        }
    }
    else
    {
        if (!work.language) { ImGui::TextDisabled("Open a C++ script to use language tools."); ImGui::EndChild(); return; }
        ImGui::SetNextItemWidth(240); ImGui::InputTextWithHint("##symbolfilter", "Filter results", &work.filter);
        if (!work.language->Error().empty()) ImGui::TextWrapped("%s", work.language->Error().c_str());
        if (work.results.empty()) ImGui::TextDisabled(work.language->Ready() ? "Waiting for results..." : "Starting C++ language support...");
        else try
        {
            const auto reply = Json::parse(work.results);
            if (reply.contains("error")) ImGui::TextWrapped("%s", reply["error"].dump().c_str());
            else if (reply.contains("result"))
            {
                auto result = reply["result"];
                if (work.panel == "textDocument/completion" && result.is_object()) result = result.value("items", Json::array());
                if (work.panel == "textDocument/signatureHelp" && result.is_object())
                { for (const auto& signature : result.value("signatures", Json::array())) ImGui::TextWrapped("%s", signature.value("label", "").c_str()); }
                else
                {
                    if (result.is_object()) result = Json::array({result});
                    std::function<void(const Json&)> drawItems = [&](const Json& items)
                    {
                        if (!items.is_array()) return;
                        int id = 0;
                        for (const auto& item : items)
                        {
                            ImGui::PushID(id++);
                            std::string label = item.value("label", item.value("name", item.value("uri", item.value("targetUri", "Result"))));
                            if (work.filter.empty() || label.find(work.filter) != std::string::npos)
                            {
                                if (ImGui::Selectable(label.c_str()))
                                {
                                    if (work.panel == "textDocument/completion" && active)
                                    {
                                        if (active->path.string() != work.resultPath || active->text != work.resultSnapshot)
                                            m_scriptEditorMessage = "Completion expired; request suggestions again.";
                                        else
                                        {
                                            int begin = active->cursor; while (begin > 0 && (std::isalnum((unsigned char)active->text[begin - 1]) || active->text[begin - 1] == '_')) --begin;
                                            int end = active->cursor; std::string insertion = item.value("insertText", label);
                                            if (item.contains("textEdit"))
                                            {
                                                const auto& edit = item["textEdit"]; const auto range = edit.contains("range") ? edit["range"] : edit["replace"];
                                                begin = Offset(active->text, range["start"]); end = Offset(active->text, range["end"]); insertion = edit.value("newText", insertion);
                                            }
                                            active->pendingEdit = ScriptEditing::TextEdit{begin, end, insertion, begin + (int)insertion.size()};
                                            work.tools = false; m_focusScriptDocument = true;
                                        }
                                    }
                                    else
                                    {
                                        auto location = item.contains("location") ? item["location"] : item;
                                        std::string uri = location.value("uri", location.value("targetUri", ScriptEditing::FileUri(work.resultPath)));
                                        auto range = location.contains("targetSelectionRange") ? location["targetSelectionRange"] :
                                            location.value("selectionRange", location.value("range", Json::object()));
                                        if (range.contains("start")) { work.navigatePath = ScriptEditing::UriPath(uri); work.navigateLine = range["start"].value("line", 0) + 1; work.navigateColumn = range["start"].value("character", 0) + 1; }
                                    }
                                }
                                if (ImGui::IsItemHovered() && item.contains("detail")) ImGui::SetTooltip("%s", item["detail"].get<std::string>().c_str());
                            }
                            if (item.contains("children")) drawItems(item["children"]);
                            ImGui::PopID();
                        }
                    };
                    drawItems(result);
                    if (result.is_null() || result.empty()) ImGui::TextDisabled("No results.");
                }
            }
        } catch (const std::exception& error) { ImGui::TextWrapped("Cannot display response: %s", error.what()); }
    }
    ImGui::EndChild();
}
}
