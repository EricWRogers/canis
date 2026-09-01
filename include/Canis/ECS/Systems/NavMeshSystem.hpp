#pragma once

#include <cstddef>
#include <memory>
#include <unordered_map>
#include <vector>

#include <Canis/DataStructure/AStar.hpp>
#include <Canis/Entity.hpp>
#include <Canis/System.hpp>

namespace Canis
{
    class NavMeshSystem : public System
    {
    public:
        NavMeshSystem();

        void Update(entt::registry& _registry, float _deltaTime) override;
        void OnDestroy() override;
        bool UpdateWhenPaused() const override { return true; }

        bool Build(entt::registry& _registry, Entity& _surfaceEntity);
        void Invalidate(const Entity& _surfaceEntity);
        std::vector<Vector3> FindPath(
            entt::registry& _registry,
            Entity& _surfaceEntity,
            const Vector3& _start,
            const Vector3& _destination);
        std::size_t GetPointCount(const Entity& _surfaceEntity) const;
        bool NeedsBuild(const Entity& _surfaceEntity) const;
        void DrawVisualization(entt::registry& _registry, Entity* _selectedSurface = nullptr);

    private:
        struct RuntimeSurface
        {
            AStar graph = {};
            std::vector<unsigned int> gridIds = {};
            int width = 0;
            int height = 0;
            u64 revision = 0u;
            Vector3 center = Vector3(0.0f);
            bool built = false;
        };

        std::unordered_map<entt::entity, std::unique_ptr<RuntimeSurface>> m_surfaces = {};

        bool IsWalkable(const NavMeshSurface& _surface, const Vector3& _point) const;
        bool HasClearance(
            const NavMeshSurface& _surface,
            const Vector3& _from,
            const Vector3& _to) const;
        bool IsStaticSurfaceHit(const NavMeshSurface& _surface, const Entity* _entity) const;
    };
}
