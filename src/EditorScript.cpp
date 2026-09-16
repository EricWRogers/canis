#include <SDL3/SDL.h>
#include <Canis/Editor.hpp>
#include <Canis/ScriptSync.hpp>
#include <set>
#include <imgui.h>
#include <imgui_internal.h>
#include <cmath>
#include <cfloat>
#include <imgui_stdlib.h>
#include <algorithm>
#include <cctype>
#include <sstream>

namespace Canis
{
    namespace
    {
        ImU32 SyntaxColor(ScriptEditing::CppColor color)
        {
            using C = ScriptEditing::CppColor;
            switch (color)
            {
            case C::Keyword: return ImGui::GetColorU32(ImVec4(0.43f, 0.68f, 1.0f, 1.0f));
            case C::Type: return ImGui::GetColorU32(ImVec4(0.35f, 0.82f, 0.74f, 1.0f));
            case C::Function: return ImGui::GetColorU32(ImVec4(0.94f, 0.83f, 0.53f, 1.0f));
            case C::Number: return ImGui::GetColorU32(ImVec4(0.68f, 0.85f, 0.60f, 1.0f));
            case C::String: return ImGui::GetColorU32(ImVec4(0.95f, 0.65f, 0.47f, 1.0f));
            case C::Comment: return ImGui::GetColorU32(ImVec4(0.49f, 0.67f, 0.48f, 1.0f));
            case C::Preprocessor: return ImGui::GetColorU32(ImVec4(0.78f, 0.61f, 0.94f, 1.0f));
            default: return ImGui::GetColorU32(ImGuiCol_Text);
            }
        }

        float DrawSyntaxLine(ImDrawList* draw, ImFont* font, float fontSize, ImVec2 position,
                             const ImRect& clip, const ScriptEditing::CppHighlightCache& cache,
                             size_t begin, size_t end)
        {
            auto span = std::lower_bound(cache.spans.begin(), cache.spans.end(), begin,
                [](const auto& token, size_t offset) { return token.end <= offset; });
            const ImVec4 fineClip(clip.Min.x, clip.Min.y, clip.Max.x, clip.Max.y);
            while (begin < end && span != cache.spans.end())
            {
                const auto tokenEnd = std::min(end, span->end);
                const char* first = cache.text.data() + begin;
                const char* last = cache.text.data() + tokenEnd;
                const float width = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, first, last).x;
                if (position.x < clip.Max.x && position.x + width > clip.Min.x)
                    draw->AddText(font, fontSize, position, SyntaxColor(span->color), first, last, 0.0f, &fineClip);
                position.x += width;
                begin = tokenEnd;
                ++span;
            }
            return position.x;
        }

        void DrawHighlightedCode(ImGuiWindow* parent, ImGuiID id, ScriptEditing::Document& doc, const std::vector<ScriptEditing::Diagnostic>& diagnostics)
        {
            // This adapter targets the vendored ImGui InputTextMultiline child layout.
            // Only glyph rendering is replaced; ImGui owns editing, selection and caret.
            ImGuiWindow* child = nullptr;
            for (auto* candidate : parent->DC.ChildWindows)
                if (candidate->ChildId == id && candidate->LastFrameActive == ImGui::GetFrameCount()) child = candidate;
            if (!child || child->SkipItems) return;
            auto* state = ImGui::GetInputTextState(id);
            float horizontal = 0.0f;
            if (state && (GImGui->ActiveId == id || GImGui->ActiveId == ImGui::GetWindowScrollbarID(child, ImGuiAxis_Y)))
                horizontal = state->Scroll.x;
            const auto& padding = ImGui::GetStyle().FramePadding;
            const ImVec2 origin(child->Pos.x + child->WindowPadding.x + child->DecoOuterSizeX1 + padding.x - child->Scroll.x - horizontal,
                child->Pos.y + child->WindowPadding.y + child->DecoOuterSizeY1 + padding.y - child->Scroll.y);
            const auto clip = child->InnerClipRect;
            if (clip.Max.y <= clip.Min.y || clip.Max.x <= clip.Min.x) return;
            doc.highlight.Update(doc.text, doc.path.extension() == ".cs");
            const auto& cache = doc.highlight;
            const float height = ImGui::GetFontSize();
            const auto first = static_cast<size_t>(std::max(0.0f, std::floor((clip.Min.y - origin.y) / height)));
            const auto last = std::min(cache.lines.size(), static_cast<size_t>(std::max(0.0f, std::ceil((clip.Max.y - origin.y) / height))));
            if (first >= last) return;
            auto* font = ImGui::GetFont();
            auto* draw = child->DrawList;
            draw->PushClipRect(clip.Min, clip.Max, true);
            for (size_t line = first; line < last; ++line)
            {
                const size_t end = line + 1 < cache.lines.size() ? cache.lines[line + 1] - 1 : cache.text.size();
                DrawSyntaxLine(draw, font, height, ImVec2(origin.x, origin.y + static_cast<float>(line) * height),
                    clip, cache, cache.lines[line], end);
            }
            const int cursor = std::clamp(doc.cursor, 0, (int)doc.text.size());
            const int cursorLine = (int)(std::upper_bound(cache.lines.begin(), cache.lines.end(), cursor) - cache.lines.begin()) - 1;
            const float currentY = origin.y + cursorLine * height;
            draw->AddRectFilled(ImVec2(clip.Min.x, currentY), ImVec2(clip.Max.x, currentY + height), IM_COL32(110, 150, 200, 22));
            int bracket = cursor;
            if (bracket >= (int)doc.text.size() || std::string("(){}[]").find(doc.text[bracket]) == std::string::npos) --bracket;
            const int match = bracket >= 0 && bracket < (int)cache.brackets.size() ? cache.brackets[bracket] : -1;
            if (match >= 0) for (int offset : {bracket, match})
            {
                auto line = std::upper_bound(cache.lines.begin(), cache.lines.end(), offset) - cache.lines.begin() - 1;
                const float x = origin.x + font->CalcTextSizeA(height, FLT_MAX, 0, doc.text.data() + cache.lines[line], doc.text.data() + offset).x;
                const float y = origin.y + line * height;
                draw->AddRect(ImVec2(x, y), ImVec2(x + font->CalcTextSizeA(height, FLT_MAX, 0, "{").x, y + height), IM_COL32(240, 200, 90, 255));
            }
            draw->PopClipRect();
            auto* gutter = parent->DrawList;
            const float gutterX = child->Pos.x - 62.0f;
            gutter->PushClipRect(ImVec2(gutterX, clip.Min.y), ImVec2(child->Pos.x, clip.Max.y), true);
            for (size_t line = first; line < last; ++line)
            {
                const float y = origin.y + line * height;
                const std::string number = std::to_string(line + 1);
                gutter->AddText(ImVec2(gutterX + 12, y), IM_COL32(140, 150, 165, 255), number.c_str());
                for (const auto& diagnostic : diagnostics) if (diagnostic.line == (int)line + 1)
                {
                    gutter->AddCircleFilled(ImVec2(gutterX + 5, y + height / 2), 4, diagnostic.severity == 1 ? IM_COL32(255, 110, 100, 255) : IM_COL32(240, 190, 80, 255));
                    if (ImGui::IsMouseHoveringRect(ImVec2(gutterX, y), ImVec2(child->Pos.x, y + height))) ImGui::SetTooltip("%s", diagnostic.message.c_str());
                }
                const auto end = line + 1 < cache.lines.size() ? cache.lines[line + 1] : doc.text.size();
                const auto brace = doc.text.find('{', cache.lines[line]);
                if (brace < end && cache.brackets[brace] >= (int)end)
                {
                    gutter->AddText(ImVec2(child->Pos.x - 14, y), IM_COL32(180, 190, 200, 255), "-");
                    if (ImGui::IsMouseHoveringRect(ImVec2(child->Pos.x - 16, y), ImVec2(child->Pos.x, y + height)) && ImGui::IsMouseClicked(0))
                        doc.foldedLines.push_back((int)line);
                }
            }
            gutter->PopClipRect();

        }

        void DrawFoldedCode(ScriptEditing::Document& doc, ImVec2 size)
        {
            // Capture the editor's font before entering the child. Use explicit
            // glyph sizes and row heights, just like the editable code renderer.
            auto* font = ImGui::GetFont();
            const float fontSize = ImGui::GetFontSize();
            ImGui::BeginChild("FoldedCode", size, ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            doc.highlight.Update(doc.text, doc.path.extension() == ".cs");
            const auto& cache = doc.highlight;
            const auto& lines = cache.lines;
            auto* window = ImGui::GetCurrentWindow();
            auto* draw = window->DrawList;
            const auto clip = window->InnerClipRect;
            draw->PushClipRect(clip.Min, clip.Max, true);
            int toggle = -1;
            for (size_t line = 0; line < lines.size(); ++line)
            {
                const auto end = line + 1 < lines.size() ? lines[line + 1] - 1 : doc.text.size();
                const auto brace = doc.text.find('{', lines[line]);
                const int match = brace < end ? cache.brackets[brace] : -1;
                const bool foldable = match >= 0 && (size_t)match > end;
                const bool folded = foldable && std::find(doc.foldedLines.begin(), doc.foldedLines.end(), (int)line) != doc.foldedLines.end();
                const auto position = ImGui::GetCursorScreenPos();
                const float textWidth = font->CalcTextSizeA(fontSize, FLT_MAX, 0, doc.text.data() + lines[line], doc.text.data() + end).x;
                const float rowWidth = std::max(ImGui::GetContentRegionAvail().x, 64.0f + textWidth + (folded ? fontSize * 3 : 0));
                ImGui::PushID((int)line);
                ImGui::InvisibleButton("line", ImVec2(rowWidth, fontSize));
                const bool overFold = foldable && ImGui::IsItemHovered() && ImGui::GetIO().MousePos.x < position.x + 64.0f;
                if (overFold)
                {
                    ImGui::SetTooltip(folded ? "Expand block" : "Fold block");
                    if (ImGui::IsMouseClicked(0)) toggle = (int)line;
                }
                else if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
                { doc.jump = (int)lines[line]; doc.foldedLines.clear(); }
                if (position.y + fontSize >= clip.Min.y && position.y < clip.Max.y)
                {
                    if (doc.cursor >= (int)lines[line] && doc.cursor <= (int)end)
                        draw->AddRectFilled(ImVec2(clip.Min.x, position.y), ImVec2(clip.Max.x, position.y + fontSize), IM_COL32(110, 150, 200, 22));
                    const auto number = std::to_string(line + 1);
                    draw->AddText(font, fontSize, ImVec2(position.x + 12, position.y), IM_COL32(140, 150, 165, 255), number.c_str());
                    if (foldable) draw->AddText(font, fontSize, ImVec2(position.x + 50, position.y), IM_COL32(180, 190, 200, 255), folded ? "+" : "-");
                    const float lastX = DrawSyntaxLine(draw, font, fontSize, ImVec2(position.x + 64, position.y), clip, cache, lines[line], end);
                    if (folded) draw->AddText(font, fontSize, ImVec2(lastX, position.y), IM_COL32(140, 150, 165, 255), " ...");
                }
                ImGui::PopID();
                if (folded) line = std::upper_bound(lines.begin(), lines.end(), match) - lines.begin() - 1;
            }
            if (toggle >= 0)
            {
                if (std::find(doc.foldedLines.begin(), doc.foldedLines.end(), toggle) == doc.foldedLines.end()) doc.foldedLines.push_back(toggle);
                else std::erase(doc.foldedLines, toggle);
            }
            draw->PopClipRect();
            ImGui::PopStyleVar();
            ImGui::EndChild();
        }

        bool IsScriptHeader(const std::filesystem::path& path)
        {
            return path.extension() == ".hpp" || path.extension() == ".h";
        }

        std::filesystem::path ScriptTabPath(const std::filesystem::path& path)
        {
            auto root = path.parent_path();
            while (root != root.root_path() && !root.empty())
            {
                if ((root.filename() == "include" || root.filename() == "src") && root.parent_path().filename() == "game")
                {
                    auto key = root.parent_path() / path.lexically_relative(root);
                    return key.replace_extension();
                }
                root = root.parent_path();
            }
            auto key = path;
            if (IsScriptHeader(path) || path.extension() == ".cpp") key.replace_extension();
            return key;
        }

        std::filesystem::path PairedScriptFile(const std::filesystem::path& path)
        {
            if (!IsScriptHeader(path) && path.extension() != ".cpp") return {};
            auto candidate = path;
            auto root = path.parent_path();
            while (root != root.root_path() && !root.empty())
            {
                if ((root.filename() == "include" || root.filename() == "src") && root.parent_path().filename() == "game")
                {
                    candidate = root.parent_path() / (IsScriptHeader(path) ? "src" : "include") / path.lexically_relative(root);
                    break;
                }
                root = root.parent_path();
            }
            for (const auto& extension : (IsScriptHeader(path) ? std::vector<std::string>{".cpp"} : std::vector<std::string>{".hpp", ".h"}))
            {
                candidate.replace_extension(extension);
                std::error_code error;
                if (std::filesystem::is_regular_file(candidate, error)) return candidate;
            }
            return {};
        }

        bool Word(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }
        struct EditAction
        {
            ScriptEditing::Document* document;
            bool complete = false;
            std::string* message;
            bool indent = false, unindent = false, comment = false;
        };

        int EditCallback(ImGuiInputTextCallbackData* data)
        {
            if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter) return data->EventChar == '\t' ? 1 : 0;
            auto& action = *static_cast<EditAction*>(data->UserData);
            auto& doc = *action.document;
            if (doc.jump >= 0)
            {
                data->CursorPos = std::clamp(doc.jump, 0, data->BufTextLen);
                data->SelectionStart = data->CursorPos;
                data->SelectionEnd = doc.selectionEnd < 0 ? data->CursorPos :
                    std::clamp(doc.selectionEnd, 0, data->BufTextLen);
                doc.jump = doc.selectionEnd = -1;
            }
            auto apply = [&](const ScriptEditing::TextEdit& edit)
            {
                data->DeleteChars(edit.begin, edit.end - edit.begin);
                data->InsertChars(edit.begin, edit.text.c_str());
                data->CursorPos = std::clamp(edit.cursor, 0, data->BufTextLen);
                data->SelectionStart = data->SelectionEnd = data->CursorPos;
            };
            if (doc.pendingEdit)
            { apply(*doc.pendingEdit); doc.pendingEdit.reset(); }
            if (action.indent || action.unindent || action.comment)
            {
                const std::string text(data->Buf, data->BufTextLen);
                const bool selected = data->SelectionStart != data->SelectionEnd;
                auto edit = action.indent && !selected ? ScriptEditing::TextEdit{data->CursorPos, data->CursorPos, "    ", data->CursorPos + 4} :
                    ScriptEditing::IndentLines(text, std::min(data->SelectionStart, data->SelectionEnd),
                        std::max(data->SelectionStart, data->SelectionEnd), action.unindent, action.comment);
                apply(edit);
                if (selected) { data->SelectionStart = edit.begin; data->SelectionEnd = edit.cursor; }
                action.indent = action.unindent = action.comment = false;
            }
            // ImGui applies normal typing first. Extend single-character edits
            // through its callback API so undo/redo still owns the changes.
            if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit && !ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeySuper)
            {
                const std::string text(data->Buf, data->BufTextLen);
                int at = data->CursorPos - 1;
                if (at >= 0 && text.size() == doc.highlight.text.size() + 1 &&
                    text.substr(0, at) == doc.highlight.text.substr(0, at) &&
                    text.substr(at + 1) == doc.highlight.text.substr(at))
                {
                    char typed = text[at];
                    if (typed == '\n')
                    {
                        auto previous = at ? text.rfind('\n', at - 1) : std::string::npos;
                        size_t begin = previous == std::string::npos ? 0 : previous + 1;
                        std::string indentation;
                        while (begin < (size_t)at && (text[begin] == ' ' || text[begin] == '\t')) indentation += text[begin++];
                        const std::string baseIndent = indentation;
                        const bool block = at && text[at - 1] == '{';
                        if (block) indentation += "    ";
                        const int caret = data->CursorPos + (int)indentation.size();
                        if (block && data->CursorPos < data->BufTextLen && data->Buf[data->CursorPos] == '}')
                            indentation += "\n" + baseIndent;
                        data->InsertChars(data->CursorPos, indentation.c_str());
                        data->CursorPos = caret; data->SelectionStart = data->SelectionEnd = caret;
                    }
                    else if (std::string(")]}\"'").find(typed) != std::string::npos && at + 1 < (int)text.size() && text[at + 1] == typed &&
                             (at == 0 || text[at - 1] != '\\'))
                        data->DeleteChars(at + 1, 1);
                    else if (!ScriptEditing::InLiteral(doc.highlight.text, std::max(0, at - 1)))
                    {
                        const std::string opens = "([{\"'", closes = ")]}\"'";
                        const auto kind = opens.find(typed);
                        if (kind != std::string::npos && (at + 1 == (int)text.size() || std::isspace((unsigned char)text[at + 1]) || std::string(")]};,").find(text[at + 1]) != std::string::npos))
                        {
                            const char pair[] = { std::string(")]}\"'")[kind], 0 };
                            const int cursor = data->CursorPos;
                            data->InsertChars(cursor, pair); data->CursorPos = cursor;
                            data->SelectionStart = data->SelectionEnd = cursor;
                        }
                    }
                }
            }
            if (action.complete)
            {
                action.complete = false;
                int begin = data->CursorPos;
                while (begin > 0 && Word(data->Buf[begin - 1])) --begin;
                const std::string prefix(data->Buf + begin, data->CursorPos - begin);
                std::vector<std::string> words = {"Canis", "ScriptableEntity", "Transform", "Rigidbody",
                    "Create", "Ready", "Destroy", "Update", "entity", "GetComponent", "HasComponents",
                    "REGISTER_PROPERTY", "DEFAULT_CONFIG", "DEFAULT_CONFIG_AND_REQUIRED", "ScriptName",
                    "float", "bool", "return", "override", "constexpr", "nullptr", "public", "private"};
                const std::string buffer(data->Buf, data->BufTextLen);
                for (size_t i = 0; i < buffer.size();)
                {
                    if (!Word(buffer[i])) { ++i; continue; }
                    const auto start = i;
                    while (i < buffer.size() && Word(buffer[i])) ++i;
                    words.push_back(buffer.substr(start, i - start));
                }
                std::sort(words.begin(), words.end());
                words.erase(std::unique(words.begin(), words.end()), words.end());
                std::vector<std::string> matches;
                for (const auto& word : words)
                    if (!prefix.empty() && word.size() > prefix.size() && word.compare(0, prefix.size(), prefix) == 0)
                        matches.push_back(word);
                if (matches.size() == 1)
                {
                    data->DeleteChars(begin, data->CursorPos - begin);
                    data->InsertChars(begin, matches.front().c_str());
                    *action.message = "Completed " + matches.front();
                }
                else
                {
                    *action.message = matches.empty() ? "No word completions for this prefix." : "Suggestions: ";
                    for (size_t i = 0; i < std::min<size_t>(matches.size(), 12); ++i)
                        *action.message += (i ? ", " : "") + matches[i];
                }
            }
            doc.cursor = data->CursorPos;
            doc.selectionStart = data->SelectionStart; doc.selectionFinish = data->SelectionEnd;
            return 0;
        }
    }

    void Editor::OpenScriptDocument(const std::filesystem::path& path, int line, int column)
    {
        m_showScriptEditor = true;
        const auto normalized = std::filesystem::absolute(path).lexically_normal();
        auto it = std::find_if(m_scriptDocuments.begin(), m_scriptDocuments.end(),
            [&](const auto& doc) { return doc.path == normalized; });
        if (it == m_scriptDocuments.end())
        {
            ScriptEditing::Document doc;
            if (!doc.Load(normalized)) { m_scriptEditorMessage = doc.error; return; }
            // Binary files cannot be edited by ImGui's text widget.
            if (doc.text.find('\0') != std::string::npos)
            { m_scriptEditorMessage = "Cannot edit a binary file."; return; }
            m_scriptDocuments.push_back(std::move(doc));
            it = std::prev(m_scriptDocuments.end());
        }
        m_activeScriptDocument = normalized.string();
        m_scriptTabFiles[ScriptTabPath(normalized).string()] = m_activeScriptDocument;
        m_focusScriptDocument = true;
        it->jump = line > 0 ? ScriptEditing::LineOffset(it->text, line, column) : it->cursor;
    }

    bool Editor::SaveScriptDocuments(const std::string& onlyPath)
    {
        // Stage changes using copies: a conflicting source or unfinished header must
        // not lose edits in either open tab or partially apply generated code.
        auto staged = m_scriptDocuments;
        std::set<std::string> savePaths;
        for (const auto& doc : staged)
            if (onlyPath.empty() || doc.path.string() == onlyPath) savePaths.insert(doc.path.string());
        const auto headers = savePaths;
        std::string summary;
        auto fail = [&](const std::string& message)
        {
            m_scriptEditorMessage = message;
            m_showScriptEditor = true;
            m_scriptBuildRequested = false;
            return false;
        };
        if (m_syncScriptHeaders)
        {
            for (const auto& headerPath : headers)
            {
                const auto header = std::filesystem::path(headerPath);
                if (header.extension() != ".hpp" && header.extension() != ".h") continue;
                auto includeRoot = header.parent_path();
                while (!includeRoot.empty() && includeRoot != includeRoot.root_path() &&
                    !(includeRoot.filename() == "include" && includeRoot.parent_path().filename() == "game"))
                    includeRoot = includeRoot.parent_path();
                if (includeRoot.filename() != "include") continue;
                auto sourcePath = includeRoot.parent_path() / "src" / header.lexically_relative(includeRoot);
                sourcePath.replace_extension(".cpp");
                const auto headerIt = std::find_if(staged.begin(), staged.end(), [&](const auto& d) { return d.path == header; });
                const std::string headerText = headerIt->text;
                auto sourceIt = std::find_if(staged.begin(), staged.end(), [&](const auto& d) { return d.path == sourcePath; });
                if (sourceIt == staged.end())
                {
                    ScriptEditing::Document source;
                    if (!source.Load(sourcePath))
                    {
                        // Ordinary helper headers do not have script source pairs.
                        if (headerText.find("ScriptName") == std::string::npos) continue;
                        return fail("Header sync: " + source.error + ". Create a script pair with New Script first.");
                    }
                    staged.push_back(std::move(source));
                    sourceIt = std::prev(staged.end());
                }
                const auto sync = ScriptEditing::SynchronizeHeader(headerText, sourceIt->text);
                if (!sync.error.empty()) return fail("Header sync: " + sync.error + " Disable Sync headers on save to save manually.");
                if (sync.source != sourceIt->text)
                {
                    sourceIt->text = sync.source;
                    savePaths.insert(sourcePath.string());
                    summary += " Synced " + sourcePath.filename().string() + ": " + std::to_string(sync.properties) +
                        " auto properties, " + std::to_string(sync.methods) + " new methods.";
                }
                for (const auto& note : sync.notes) summary += " " + note;
            }
        }
        // Validate all affected files before any write, including dirty paired sources.
        for (const auto& doc : staged)
            if (savePaths.count(doc.path.string()))
            {
                std::string disk;
                if (!ScriptEditing::Document::Read(doc.path, disk) || disk != doc.savedText)
                    return fail("Save canceled: " + doc.path.string() + " changed or was deleted on disk. Reopen it before syncing.");
            }
        for (auto& doc : staged)
            if (savePaths.count(doc.path.string()))
            {
                m_scriptsNeedBuild = m_scriptsNeedBuild || (doc.path.extension() != ".cs" && doc.Dirty());
                if (!doc.Save())
                {
                    // Retain staged edits and the exact saved state of any earlier writes.
                    const std::string error = doc.error;
                    m_scriptDocuments = std::move(staged);
                    return fail("Save stopped; some earlier files may have been saved. " + error);
                }
            }
        m_scriptDocuments = std::move(staged);
        m_scriptEditorMessage = "Saved." + summary;
        return true;
    }

    void Editor::DrawScriptBuildLog(const std::string& output)
    {
        std::istringstream lines(output);
        std::string line;
        int index = 0;
        while (std::getline(lines, line))
        {
            ScriptEditing::Diagnostic diagnostic;
            if (ScriptEditing::ParseDiagnostic(line, diagnostic))
            {
                ImGui::PushID(index);
                if (ImGui::Selectable(line.c_str()))
                {
                    auto path = std::filesystem::path(diagnostic.path);
                    if (path.is_relative()) path = std::filesystem::path(CANIS_GAME_BUILD_DIR) / path;
                    m_scriptWorkspace.navigatePath = path.string();
                    m_scriptWorkspace.navigateLine = diagnostic.line;
                    m_scriptWorkspace.navigateColumn = diagnostic.column;
                    std::scoped_lock lock(m_reloadBuildMutex);
                    if (m_reloadBuildFinished)
                    {
                        m_showReloadBuildPopup = false;
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::PopID();
            }
            else ImGui::TextUnformatted(line.c_str());
            ++index;
        }
    }

    void Editor::DrawScriptEditor()
    {
        ImGui::SetNextWindowSize(ImVec2(1000, 650), ImGuiCond_FirstUseEver);
        if (m_focusScriptDocument) ImGui::SetNextWindowFocus();
        if (!m_panelMaximizer.Begin("Script Editor", &m_showScriptEditor)) { ImGui::End(); return; }
        const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        const auto& io = ImGui::GetIO();
        auto current = std::find_if(m_scriptDocuments.begin(), m_scriptDocuments.end(),
            [&](const auto& doc) { return doc.path.string() == m_activeScriptDocument; });
        if (current == m_scriptDocuments.end() && !m_scriptDocuments.empty())
        {
            current = m_scriptDocuments.begin();
            m_activeScriptDocument = current->path.string();
        }
        auto* active = current == m_scriptDocuments.end() ? nullptr : &*current;
        bool saveAll = focused && io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_S, false);
        std::string savePath = active && focused && io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_S, false)
            ? active->path.string() : "";
        bool build = false, play = false, reopen = false, help = false;
        const bool csharp = active && active->path.extension() == ".cs";
        if (csharp && focused && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_R, false)) build = true;
        bool goTo = focused && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_G, false);
        bool findNext = focused && ImGui::IsKeyPressed(ImGuiKey_F3, false);
        const bool focusFind = focused && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F, false);
        const bool switchKey = focused && io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_O, false);
        std::filesystem::path switchPath;
        std::string closePath;
        auto tooltip = [](const char* text)
        {
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", text);
        };

        if (ImGui::Button("File") || (focused && io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_F, false))) ImGui::OpenPopup("ScriptFileMenu");
        const ImVec2 fileMenuAnchor(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y);
        tooltip("Alt+F — save, build, and script settings");
        if (ImGui::IsPopupOpen("ScriptFileMenu")) ImGui::SetNextWindowPos(fileMenuAnchor);
        if (ImGui::BeginPopup("ScriptFileMenu"))
        {
            if (ImGui::MenuItem("Save", "Ctrl+S", false, active != nullptr)) savePath = active->path.string();
            if (ImGui::MenuItem("Save All", "Ctrl+Shift+S", false, !m_scriptDocuments.empty())) saveAll = true;
            if (ImGui::MenuItem("Reopen disk version...", nullptr, false, active != nullptr)) reopen = true;
            ImGui::Separator();
            if (ImGui::MenuItem(csharp ? "Save + Compile / Reload C#" : "Save + Build / Reload", "Ctrl+R", false, csharp || m_mode == EditorMode::EDIT)) build = true;
            if (ImGui::MenuItem("Play", "Ctrl+P", false, m_mode == EditorMode::EDIT)) play = true;
            if (ImGui::MenuItem("Build Output"))
            {
                std::scoped_lock lock(m_reloadBuildMutex);
                m_showReloadBuildPopup = m_openReloadBuildPopup = true;
                m_scriptBuildOutputPinned = true;
            }
            ImGui::Separator();
            if (!csharp) ImGui::MenuItem("Sync headers on save", nullptr, &m_syncScriptHeaders);
#if CANIS_CSHARP
            if (ImGui::MenuItem("C# reload on save", nullptr, &m_csharpReloadOnSave) && m_csharp)
                m_csharp->SetReloadOnSave(m_csharpReloadOnSave);
            if(csharp && m_csharp && ImGui::MenuItem("Open generated C# project")){auto uri=std::string("file://")+m_csharp->ProjectPath();SDL_OpenURL(uri.c_str());}
            if(m_csharp) {bool live=m_csharp->LiveReload();if(ImGui::MenuItem("C# reload during Play (serialized fields)",nullptr,&live))m_csharp->SetLiveReload(live);}
#endif
            if (ImGui::MenuItem("Go to line...", "Ctrl+G", false, active != nullptr)) goTo = true;
            if (ImGui::MenuItem("Find and replace / Search scripts", "Ctrl+H"))
            { m_scriptWorkspace.tools = true; m_scriptWorkspace.panel = "Search"; m_scriptWorkspace.results.clear(); }
            if (ImGui::MenuItem("Preview header synchronization"))
            { m_scriptWorkspace.tools = true; m_scriptWorkspace.panel = "Sync preview"; }
            if (ImGui::BeginMenu(csharp ? "C# navigation" : "C++ navigation", active != nullptr))
            {
                for (const auto& entry : std::vector<std::pair<const char*, const char*>>{
                    {"Complete (Ctrl+Space)", "textDocument/completion"}, {"Parameter hints (Ctrl+Shift+Space)", "textDocument/signatureHelp"},
                    {"Go to definition (F12)", "textDocument/definition"}, {"Find references (Shift+F12)", "textDocument/references"},
                    {"Methods and symbols (Ctrl+Shift+O)", "textDocument/documentSymbol"}})
                    if (ImGui::MenuItem(entry.first))
                    {
                        m_scriptWorkspace.tools = true; m_scriptWorkspace.panel = entry.second; m_scriptWorkspace.results.clear();
#if CANIS_CSHARP
                        if(csharp && m_csharp){std::map<std::string,std::string> overlays;for(auto& doc:m_scriptDocuments)if(doc.path.extension()==".cs")overlays[doc.path.string()]=doc.text;m_csharp->RequestLanguage(entry.second,active->path.string(),active->text,active->cursor,overlays);}else
#endif
                        m_scriptWorkspace.language->Request(entry.second, active->path.string(), active->text, active->cursor);
                    }
                ImGui::EndMenu();
            }
            ImGui::SliderFloat("Font zoom", &m_scriptWorkspace.zoom, 0.7f, 2.0f, "%.1fx");
            if (active && ImGui::MenuItem("Unfold all")) active->foldedLines.clear();
            if (ImGui::MenuItem("C++ scripting quick reference")) help = true;
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        auto paired = active ? PairedScriptFile(active->path) : std::filesystem::path();
        if (active)
            for (const auto& doc : m_scriptDocuments)
                if (ScriptTabPath(doc.path) == ScriptTabPath(active->path) && IsScriptHeader(doc.path) != IsScriptHeader(active->path))
                    paired = doc.path;
        if (!csharp)
        {
        ImGui::BeginDisabled(paired.empty());
        if (ImGui::Button(active && IsScriptHeader(active->path) ? "Switch to Source" : "Switch to Header") || switchKey)
            switchPath = paired;
        ImGui::EndDisabled();
        tooltip(paired.empty() ? "No paired header/source file exists." : "Alt+O — switch between header and source");
        }
        ImGui::SameLine();
        const float nextWidth = ImGui::CalcTextSize("Find Next").x + ImGui::GetStyle().FramePadding.x * 2;
        ImGui::SetNextItemWidth(std::max(60.0f, ImGui::GetContentRegionAvail().x - nextWidth - ImGui::GetStyle().ItemSpacing.x));
        if (focusFind) { ImGui::SetKeyboardFocusHere(); m_focusScriptDocument = false; }
        if (ImGui::InputTextWithHint("##find", "Find (case sensitive)", &m_scriptSearch, ImGuiInputTextFlags_EnterReturnsTrue)) findNext = true;
        tooltip("Ctrl+F — find text (case sensitive)");
        ImGui::SameLine();
        ImGui::BeginDisabled(active == nullptr || m_scriptSearch.empty());
        if (ImGui::Button("Find Next")) findNext = true;
        ImGui::EndDisabled();
        tooltip("F3 or Enter in Find — next match");

        if (reopen) ImGui::OpenPopup("Reopen script from disk?");
        if (ImGui::BeginPopupModal("Reopen script from disk?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted("Replace the current file with its disk version? Unsaved edits in this file will be lost.");
            if (ImGui::Button("Reopen"))
            {
                if (active && !active->Load(active->path)) m_scriptEditorMessage = active->error;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        if (goTo) ImGui::OpenPopup("Go to script line");
        if (ImGui::BeginPopup("Go to script line"))
        {
            if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            ImGui::SetNextItemWidth(140);
            ImGui::InputInt("Line", &m_scriptGotoLine, 0, 0);
            const bool enter = ImGui::IsItemFocused() &&
                (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false));
            if ((ImGui::Button("Go") || enter) && active)
            {
                active->jump = ScriptEditing::LineOffset(active->text, m_scriptGotoLine);
                active->selectionEnd = -1;
                m_focusScriptDocument = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        if (help) ImGui::OpenPopup("C++ scripting quick reference");
        if (ImGui::BeginPopup("C++ scripting quick reference"))
        {
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 560);
            ImGui::TextUnformatted("Create: initialize state. Ready: scene is ready. Update(float _dt): advance using seconds. Destroy: release resources.");
            ImGui::TextUnformatted("Sync headers on save registers supported public fields and creates missing public/private method definitions. Existing implementations are preserved; non-void stubs throw until implemented.");
            ImGui::TextUnformatted("Save + Build / Reload, then Play to test. Stop Play mode before rebuilding. Ctrl+Space completes words; compiler diagnostics appear in Build Output.");
            ImGui::PopTextWrapPos();
            ImGui::EndPopup();
        }
        if (findNext && active && !m_scriptSearch.empty())
        {
            auto& work = m_scriptWorkspace;
            const bool scoped = work.selectionOnly && work.searchPath == active->path.string();
            const size_t begin = scoped ? std::clamp(work.searchBegin, 0, (int)active->text.size()) : 0;
            const size_t end = scoped ? std::clamp(work.searchEnd, (int)begin, (int)active->text.size()) : active->text.size();
            auto offset = active->text.find(m_scriptSearch, std::clamp((size_t)active->cursor + 1, begin, end));
            if (offset == std::string::npos || offset + m_scriptSearch.size() > end) offset = active->text.find(m_scriptSearch, begin);
            if (offset != std::string::npos && offset + m_scriptSearch.size() <= end)
            {
                active->jump = static_cast<int>(offset);
                active->selectionEnd = active->jump + static_cast<int>(m_scriptSearch.size());
                m_focusScriptDocument = true;
                m_scriptEditorMessage.clear();
            }
            else m_scriptEditorMessage = "No matches.";
        }

        if (focused && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_H, false))
        { m_scriptWorkspace.tools = true; m_scriptWorkspace.panel = "Search"; m_scriptWorkspace.results.clear(); }
        if (active && focused)
        {
            if (io.KeyCtrl && io.MouseWheel != 0 && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows))
                m_scriptWorkspace.zoom = std::clamp(m_scriptWorkspace.zoom + io.MouseWheel * 0.1f, 0.7f, 2.0f);
            std::string method;
            if (ImGui::IsKeyPressed(ImGuiKey_F12, false)) method = io.KeyShift ? "textDocument/references" : "textDocument/definition";
            if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_O, false)) method = "textDocument/documentSymbol";
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Space, false)) method = io.KeyShift ? "textDocument/signatureHelp" : "textDocument/completion";
            if (!method.empty())
            {
                m_scriptWorkspace.tools = true; m_scriptWorkspace.panel = method; m_scriptWorkspace.results.clear();
#if CANIS_CSHARP
                if(csharp && m_csharp){std::map<std::string,std::string> overlays;for(auto& doc:m_scriptDocuments)if(doc.path.extension()==".cs")overlays[doc.path.string()]=doc.text;m_csharp->RequestLanguage(method,active->path.string(),active->text,active->cursor,overlays);}else
#endif
                if (!csharp) m_scriptWorkspace.language->Request(method, active->path.string(), active->text, active->cursor);
            }
        }
        DrawScriptTools(active);
        std::vector<std::pair<std::string, std::vector<size_t>>> tabs;
        for (size_t i = 0; i < m_scriptDocuments.size(); ++i)
        {
            const auto key = ScriptTabPath(m_scriptDocuments[i].path).string();
            auto tab = std::find_if(tabs.begin(), tabs.end(), [&](const auto& t) { return t.first == key; });
            if (tab == tabs.end()) tabs.push_back({key, {i}});
            else tab->second.push_back(i);
        }
        if (ImGui::BeginTabBar("ScriptFiles", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_FittingPolicyScroll))
        {
            for (const auto& [key, documents] : tabs)
            {
                size_t visible = documents.front();
                const auto remembered = m_scriptTabFiles.find(key);
                bool dirty = false;
                for (const auto index : documents)
                {
                    const auto& doc = m_scriptDocuments[index];
                    dirty = dirty || doc.Dirty();
                    if (remembered != m_scriptTabFiles.end() && doc.path.string() == remembered->second) visible = index;
                }
                if (m_focusScriptDocument)
                    for (const auto index : documents)
                        if (m_scriptDocuments[index].path.string() == m_activeScriptDocument) visible = index;
                auto& doc = m_scriptDocuments[visible];
                const auto path = doc.path.string();
                const std::string label = doc.path.stem().string() + "###" + key;
                bool open = true;
                ImGuiTabItemFlags flags = dirty ? ImGuiTabItemFlags_UnsavedDocument : 0;
                if (m_focusScriptDocument && path == m_activeScriptDocument) flags |= ImGuiTabItemFlags_SetSelected;
                if (ImGui::BeginTabItem(label.c_str(), &open, flags))
                {
                    if (!m_focusScriptDocument) m_activeScriptDocument = path;
                    active = &doc;
                    ImGui::EndTabItem();
                }
                tooltip(path.c_str());
                if (!open)
                {
                    if (dirty) m_scriptEditorMessage = "Unsaved edits: save both files or reopen their disk versions before closing.";
                    else closePath = key;
                }
            }
            ImGui::EndTabBar();
        }
#if CANIS_CSHARP
        if (csharp && m_csharp)
        {
            ImGui::TextWrapped("%s", m_csharp->Status().c_str());
            if (ImGui::CollapsingHeader("C# Build Output"))
            {
                ImGui::BeginChild("CSharpBuildOutput", ImVec2(0, 150));
                DrawScriptBuildLog(m_csharp->BuildOutput());
                ImGui::EndChild();
            }
        }
#endif
        const float statusHeight = ImGui::GetFrameHeightWithSpacing();
        if (active)
        {
            auto& doc = *active;
            ImGui::PushID(doc.path.string().c_str());
            const bool editing = ImGui::GetActiveID() == ImGui::GetID("##code");
            if (editing) ImGui::SetKeyOwner(ImGuiKey_Tab, ImGui::GetID("##code"));
            EditAction action{&doc, false, &m_scriptEditorMessage};
            action.indent = editing && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Tab, false);
            action.unindent = editing && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Tab, false);
            action.comment = editing && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Slash, false);
            if (doc.pendingEdit || (doc.jump >= 0 && doc.foldedLines.empty()) || (m_focusScriptDocument && doc.path.string() == m_activeScriptDocument))
            {
                ImGui::SetKeyboardFocusHere();
                m_focusScriptDocument = false;
            }
            auto* codeParent = ImGui::GetCurrentWindow();
            const auto codeId = ImGui::GetID("##code");
            ImGui::SetWindowFontScale(m_scriptWorkspace.zoom);
            if (!doc.foldedLines.empty() && (doc.pendingEdit || doc.jump >= 0)) doc.foldedLines.clear();
            if (!doc.foldedLines.empty())
                DrawFoldedCode(doc, ImVec2(0, std::max(40.0f, ImGui::GetContentRegionAvail().y - statusHeight)));
            else
            {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 64.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 0));
            const float editorHeight = std::max(40.0f, ImGui::GetContentRegionAvail().y - statusHeight);
            if (doc.editorHeight > 0 && std::abs(doc.editorHeight - editorHeight) > 1)
                if (auto* state = ImGui::GetInputTextState(codeId)) state->CursorFollow = true;
            doc.editorHeight = editorHeight;
            ImGui::InputTextMultiline("##code", &doc.text,
                ImVec2(-1, editorHeight),
                ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackCharFilter | ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_CallbackEdit, EditCallback, &action);
            ImGui::PopStyleColor();
            auto diagnostics = m_scriptWorkspace.diagnostics[doc.path.string()];
            for (const auto& diagnostic : m_scriptWorkspace.buildDiagnostics)
                if (diagnostic.path == doc.path.string()) diagnostics.push_back(diagnostic);
            DrawHighlightedCode(codeParent, codeId, doc, diagnostics);
            }
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopID();
        }
        else
        {
            ImGui::BeginChild("EmptyScript", ImVec2(0, std::max(40.0f, ImGui::GetContentRegionAvail().y - statusHeight)));
            ImGui::TextWrapped("Double-click a file in Scripts to start editing.");
            ImGui::EndChild();
        }
        ImGui::BeginChild("ScriptStatus", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        if (active)
        {
            const auto cursor = std::min(active->text.size(), static_cast<size_t>(std::max(0, active->cursor)));
            const int line = 1 + static_cast<int>(std::count(active->text.begin(), active->text.begin() + cursor, '\n'));
            const auto previous = cursor ? active->text.rfind('\n', cursor - 1) : std::string::npos;
            bool dirty = false;
            for (const auto& doc : m_scriptDocuments)
                if (ScriptTabPath(doc.path) == ScriptTabPath(active->path)) dirty = dirty || doc.Dirty();
            ImGui::Text("Ln %d, Col %d | %s | %s", line,
                static_cast<int>(cursor - (previous == std::string::npos ? 0 : previous + 1)) + 1,
                dirty ? "Unsaved" : "Saved", active->path.filename().string().c_str());
            tooltip(active->path.string().c_str());
        }
        else ImGui::TextDisabled("No file open");
        if (!m_scriptEditorMessage.empty())
        {
            ImGui::SameLine();
            ImGui::TextDisabled("| %s", m_scriptEditorMessage.c_str());
            if (ImGui::IsWindowHovered()) ImGui::SetTooltip("%s", m_scriptEditorMessage.c_str());
        }
        ImGui::EndChild();

        if (!savePath.empty()) SaveScriptDocuments(savePath);
        if (saveAll || build)
            if (SaveScriptDocuments() && build)
            {
                if (csharp)
                {
#if CANIS_CSHARP
                    if (m_csharp) m_csharp->RequestBuild();
#else
                    m_scriptEditorMessage = "Configure with CANIS_ENABLE_CSHARP=ON to compile C# assets.";
#endif
                }
                else m_scriptBuildRequested = true;
            }
        if (play)
        {
            const bool dirty = std::any_of(m_scriptDocuments.begin(), m_scriptDocuments.end(), [](const auto& d) { return d.Dirty(); });
            if (dirty || m_scriptsNeedBuild) m_scriptEditorMessage = "Save + Build / Reload before testing your changes.";
            else StartPlayModeAt(nullptr);
        }
        if (!closePath.empty())
        {
            for (const auto& doc : m_scriptDocuments)
                if (ScriptTabPath(doc.path).string() == closePath && m_scriptWorkspace.language)
                {
                    m_scriptWorkspace.language->Close(doc.path.string());
                    m_scriptWorkspace.diagnostics.erase(doc.path.string());
                }
            std::erase_if(m_scriptDocuments, [&](const auto& doc) { return ScriptTabPath(doc.path).string() == closePath; });
            m_scriptTabFiles.erase(closePath);
        }
        if (!switchPath.empty()) OpenScriptDocument(switchPath);
        ImGui::End();
    }
}
