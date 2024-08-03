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
#include <Canis/AssetManager.hpp>

#include <Canis/ECS/Components/MeshComponent.hpp>
#include <Canis/ECS/Components/TransformComponent.hpp>

namespace Canis
{
    bool IntersectRayTriangle(
            const Ray& _ray,
            const Vertex& _vertex0,
            const Vertex& _vertex1,
            const Vertex& _vertex2,
            float& _t) {
        const float EPSILON = 0.00000001f;
        vec3 edge1, edge2, h, s, q;
        float a,f,u,v;
        edge1 = _vertex1.position - _vertex0.position;
        edge2 = _vertex2.position - _vertex0.position;
        h = cross(_ray.direction, edge2);
        a = dot(edge1, h);
        if (a > -EPSILON && a < EPSILON)
            return false;    // This ray is parallel to this triangle.
        f = 1.0/a;
        s = _ray.origin - _vertex0.position;
        u = f * dot(s, h);
        if (u < 0.0 || u > 1.0)
            return false;
        q = cross(s, edge1);
        v = f * dot(_ray.direction, q);
        if (v < 0.0 || u + v > 1.0)
            return false;
        // At this stage we can compute t to find out where the intersection point is on the line.
        _t = f * dot(edge2, q);
        if (_t > EPSILON) // ray intersection
            return true;
        else // This means that there is a line intersection but not a ray intersection.
            return false;
    }

    bool FindRayMeshIntersection(Entity _entity, Ray _ray, Hit &_hit) {
        TransformComponent& transform = _entity.GetComponent<TransformComponent>();
        MeshComponent& mesh = _entity.GetComponent<MeshComponent>();
        ModelAsset& model = *AssetManager::Get<ModelAsset>(mesh.modelHandle.id);

        float min_t = std::numeric_limits<float>::max();
        bool hit = false;

        glm::mat4 inverseModelMatrix = glm::inverse(transform.modelMatrix);

        // Transform the ray into local space
        _ray.origin = inverseModelMatrix * vec4(_ray.origin, 1.0f);
        _ray.direction = glm::mat3(inverseModelMatrix) * _ray.direction;

        for (size_t i = 0; i < model.indices.size(); i += 3) {
            float t = 0;
            if (IntersectRayTriangle(
                _ray,
                model.vertices[model.indices[i]],
                model.vertices[model.indices[i+1]],
                model.vertices[model.indices[i+2]],
                 t)) {
                if (t < min_t) {
                    min_t = t;
                    hit = true;
                }
            }
        }

        if (hit) {
            _hit.position = _ray.origin + _ray.direction * min_t;
            _hit.position = transform.modelMatrix * vec4(_hit.position, 1.0f);
            _hit.entity = _entity;
        }

        return hit;
    }

    bool CheckRay(Entity _entity, Ray _ray, Hit &_hit)
    {
        TransformComponent& transform = _entity.GetComponent<TransformComponent>();
        MeshComponent& mesh = _entity.GetComponent<MeshComponent>();
        ModelAsset& model = *AssetManager::Get<ModelAsset>(mesh.modelHandle.id);

        bool didHit = false;
        float closestDistance = 0.0f;

        glm::mat4 inverseModelMatrix = glm::inverse(transform.modelMatrix);

        // Transform the ray into local space
        _ray.origin = inverseModelMatrix * vec4(_ray.origin, 1.0f);
        _ray.direction = normalize(mat3(inverseModelMatrix) * _ray.direction);

        for (size_t i = 0; i < model.indices.size(); i += 3)
        {
            // Assuming vertices are organized as triangles (groups of three)
            const glm::vec3 v0(model.vertices[model.indices[i]].position);
            const glm::vec3 v1(model.vertices[model.indices[i+1]].position);
            const glm::vec3 v2(model.vertices[model.indices[i+2]].position);

            // Calculate the normal of the triangle
            glm::vec3 normal = glm::cross(v1 - v0, v2 - v0);//model.vertices[model.indices[i]].normal);//glm::cross(v1 - v0, v2 - v0);

            // Check if the ray and the triangle are parallel
            float dotNormalRay = glm::dot(normal, _ray.direction);
            if (std::abs(dotNormalRay) < 1e-9)
            {
                continue; // Ray and triangle are parallel, no intersection
            }

            // Calculate the distance along the ray to the point of intersection with the triangle
            float t = glm::dot(v0 - _ray.origin, normal) / dotNormalRay;

            // Check if the point of intersection is in front of the ray's origin
            if (t < 0.0f)
            {
                continue; // Intersection point is behind the ray's origin
            }

            // Calculate the point of intersection with the triangle
            glm::vec3 intersectionPoint = _ray.origin + t * _ray.direction;

            // Check if the intersection point is inside the triangle
            glm::vec3 edge0 = v1 - v0;
            glm::vec3 edge1 = v2 - v1;
            glm::vec3 edge2 = v0 - v2;

            glm::vec3 c0 = glm::cross(v0 - intersectionPoint, edge0);
            glm::vec3 c1 = glm::cross(v1 - intersectionPoint, edge1);
            glm::vec3 c2 = glm::cross(v2 - intersectionPoint, edge2);

            if (glm::dot(normal, c0) >= 0 && glm::dot(normal, c1) >= 0 && glm::dot(normal, c2) >= 0)
            {
                if (didHit == false)
                {
                    _hit.position = intersectionPoint;
                    closestDistance = distance(_ray.origin, _hit.position);
                }
                else
                {
                    if (closestDistance > distance(_ray.origin, intersectionPoint))
                    {
                        _hit.position = intersectionPoint;
                        closestDistance = distance(_ray.origin, intersectionPoint);
                    }
                }
                didHit = true;
            }
        }

        if (didHit)
        {
            _hit.position = transform.modelMatrix * vec4(_hit.position, 1.0f);
            _hit.entity = _entity;
        }

        return didHit;
    }

    Ray RayFromScreen(Camera &_camera, Window &_window, const glm::vec2 &_screenPoint)
    {
        vec4 lRayStart_NDC(
            (_screenPoint.x / (float)_window.GetScreenWidth() - 0.5f) * 2.0f,
            (_screenPoint.y / (float)_window.GetScreenHeight() - 0.5f) * 2.0f,
            -1.0, // The near plane maps to Z=-1 in Normalized Device Coordinates
            1.0f);
        vec4 lRayEnd_NDC(
            (_screenPoint.x / (float)_window.GetScreenWidth() - 0.5f) * 2.0f,
            (_screenPoint.y / (float)_window.GetScreenHeight() - 0.5f) * 2.0f,
            0.0,
            1.0f);

        mat4 projection = perspective(_camera.FOV, (float)_window.GetScreenWidth() / (float)_window.GetScreenHeight(), _camera.nearPlane, _camera.farPlane);

        mat4 M = inverse(projection * _camera.GetViewMatrix());
        vec4 lRayStart_world = M * lRayStart_NDC;
        lRayStart_world /= lRayStart_world.w;
        vec4 lRayEnd_world = M * lRayEnd_NDC;
        lRayEnd_world /= lRayEnd_world.w;

        vec3 lRayDir_world(lRayEnd_world - lRayStart_world);
        lRayDir_world = normalize(lRayDir_world);

        return Ray{lRayStart_world, lRayDir_world};
    }

    vec2 WorldToScreenSpace(Camera &_camera, Window &_window, InputManager &_inputManager, vec3 _position)
    {
        // Calculate the projection matrix
        mat4 projectionMatrix = perspective(_camera.FOV, _window.GetScreenWidth() / (float)_window.GetScreenHeight(), _camera.nearPlane, _camera.farPlane);

        // Convert world space to screen space
        vec4 viewSpacePoint = _camera.GetViewMatrix() * vec4(_position, 1.0f);
        vec4 clipSpacePoint = projectionMatrix * viewSpacePoint;

        // Normalize to NDC
        clipSpacePoint /= clipSpacePoint.w;

        // Convert NDC to screen coordinates
        return vec2((clipSpacePoint.x + 1.0f) * 0.5f * _window.GetScreenWidth(), _window.GetScreenHeight() - ((1.0f - clipSpacePoint.y) * 0.5f * _window.GetScreenHeight()));
    }

    bool HitSphere(vec3 center, float radius, Ray ray)
    {
        vec3 oc = ray.origin - center;
        float a = dot(ray.direction, ray.direction);
        float b = 2.0f * dot(oc, ray.direction);
        float c = dot(oc, oc) - radius * radius;
        float discriminant = b * b - 4 * a * c;
        return (discriminant > 0);
    }

    quat RotationBetweenVectors(vec3 start, vec3 dest)
    {
        start = normalize(start);
        dest = normalize(dest);

        float cosTheta = dot(start, dest);
        vec3 rotationAxis;

        if (cosTheta < -1 + 0.001f)
        {
            // special case when vectors in opposite directions:
            // there is no "ideal" rotation axis
            // So guess one; any will do as long as it's perpendicular to start
            rotationAxis = cross(vec3(0.0f, 0.0f, 1.0f), start);
            if (length2(rotationAxis) < 0.01) // bad luck, they were parallel, try again!
                rotationAxis = cross(vec3(1.0f, 0.0f, 0.0f), start);

            rotationAxis = normalize(rotationAxis);
            return angleAxis(radians(180.0f), rotationAxis);
        }

        rotationAxis = cross(start, dest);

        float s = sqrt((1 + cosTheta) * 2);
        float invs = 1 / s;

        return quat(
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
        const mat4 roationMatrix = toMat4(_transform.rotation);

        // translation * rotation * scale (also know as TRS matrix)
        _transform.modelMatrix = translate(mat4(1.0f), _transform.position) *
                                 roationMatrix *
                                 scale(mat4(1.0f), _transform.scale);

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

    const mat4 &GetModelMatrix(TransformComponent &_transform)
    {
        return _transform.modelMatrix;
    }

    vec3 GetGlobalPosition(const TransformComponent &_transform)
    {
        return vec3(_transform.modelMatrix[3]);
    }

    vec3 GetGlobalRotation(TransformComponent &_transform)
    {
        return vec3(_transform.modelMatrix[3]);
    }

    vec3 GetGlobalScale(TransformComponent &_transform)
    {
        vec3 size;
        size.x = length(vec3(_transform.modelMatrix[0])); // Basis vector X
        size.y = length(vec3(_transform.modelMatrix[1])); // Basis vector Y
        size.z = length(vec3(_transform.modelMatrix[2])); // Basis vector Z
        return size;
    }

    void MoveTransformPosition(TransformComponent &_transform, vec3 _offset)
    {
        _transform.position += _offset;

        UpdateModelMatrix(_transform);
    }

    void SetTransformPosition(TransformComponent &_transform, vec3 _position)
    {
        _transform.position = _position;

        UpdateModelMatrix(_transform);
    }

    vec3 GetTransformForward(TransformComponent &_transform)
    {
        return normalize(_transform.rotation * vec3(0.0f, 0.0f, 1.0f));
    }

    vec3 GetTransformRight(TransformComponent &_transform)
    {
        return normalize(_transform.rotation * vec3(1.0f, 0.0f, 0.0f));
    }

    void Rotate(TransformComponent &_transform, vec3 _rotate)
    {
        _transform.rotation = quat(_rotate) * _transform.rotation;

        UpdateModelMatrix(_transform);
    }

    void SetTransformRotation(TransformComponent &_transform, vec3 _rotation)
    {
        _transform.rotation = quat(_rotation);

        UpdateModelMatrix(_transform);
    }

    void SetTransformRotation(TransformComponent &_transform, quat _rotation)
    {
        _transform.rotation = _rotation;

        UpdateModelMatrix(_transform);
    }

    void SetTransformScale(TransformComponent &_transform, vec3 _scale)
    {
        _transform.scale = _scale;

        UpdateModelMatrix(_transform);
    }

    void LookAt(TransformComponent &_transform, vec3 _target, vec3 _up)
    {
        vec3 direction = normalize(_target - _transform.position);
        _transform.rotation = quatLookAt(direction, _up);
        UpdateModelMatrix(_transform);
    }

    // Like SLERP, but forbids rotation greater than maxAngle (in radians)
    // In conjunction to LookAt, can make your characters
    quat RotateTowards(quat _q1, quat _q2, float _maxAngle)
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

        quat res = (sin((1.0f - t) * angle) * _q1 + sin(t * angle) * _q2) / sin(angle);
        res = normalize(res);
        return res;
    }

    void RotateTowardsLookAt(TransformComponent &_transform, vec3 _target, vec3 _up, float _maxAngle)
    {
        vec3 direction = normalize(_transform.position - _target);
        quat targetQuat = quatLookAt(direction, _up);
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

    vec4 HexToRGBA(unsigned int _RRGGBBAA)
    {
        vec4 color = vec4(0.0f);
        color.r = ((_RRGGBBAA >> 24) & 0xFF) / 255.0f;
        color.g = ((_RRGGBBAA >> 16) & 0xFF) / 255.0f;
        color.b = ((_RRGGBBAA >> 8) & 0xFF) / 255.0f;
        color.a = ((_RRGGBBAA) & 0xFF) / 255.0f;
        return color;
    }

    vec4 UIntToRGBA(unsigned int _red, unsigned int _green, unsigned int _blue, unsigned int _alpha)
    {
        vec4 color = vec4(0.0f);
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

    void Lerp(vec3 &_value, const vec3 &_min, const vec3 &_max, const float &_fraction)
    {
        _value = _min + _fraction * (_max - _min);
    }

    void Lerp(vec4 &_value, const vec4 &_min, const vec4 &_max, const float &_fraction)
    {
        _value = _min + _fraction * (_max - _min);
    }

    void AnimationBellCurve(float &_value, const float &_min, const float &_max, const float &_fraction)
    {
        _value = _min + sin(Canis::PI * _fraction) * (_max - _min);
    }

    void AnimationBellCurve(vec3 &_value, const vec3 &_min, const vec3 &_max, const float &_fraction)
    {
        _value = _min + sin(Canis::PI * _fraction) * (_max - _min);
    }

    void AnimationBellCurve(vec4 &_value, const vec4 &_min, const vec4 &_max, const float &_fraction)
    {
        _value = _min + sin(Canis::PI * _fraction) * (_max - _min);
    }

    float Min(float _x, float _y)
    {
        return (_x < _y) ? _x : _y;
    }

    float Max(float _x, float _y)
    {
        return (_x > _y) ? _x : _y;
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

    void Clamp(int &_value, int _min, int _max)
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

    void ClampRap(int &_value, int _min, int _max)
    {
        if (_value < _min)
        {
            _value = _max;
        }

        if (_value > _max)
        {
            _value = _min;
        }
    }

    void RotatePoint(vec2 &_point, const float &_cosAngle, const float &_sinAngle)
    {
        float x = _point.x;
        float y = _point.y;
        _point.x = x * _cosAngle - y * _sinAngle;
        _point.y = x * _sinAngle + y * _cosAngle;
    }

    void RotatePointAroundPivot(vec2 &_point, const vec2 &_pivot, float _radian)
    {
        float s = sin(-_radian);
        float c = cos(-_radian);

        vec2 holder = _point;

        holder -= _pivot;

        _point.x = holder.x * c - holder.y * s;
        _point.y = holder.x * s + holder.y * c;

        _point += _pivot;
    }
}