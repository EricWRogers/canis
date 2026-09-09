#include <Canis/ECS/Systems/NavMeshSystem.hpp>

#include <algorithm>
#include <cmath>

#include <Canis/Debug.hpp>
#include <Canis/Scene.hpp>

namespace Canis
{
    NavMeshSystem::NavMeshSystem()
    {
        m_name = type_name<NavMeshSystem>();
    }

    void NavMeshSystem::Update(entt::registry& _registry, float _deltaTime)
    {
        (void)_deltaTime;
        auto view = _registry.view<NavMeshSurface, Transform>();
        for (const entt::entity handle : view)
        {
            NavMeshSurface& surface = view.get<NavMeshSurface>(handle);
            if (!surface.active || !surface.buildOnReady || surface.entity == nullptr)
                continue;

            if (NeedsBuild(*surface.entity))
                Build(_registry, *surface.entity);
        }

    }

    void NavMeshSystem::OnDestroy()
    {
        m_surfaces.clear();
    }

    bool NavMeshSystem::Build(entt::registry& _registry, Entity& _surfaceEntity)
    {
        (void)_registry;
        if (!_surfaceEntity.HasComponent<NavMeshSurface>() ||
            !_surfaceEntity.HasComponent<Transform>() || scene == nullptr)
            return false;

        NavMeshSurface& surface = _surfaceEntity.GetComponent<NavMeshSurface>();
        std::unique_ptr<RuntimeSurface> runtime = std::make_unique<RuntimeSurface>();
        runtime->revision = surface.revision;

        const Transform& transform = _surfaceEntity.GetComponent<Transform>();
        const Vector3 center = transform.GetGlobalPosition();
        runtime->center = center;
        const float cellSize = std::max(0.1f, surface.cellSize);
        const Vector3 size = glm::max(glm::abs(surface.size), Vector3(cellSize, 0.2f, cellSize));
        const float minX = center.x - size.x * 0.5f;
        const float minZ = center.z - size.z * 0.5f;
        runtime->width = std::max(1, static_cast<int>(std::floor(size.x / cellSize)) + 1);
        runtime->height = std::max(1, static_cast<int>(std::floor(size.z / cellSize)) + 1);
        runtime->gridIds.assign(
            static_cast<std::size_t>(runtime->width * runtime->height),
            0u);

        const auto index = [&runtime](int _x, int _z)
        {
            return static_cast<std::size_t>(_z * runtime->width + _x);
        };

        for (int z = 0; z < runtime->height; ++z)
        {
            for (int x = 0; x < runtime->width; ++x)
            {
                const Vector3 point(minX + x * cellSize, center.y, minZ + z * cellSize);
                if (IsWalkable(surface, point))
                    runtime->gridIds[index(x, z)] = runtime->graph.AddPoint(point);
            }
        }

        constexpr int neighborOffsets[4][2] = {
            {-1, 0}, {0, -1}, {-1, -1}, {1, -1}
        };
        for (int z = 0; z < runtime->height; ++z)
        {
            for (int x = 0; x < runtime->width; ++x)
            {
                const unsigned int from = runtime->gridIds[index(x, z)];
                if (from == 0u)
                    continue;

                for (const auto& offset : neighborOffsets)
                {
                    const int nx = x + offset[0];
                    const int nz = z + offset[1];
                    if (nx < 0 || nz < 0 || nx >= runtime->width || nz >= runtime->height)
                        continue;

                    const unsigned int to = runtime->gridIds[index(nx, nz)];
                    if (to != 0u && HasClearance(
                        surface,
                        runtime->graph.GetPointPosition(from),
                        runtime->graph.GetPointPosition(to)))
                    {
                        runtime->graph.ConnectPoints(from, to);
                    }
                }
            }
        }

        runtime->built = true;
        const std::size_t pointCount = runtime->graph.GetPointCount();
        m_surfaces[_surfaceEntity.GetHandle()] = std::move(runtime);
        /*Debug::Log(
            "Nav mesh '%s' built with %zu walkable points.",
            _surfaceEntity.GetName().c_str(),
            pointCount);*/
        return pointCount > 0u;
    }

    void NavMeshSystem::Invalidate(const Entity& _surfaceEntity)
    {
        m_surfaces.erase(_surfaceEntity.GetHandle());
    }

    std::vector<Vector3> NavMeshSystem::FindPath(
        entt::registry& _registry,
        Entity& _surfaceEntity,
        const Vector3& _start,
        const Vector3& _destination)
    {
        if (!_surfaceEntity.HasComponent<NavMeshSurface>())
            return {};

        NavMeshSurface& surface = _surfaceEntity.GetComponent<NavMeshSurface>();
        auto found = m_surfaces.find(_surfaceEntity.GetHandle());
        if (NeedsBuild(_surfaceEntity))
        {
            if (!Build(_registry, _surfaceEntity))
                return {};
            found = m_surfaces.find(_surfaceEntity.GetHandle());
        }

        RuntimeSurface& runtime = *found->second;
        const unsigned int start = runtime.graph.GetClosestPoint(_start);
        const unsigned int destination = runtime.graph.GetClosestPoint(_destination);
        std::vector<Vector3> path = runtime.graph.GetPath(start, destination);
        if (!path.empty() && HasClearance(surface, path.back(), _destination))
            path.push_back(_destination);
        return path;
    }

    std::size_t NavMeshSystem::GetPointCount(const Entity& _surfaceEntity) const
    {
        const auto found = m_surfaces.find(_surfaceEntity.GetHandle());
        if (found == m_surfaces.end() || !found->second || !found->second->built)
            return 0u;
        return found->second->graph.GetPointCount();
    }

    bool NavMeshSystem::NeedsBuild(const Entity& _surfaceEntity) const
    {
        if (!_surfaceEntity.HasComponent<NavMeshSurface>() ||
            !_surfaceEntity.HasComponent<Transform>())
            return false;
        const NavMeshSurface& surface = _surfaceEntity.GetComponent<NavMeshSurface>();
        const auto found = m_surfaces.find(_surfaceEntity.GetHandle());
        return found == m_surfaces.end() || !found->second ||
            !found->second->built || found->second->revision != surface.revision ||
            glm::distance(
                found->second->center,
                _surfaceEntity.GetComponent<Transform>().GetGlobalPosition()) > 0.0001f;
    }

    void NavMeshSystem::DrawVisualization(entt::registry& _registry, Entity* _selectedSurface)
    {
        if (scene == nullptr)
            return;

        const Color edgeColor(0.05f, 0.8f, 1.0f, 0.7f);
        const Color nodeColor(0.2f, 1.0f, 0.55f, 1.0f);
        const Color boundsColor(1.0f, 0.72f, 0.15f, 0.9f);

        if (_selectedSurface != nullptr &&
            _selectedSurface->HasComponent<NavMeshSurface>() &&
            _selectedSurface->HasComponent<Transform>())
        {
            const NavMeshSurface& surface = _selectedSurface->GetComponent<NavMeshSurface>();
            if (surface.showDebug)
            {
                const Vector3 center = _selectedSurface->GetComponent<Transform>().GetGlobalPosition();
                const Vector3 halfSize = glm::abs(surface.size) * 0.5f;
                Vector3 corners[8] = {
                    center + Vector3(-halfSize.x, -halfSize.y, -halfSize.z),
                    center + Vector3( halfSize.x, -halfSize.y, -halfSize.z),
                    center + Vector3( halfSize.x, -halfSize.y,  halfSize.z),
                    center + Vector3(-halfSize.x, -halfSize.y,  halfSize.z),
                    center + Vector3(-halfSize.x,  halfSize.y, -halfSize.z),
                    center + Vector3( halfSize.x,  halfSize.y, -halfSize.z),
                    center + Vector3( halfSize.x,  halfSize.y,  halfSize.z),
                    center + Vector3(-halfSize.x,  halfSize.y,  halfSize.z)
                };
                constexpr int edges[12][2] = {
                    {0, 1}, {1, 2}, {2, 3}, {3, 0},
                    {4, 5}, {5, 6}, {6, 7}, {7, 4},
                    {0, 4}, {1, 5}, {2, 6}, {3, 7}
                };
                for (const auto& edge : edges)
                    scene->DrawDebugGizmoLine(corners[edge[0]], corners[edge[1]], boundsColor);
            }
        }

        for (const auto& [handle, runtimePointer] : m_surfaces)
        {
            if (!runtimePointer || !runtimePointer->built)
                continue;

            Entity* surfaceEntity = nullptr;
            if (_selectedSurface != nullptr && _selectedSurface->GetHandle() == handle)
                surfaceEntity = _selectedSurface;
            else if (_selectedSurface == nullptr && _registry.valid(handle))
            {
                NavMeshSurface* component = _registry.try_get<NavMeshSurface>(handle);
                surfaceEntity = component != nullptr ? component->entity : nullptr;
            }
            if (surfaceEntity == nullptr ||
                (_selectedSurface != nullptr && surfaceEntity != _selectedSurface) ||
                !surfaceEntity->HasComponent<NavMeshSurface>() ||
                !surfaceEntity->GetComponent<NavMeshSurface>().showDebug)
                continue;

            const std::vector<AStarNode>& nodes = runtimePointer->graph.GetNodes();
            for (unsigned int id = 1u; id < nodes.size(); ++id)
            {
                const Vector3 point = nodes[id].position + Vector3(0.0f, 0.04f, 0.0f);
                const float markerSize = 0.08f;
                scene->DrawDebugGizmoLine(
                    point - Vector3(markerSize, 0.0f, 0.0f),
                    point + Vector3(markerSize, 0.0f, 0.0f),
                    nodeColor);
                scene->DrawDebugGizmoLine(
                    point - Vector3(0.0f, 0.0f, markerSize),
                    point + Vector3(0.0f, 0.0f, markerSize),
                    nodeColor);

                for (const unsigned int adjacent : nodes[id].adjacentPointIDs)
                {
                    if (adjacent <= id || adjacent >= nodes.size())
                        continue;
                    scene->DrawDebugGizmoLine(
                        point,
                        nodes[adjacent].position + Vector3(0.0f, 0.04f, 0.0f),
                        edgeColor);
                }
            }
        }
    }

    bool NavMeshSystem::IsWalkable(const NavMeshSurface& _surface, const Vector3& _point) const
    {
        if (scene == nullptr || _surface.entity == nullptr ||
            !_surface.entity->HasComponent<Transform>())
            return false;

        const Transform& transform = _surface.entity->GetComponent<Transform>();
        const float castHeight = std::max(0.2f, std::abs(_surface.size.y));
        const Vector3 center = transform.GetGlobalPosition();
        const std::vector<RaycastHit> hits = scene->RaycastAll(
            _point + Vector3(0.0f, castHeight * 0.5f, 0.0f),
            Vector3(0.0f, -1.0f, 0.0f),
            castHeight,
            _surface.collisionMask);
        for (const RaycastHit& hit : hits)
        {
            if (!IsStaticSurfaceHit(_surface, hit.entity) || hit.normal.y < 0.7f)
                continue;
            return std::abs(hit.point.y - center.y) <= std::max(0.05f, _surface.maxFloorDelta);
        }
        return false;
    }

    bool NavMeshSystem::HasClearance(
        const NavMeshSurface& _surface,
        const Vector3& _from,
        const Vector3& _to) const
    {
        if (scene == nullptr)
            return false;

        Vector3 direction = _to - _from;
        direction.y = 0.0f;
        const float distance = glm::length(direction);
        if (distance < 0.001f)
            return true;
        direction /= distance;

        const Vector3 side(-direction.z, 0.0f, direction.x);
        const float radius = std::max(0.0f, _surface.agentRadius);
        const float rayHeight = std::max(0.1f, _surface.agentHeight * 0.5f);
        for (const float offset : {-radius, 0.0f, radius})
        {
            const Vector3 origin = _from + side * offset + Vector3(0.0f, rayHeight, 0.0f);
            const std::vector<RaycastHit> hits = scene->RaycastAll(
                origin,
                direction,
                distance,
                _surface.collisionMask);
            for (const RaycastHit& hit : hits)
            {
                if (hit.entity == _surface.entity)
                    continue;
                if (hit.entity != nullptr && hit.entity->Active() &&
                    hit.entity->HasComponent<Rigidbody>() &&
                    hit.entity->GetComponent<Rigidbody>().motionType == RigidbodyMotionType::STATIC &&
                    hit.distance < distance - 0.05f)
                    return false;
            }
        }
        return true;
    }

    bool NavMeshSystem::IsStaticSurfaceHit(
        const NavMeshSurface& _surface,
        const Entity* _entity) const
    {
        if (_entity == nullptr || !_entity->Active() || !_entity->HasComponent<Rigidbody>())
            return false;
        const Rigidbody& rigidbody = _entity->GetComponent<Rigidbody>();
        if (!rigidbody.active || rigidbody.motionType != RigidbodyMotionType::STATIC)
            return false;
        return !_surface.meshCollidersOnly ||
            _entity->HasComponent<MeshCollider>() ||
            _entity->HasComponent<Terrain>();
    }
}
