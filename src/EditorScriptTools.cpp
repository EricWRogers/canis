#include <Canis/Editor.hpp>
#include <Canis/ScriptSync.hpp>
#include <Canis/External/tinygltf/json.hpp>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <SDL3/SDL.h>
#include <fstream>
#include <sstream>
#include <functional>
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
                  {"tabs", m_scriptTabFiles}, {"zoom", m_scriptWorkspace.zoom}, {"visible", m_showScriptEditor}, {"documents", Json::array()}};
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
        m_focusScriptDocument = m_showScriptEditor;
    }
    catch (const std::exception& error) { m_scriptWorkspace.recoveryWritable = false; m_scriptEditorMessage = "Script recovery could not be read: " + std::string(error.what()); }
}
void Editor::TickScriptWorkspace()
{
    auto& work = m_scriptWorkspace;
    if (!work.language && m_scriptDocuments.empty()) return;
    if (!work.navigatePath.empty())
    {
        OpenScriptDocument(work.navigatePath, work.navigateLine, work.navigateColumn);
        work.navigatePath.clear();
    }
    if (!work.language)
        work.language = std::make_shared<ScriptEditing::ScriptLanguageClient>(
            std::filesystem::path(CANIS_GAME_SOURCE_DIR).parent_path().string(), CANIS_GAME_BUILD_DIR);
    for (const auto& reply : work.language->Poll())
    {
        try
        {
            auto json = Json::parse(reply.json);
            if (reply.method == "diagnostics")
            {
                auto& diagnostics = work.diagnostics[reply.path]; diagnostics.clear();
                for (const auto& item : json.at("diagnostics"))
                    diagnostics.push_back({reply.path, item["range"]["start"].value("line", 0) + 1,
                        item["range"]["start"].value("character", 0) + 1, item.value("message", ""), item.value("severity", 1)});
            }
            else if (reply.method != "initialize")
            {
                work.results = json.dump(); work.resultPath = reply.path; work.resultSnapshot = reply.snapshot;
                work.panel = reply.method; work.tools = true;
            }
        }
        catch (...) { m_scriptEditorMessage = "Invalid response from clangd."; }
    }
    const auto now = ImGui::GetTime();
    if (now - work.lastSync > 0.8)
    {
        for (const auto& doc : m_scriptDocuments) work.language->Sync(doc.path.string(), doc.text);
        std::string output;
        { std::scoped_lock lock(m_reloadBuildMutex); output = m_reloadBuildOutput; }
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
                    diagnostic.severity = diagnostic.message.find("error:") == std::string::npos ? 2 : 1;
                    work.buildDiagnostics.push_back(std::move(diagnostic));
                }
            }
        }
        work.lastSync = now;
    }
    if (now - work.lastRecovery > 2.0) { SaveScriptSession(); work.lastRecovery = now; }
}
void Editor::DrawScriptTools(ScriptEditing::Document* active)
{
    auto& work = m_scriptWorkspace;
    if (!work.tools) return;
    ImGui::BeginChild("ScriptTools", ImVec2(0, 205), ImGuiChildFlags_Borders);
    if (ImGui::SmallButton("Close tools")) work.tools = false;
    std::string title = work.panel;
    if (title == "textDocument/completion") title = "C++ completion";
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
            const auto root = std::filesystem::path(CANIS_GAME_SOURCE_DIR);
            std::error_code error;
            for (auto it = std::filesystem::recursive_directory_iterator(root, error); !error && it != std::filesystem::recursive_directory_iterator(); it.increment(error))
            {
                const auto path = it->path(); auto extension = path.extension();
                if (extension != ".cpp" && extension != ".hpp" && extension != ".h") continue;
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
