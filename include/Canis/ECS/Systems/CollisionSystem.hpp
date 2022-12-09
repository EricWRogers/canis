#pragma once
#include <map>
#include <vector>
#include <glm/glm.hpp>

#include <Canis/Data/Bit.hpp>

#include <Canis/External/entt.hpp>

#include <Canis/ECS/Systems/System.hpp>

#include <Canis/ECS/Components/TransformComponent.hpp>
#include <Canis/ECS/Components/SphereColliderComponent.hpp>

namespace Canis
{
    struct CollisionSystemPoint
    {
        glm::vec3 position = glm::vec3(0.0f);
        entt::entity entity = entt::null;
        float radius = 0.0f;
    };

    class CollisionSystem : public System
    {
    private:
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

        void Add(const glm::vec3 &_pos,
                 const unsigned int &_layer,
                 const unsigned int &_mask,
                 const entt::entity _entity,
                 const float &_radius)
        {
            CollisionSystemPoint point = {};
            point.position = _pos;
            point.entity   = _entity;
            point.radius   = _radius;

            if (_layer != 0u)
            {
                if (_layer & BIT::ONE)
                    Layer01.push_back(point);
                if (_layer & BIT::TWO)
                    Layer02.push_back(point);
                if (_layer & BIT::THREE)
                    Layer03.push_back(point);
                if (_layer & BIT::FOUR)
                    Layer04.push_back(point);
                if (_layer & BIT::FIVE)
                    Layer05.push_back(point);
                if (_layer & BIT::SIX)
                    Layer06.push_back(point);
                if (_layer & BIT::SEVEN)
                    Layer07.push_back(point);
                if (_layer & BIT::EIGHT)
                    Layer08.push_back(point);
                if (_layer & BIT::NINE)
                    Layer09.push_back(point);
                if (_layer & BIT::TEN)
                    Layer10.push_back(point);
                if (_layer & BIT::ELEVEN)
                    Layer11.push_back(point);
                if (_layer & BIT::TWELVE)
                    Layer12.push_back(point);
                if (_layer & BIT::THIRTEEN)
                    Layer13.push_back(point);
                if (_layer & BIT::FOURTEEN)
                    Layer14.push_back(point);
                if (_layer & BIT::FIFTEEN)
                    Layer15.push_back(point);
                if (_layer & BIT::SIXTEEN)
                    Layer16.push_back(point);
            }

            if (_mask != 0u)
            {
                if (_mask & BIT::ONE)
                    Mask01.push_back(point);
                if (_mask & BIT::TWO)
                    Mask02.push_back(point);
                if (_mask & BIT::THREE)
                    Mask03.push_back(point);
                if (_mask & BIT::FOUR)
                    Mask04.push_back(point);
                if (_mask & BIT::FIVE)
                    Mask05.push_back(point);
                if (_mask & BIT::SIX)
                    Mask06.push_back(point);
                if (_mask & BIT::SEVEN)
                    Mask07.push_back(point);
                if (_mask & BIT::EIGHT)
                    Mask08.push_back(point);
                if (_mask & BIT::NINE)
                    Mask09.push_back(point);
                if (_mask & BIT::TEN)
                    Mask10.push_back(point);
                if (_mask & BIT::ELEVEN)
                    Mask11.push_back(point);
                if (_mask & BIT::TWELVE)
                    Mask12.push_back(point);
                if (_mask & BIT::THIRTEEN)
                    Mask13.push_back(point);
                if (_mask & BIT::FOURTEEN)
                    Mask14.push_back(point);
                if (_mask & BIT::FIFTEEN)
                    Mask15.push_back(point);
                if (_mask & BIT::SIXTEEN)
                    Mask16.push_back(point);
            }
            
        }

        void LoadLayers(entt::registry &registry)
        {
            auto view = registry.view<TransformComponent, SphereColliderComponent>();
            for (auto [entity, transform, sphere] : view.each())
			{
                Add(transform.position, sphere.layer, sphere.mask, entity, sphere.radius);
            }
        }

        bool VecContainsEntt(std::vector<entt::entity> &v, entt::entity e)
        {
            for(int i = 0; i < v.size(); i++)
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
            for(int m = 0; m < _mask.size(); m++)
            {
                for(int l = 0; l < _layer.size(); l++)
                {
                    if (_mask[m].entity != _layer[l].entity)
                    {
                        if (glm::distance(_mask[m].position,_layer[l].position) < (_mask[m].radius+_layer[l].radius))
                        {
                            if (hits.contains(_mask[m].entity))
                            {
                                if(!VecContainsEntt(hits[_mask[m].entity],_layer[l].entity))
                                    hits[_mask[m].entity].push_back(_layer[l].entity);
                            }
                            else
                            {
                                hits.insert({_mask[m].entity, { _layer[l].entity }});
                            }
                        }
                    }
                }
            }
        }

        void CalculateOverlap()
        {
            CalculateMask(Mask01,Layer01);
            CalculateMask(Mask02,Layer02);
            CalculateMask(Mask03,Layer03);
            CalculateMask(Mask04,Layer04);
            CalculateMask(Mask05,Layer05);
            CalculateMask(Mask06,Layer06);
            CalculateMask(Mask07,Layer07);
            CalculateMask(Mask08,Layer08);
            CalculateMask(Mask09,Layer09);
            CalculateMask(Mask10,Layer10);
            CalculateMask(Mask11,Layer11);
            CalculateMask(Mask12,Layer12);
            CalculateMask(Mask13,Layer13);
            CalculateMask(Mask14,Layer14);
            CalculateMask(Mask15,Layer15);
            CalculateMask(Mask16,Layer16);
        }

    public:
        CollisionSystem() = default;
        CollisionSystem(std::string _name) : System(_name) {}

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
                return hits[_entity];
            }

            return std::vector<entt::entity> {};
        }
    };
} // end of Canis namespace