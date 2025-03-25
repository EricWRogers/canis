#include <Canis/Camera2D.hpp>

namespace Canis
{
    Camera2D::Camera2D() : m_cameraMatrix(1.0f),
                            m_view(1.0f),
                            m_projection(1.0f),
                           m_position(0.0f, 0.0f),
                           m_scale(1.0f),
                           m_needsMatrixUpdate(true),
                           m_screenWidth(500),
                           m_screenHeight(500)
    {
    }

    Camera2D::~Camera2D()
    {
    }

    void Camera2D::Init(int screenWidth, int screenHeight)
    {
        m_screenWidth = screenWidth;
        m_screenHeight = screenHeight;
        m_projection = glm::ortho(0.0f, (float)m_screenWidth, 0.0f, (float)m_screenHeight);
        SetPosition(glm::vec2((float)m_screenWidth / 2, (float)m_screenHeight / 2));
    }

    void Camera2D::Update()
    {
        if (m_needsMatrixUpdate)
        {
            m_view = glm::mat4(1.0f);
            m_view = glm::translate(m_view, glm::vec3(-m_position.x + m_screenWidth / 2, -m_position.y + m_screenHeight / 2, 0.0f));
            m_view = glm::scale(m_view, glm::vec3(m_scale, m_scale, 0.0f));

            m_cameraMatrix = m_projection * m_view;

            m_needsMatrixUpdate = false;
        }
    }
} // end of Canis namespace