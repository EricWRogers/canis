#include <Canis/Editor.hpp>
#include <Canis/Scene.hpp>
#include <Canis/Window.hpp>
#include <Canis/EditorTransformConstraints.hpp>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <ImGuizmo.h>
#include <set>

namespace Canis
{
    Entity *Editor::MeshEditEntity() const
    {
        if (!m_scene || m_mode != EditorMode::EDIT || m_meshEditEntity == UUID(0))
            return nullptr;
        Entity *e = m_scene->GetEntityWithUUID(m_meshEditEntity);
        if (!e || e->EditorLocked() || !e->HasComponent<BlockoutShape>() || !e->HasComponent<Transform>())
            return nullptr;
        return e->GetComponent<BlockoutShape>().meshEdited ? e : nullptr;
    }

    void Editor::ToggleMeshEdit()
    {
        if (m_meshEditEntity != UUID(0))
        {
            ExitMeshEdit();
            return;
        }
        auto selected = GetSelectedEntities();
        if (m_mode != EditorMode::EDIT || selected.size() != 1 || selected[0]->EditorLocked() ||
            !selected[0]->HasComponent<BlockoutShape>() || !selected[0]->HasComponent<Transform>() ||
            m_sceneCameraMode != SCENE_CAMERA_3D)
            return;
        auto before = CaptureSceneHistoryState();
        Entity &e = *selected[0];
        if (!ConvertBlockoutToEditable(e.GetComponent<BlockoutShape>(), m_meshError))
            return;
        RebuildBlockoutEntity(e);
        CommitSceneHistoryImmediateChange(before);
        m_meshEditEntity = e.GetUUID();
        m_meshSelection.clear();
        m_meshEdges.clear();
        m_meshSelectMode = 2;
        m_blockoutDrawType = -1;
        m_marqueeSelectActive = false;
        m_terrainToolEnabled = false;
    }

    void Editor::ExitMeshEdit()
    {
        FinishMeshOperation(false);
        m_meshEditEntity = UUID(0);
        m_meshSelection.clear();
        m_meshBoxSelect = false;
    }

    void Editor::BeginMeshOperation(int operation)
    {
        Entity *e = MeshEditEntity();
        if (!e || m_meshOperation)
            return;
        if (operation != 5 && m_meshSelection.empty())
        {
            m_meshError = "Select mesh elements first.";
            return;
        }
        if (operation == 4 && m_meshSelectMode != 2)
        {
            m_meshError = "Extrude requires Face selection (3).";
            return;
        }
        m_meshBefore = e->GetComponent<BlockoutShape>().editMesh;
        m_meshPreview = m_meshBefore;
        m_meshHistoryBefore = CaptureSceneHistoryState();
        m_meshOperation = operation;
        m_meshAxis = -1;
        m_meshPlaneConstraint = false;
        m_meshAmount = 0;
        m_meshNumeric.clear();
        m_meshError.clear();
        auto p = ImGui::GetMousePos();
        m_meshDragStart = {p.x, p.y};
    }

    void Editor::FinishMeshOperation(bool confirm)
    {
        if (!m_meshOperation)
            return;
        if (confirm)
        {
            Entity *e = MeshEditEntity();
            if (!e || !m_meshError.empty())
                return;
            if (!ValidateBlockoutTopology(m_meshPreview, m_meshError))
                return;
            auto &shape = e->GetComponent<BlockoutShape>();
            shape.editMesh = m_meshPreview;
            shape.MarkDirty();
            RebuildBlockoutEntity(*e);
            CommitSceneHistoryImmediateChange(m_meshHistoryBefore);
        }
        else if (Entity *e = MeshEditEntity())
        {
            // The authored topology is untouched during preview. Restore only its render data.
            RebuildBlockoutEntity(*e, false);
        }
        if (!confirm && m_meshOperation == 6)
            ImGuizmo::Enable(false);
        m_meshOperation = 0;
        m_meshNumeric.clear();
        m_meshBefore = {};
        m_meshPreview = {};
    }

    void Editor::DrawMeshEditToolbar()
    {
        if (m_meshEditEntity != UUID(0) && !MeshEditEntity())
            ExitMeshEdit();
        auto selected = GetSelectedEntities();
        if (m_meshEditEntity != UUID(0) && (selected.size() != 1 || selected[0]->GetUUID() != m_meshEditEntity))
            ExitMeshEdit();
        bool keyboard = m_sceneViewFocused && !ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive() &&
                        !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
        if (keyboard && ImGui::IsKeyPressed(ImGuiKey_Tab, false))
            ToggleMeshEdit();
        Entity *e = MeshEditEntity();
        if (!e)
        {
            if (selected.size() == 1 && selected[0]->HasComponent<BlockoutShape>() &&
                !selected[0]->EditorLocked() && m_mode == EditorMode::EDIT)
            {
                if (ImGui::Button("Edit Mesh (Tab)"))
                    ToggleMeshEdit();
                if (!m_meshError.empty() && ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", m_meshError.c_str());
                ImGui::SameLine();
            }
            return;
        }
        if (ImGui::Button("Exit (Tab)"))
        {
            ExitMeshEdit();
            return;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Editing %s", e->GetName().c_str());
        ImGui::BeginDisabled(m_meshOperation != 0);
        const char *modes[] = {"Vertex", "Edge", "Face"};
        for (int i = 0; i < 3; ++i)
        {
            ImGui::SameLine();
            if (ImGui::RadioButton(modes[i], m_meshSelectMode == i))
            {
                m_meshSelectMode = i;
                m_meshSelection.clear();
            }
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(85);
        ImGui::Combo("##MeshGizmo", &m_meshGizmoMode, "Move\0Rotate\0Scale\0");
        const char *ops[] = {"Move", "Rotate", "Scale", "Extrude", "Loop Cut"};
        for (int i = 0; i < 5; ++i)
        {
            if (i)
                ImGui::SameLine();
            if (ImGui::Button(ops[i]))
                BeginMeshOperation(i + 1);
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete / Dissolve"))
            ImGui::OpenPopup("MeshDelete");
        if (m_meshDeleteRequested)
        {
            ImGui::OpenPopup("MeshDelete");
            m_meshDeleteRequested = false;
        }
        if (ImGui::BeginPopup("MeshDelete"))
        {
            auto &shape = e->GetComponent<BlockoutShape>();
            if (ImGui::MenuItem("Delete selected faces/elements"))
            {
                auto before = CaptureSceneHistoryState();
                auto candidate = shape.editMesh;
                std::set<u32> vertices;
                if (m_meshSelectMode == 0)
                    vertices.insert(m_meshSelection.begin(), m_meshSelection.end());
                for (u32 id = 0; id < candidate.faces.size(); ++id)
                {
                    auto &f = candidate.faces[id];
                    bool remove = m_meshSelectMode == 2 &&
                                  std::find(m_meshSelection.begin(), m_meshSelection.end(), id) !=
                                      m_meshSelection.end();
                    for (u32 v : f.vertices)
                        remove |= vertices.count(v) > 0;
                    if (m_meshSelectMode == 1)
                        for (u32 edge : m_meshSelection)
                            if (edge < m_meshEdges.size())
                                for (size_t i = 0; i < f.vertices.size(); ++i)
                                    remove |= std::pair<u32,u32>(std::minmax(f.vertices[i],
                                        f.vertices[(i+1)%f.vertices.size()])) == m_meshEdges[edge];
                    if (remove)
                        f = {};
                }
                if (ValidateBlockoutTopology(candidate, m_meshError))
                {
                    shape.editMesh = std::move(candidate);
                    shape.MarkDirty();
                    RebuildBlockoutEntity(*e);
                    CommitSceneHistoryImmediateChange(before);
                    m_meshSelection.clear();
                }
            }
            if (ImGui::MenuItem("Dissolve selected edge", nullptr, false,
                                m_meshSelectMode == 1 && m_meshSelection.size() == 1))
            {
                u32 id = m_meshSelection[0];
                auto before = CaptureSceneHistoryState();
                if (id < m_meshEdges.size() && DissolveBlockoutEdge(shape.editMesh, m_meshEdges[id].first,
                                                                    m_meshEdges[id].second, m_meshError))
                {
                    shape.MarkDirty();
                    RebuildBlockoutEntity(*e);
                    CommitSceneHistoryImmediateChange(before);
                    m_meshSelection.clear();
                }
            }
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset to Primitive"))
            ImGui::OpenPopup("ResetMeshConfirm");
        if (ImGui::BeginPopup("ResetMeshConfirm"))
        {
            ImGui::TextUnformatted("Discard direct mesh edits and restore the source primitive?");
            if (ImGui::Button("Reset"))
            {
                auto before = CaptureSceneHistoryState();
                auto &shape = e->GetComponent<BlockoutShape>();
                shape.meshEdited = false;
                shape.editMesh = {};
                shape.MarkDirty();
                RebuildBlockoutEntity(*e);
                ExitMeshEdit();
                CommitSceneHistoryImmediateChange(before);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        ImGui::EndDisabled();
        if (m_meshOperation)
        {
            ImGui::SameLine();
            if (ImGui::Button("Confirm"))
                FinishMeshOperation(true);
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                FinishMeshOperation(false);
        }
    }

    void Editor::DrawMeshEditViewport(bool hovered)
    {
        Entity *e = MeshEditEntity();
        if (!e)
            return;
        m_sceneViewClicked = false;
        const auto &mesh = e->GetComponent<BlockoutShape>().editMesh;
        Matrix4 model = e->GetComponent<Transform>().GetModelMatrix();
        Matrix4 vp = m_scene->GetEditorCamera3DProjection() * m_scene->GetEditorCamera3DView();
        auto project = [&](Vector3 p, ImVec2 &screen, float &depth) {
            Vector4 c = vp * model * Vector4(p, 1);
            if (c.w <= 0.0001f)
                return false;
            c /= c.w;
            depth = c.z;
            screen = {m_gameViewportPosX + (c.x * 0.5f + 0.5f) * m_gameViewportDrawWidth,
                      m_gameViewportPosY + (0.5f - c.y * 0.5f) * m_gameViewportDrawHeight};
            return true;
        };
        std::set<std::pair<u32, u32>> edges;
        for (const auto &f : mesh.faces)
            for (size_t i = 0; i < f.vertices.size(); ++i)
                edges.insert(std::minmax(f.vertices[i], f.vertices[(i + 1) % f.vertices.size()]));
        m_meshEdges.assign(edges.begin(), edges.end());
        ImVec2 mouse = ImGui::GetMousePos();
        // Pick faces by nearest ray/triangle intersection in object space.
        float nx = (mouse.x - m_gameViewportPosX) / m_gameViewportDrawWidth * 2 - 1;
        float ny = 1 - (mouse.y - m_gameViewportPosY) / m_gameViewportDrawHeight * 2;
        Matrix4 inverse = glm::inverse(vp * model);
        Vector4 p0 = inverse * Vector4(nx, ny, -1, 1), p1 = inverse * Vector4(nx, ny, 1, 1);
        Vector3 origin = Vector3(p0) / p0.w, dir = glm::normalize(Vector3(p1) / p1.w - origin);
        float nearest = 1e30f;
        int faceHit = -1;
        for (u32 id = 0; id < mesh.faces.size(); ++id)
        {
            const auto &f = mesh.faces[id];
            for (size_t i = 1; i + 1 < f.vertices.size(); ++i)
            {
                Vector3 a = mesh.vertices[f.vertices[0]], ab = mesh.vertices[f.vertices[i]] - a,
                        ac = mesh.vertices[f.vertices[i + 1]] - a;
                Vector3 h = glm::cross(dir, ac);
                float det = glm::dot(ab, h);
                if (std::abs(det) < 1e-7f)
                    continue;
                Vector3 s = origin - a;
                float u = glm::dot(s, h) / det;
                if (u < 0 || u > 1)
                    continue;
                Vector3 q = glm::cross(s, ab);
                float v = glm::dot(dir, q) / det;
                if (v < 0 || u + v > 1)
                    continue;
                float t = glm::dot(ac, q) / det;
                if (t > 0 && t < nearest)
                {
                    nearest = t;
                    faceHit = id;
                }
            }
        }
        std::set<u32> visibleVertices;
        if (faceHit >= 0)
            visibleVertices.insert(mesh.faces[faceHit].vertices.begin(), mesh.faces[faceHit].vertices.end());
        int edgeHit = -1, vertexHit = -1;
        float bestEdge = 10.0f, bestVertex = 9.0f;
        for (u32 id = 0; id < m_meshEdges.size(); ++id)
        {
            auto [a, b] = m_meshEdges[id];
            ImVec2 sa, sb;
            float z;
            if (!project(mesh.vertices[a], sa, z) || !project(mesh.vertices[b], sb, z))
                continue;
            Vector2 ab(sb.x - sa.x, sb.y - sa.y), am(mouse.x - sa.x, mouse.y - sa.y);
            float t = glm::clamp(glm::dot(am, ab) / std::max(glm::dot(ab, ab), 0.001f), 0.0f, 1.0f);
            float d = glm::length(am - ab * t);
            if (d < bestEdge && (faceHit < 0 || (visibleVertices.count(a) && visibleVertices.count(b))))
            {
                bestEdge = d;
                edgeHit = id;
            }
        }
        std::set<u32> used;
        for (auto [a, b] : m_meshEdges)
        {
            used.insert(a);
            used.insert(b);
        }
        for (u32 id : used)
        {
            ImVec2 p;
            float z;
            if (!project(mesh.vertices[id], p, z))
                continue;
            float d = glm::length(Vector2(mouse.x - p.x, mouse.y - p.y));
            if (d < bestVertex && (faceHit < 0 || visibleVertices.count(id)))
            {
                bestVertex = d;
                vertexHit = id;
            }
        }
        m_meshHover = m_meshSelectMode == 0 ? vertexHit : (m_meshSelectMode == 1 ? edgeHit : faceHit);
        if (edgeHit >= 0 && m_meshOperation != 5)
            m_meshLoopEdge = m_meshEdges[edgeHit];
        bool keys = (hovered || (m_meshOperation && m_sceneViewFocused)) && !ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive() &&
                    !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
                    !ImGui::IsMouseDown(ImGuiMouseButton_Right);
        if (!m_meshOperation && keys)
        {
            for (int i = 0; i < 3; ++i)
                if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_1 + i)))
                {
                    m_meshSelectMode = i;
                    m_meshSelection.clear();
                }
            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_R))
            {
                if (edgeHit >= 0)
                    BeginMeshOperation(5);
                else
                    m_meshError = "Hover an edge for a loop cut.";
            }
            else if (!ImGui::GetIO().KeyCtrl)
            {
                if (ImGui::IsKeyPressed(ImGuiKey_G))
                    BeginMeshOperation(1);
                if (ImGui::IsKeyPressed(ImGuiKey_R))
                    BeginMeshOperation(2);
                if (ImGui::IsKeyPressed(ImGuiKey_S))
                    BeginMeshOperation(3);
                if (ImGui::IsKeyPressed(ImGuiKey_E))
                    BeginMeshOperation(4);
                if (ImGui::IsKeyPressed(ImGuiKey_B))
                {
                    m_meshBoxSelect = true;
                    m_meshDragStart = {mouse.x, mouse.y};
                }
                if (!m_meshOperation && (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_X)))
                    m_meshDeleteRequested = true;
            }
        }
        std::set<u32> selectedVertices;
        for (u32 id : m_meshSelection)
        {
            if (m_meshSelectMode == 0 && id < mesh.vertices.size())
                selectedVertices.insert(id);
            if (m_meshSelectMode == 1 && id < m_meshEdges.size())
            {
                selectedVertices.insert(m_meshEdges[id].first);
                selectedVertices.insert(m_meshEdges[id].second);
            }
            if (m_meshSelectMode == 2 && id < mesh.faces.size())
                selectedVertices.insert(mesh.faces[id].vertices.begin(), mesh.faces[id].vertices.end());
        }
        if ((m_meshOperation == 0 || m_meshOperation == 6) && !selectedVertices.empty() && !m_meshBoxSelect)
        {
            Vector3 center(0);
            for (u32 id : selectedVertices)
                center += mesh.vertices[id];
            center /= float(selectedVertices.size());
            Matrix4 start = model * glm::translate(Matrix4(1), center);
            Matrix4 gizmo = m_meshOperation == 6 ? m_meshGizmoCurrent : start;
            auto view = m_scene->GetEditorCamera3DView();
            auto projection = m_scene->GetEditorCamera3DProjection();
            PrepareSceneViewGizmo();
            ImGuizmo::SetOrthographic(false);
            ImGuizmo::OPERATION op = m_meshGizmoMode == 0
                                         ? ImGuizmo::TRANSLATE
                                         : (m_meshGizmoMode == 1 ? ImGuizmo::ROTATE : ImGuizmo::SCALE);
            float snap = m_meshGizmoMode == 1 ? m_rotationSnapDegrees
                                              : (m_meshGizmoMode == 2 ? m_scaleSnap : m_translationSnap);
            float snaps[] = {snap, snap, snap};
            ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection), op, ImGuizmo::LOCAL,
                                 glm::value_ptr(gizmo), nullptr, m_gridSnappingEnabled ? snaps : nullptr);
            if (ImGuizmo::IsUsing())
            {
                if (!m_meshOperation)
                {
                    BeginMeshOperation(6);
                    m_meshGizmoStart = start;
                }
                m_meshGizmoCurrent = gizmo;
                m_meshPreview = m_meshBefore;
                Matrix4 delta = glm::inverse(model) * gizmo * glm::inverse(m_meshGizmoStart) * model;
                for (u32 id : selectedVertices)
                    m_meshPreview.vertices[id] = Vector3(delta * Vector4(m_meshBefore.vertices[id], 1));
                ValidateBlockoutTopology(m_meshPreview, m_meshError);
            }
            else if (m_meshOperation == 6)
            {
                if (m_meshError.empty())
                    FinishMeshOperation(true);
                else
                    FinishMeshOperation(false);
            }
            if (m_meshOperation == 6 &&
                (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)))
                FinishMeshOperation(false);
        }
        if (!m_meshOperation && !ImGuizmo::IsOver() && hovered &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !m_meshBoxSelect)
        {
            if (!ImGui::GetIO().KeyShift)
                m_meshSelection.clear();
            if (m_meshHover >= 0)
            {
                auto it = std::find(m_meshSelection.begin(), m_meshSelection.end(), u32(m_meshHover));
                if (it != m_meshSelection.end())
                    m_meshSelection.erase(it);
                else
                    m_meshSelection.push_back(m_meshHover);
            }
        }
        ImDrawList *draw = ImGui::GetWindowDrawList();
        draw->PushClipRect(
            {m_gameViewportPosX, m_gameViewportPosY},
            {m_gameViewportPosX + m_gameViewportDrawWidth, m_gameViewportPosY + m_gameViewportDrawHeight},
            true);
        if (m_meshBoxSelect)
        {
            Vector2 lo = glm::min(m_meshDragStart, Vector2(mouse.x, mouse.y)),
                    hi = glm::max(m_meshDragStart, Vector2(mouse.x, mouse.y));
            draw->AddRect({lo.x, lo.y}, {hi.x, hi.y}, IM_COL32(255, 180, 30, 255));
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                if (!ImGui::GetIO().KeyShift)
                    m_meshSelection.clear();
                size_t count = m_meshSelectMode == 0
                                   ? mesh.vertices.size()
                                   : (m_meshSelectMode == 1 ? m_meshEdges.size() : mesh.faces.size());
                for (u32 id = 0; id < count; ++id)
                {
                    std::vector<u32> ids;
                    if (m_meshSelectMode == 0)
                    {
                        if (!used.count(id))
                            continue;
                        ids = {id};
                    }
                    else if (m_meshSelectMode == 1)
                        ids = {m_meshEdges[id].first, m_meshEdges[id].second};
                    else
                        ids = mesh.faces[id].vertices;
                    bool inside = !ids.empty();
                    for (u32 v : ids)
                    {
                        ImVec2 p;
                        float z;
                        inside &= project(mesh.vertices[v], p, z) && p.x >= lo.x && p.x <= hi.x &&
                                  p.y >= lo.y && p.y <= hi.y;
                    }
                    if (inside && std::find(m_meshSelection.begin(), m_meshSelection.end(), id) ==
                                      m_meshSelection.end())
                        m_meshSelection.push_back(id);
                }
                m_meshBoxSelect = false;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                m_meshBoxSelect = false;
        }
        if (m_meshOperation && m_meshOperation != 6)
        {
            if (m_meshOperation == 5 && hovered && edgeHit >= 0 &&
                m_meshLoopEdge == std::pair<u32, u32>(0, 0))
                m_meshLoopEdge = m_meshEdges[edgeHit];
            if (keys)
            {
                const ImGuiKey axisKeys[] = {ImGuiKey_X, ImGuiKey_Y, ImGuiKey_Z};
                for (int axis = 0; axis < 3; ++axis)
                    if (!ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(axisKeys[axis], false))
                    {
                        const bool plane = ImGui::GetIO().KeyShift && m_meshOperation != 2;
                        if (m_meshAxis == axis && m_meshPlaneConstraint == plane)
                            m_meshAxis = -1;
                        else
                            m_meshAxis = axis;
                        m_meshPlaneConstraint = m_meshAxis >= 0 && plane;
                    }
                // Modeling uses key events because no text field owns SDL text input.
                for (int digit = 0; digit < 10; ++digit)
                    if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_0 + digit), false) ||
                        ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_Keypad0 + digit), false))
                        m_meshNumeric.push_back(char('0' + digit));
                if (ImGui::IsKeyPressed(ImGuiKey_Period, false) ||
                    ImGui::IsKeyPressed(ImGuiKey_KeypadDecimal, false))
                    m_meshNumeric.push_back('.');
                if (ImGui::IsKeyPressed(ImGuiKey_Minus, false) ||
                    ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract, false))
                    m_meshNumeric.push_back('-');
                if (ImGui::IsKeyPressed(ImGuiKey_Backspace) && !m_meshNumeric.empty())
                    m_meshNumeric.pop_back();
            }
            float delta = mouse.x - m_meshDragStart.x;
            m_meshAmount =
                m_meshNumeric.empty() ? delta * 0.02f : std::strtof(m_meshNumeric.c_str(), nullptr);
            if (m_meshOperation == 3 && m_meshNumeric.empty())
                m_meshAmount = std::max(0.01f, 1 + delta * 0.01f);
            if (m_meshOperation == 2 && m_meshNumeric.empty())
                m_meshAmount = delta * 0.5f;
            if (m_gridSnappingEnabled && m_meshNumeric.empty() && m_meshOperation != 5)
            {
                float snap = m_meshOperation == 2 ? m_rotationSnapDegrees
                                                  : (m_meshOperation == 3 ? m_scaleSnap : m_translationSnap);
                m_meshAmount = std::round(m_meshAmount / std::max(snap, 0.001f)) * snap;
            }
            m_meshPreview = m_meshBefore;
            m_meshError.clear();
            if (m_meshOperation == 5)
            {
                m_meshCutCount = std::clamp(m_meshCutCount + int(ImGui::GetIO().MouseWheel), 1, 32);
                CutBlockoutLoop(m_meshPreview, m_meshLoopEdge.first, m_meshLoopEdge.second, m_meshCutCount,
                                m_meshError);
            }
            else if (m_meshOperation == 4)
                ExtrudeBlockoutFaces(m_meshPreview, m_meshSelection, m_meshAmount, m_meshError);
            else
            {
                std::set<u32> ids;
                for (u32 id : m_meshSelection)
                {
                    if (m_meshSelectMode == 0 && id < m_meshBefore.vertices.size())
                        ids.insert(id);
                    if (m_meshSelectMode == 1 && id < m_meshEdges.size())
                    {
                        ids.insert(m_meshEdges[id].first);
                        ids.insert(m_meshEdges[id].second);
                    }
                    if (m_meshSelectMode == 2 && id < m_meshBefore.faces.size())
                        ids.insert(m_meshBefore.faces[id].vertices.begin(),
                                   m_meshBefore.faces[id].vertices.end());
                }
                Vector3 center(0);
                for (u32 id : ids)
                    center += m_meshBefore.vertices[id];
                if (!ids.empty())
                    center /= float(ids.size());
                Vector3 axis(0);
                axis[m_meshAxis < 0 ? 1 : m_meshAxis] = 1;
                Vector3 move(0);
                if (m_meshOperation == 1)
                {
                    const Matrix4 invView = glm::inverse(m_scene->GetEditorCamera3DView());
                    const Vector3 world = Vector3(invView[0]) * (mouse.x - m_meshDragStart.x) * 0.02f +
                        Vector3(invView[1]) * (m_meshDragStart.y - mouse.y) * 0.02f;
                    move = ConstrainEditorVector(Vector3(glm::inverse(model) * Vector4(world, 0)),
                                                 m_meshAxis, m_meshPlaneConstraint);
                    if (!m_meshNumeric.empty())
                    {
                        Vector3 direction = m_meshAxis >= 0 && !m_meshPlaneConstraint ? axis :
                            ConstrainEditorVector(Vector3(glm::inverse(model) * Vector4(invView[0])),
                                                  m_meshAxis, m_meshPlaneConstraint);
                        if (glm::length(direction) < 0.0001f)
                        {
                            direction = Vector3(0);
                            direction[m_meshAxis == 0 ? 1 : 0] = 1;
                        }
                        move = glm::normalize(direction) * m_meshAmount;
                    }
                    else if (m_gridSnappingEnabled)
                    {
                        const float snap = std::max(m_translationSnap, 0.001f);
                        move = glm::round(move / snap) * snap;
                    }
                    if (m_meshNumeric.empty())
                        m_meshAmount = m_meshAxis >= 0 && !m_meshPlaneConstraint
                            ? move[m_meshAxis] : glm::length(move);
                }
                for (u32 id : ids)
                {
                    Vector3 p = m_meshBefore.vertices[id] - center;
                    if (m_meshOperation == 1)
                        m_meshPreview.vertices[id] += move;
                    if (m_meshOperation == 2)
                        m_meshPreview.vertices[id] =
                            center + glm::angleAxis(m_meshAmount * 0.01745329252f, axis) * p;
                    if (m_meshOperation == 3)
                    {
                        p *= EditorScaleFactors(m_meshAmount, m_meshAxis, m_meshPlaneConstraint);
                        m_meshPreview.vertices[id] = center + p;
                    }
                }
                ValidateBlockoutTopology(m_meshPreview, m_meshError);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                FinishMeshOperation(false);
            else if ((keys && ImGui::IsKeyPressed(ImGuiKey_Enter)) ||
                     (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)))
                FinishMeshOperation(true);
        }
        if (m_meshOperation && m_meshError.empty())
        {
            // Render the preview without changing the serialized source or rebuilding
            // collision/navigation. Commit performs the collision update once.
            auto &shape = e->GetComponent<BlockoutShape>();
            std::swap(shape.editMesh, m_meshPreview);
            RebuildBlockoutEntity(*e, false);
            std::swap(shape.editMesh, m_meshPreview);
        }
        const auto &shown = m_meshOperation ? m_meshPreview : mesh;
        for (u32 id = 0; id < shown.faces.size(); ++id)
        {
            const auto &f = shown.faces[id];
            std::vector<ImVec2> points;
            bool valid = !f.vertices.empty();
            for (u32 v : f.vertices)
            {
                ImVec2 p;
                float z;
                if (!project(shown.vertices[v], p, z))
                {
                    valid = false;
                    break;
                }
                points.push_back(p);
            }
            if (!valid)
                continue;
            bool sel = m_meshSelectMode == 2 &&
                       std::find(m_meshSelection.begin(), m_meshSelection.end(), id) != m_meshSelection.end();
            if (sel || (m_meshSelectMode == 2 && int(id) == m_meshHover && hovered))
                draw->AddConvexPolyFilled(points.data(), int(points.size()),
                                          sel ? IM_COL32(255, 135, 20, 65) : IM_COL32(255, 240, 80, 60));
            ImU32 color = m_meshOperation ? (m_meshError.empty() ? IM_COL32(80, 200, 255, 255)
                                                                 : IM_COL32(255, 70, 70, 255))
                                          : IM_COL32(170, 180, 195, 180);
            for (size_t i = 0; i < points.size(); ++i)
                draw->AddLine(points[i], points[(i + 1) % points.size()], color, 1.3f);
        }
        if (!m_meshOperation && m_meshSelectMode != 2)
        {
            size_t count = m_meshSelectMode == 0 ? mesh.vertices.size() : m_meshEdges.size();
            for (u32 id = 0; id < count; ++id)
            {
                bool sel =
                    std::find(m_meshSelection.begin(), m_meshSelection.end(), id) != m_meshSelection.end();
                ImU32 color = sel ? IM_COL32(255, 140, 20, 255)
                                  : (int(id) == m_meshHover && hovered ? IM_COL32(255, 240, 70, 255)
                                                                       : IM_COL32(185, 190, 200, 230));
                ImVec2 a, b;
                float z;
                if (m_meshSelectMode == 0 && used.count(id) && project(mesh.vertices[id], a, z))
                    draw->AddCircleFilled(a, sel ? 4 : 3, color);
                if (m_meshSelectMode == 1 && project(mesh.vertices[m_meshEdges[id].first], a, z) &&
                    project(mesh.vertices[m_meshEdges[id].second], b, z))
                    draw->AddLine(a, b, color, sel ? 3 : 1);
            }
        }
        std::string help =
            m_meshOperation
                ? "Confirm: Enter / click | Cancel: Esc / right-click | X Y Z axis | Shift+axis plane | Type a value"
                : "Tab exit | 1 vertex 2 edge 3 face | G R S | E extrude | Ctrl+R loop | B box select";
        if (m_meshOperation && m_meshAxis >= 0)
        {
            const char* axes[] = {"X", "Y", "Z"};
            const char* planes[] = {"YZ", "XZ", "XY"};
            help += std::string(" | Local ") +
                (m_meshPlaneConstraint ? planes[m_meshAxis] : axes[m_meshAxis]);
        }
        if (m_meshOperation == 5)
            help = "Loop cut: " + std::to_string(m_meshCutCount) +
                   " | Wheel: count | Click: confirm | Esc: cancel";
        if (m_meshOperation == 6)
            help = "Drag handle | Release: confirm | Esc / right-click: cancel";
        if (!m_meshError.empty())
            help = m_meshError;
        const float helpWidth = std::max(100.0f, float(m_gameViewportWidth) - 20.0f);
        draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                      {m_gameViewportPosX + 10, m_gameViewportPosY + 10},
                      IM_COL32(255, 230, 150, 255), help.c_str(), nullptr, helpWidth);
        if (m_meshOperation && m_meshOperation != 5 && m_meshOperation != 6)
            draw->AddText({m_gameViewportPosX + 10,
                           m_gameViewportPosY + 14 + ImGui::CalcTextSize(help.c_str(), nullptr, false, helpWidth).y},
                          IM_COL32(255, 255, 255, 255),
                          std::to_string(m_meshAmount).c_str());
        draw->PopClipRect();
    }
} // namespace Canis
