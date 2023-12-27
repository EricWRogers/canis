#include <Canis/Math.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/ext.hpp>
#include <glm/gtx/quaternion.hpp>

#include <Canis/Scene.hpp>
#include <Canis/Camera.hpp>
#include <Canis/Window.hpp>
#include <Canis/InputManager.hpp>

#include <Canis/ECS/Components/TransformComponent.hpp>

namespace Canis
{
    Ray RayFromMouse(Camera &camera, Window &window, InputManager &inputManager)
    {
        glm::vec4 lRayStart_NDC(
            ((float)inputManager.mouse.x / (float)window.GetScreenWidth() - 0.5f) * 2.0f,
            ((float)inputManager.mouse.y / (float)window.GetScreenHeight() - 0.5f) * 2.0f,
            -1.0, // The near plane maps to Z=-1 in Normalized Device Coordinates
            1.0f);
        glm::vec4 lRayEnd_NDC(
            ((float)inputManager.mouse.x / (float)window.GetScreenWidth() - 0.5f) * 2.0f,
            ((float)inputManager.mouse.y / (float)window.GetScreenHeight() - 0.5f) * 2.0f,
            0.0,
            1.0f);

        glm::mat4 projection = glm::perspective(camera.FOV, (float)window.GetScreenWidth() / (float)window.GetScreenHeight(), 0.1f, 100.0f);

        glm::mat4 M = glm::inverse(projection * camera.GetViewMatrix());
        glm::vec4 lRayStart_world = M * lRayStart_NDC;
        lRayStart_world /= lRayStart_world.w;
        glm::vec4 lRayEnd_world = M * lRayEnd_NDC;
        lRayEnd_world /= lRayEnd_world.w;

        glm::vec3 lRayDir_world(lRayEnd_world - lRayStart_world);
        lRayDir_world = glm::normalize(lRayDir_world);

        return Ray{lRayStart_world, lRayDir_world};
    }

    glm::vec2 WorldToScreenSpace(Camera &_camera, Window &_window, InputManager &_inputManager, glm::vec3 _position)
    {
        // Calculate the projection matrix
        glm::mat4 projectionMatrix = glm::perspective(_camera.FOV, _window.GetScreenWidth() / (float)_window.GetScreenHeight(), _camera.nearPlane, _camera.farPlane);

        // Convert world space to screen space
        glm::vec4 viewSpacePoint = _camera.GetViewMatrix() * glm::vec4(_position, 1.0f);
        glm::vec4 clipSpacePoint = projectionMatrix * viewSpacePoint;

        // Normalize to NDC
        clipSpacePoint /= clipSpacePoint.w;

        // Convert NDC to screen coordinates
        return glm::vec2((clipSpacePoint.x + 1.0f) * 0.5f * _window.GetScreenWidth(), _window.GetScreenHeight() - ((1.0f - clipSpacePoint.y) * 0.5f * _window.GetScreenHeight()));
    }

    bool HitSphere(glm::vec3 center, float radius, Ray ray)
    {
        glm::vec3 oc = ray.origin - center;
        float a = glm::dot(ray.direction, ray.direction);
        float b = 2.0f * glm::dot(oc, ray.direction);
        float c = glm::dot(oc, oc) - radius * radius;
        float discriminant = b * b - 4 * a * c;
        return (discriminant > 0);
    }

    glm::quat RotationBetweenVectors(glm::vec3 start, glm::vec3 dest)
    {
        start = glm::normalize(start);
        dest = glm::normalize(dest);

        float cosTheta = glm::dot(start, dest);
        glm::vec3 rotationAxis;

        if (cosTheta < -1 + 0.001f)
        {
            rotationAxis = glm::cross(glm::vec3(0.0f, 0.0f, 1.0f), start);
            float normalLength2 = glm::length2(glm::normalize(rotationAxis));

            if (normalLength2 < 0.01) // parallel
            {
                rotationAxis = glm::cross(glm::vec3(1.0f, 0.0f, 0.0f), start);
            }

            rotationAxis = glm::normalize(rotationAxis);
            return glm::angleAxis(glm::radians(180.0f), rotationAxis);
        }

        rotationAxis = glm::cross(start, dest);

        float s = sqrt((1 + cosTheta) * 2);
        float invs = 1 / s;

        return glm::quat(
            s * 0.5f,
            rotationAxis.x * invs,
            rotationAxis.y * invs,
            rotationAxis.z * invs);
    }

    glm::vec3 RotateTowardsTarget(TransformComponent &_transform, const glm::vec3 &_target, float _rotationSpeed, float _deltaTime)
    {
        const glm::mat4 transformX = glm::rotate(glm::mat4(1.0f),
                                                 glm::radians(_transform.rotation.x),
                                                 glm::vec3(1.0f, 0.0f, 0.0f));
        const glm::mat4 transformY = glm::rotate(glm::mat4(1.0f),
                                                 glm::radians(_transform.rotation.y),
                                                 glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::mat4 transformZ = glm::rotate(glm::mat4(1.0f),
                                                 glm::radians(_transform.rotation.z),
                                                 glm::vec3(0.0f, 0.0f, 1.0f));

        // Y * X * Z
        const glm::mat4 roationMatrix = transformY * transformX * transformZ;

        // Calculate the direction towards the target
        glm::vec3 currentForward = glm::normalize(glm::vec3(roationMatrix[2]));
        glm::vec3 targetDirection = glm::normalize(_target - glm::vec3(roationMatrix[3]));

        // Calculate the rotation axis
        glm::vec3 rotationAxis = glm::cross(currentForward, targetDirection);

        // Calculate the rotation angle
        float rotationAngle = _rotationSpeed * _deltaTime;

        // Create a quaternion for the rotation
        glm::quat rotationQuat = glm::angleAxis(rotationAngle, rotationAxis);
        return glm::degrees(glm::eulerAngles(rotationQuat));
    }

    void UpdateModelMatrix(TransformComponent &_transform)
    {
        _transform.isDirty = false;

        if (_transform.registry == nullptr)
        {
            Canis::Warning("Transform registry MISSING");
            return;
        }

        const glm::mat4 transformX = glm::rotate(glm::mat4(1.0f),
                                                 glm::radians(_transform.rotation.x),
                                                 glm::vec3(1.0f, 0.0f, 0.0f));
        const glm::mat4 transformY = glm::rotate(glm::mat4(1.0f),
                                                 glm::radians(_transform.rotation.y),
                                                 glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::mat4 transformZ = glm::rotate(glm::mat4(1.0f),
                                                 glm::radians(_transform.rotation.z),
                                                 glm::vec3(0.0f, 0.0f, 1.0f));

        // Y * X * Z
        const glm::mat4 roationMatrix = transformY * transformX * transformZ;

        // translation * rotation * scale (also know as TRS matrix)
        _transform.modelMatrix = glm::translate(glm::mat4(1.0f), _transform.position) *
                                 roationMatrix *
                                 glm::scale(glm::mat4(1.0f), _transform.scale);

        /*glm::mat4 new_transform = glm::mat4(1);
        new_transform = glm::translate(new_transform, _transform.position);
        new_transform = glm::rotate(new_transform, glm::radians(_transform.rotation.x), glm::vec3(1, 0, 0));
        new_transform = glm::rotate(new_transform, glm::radians(_transform.rotation.y), glm::vec3(0, 1, 0));
        new_transform = glm::rotate(new_transform, glm::radians(_transform.rotation.z), glm::vec3(0, 0, 1));
        _transform.modelMatrix = glm::scale(new_transform, _transform.scale);*/

        if (_transform.parent != entt::null)
        {
            TransformComponent &parentTransform = _transform.registry->get<TransformComponent>(_transform.parent);

            _transform.modelMatrix = parentTransform.modelMatrix * _transform.modelMatrix;
        }

        for (int i = 0; i < _transform.children.size(); i++)
        {
            TransformComponent &childTransform = _transform.registry->get<TransformComponent>(_transform.children[i]);

            UpdateModelMatrix(childTransform);
        }
    }

    const glm::mat4 &GetModelMatrix(TransformComponent &_transform)
    {
        return _transform.modelMatrix;
    }

    glm::vec3 GetGlobalPosition(TransformComponent &_transform)
    {
        return glm::vec3(_transform.modelMatrix[3]);
    }

    void MoveTransformPosition(TransformComponent &_transform, glm::vec3 _offset)
    {
        _transform.position += _offset;

        UpdateModelMatrix(_transform);
    }

    void SetTransformPosition(TransformComponent &_transform, glm::vec3 _position)
    {
        _transform.position = _position;

        UpdateModelMatrix(_transform);
    }

    glm::vec3 GetTransformForward(TransformComponent &_transform)
    {
        glm::mat4 rx = glm::rotate(glm::mat4(1.0f), glm::radians(_transform.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        glm::mat4 ry = glm::rotate(glm::mat4(1.0f), glm::radians(_transform.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 rz = glm::rotate(glm::mat4(1.0f), glm::radians(_transform.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

        glm::mat3 rotationMatrix = glm::mat3(rz * ry * rx);

        return glm::normalize(glm::column(rotationMatrix, 2));
    }

    void Rotate(TransformComponent &_transform, glm::vec3 _rotate)
    {
        _transform.rotation += _rotate;

        UpdateModelMatrix(_transform);
    }

    void SetTransformRotation(TransformComponent &_transform, glm::vec3 _rotation)
    {
        _transform.rotation = _rotation;

        UpdateModelMatrix(_transform);
    }

    void SetTransformScale(TransformComponent &_transform, glm::vec3 _scale)
    {
        _transform.scale = _scale;

        UpdateModelMatrix(_transform);
    }

    void LookAt(TransformComponent &_transform, glm::vec3 _target)
    {
        glm::vec3 direction = glm::normalize(_target - _transform.position);

        _transform.rotation.y = atan2(direction.x, direction.z);

        if (direction.z >= 0.0f)
            _transform.rotation.x = -atan2(direction.y * cos(_transform.rotation.y), direction.z);
        else
            _transform.rotation.x = atan2(direction.y * cos(_transform.rotation.y), -direction.z);

        _transform.rotation.z = atan2(cos(_transform.rotation.x), sin(_transform.rotation.x) * sin(_transform.rotation.y));

        _transform.rotation = glm::degrees(_transform.rotation);

        _transform.rotation.z -= 90;

        UpdateModelMatrix(_transform);
    }

    float RandomFloat(float min, float max)
    {
        if (max > min)
        {
            float random = ((float)rand()) / (float)RAND_MAX;
            float range = max - min;
            return (random * range) + min;
        }

        if (min == max)
            return min;

        return RandomFloat(max, min);
    }

    glm::vec4 HexToRGBA(unsigned int _RRGGBBAA)
    {
        glm::vec4 color = glm::vec4(0.0f);
        color.r = ((_RRGGBBAA >> 24) & 0xFF) / 255.0f;
        color.g = ((_RRGGBBAA >> 16) & 0xFF) / 255.0f;
        color.b = ((_RRGGBBAA >> 8) & 0xFF) / 255.0f;
        color.a = ((_RRGGBBAA) & 0xFF) / 255.0f;
        return color;
    }

    glm::vec4 UIntToRGBA(unsigned int _red, unsigned int _green, unsigned int _blue, unsigned int _alpha)
    {
        glm::vec4 color = glm::vec4(0.0f);
        color.r = _red / 255.0f;
        color.g = _green / 255.0f;
        color.b = _blue / 255.0f;
        color.a = _alpha / 255.0f;
        return color;
    }

    void Lerp(float &_value, const float &_min, const float &_max, const float &_fraction)
    {
        _value = _min + _fraction * (_max - _min);
    }

    void Lerp(glm::vec3 &_value, const glm::vec3 &_min, const glm::vec3 &_max, const float &_fraction)
    {
        _value = _min + _fraction * (_max - _min);
    }

    void Lerp(glm::vec4 &_value, const glm::vec4 &_min, const glm::vec4 &_max, const float &_fraction)
    {
        _value = _min + _fraction * (_max - _min);
    }

    void AnimationBellCurve(float &_value, const float &_min, const float &_max, const float &_fraction)
    {
        _value = _min + ((sin(2 * Canis::PI * (_fraction - 1/4)) + 1) / 2) * (_max - _min);
    }

    void Clamp(float &_value, float _min, float _max)
    {
        if (_value < _min)
        {
            _value = _min;
        }

        if (_value > _max)
        {
            _value = _max;
        }
    }

    void RotatePoint(glm::vec2 &_point, const float &_cosAngle, const float &_sinAngle)
    {
        float x = _point.x;
        float y = _point.y;
        _point.x = x * _cosAngle - y * _sinAngle;
        _point.y = x * _sinAngle + y * _cosAngle;
    }

    void RotatePointAroundPivot(glm::vec2 &_point, const glm::vec2 &_pivot, float _radian)
    {
        float s = sin(-_radian);
        float c = cos(-_radian);

        glm::vec2 holder = _point;

        holder -= _pivot;

        _point.x = holder.x * c - holder.y * s;
        _point.y = holder.x * s + holder.y * c;

        _point += _pivot;
    }
}