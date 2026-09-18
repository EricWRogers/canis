#include "../src/EditorHistoryEvent.hpp"
#include <iostream>
#include <stdexcept>

static void Check(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

int main()
{
    ImGui::CreateContext();
    try
    {
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(800, 600);
        unsigned char* pixels; int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        ImGui::NewFrame();
        ImGui::Begin("Inspector");
        auto* context = ImGui::GetCurrentContext();
        const ImGuiID id = ImGui::GetID("Enabled");
        ImGui::SetActiveID(id, ImGui::GetCurrentWindow());
        Check(!Canis::HasActiveHistoryEdit(context, false), "Unedited asset click treated as a scene edit");
        Check(Canis::HasActiveHistoryEdit(context, true), "Gizmo edit not tracked");
        context->LastItemData.ID = id;
        ImGui::ClearActiveID();
        Check(!Canis::HasCurrentDeactivatedEdit(context), "Unedited click release requests a scene snapshot");
        ImGui::SetActiveID(id, ImGui::GetCurrentWindow());
        ImGui::MarkItemEdited(id);
        Check(Canis::HasActiveHistoryEdit(context, false), "Edited widget not tracked");
        context->LastItemData.ID = id;
        Check(!Canis::HasCurrentDeactivatedEdit(context), "Active edits should not commit");
        ImGui::ClearActiveID();
        Check(Canis::HasCurrentDeactivatedEdit(context), "Checkbox release lost its undo event");
        ImGui::End(); ImGui::Render();
        for (int i = 0; i < 120; ++i)
        {
            ImGui::NewFrame(); ImGui::Begin("Inspector");
            Check(context->DeactivatedItemData.HasBeenEditedBefore, "Regression fixture lost stale flag");
            Check(!Canis::HasCurrentDeactivatedEdit(context), "Idle frame repeats history capture");
            ImGui::End(); ImGui::Render();
        }
        Check(!Canis::HasCurrentDeactivatedEdit(nullptr), "Missing context accepted");
        ImGui::DestroyContext();
        std::cout << "Edited release and 120 idle history frames passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        ImGui::DestroyContext();
        return 1;
    }
}
