#include "../src/EditorGizmo.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

int main()
{
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(800, 600);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    glm::mat4 matrix(1);
    auto view = glm::lookAt(glm::vec3(0, 0, 5), glm::vec3(0), glm::vec3(0, 1, 0));
    auto projection = glm::perspective(glm::radians(60.0f), 800.0f / 600.0f, 0.1f, 100.0f);
    auto frame = [&](float x, float y, bool down) {
        io.AddMousePosEvent(x, y);
        io.AddMouseButtonEvent(0, down);
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
        // The Scene image remains the last item before the mesh gizmo is drawn.
        ImGui::Image((ImTextureID)1, ImVec2(800, 600));
        Canis::PrepareViewportGizmo(0, 0, 800, 600);
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection),
            ImGuizmo::TRANSLATE, ImGuizmo::LOCAL, glm::value_ptr(matrix));
        const bool usingGizmo = ImGuizmo::IsUsing();
        ImGui::End();
        ImGui::Render();
        return usingGizmo;
    };
    frame(430, 300, false);
    frame(430, 300, false);
    const bool grabbed = frame(430, 300, true);
    frame(460, 300, true);
    frame(460, 300, false);
    const bool moved = matrix[3].x > 0.1f && std::abs(matrix[3].y) < 0.001f;
    ImGui::DestroyContext();
    if (!grabbed || !moved)
    {
        std::cerr << "Scene gizmo did not grab and translate: " << grabbed << ", " << matrix[3].x << '\n';
        return 1;
    }
    std::cout << "Scene gizmo mouse drag passed.\n";
}
