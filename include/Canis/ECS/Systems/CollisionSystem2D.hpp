#pragma once
#include <map>
#include <vector>
#include <glm/glm.hpp>

#include <Canis/Data/Bit.hpp>

#include <Canis/External/entt.hpp>

#include <Canis/ECS/Systems/System.hpp>

#include <Canis/ECS/Components/RectTransformComponent.hpp>
#include <Canis/ECS/Components/CircleColliderComponent.hpp>

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

        void Add(const glm::vec2 &_pos,
                 const unsigned int &_layer,
                 const unsigned int &_mask,
                 const entt::entity _entity,
                 const float &_radius)
        {
            CollisionSystem2DPoint point = {};
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
            auto view = registry.view<const RectTransformComponent, CircleColliderComponent>();
            for (auto [entity, transform, circle] : view.each())
			{
                Add(transform.position, circle.layer, circle.mask, entity, circle.radius);
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

        void CalculateMask(const std::vector<CollisionSystem2DPoint> &_mask, const std::vector<CollisionSystem2DPoint> &_layer)
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

        glm::vec2 RotatePoint(glm::vec2 point, float angle)
        {
            glm::vec2 pos;

            pos.x = point.x * cos(angle) - point.y * sin(angle);
            pos.y = point.x * sin(angle) + point.y * cos(angle);

            return pos;
        }


        
        void BoxCast(const glm::vec2 &_position, const glm::vec2 &_size,
                                            const glm::vec2 &_origin, const float &_radians,
                                            const std::vector<CollisionSystem2DPoint> &_layer,
                                            std::vector<entt::entity> &_result)
        {
            glm::vec2 halfDims(_size.x / 2.0f, _size.y / 2.0f);

            for(CollisionSystem2DPoint cs2dp : _layer)
            {
                // check if a circle is within the box
                glm::vec2 ccib = cs2dp.position - _position;

                // rotate oposite of box
                ccib = RotatePoint(ccib, -_radians);

                // check if inside unrotated box
                if (ccib.x > -halfDims.x - cs2dp.radius + _origin.x && ccib.x < halfDims.x + cs2dp.radius + _origin.x &&
                    ccib.y > -halfDims.y - cs2dp.radius + _origin.y && ccib.y < halfDims.y + cs2dp.radius + _origin.y)
                {
                    _result.push_back(cs2dp.entity);
                    continue;
                }
            }
        }
    
    public:
        CollisionSystem2D() : System() {}

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
                // really should have delete funcution that lives in scene that everyone call to delete entts the it calls a list of delegate
                std::vector<entt::entity> &e = hits[_entity];
                int i = 0;
                while (i < e.size())
                {
                    if (e[i] == entt::tombstone || !GetScene().entityRegistry.valid(e[i])) {
                        e.erase(e.begin() + i);
                    }
                    else {
                        i++;
                    }
                }
                return hits[_entity];
            }

            return std::vector<entt::entity> {};
        }

        std::vector<entt::entity> BoxCast(const glm::vec2 &_position, const glm::vec2 &_size,
                                            const glm::vec2 &_origin, const float &_radians,
                                            const unsigned int &_layers)
        {
            std::vector<entt::entity> entities = {};

            if (_layers & BIT::ONE)
                BoxCast(_position, _size, _origin, _radians, Layer01, entities);
            if (_layers & BIT::TWO)
                BoxCast(_position, _size, _origin, _radians, Layer02, entities);
            if (_layers & BIT::THREE)
                BoxCast(_position, _size, _origin, _radians, Layer03, entities);
            if (_layers & BIT::FOUR)
                BoxCast(_position, _size, _origin, _radians, Layer04, entities);
            if (_layers & BIT::FIVE)
                BoxCast(_position, _size, _origin, _radians, Layer05, entities);
            if (_layers & BIT::SIX)
                BoxCast(_position, _size, _origin, _radians, Layer06, entities);
            if (_layers & BIT::SEVEN)
                BoxCast(_position, _size, _origin, _radians, Layer07, entities);
            if (_layers & BIT::EIGHT)
                BoxCast(_position, _size, _origin, _radians, Layer08, entities);
            if (_layers & BIT::NINE)
                BoxCast(_position, _size, _origin, _radians, Layer09, entities);
            if (_layers & BIT::TEN)
                BoxCast(_position, _size, _origin, _radians, Layer10, entities);
            if (_layers & BIT::ELEVEN)
                BoxCast(_position, _size, _origin, _radians, Layer11, entities);
            if (_layers & BIT::TWELVE)
                BoxCast(_position, _size, _origin, _radians, Layer12, entities);
            if (_layers & BIT::THIRTEEN)
                BoxCast(_position, _size, _origin, _radians, Layer13, entities);
            if (_layers & BIT::FOURTEEN)
                BoxCast(_position, _size, _origin, _radians, Layer14, entities);
            if (_layers & BIT::FIFTEEN)
                BoxCast(_position, _size, _origin, _radians, Layer15, entities);
            if (_layers & BIT::SIXTEEN)
                BoxCast(_position, _size, _origin, _radians, Layer16, entities);
            
            int i = 0;
            while (i < entities.size())
            {
                if (entities[i] == entt::tombstone || !GetScene().entityRegistry.valid(entities[i])) {
                    entities.erase(entities.begin() + i);
                }
                else {
                    i++;
                }
            }

            return entities;
        }
    };
} // end of Canis namespace