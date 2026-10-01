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
            {});

        const auto index = [&runtime](int _x, int _z)
        {
            return static_cast<std::size_t>(_z * runtime->width + _x);
        };

        for (int z = 0; z < runtime->height; ++z)
        {
            for (int x = 0; x < runtime->width; ++x)
            {
                const Vector3 point(minX + x * cellSize, center.y, minZ + z * cellSize);
                for (const Vector3& floor : SampleFloors(surface, point))
                    runtime->gridIds[index(x, z)].push_back(runtime->graph.AddPoint(floor));
            }
        }

        constexpr int neighborOffsets[4][2] = {
            {-1, 0}, {0, -1}, {-1, -1}, {1, -1}
        };
        for (int z = 0; z < runtime->height; ++z)
        {
            for (int x = 0; x < runtime->width; ++x)
            {
                const auto& sources = runtime->gridIds[index(x, z)];
                for (const auto& offset : neighborOffsets)
                {
                    const int nx = x + offset[0], nz = z + offset[1];
                    if (nx < 0 || nz < 0 || nx >= runtime->width || nz >= runtime->height) continue;
                    for (const unsigned int from : sources)
                        for (const unsigned int to : runtime->gridIds[index(nx, nz)])
                            if (HasClearance(surface, runtime->graph.GetPointPosition(from), runtime->graph.GetPointPosition(to)))
                                runtime->graph.ConnectPoints(from, to);
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
        auto project = [&](const Vector3& requested, Vector3& projected) {
            float best = std::max(.05f, surface.maxFloorDelta) + .03f;
            bool foundFloor = false;
            for (const Vector3& floor : SampleFloors(surface, Vector3(requested.x, runtime.center.y, requested.z)))
                if (const float delta = std::abs(requested.y - floor.y); delta <= best)
                { best = delta; projected = floor; foundFloor = true; }
            return foundFloor;
        };
        Vector3 projectedStart, projectedDestination;
        if (!project(_start, projectedStart) || !project(_destination, projectedDestination)) return {};
        auto closestReachable = [&](const Vector3& point) {
            std::vector<std::pair<float, unsigned int>> candidates;
            const auto& nodes = runtime.graph.GetNodes();
            const float range = std::max(.2f, surface.cellSize * 2);
            for (unsigned int id = 1; id < nodes.size(); ++id)
            {
                const float distance = glm::length(nodes[id].position - point);
                if (distance <= range) candidates.emplace_back(distance, id);
            }
            std::sort(candidates.begin(), candidates.end());
            for (const auto& [distance, id] : candidates)
                if (HasClearance(surface, point, nodes[id].position)) return id;
            return 0u;
        };
        const unsigned int start = closestReachable(projectedStart);
        const unsigned int destination = closestReachable(projectedDestination);
        if (start == 0u || destination == 0u) return {};
        std::vector<Vector3> path = runtime.graph.GetPath(start, destination);
        if (!path.empty()) path.push_back(projectedDestination);
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

    std::vector<Vector3> NavMeshSystem::SampleFloors(const NavMeshSurface& _surface, const Vector3& _point) const
    {
        std::vector<Vector3> floors;
        if (scene == nullptr) return floors;
        const float height = std::max(0.2f, std::abs(_surface.size.y));
        const float normalY = std::cos(glm::radians(std::clamp(_surface.maxSlopeAngle, 0.0f, 75.0f)));
        // Avoid ambiguous normals exactly on shared box/triangle edges.
        for (const RaycastHit& hit : scene->RaycastAll(_point + Vector3(.001f, height * .5f, .001f),
                Vector3(0, -1, 0), height, _surface.collisionMask))
        {
            if (!IsStaticSurfaceHit(_surface, hit.entity) || hit.normal.y < normalY) continue;
            // Keep each supported layer, including floors beneath a balcony.
            if (!floors.empty() && std::abs(floors.back().y - hit.point.y) < .02f) continue;
            if (HasHeadroom(_surface, hit.point)) floors.push_back(hit.point);
        }
        return floors;
    }

    bool NavMeshSystem::HasHeadroom(const NavMeshSurface& _surface, const Vector3& _point) const
    {
        for (const RaycastHit& hit : scene->RaycastAll(_point + Vector3(0, .03f, 0), Vector3(0, 1, 0),
                std::max(.1f, _surface.agentHeight) - .03f, _surface.collisionMask))
            if (hit.entity && hit.entity->Active() && hit.entity->HasComponent<Rigidbody>() &&
                hit.entity->GetComponent<Rigidbody>().motionType == RigidbodyMotionType::STATIC) return false;
        return true;
    }

    bool NavMeshSystem::HasClearance(const NavMeshSurface& _surface, const Vector3& _from, const Vector3& _to) const
    {
        if (scene == nullptr) return false;
        Vector3 flat = _to - _from;
        flat.y = 0;
        const float distance = glm::length(flat);
        const float maxStep = std::max(0.0f, _surface.maxFloorDelta);
        const float slope = std::tan(glm::radians(std::clamp(_surface.maxSlopeAngle, 0.0f, 75.0f)));
        if (std::abs(_to.y - _from.y) > maxStep + distance * slope + .02f) return false;
        if (distance < .001f) return std::abs(_to.y - _from.y) < .02f;
        const Vector3 side(-flat.z / distance, 0, flat.x / distance);
        const float radius = std::max(0.0f, _surface.agentRadius);
        const int steps = std::max(1, static_cast<int>(std::ceil(distance / .15f)));
        const float normalY = std::cos(glm::radians(std::clamp(_surface.maxSlopeAngle, 0.0f, 75.0f)));
        Vector3 previous = _from;
        for (int i = 1; i <= steps; ++i)
        {
            Vector3 next = glm::mix(_from, _to, static_cast<float>(i) / steps);
            const float probe = std::max(maxStep, distance / steps * slope) + .03f;
            bool supported = false;
            for (const RaycastHit& hit : scene->RaycastAll(Vector3(next.x, previous.y + probe, next.z),
                    Vector3(0, -1, 0), probe * 2, _surface.collisionMask))
            {
                if (!IsStaticSurfaceHit(_surface, hit.entity) || hit.normal.y < normalY) continue;
                next.y = hit.point.y;
                supported = true;
                break;
            }
            if (!supported || !HasHeadroom(_surface, next)) return false;
            if (i == steps && std::abs(next.y - _to.y) > .05f) return false;
            // Trace the full body's width and height, with forward radius clearance.
            for (const float offset : {-radius, 0.0f, radius})
                for (const float level : {std::max(.1f, maxStep + .05f), _surface.agentHeight * .6f, _surface.agentHeight - .03f})
                {
                    Vector3 start = previous + side * offset + Vector3(0, level, 0);
                    Vector3 travel = next - previous;
                    const float length = glm::length(travel);
                    if (length < .001f) continue;
                    for (const RaycastHit& hit : scene->RaycastAll(start, travel / length, length + radius, _surface.collisionMask))
                        if (hit.entity && hit.entity->Active() && hit.entity->HasComponent<Rigidbody>() &&
                            hit.entity->GetComponent<Rigidbody>().motionType == RigidbodyMotionType::STATIC)
                            return false;
                }
            previous = next;
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
