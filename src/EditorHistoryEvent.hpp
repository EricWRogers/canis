#pragma once
#include <imgui_internal.h>

namespace Canis
{
    inline bool HasActiveHistoryEdit(const ImGuiContext* context, bool gizmoEditing)
    {
        return gizmoEditing || (context && context->ActiveId != 0 && context->ActiveIdHasBeenEditedBefore);
    }

    inline bool HasCurrentDeactivatedEdit(const ImGuiContext* context)
    {
        // ImGui expires the ID, but deliberately leaves the edited flag set.
        return context && context->ActiveId == 0 &&
            context->DeactivatedItemData.ID != 0 &&
            context->DeactivatedItemData.ElapseFrame >= context->FrameCount &&
            context->DeactivatedItemData.HasBeenEditedBefore;
    }
}
