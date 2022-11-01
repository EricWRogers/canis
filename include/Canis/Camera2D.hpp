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
            _position = newPosition;
            _needsMatrixUpdate = true;
        }
        void SetScale(float newScale)
        {
            _scale = newScale;
            _needsMatrixUpdate = true;
        }

        glm::vec2 GetPosition() { return _position; }
        glm::mat4 GetCameraMatrix() { return _cameraMatrix; }
        float GetScale() { return _scale; }

    private:
        int _screenWidth, _screenHeight;
        bool _needsMatrixUpdate;
        float _scale;
        glm::vec2 _position;
        glm::mat4 _cameraMatrix;
        glm::mat4 _orthoMatrix;
    };
} // end of Canis namespace