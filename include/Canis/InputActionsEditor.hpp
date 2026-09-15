#pragma once
namespace Canis {
class InputManager;
class EditorPanelMaximizer;
void DrawInputActionsEditor(InputManager& input, bool& open, EditorPanelMaximizer* panels = nullptr);
}
