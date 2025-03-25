#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Canis
{
    class Camera2D
    {
    public:
        Camera2D();
        ~Camera2D();

        void Init(int screenWidth, int screenHeight);

        void Update();

        void SetPosition(const glm::vec2 &newPosition)
        {
            m_position = newPosition;
            m_needsMatrixUpdate = true;
        }
        void SetScale(float newScale)
        {
            m_scale = newScale;
            m_needsMatrixUpdate = true;
        }

        glm::vec2 GetPosition() { return m_position; }
        glm::mat4 GetCameraMatrix() { return m_cameraMatrix; }
        glm::mat4 GetViewMatrix() { return m_view; }
        glm::mat4 GetProjectionMatrix() { return m_projection; }
        float GetScale() { return m_scale; }

    private:
        int m_screenWidth, m_screenHeight;
        bool m_needsMatrixUpdate;
        float m_scale;
        glm::vec2 m_position;
        glm::mat4 m_cameraMatrix;
        glm::mat4 m_view;
        glm::mat4 m_projection;
    };
} // end of Canis namespace