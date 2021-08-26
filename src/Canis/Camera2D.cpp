#include "Camera2D.hpp"

namespace Canis
{
    Camera2D::Camera2D() : _cameraMatrix(1.0f),
                           _orthoMatrix(1.0f),
                           _position(0.0f, 0.0f),
                           _scale(1.0f),
                           _needsMatrixUpdate(true),
                           _screenWidth(500),
                           _screenHeight(500)
    {
    }

    Camera2D::~Camera2D()
    {
    }

    void Camera2D::init(int screenWidth, int screenHeight)
    {
        _screenWidth = screenWidth;
        _screenHeight = screenHeight;
        _orthoMatrix = glm::ortho(0.0f, (float)_screenWidth, 0.0f, (float)_screenHeight);
        setPosition(glm::vec2((float)_screenWidth / 2, (float)_screenHeight / 2));
    }

    void Camera2D::update()
    {
        if (_needsMatrixUpdate)
        {
            glm::vec3 translate(-_position.x + _screenWidth / 2, -_position.y + _screenHeight / 2, 0.0f);
            _cameraMatrix = glm::translate(_orthoMatrix, translate);
            glm::vec3 scale(_scale, _scale, 0.0f);
            _cameraMatrix = glm::scale(_cameraMatrix, scale);

            _needsMatrixUpdate = false;
        }
    }
} // end of Canis namespace