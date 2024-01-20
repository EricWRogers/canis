#include <Canis/Math.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/ext.hpp>

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
        start = normalize(start);
        dest = normalize(dest);

        float cosTheta = dot(start, dest);
        glm::vec3 rotationAxis;

        if (cosTheta < -1 + 0.001f)
        {
            // special case when vectors in opposite directions:
            // there is no "ideal" rotation axis
            // So guess one; any will do as long as it's perpendicular to start
            rotationAxis = glm::cross(glm::vec3(0.0f, 0.0f, 1.0f), start);
            if (glm::length2(rotationAxis) < 0.01) // bad luck, they were parallel, try again!
                rotationAxis = glm::cross(glm::vec3(1.0f, 0.0f, 0.0f), start);

            rotationAxis = glm::normalize(rotationAxis);
            return glm::angleAxis(glm::radians(180.0f), rotationAxis);
        }

        rotationAxis = cross(start, dest);

        float s = sqrt((1 + cosTheta) * 2);
        float invs = 1 / s;

        return glm::quat(
            s * 0.5f,
            rotationAxis.x * invs,
            rotationAxis.y * invs,
            rotationAxis.z * invs);
    }

    void UpdateModelMatrix(TransformComponent &_transform)
    {
        _transform.isDirty = false;

        if (_transform.registry == nullptr)
        {
            Canis::Warning("Transform registry MISSING");
            return;
        }

        // Y * X * Z
        const glm::mat4 roationMatrix = glm::toMat4(_transform.rotation);

        // translation * rotation * scale (also know as TRS matrix)
        _transform.modelMatrix = glm::translate(glm::mat4(1.0f), _transform.position) *
                                 roationMatrix *
                                 glm::scale(glm::mat4(1.0f), _transform.scale);

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

    glm::vec3 GetGlobalRotation(TransformComponent &_transform)
    {
        return glm::vec3(_transform.modelMatrix[3]);
    }

    glm::vec3 GetGlobalScale(TransformComponent &_transform)
    {
        glm::vec3 size;
        size.x = glm::length(glm::vec3(_transform.modelMatrix[0])); // Basis vector X
        size.y = glm::length(glm::vec3(_transform.modelMatrix[1])); // Basis vector Y
        size.z = glm::length(glm::vec3(_transform.modelMatrix[2])); // Basis vector Z
        return size;
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
        glm::mat3 rotationMatrix = glm::toMat3(_transform.rotation);

        return glm::normalize(glm::column(rotationMatrix, 2));
    }

    void Rotate(TransformComponent &_transform, glm::vec3 _rotate)
    {
        _transform.rotation = glm::quat(_rotate) * _transform.rotation;

        UpdateModelMatrix(_transform);
    }

    void SetTransformRotation(TransformComponent &_transform, glm::vec3 _rotation)
    {
        _transform.rotation = glm::quat(_rotation);

        UpdateModelMatrix(_transform);
    }

    void SetTransformRotation(TransformComponent &_transform, glm::quat _rotation)
    {
        _transform.rotation = _rotation;

        UpdateModelMatrix(_transform);
    }

    void SetTransformScale(TransformComponent &_transform, glm::vec3 _scale)
    {
        _transform.scale = _scale;

        UpdateModelMatrix(_transform);
    }

    void LookAt(TransformComponent &_transform, glm::vec3 _target, glm::vec3 _up)
    {
        glm::vec3 direction = glm::normalize(_target - _transform.position);
        _transform.rotation = glm::quatLookAt(direction, _up);
        UpdateModelMatrix(_transform);
    }

    // Like SLERP, but forbids rotation greater than maxAngle (in radians)
    // In conjunction to LookAt, can make your characters
    glm::quat RotateTowards(glm::quat _q1, glm::quat _q2, float _maxAngle)
    {

        if (_maxAngle < 0.001f)
        {
            // No rotation allowed. Prevent dividing by 0 later.
            return _q1;
        }

        float cosTheta = dot(_q1, _q2);

        // q1 and q2 are already equal.
        // Force q2 just to be sure
        if (cosTheta > 0.9999f)
        {
            return _q2;
        }

        // Avoid taking the long path around the sphere
        if (cosTheta < 0)
        {
            _q1 = _q1 * -1.0f;
            cosTheta *= -1.0f;
        }

        float angle = acos(cosTheta);

        // If there is only a 2° difference, and we are allowed 5°,
        // then we arrived.
        if (angle < _maxAngle)
        {
            return _q2;
        }

        // This is just like slerp(), but with a custom t
        float t = _maxAngle / angle;
        angle = _maxAngle;

        glm::quat res = (glm::sin((1.0f - t) * angle) * _q1 + glm::sin(t * angle) * _q2) / glm::sin(angle);
        res = normalize(res);
        return res;
    }

    void RotateTowardsLookAt(TransformComponent &_transform, glm::vec3 _target, glm::vec3 _up, float _maxAngle)
    {
        glm::vec3 direction = glm::normalize(_target - _transform.position);
        glm::quat targetQuat = glm::quatLookAt(direction, _up);
        _transform.rotation = RotateTowards(_transform.rotation, targetQuat, _maxAngle);
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
        _value = _min + sin(Canis::PI * _fraction) * (_max - _min);
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