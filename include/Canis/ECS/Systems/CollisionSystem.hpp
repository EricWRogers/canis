#pragma once
#include <map>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>

#include <Canis/Math.hpp>

#include <Canis/Data/Bit.hpp>

#include <Canis/ECS/Systems/System.hpp>

#include <Canis/ECS/Components/TransformComponent.hpp>
#include <Canis/ECS/Components/BoxColliderComponent.hpp>
#include <Canis/ECS/Components/SphereColliderComponent.hpp>

namespace Canis
{
    class CollisionSystem : public System
    {
    private:
        enum ColliderType
        {
            BOX = BIT::ONE,
            SPHERE = BIT::TWO
        };

        struct Sphere
        {
            glm::vec3 center;
            float radius;

            Sphere() {}
            Sphere(const glm::vec3 &center, float radius) : center(center), radius(radius) {}
        };

        struct Box
        {
            glm::vec3 center;
            glm::vec3 halfSize;
            std::array<glm::vec3, 3> axes; // Local x, y, and z axes

            Box() {}
            Box(const glm::vec3 &center, const glm::vec3 &halfSize, const std::array<glm::vec3, 3> &axes)
                : center(center), halfSize(halfSize), axes(axes) {}
        };

        union Shape
        {
            Box box;
            Sphere sphere;
        };

        struct CollisionSystemPoint
        {
            ColliderType type;
            union Shape shape;
            entt::entity entity = entt::null;
        };

        std::vector<CollisionSystemPoint> Layer01 = {};
        std::vector<CollisionSystemPoint> Layer02 = {};
        std::vector<CollisionSystemPoint> Layer03 = {};
        std::vector<CollisionSystemPoint> Layer04 = {};

        std::vector<CollisionSystemPoint> Layer05 = {};
        std::vector<CollisionSystemPoint> Layer06 = {};
        std::vector<CollisionSystemPoint> Layer07 = {};
        std::vector<CollisionSystemPoint> Layer08 = {};

        std::vector<CollisionSystemPoint> Layer09 = {};
        std::vector<CollisionSystemPoint> Layer10 = {};
        std::vector<CollisionSystemPoint> Layer11 = {};
        std::vector<CollisionSystemPoint> Layer12 = {};

        std::vector<CollisionSystemPoint> Layer13 = {};
        std::vector<CollisionSystemPoint> Layer14 = {};
        std::vector<CollisionSystemPoint> Layer15 = {};
        std::vector<CollisionSystemPoint> Layer16 = {};

        std::vector<CollisionSystemPoint> Mask01 = {};
        std::vector<CollisionSystemPoint> Mask02 = {};
        std::vector<CollisionSystemPoint> Mask03 = {};
        std::vector<CollisionSystemPoint> Mask04 = {};

        std::vector<CollisionSystemPoint> Mask05 = {};
        std::vector<CollisionSystemPoint> Mask06 = {};
        std::vector<CollisionSystemPoint> Mask07 = {};
        std::vector<CollisionSystemPoint> Mask08 = {};

        std::vector<CollisionSystemPoint> Mask09 = {};
        std::vector<CollisionSystemPoint> Mask10 = {};
        std::vector<CollisionSystemPoint> Mask11 = {};
        std::vector<CollisionSystemPoint> Mask12 = {};

        std::vector<CollisionSystemPoint> Mask13 = {};
        std::vector<CollisionSystemPoint> Mask14 = {};
        std::vector<CollisionSystemPoint> Mask15 = {};
        std::vector<CollisionSystemPoint> Mask16 = {};

        std::map<entt::entity, std::vector<entt::entity>> hits;

        // Function to extract axes from quaternion
        std::array<glm::vec3, 3> GetAxesFromQuaternion(const glm::quat &q)
        {
            glm::mat3 rotationMatrix = glm::mat3_cast(q);
            return {rotationMatrix[0], rotationMatrix[1], rotationMatrix[2]};
        }

        // Function to find the closest point on an OBB
        glm::vec3 ClosestPointOnOBB(const glm::vec3 &p, const Box &box)
        {
            glm::vec3 d = p - box.center;
            glm::vec3 closestPoint = box.center;

            for (int i = 0; i < 3; ++i)
            {
                float dist = glm::dot(d, box.axes[i]);
                if (dist > box.halfSize[i])
                    dist = box.halfSize[i];
                if (dist < -box.halfSize[i])
                    dist = -box.halfSize[i];
                closestPoint += box.axes[i] * dist;
            }

            return closestPoint;
        }

        // Helper function to project a box onto an axis
        float ProjectBoxOntoAxis(const Box &box, const glm::vec3 &axis)
        {
            return box.halfSize.x * glm::abs(glm::dot(box.axes[0], axis)) +
                   box.halfSize.y * glm::abs(glm::dot(box.axes[1], axis)) +
                   box.halfSize.z * glm::abs(glm::dot(box.axes[2], axis));
        }

        // Check if there is a separating axis between two OBBs
        bool IsSeparatingAxis(const Box &box1, const Box &box2, const glm::vec3 &axis, const glm::vec3 &toCenter)
        {
            float projection1 = ProjectBoxOntoAxis(box1, axis);
            float projection2 = ProjectBoxOntoAxis(box2, axis);
            float distance = glm::abs(glm::dot(toCenter, axis));
            return distance > (projection1 + projection2);
        }

        // Function to check for intersection between two OBBs
        bool IntersectOBB(const Box &box1, const Box &box2)
        {
            glm::vec3 toCenter = box2.center - box1.center;

            // Test the 15 possible separating axes
            for (int i = 0; i < 3; ++i)
            {
                if (IsSeparatingAxis(box1, box2, box1.axes[i], toCenter))
                    return false;
                if (IsSeparatingAxis(box1, box2, box2.axes[i], toCenter))
                    return false;
            }

            for (int i = 0; i < 3; ++i)
            {
                for (int j = 0; j < 3; ++j)
                {
                    glm::vec3 axis = glm::cross(box1.axes[i], box2.axes[j]);
                    if (glm::length2(axis) > 0.0001f)
                    { // Ensure axis is not a zero vector
                        if (IsSeparatingAxis(box1, box2, axis, toCenter))
                            return false;
                    }
                }
            }

            return true; // No separating axis found, the boxes must be intersecting
        }

        // Function to check for intersection between a sphere and an OBB
        bool IntersectSphereOBB(const Sphere &sphere, const Box &box)
        {
            glm::vec3 closestPoint = ClosestPointOnOBB(sphere.center, box);
            float distanceSquared = glm::length2(closestPoint - sphere.center);
            return distanceSquared <= (sphere.radius * sphere.radius);
        }

        void Clear()
        {
            hits.clear();

            Layer01.clear();
            Layer02.clear();
            Layer03.clear();
            Layer04.clear();
            Layer05.clear();
            Layer06.clear();
            Layer07.clear();
            Layer08.clear();
            Layer09.clear();
            Layer10.clear();
            Layer11.clear();
            Layer12.clear();
            Layer13.clear();
            Layer14.clear();
            Layer15.clear();
            Layer16.clear();

            Mask01.clear();
            Mask02.clear();
            Mask03.clear();
            Mask04.clear();
            Mask05.clear();
            Mask06.clear();
            Mask07.clear();
            Mask08.clear();
            Mask09.clear();
            Mask10.clear();
            Mask11.clear();
            Mask12.clear();
            Mask13.clear();
            Mask14.clear();
            Mask15.clear();
            Mask16.clear();
        }

        void Add(const CollisionSystemPoint _point,
                 const unsigned int &_layer,
                 const unsigned int &_mask)
        {
            if (_layer != 0u)
            {
                if (_layer & BIT::ONE)
                    Layer01.push_back(_point);
                if (_layer & BIT::TWO)
                    Layer02.push_back(_point);
                if (_layer & BIT::THREE)
                    Layer03.push_back(_point);
                if (_layer & BIT::FOUR)
                    Layer04.push_back(_point);
                if (_layer & BIT::FIVE)
                    Layer05.push_back(_point);
                if (_layer & BIT::SIX)
                    Layer06.push_back(_point);
                if (_layer & BIT::SEVEN)
                    Layer07.push_back(_point);
                if (_layer & BIT::EIGHT)
                    Layer08.push_back(_point);
                if (_layer & BIT::NINE)
                    Layer09.push_back(_point);
                if (_layer & BIT::TEN)
                    Layer10.push_back(_point);
                if (_layer & BIT::ELEVEN)
                    Layer11.push_back(_point);
                if (_layer & BIT::TWELVE)
                    Layer12.push_back(_point);
                if (_layer & BIT::THIRTEEN)
                    Layer13.push_back(_point);
                if (_layer & BIT::FOURTEEN)
                    Layer14.push_back(_point);
                if (_layer & BIT::FIFTEEN)
                    Layer15.push_back(_point);
                if (_layer & BIT::SIXTEEN)
                    Layer16.push_back(_point);
            }

            if (_mask != 0u)
            {
                if (_mask & BIT::ONE)
                    Mask01.push_back(_point);
                if (_mask & BIT::TWO)
                    Mask02.push_back(_point);
                if (_mask & BIT::THREE)
                    Mask03.push_back(_point);
                if (_mask & BIT::FOUR)
                    Mask04.push_back(_point);
                if (_mask & BIT::FIVE)
                    Mask05.push_back(_point);
                if (_mask & BIT::SIX)
                    Mask06.push_back(_point);
                if (_mask & BIT::SEVEN)
                    Mask07.push_back(_point);
                if (_mask & BIT::EIGHT)
                    Mask08.push_back(_point);
                if (_mask & BIT::NINE)
                    Mask09.push_back(_point);
                if (_mask & BIT::TEN)
                    Mask10.push_back(_point);
                if (_mask & BIT::ELEVEN)
                    Mask11.push_back(_point);
                if (_mask & BIT::TWELVE)
                    Mask12.push_back(_point);
                if (_mask & BIT::THIRTEEN)
                    Mask13.push_back(_point);
                if (_mask & BIT::FOURTEEN)
                    Mask14.push_back(_point);
                if (_mask & BIT::FIFTEEN)
                    Mask15.push_back(_point);
                if (_mask & BIT::SIXTEEN)
                    Mask16.push_back(_point);
            }
        }

        void LoadLayers(entt::registry &registry)
        {
            auto sphereView = registry.view<TransformComponent, SphereColliderComponent>();
            for (auto [entity, transform, sphere] : sphereView.each())
            {
                CollisionSystemPoint point = {};
                point.type = ColliderType::SPHERE;
                point.entity = entity;
                point.shape.sphere.center = Canis::GetGlobalPosition(transform);
                point.shape.sphere.radius = sphere.radius;

                Add(point, sphere.layer, sphere.mask);
            }

            auto boxView = registry.view<TransformComponent, BoxColliderComponent>();
            for (auto [entity, transform, box] : boxView.each())
            {
                float radius = 0.0f;
                glm::vec3 size = Canis::GetGlobalScale(transform) * box.size;

                CollisionSystemPoint point = {};
                point.shape.box.center = Canis::GetGlobalPosition(transform) + box.center;
                point.shape.box.halfSize = size * 0.5f;

                // this does not take into account parent rotation
                // or an offset of the box
                point.shape.box.axes = GetAxesFromQuaternion(transform.rotation);
                point.entity = entity;
                point.type = ColliderType::BOX;

                Add(point, box.layer, box.mask);
            }
        }

        bool VecContainsEntt(std::vector<entt::entity> &v, entt::entity e)
        {
            for (int i = 0; i < v.size(); i++)
            {
                if (v[i] == e)
                {
                    return true;
                }
            }

            return false;
        }

        void CalculateMask(const std::vector<CollisionSystemPoint> &_mask, const std::vector<CollisionSystemPoint> &_layer)
        {
            for (int m = 0; m < _mask.size(); m++)
            {
                for (int l = 0; l < _layer.size(); l++)
                {
                    if (_mask[m].entity != _layer[l].entity)
                    {
                        if (_mask[m].type == ColliderType::SPHERE && _layer[l].type == ColliderType::SPHERE)
                        {
                            if (glm::distance(_mask[m].shape.sphere.center, _layer[l].shape.sphere.center) < (_mask[m].shape.sphere.radius + _layer[l].shape.sphere.radius))
                            {
                                if (hits.contains(_mask[m].entity))
                                {
                                    if (!VecContainsEntt(hits[_mask[m].entity], _layer[l].entity))
                                        hits[_mask[m].entity].push_back(_layer[l].entity);
                                }
                                else
                                {
                                    hits.insert({_mask[m].entity, {_layer[l].entity}});
                                }
                            }
                        }
                        else if (_mask[m].type == ColliderType::BOX && _layer[l].type == ColliderType::BOX)
                        {
                            if (IntersectOBB(_mask[m].shape.box, _layer[l].shape.box))
                            {
                                if (hits.contains(_mask[m].entity))
                                {
                                    if (!VecContainsEntt(hits[_mask[m].entity], _layer[l].entity))
                                        hits[_mask[m].entity].push_back(_layer[l].entity);
                                }
                                else
                                {
                                    hits.insert({_mask[m].entity, {_layer[l].entity}});
                                }
                            }
                        }
                        else if (_mask[m].type == ColliderType::SPHERE && _layer[l].type == ColliderType::BOX)
                        {
                            if (IntersectSphereOBB(_mask[m].shape.sphere, _layer[l].shape.box))
                            {
                                if (hits.contains(_mask[m].entity))
                                {
                                    if (!VecContainsEntt(hits[_mask[m].entity], _layer[l].entity))
                                        hits[_mask[m].entity].push_back(_layer[l].entity);
                                }
                                else
                                {
                                    hits.insert({_mask[m].entity, {_layer[l].entity}});
                                }
                            }
                        }
                        else if (_mask[m].type == ColliderType::BOX && _layer[l].type == ColliderType::SPHERE)
                        {
                            if (IntersectSphereOBB(_layer[l].shape.sphere, _mask[m].shape.box))
                            {
                                if (hits.contains(_mask[m].entity))
                                {
                                    if (!VecContainsEntt(hits[_mask[m].entity], _layer[l].entity))
                                        hits[_mask[m].entity].push_back(_layer[l].entity);
                                }
                                else
                                {
                                    hits.insert({_mask[m].entity, {_layer[l].entity}});
                                }
                            }
                        }
                    }
                }
            }
        }

        void CalculateOverlap()
        {
            CalculateMask(Mask01, Layer01);
            CalculateMask(Mask02, Layer02);
            CalculateMask(Mask03, Layer03);
            CalculateMask(Mask04, Layer04);
            CalculateMask(Mask05, Layer05);
            CalculateMask(Mask06, Layer06);
            CalculateMask(Mask07, Layer07);
            CalculateMask(Mask08, Layer08);
            CalculateMask(Mask09, Layer09);
            CalculateMask(Mask10, Layer10);
            CalculateMask(Mask11, Layer11);
            CalculateMask(Mask12, Layer12);
            CalculateMask(Mask13, Layer13);
            CalculateMask(Mask14, Layer14);
            CalculateMask(Mask15, Layer15);
            CalculateMask(Mask16, Layer16);
        }

    public:
        CollisionSystem() : System() { m_name = "Canis::CollisionSystem"; }

        void Create()
        {
        }

        void Ready()
        {
        }

        void Update(entt::registry &_registry, float _deltaTime)
        {
            Clear();

            LoadLayers(_registry);

            CalculateOverlap();
        }

        std::vector<entt::entity> GetHits(const entt::entity &_entity)
        {
            if (hits.contains(_entity))
            {
                int i = 0;
                while (i < hits[_entity].size())
                {
                    if (hits[_entity][i] == entt::tombstone || !GetScene().entityRegistry.valid(hits[_entity][i]))
                    {
                        hits[_entity].erase(hits[_entity].begin() + i);
                    }
                    else
                    {
                        i++;
                    }
                }
                return hits[_entity];
            }

            return std::vector<entt::entity>{};
        }
    };
} // end of Canis namespace