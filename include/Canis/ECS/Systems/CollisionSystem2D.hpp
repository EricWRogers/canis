#pragma once
#include <map>
#include <vector>
#include <glm/glm.hpp>

#include <Canis/Data/Bit.hpp>

#include <Canis/ECS/Systems/System.hpp>

namespace Canis
{
    struct CollisionSystem2DPoint
    {
        glm::vec2 position = glm::vec2(0.0f);
        entt::entity entity = entt::null;
        float radius = 0.0f;
    };

    class CollisionSystem2D : public System
    {
    private:
        std::vector<CollisionSystem2DPoint> Layer01 = {};
        std::vector<CollisionSystem2DPoint> Layer02 = {};
        std::vector<CollisionSystem2DPoint> Layer03 = {};
        std::vector<CollisionSystem2DPoint> Layer04 = {};

        std::vector<CollisionSystem2DPoint> Layer05 = {};
        std::vector<CollisionSystem2DPoint> Layer06 = {};
        std::vector<CollisionSystem2DPoint> Layer07 = {};
        std::vector<CollisionSystem2DPoint> Layer08 = {};

        std::vector<CollisionSystem2DPoint> Layer09 = {};
        std::vector<CollisionSystem2DPoint> Layer10 = {};
        std::vector<CollisionSystem2DPoint> Layer11 = {};
        std::vector<CollisionSystem2DPoint> Layer12 = {};

        std::vector<CollisionSystem2DPoint> Layer13 = {};
        std::vector<CollisionSystem2DPoint> Layer14 = {};
        std::vector<CollisionSystem2DPoint> Layer15 = {};
        std::vector<CollisionSystem2DPoint> Layer16 = {};

        std::vector<CollisionSystem2DPoint> Mask01 = {};
        std::vector<CollisionSystem2DPoint> Mask02 = {};
        std::vector<CollisionSystem2DPoint> Mask03 = {};
        std::vector<CollisionSystem2DPoint> Mask04 = {};

        std::vector<CollisionSystem2DPoint> Mask05 = {};
        std::vector<CollisionSystem2DPoint> Mask06 = {};
        std::vector<CollisionSystem2DPoint> Mask07 = {};
        std::vector<CollisionSystem2DPoint> Mask08 = {};

        std::vector<CollisionSystem2DPoint> Mask09 = {};
        std::vector<CollisionSystem2DPoint> Mask10 = {};
        std::vector<CollisionSystem2DPoint> Mask11 = {};
        std::vector<CollisionSystem2DPoint> Mask12 = {};

        std::vector<CollisionSystem2DPoint> Mask13 = {};
        std::vector<CollisionSystem2DPoint> Mask14 = {};
        std::vector<CollisionSystem2DPoint> Mask15 = {};
        std::vector<CollisionSystem2DPoint> Mask16 = {};

        std::map<entt::entity, std::vector<entt::entity>> hits;

        void Clear();

        void Add(const glm::vec2 &_pos,
                 const unsigned int &_layer,
                 const unsigned int &_mask,
                 const entt::entity _entity,
                 const float &_radius);

        void LoadLayers(entt::registry &registry);

        bool VecContainsEntt(std::vector<entt::entity> &v, entt::entity e);

        void CalculateMask(const std::vector<CollisionSystem2DPoint> &_mask, const std::vector<CollisionSystem2DPoint> &_layer);

        void CalculateOverlap();

        glm::vec2 RotatePoint(const glm::vec2 &_point, const float &_angle);


        
        void BoxCast(const glm::vec2 &_position, const glm::vec2 &_size,
                                            const glm::vec2 &_origin, const float &_radians,
                                            const std::vector<CollisionSystem2DPoint> &_layer,
                                            std::vector<entt::entity> &_result);
    
    public:
        CollisionSystem2D() : System() {}

        void Create() {}

        void Ready() {}

        void Update(entt::registry &_registry, float _deltaTime);

        std::vector<entt::entity> GetHits(const entt::entity &_entity);

        std::vector<entt::entity> BoxCast(const glm::vec2 &_position, const glm::vec2 &_size,
                                            const glm::vec2 &_origin, const float &_radians,
                                            const unsigned int &_layers);
    };

    extern bool DecodeCollisionSystem2D(const std::string &_name, Canis::Scene *_scene);
} // end of Canis namespace