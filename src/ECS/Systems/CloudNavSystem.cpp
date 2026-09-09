#include <Canis/ECS/Systems/CloudNavSystem.hpp>

#include <algorithm>
#include <cmath>

#include <Canis/Scene.hpp>

namespace Canis
{
    CloudNavSystem::CloudNavSystem()
    {
        m_name = type_name<CloudNavSystem>();
    }

    void CloudNavSystem::Update(entt::registry& _registry, float _deltaTime)
    {
        (void)_deltaTime;
        auto view = _registry.view<CloudNavSurface, Transform>();
        for (const entt::entity handle : view)
        {
            CloudNavSurface& surface = view.get<CloudNavSurface>(handle);
            if (surface.active && surface.buildOnReady && surface.entity != nullptr &&
                NeedsBuild(*surface.entity))
            {
                Build(_registry, *surface.entity);
            }
        }
    }

    void CloudNavSystem::OnDestroy()
    {
        m_surfaces.clear();
    }

    bool CloudNavSystem::Build(entt::registry& _registry, Entity& _surfaceEntity)
    {
        (void)_registry;
        if (!_surfaceEntity.HasComponent<CloudNavSurface>() ||
            !_surfaceEntity.HasComponent<Transform>() || scene == nullptr)
            return false;

        CloudNavSurface& surface = _surfaceEntity.GetComponent<CloudNavSurface>();
        std::unique_ptr<RuntimeSurface> runtime = std::make_unique<RuntimeSurface>();
        runtime->revision = surface.revision;

        const Vector3 center = _surfaceEntity.GetComponent<Transform>().GetGlobalPosition();
        runtime->center = center;
        const float spacing = std::max(0.25f, surface.nodeSpacing);
        const Vector3 size = glm::max(glm::abs(surface.size), Vector3(spacing));
        runtime->width = std::max(1, static_cast<int>(std::floor(size.x / spacing)) + 1);
        runtime->height = std::max(1, static_cast<int>(std::floor(size.y / spacing)) + 1);
        runtime->depth = std::max(1, static_cast<int>(std::floor(size.z / spacing)) + 1);
        runtime->gridIds.assign(
            static_cast<std::size_t>(runtime->width * runtime->height * runtime->depth), 0u);

        const auto index = [&runtime](int _x, int _y, int _z)
        {
            return static_cast<std::size_t>(
                (_y * runtime->depth + _z) * runtime->width + _x);
        };
        const Vector3 minimum = center - size * 0.5f;
        for (int y = 0; y < runtime->height; ++y)
        {
            for (int z = 0; z < runtime->depth; ++z)
            {
                for (int x = 0; x < runtime->width; ++x)
                {
                    const Vector3 point = minimum + Vector3(x, y, z) * spacing;
                    if (IsPointClear(surface, point))
                        runtime->gridIds[index(x, y, z)] = runtime->graph.AddPoint(point);
                }
            }
        }

        // An undirected graph only needs half of the 26 neighboring offsets.
        for (int y = 0; y < runtime->height; ++y)
        {
            for (int z = 0; z < runtime->depth; ++z)
            {
                for (int x = 0; x < runtime->width; ++x)
                {
                    const unsigned int from = runtime->gridIds[index(x, y, z)];
                    if (from == 0u)
                        continue;

                    for (int oy = -1; oy <= 1; ++oy)
                    {
                        for (int oz = -1; oz <= 1; ++oz)
                        {
                            for (int ox = -1; ox <= 1; ++ox)
                            {
                                if (ox == 0 && oy == 0 && oz == 0)
                                    continue;
                                if (!(ox > 0 || (ox == 0 && oy > 0) ||
                                    (ox == 0 && oy == 0 && oz > 0)))
                                    continue;

                                const int nx = x + ox;
                                const int ny = y + oy;
                                const int nz = z + oz;
                                if (nx < 0 || ny < 0 || nz < 0 ||
                                    nx >= runtime->width || ny >= runtime->height || nz >= runtime->depth)
                                    continue;

                                const unsigned int to = runtime->gridIds[index(nx, ny, nz)];
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
                }
            }
        }

        runtime->built = true;
        const std::size_t pointCount = runtime->graph.GetPointCount();
        m_surfaces[_surfaceEntity.GetHandle()] = std::move(runtime);
        return pointCount > 0u;
    }

    void CloudNavSystem::Invalidate(const Entity& _surfaceEntity)
    {
        m_surfaces.erase(_surfaceEntity.GetHandle());
    }

    std::vector<Vector3> CloudNavSystem::FindPath(
        entt::registry& _registry,
        Entity& _surfaceEntity,
        const Vector3& _start,
        const Vector3& _destination)
    {
        if (!_surfaceEntity.HasComponent<CloudNavSurface>())
            return {};
        CloudNavSurface& surface = _surfaceEntity.GetComponent<CloudNavSurface>();
        if (NeedsBuild(_surfaceEntity) && !Build(_registry, _surfaceEntity))
            return {};

        const auto found = m_surfaces.find(_surfaceEntity.GetHandle());
        if (found == m_surfaces.end() || !found->second)
            return {};
        RuntimeSurface& runtime = *found->second;
        const unsigned int start = runtime.graph.GetClosestPoint(_start);
        const unsigned int destination = runtime.graph.GetClosestPoint(_destination);
        std::vector<Vector3> path = runtime.graph.GetPath(start, destination);
        if (!path.empty() && HasClearance(surface, path.back(), _destination))
            path.push_back(_destination);
        return path;
    }

    std::size_t CloudNavSystem::GetPointCount(const Entity& _surfaceEntity) const
    {
        const auto found = m_surfaces.find(_surfaceEntity.GetHandle());
        return found == m_surfaces.end() || !found->second || !found->second->built
            ? 0u : found->second->graph.GetPointCount();
    }

    bool CloudNavSystem::NeedsBuild(const Entity& _surfaceEntity) const
    {
        if (!_surfaceEntity.HasComponent<CloudNavSurface>() ||
            !_surfaceEntity.HasComponent<Transform>())
            return false;
        const CloudNavSurface& surface = _surfaceEntity.GetComponent<CloudNavSurface>();
        const auto found = m_surfaces.find(_surfaceEntity.GetHandle());
        return found == m_surfaces.end() || !found->second || !found->second->built ||
            found->second->revision != surface.revision ||
            glm::distance(found->second->center,
                _surfaceEntity.GetComponent<Transform>().GetGlobalPosition()) > 0.0001f;
    }

    void CloudNavSystem::DrawVisualization(entt::registry& _registry, Entity* _selectedSurface)
    {
        if (scene == nullptr)
            return;

        const Color edgeColor(0.55f, 0.25f, 1.0f, 0.45f);
        const Color nodeColor(0.9f, 0.45f, 1.0f, 1.0f);
        const Color boundsColor(0.75f, 0.35f, 1.0f, 0.9f);
        if (_selectedSurface != nullptr &&
            _selectedSurface->HasComponent<CloudNavSurface>() &&
            _selectedSurface->HasComponent<Transform>())
        {
            const CloudNavSurface& surface = _selectedSurface->GetComponent<CloudNavSurface>();
            if (surface.showDebug)
            {
                const Vector3 center = _selectedSurface->GetComponent<Transform>().GetGlobalPosition();
                const Vector3 half = glm::abs(surface.size) * 0.5f;
                const Vector3 corners[8] = {
                    center + Vector3(-half.x, -half.y, -half.z), center + Vector3(half.x, -half.y, -half.z),
                    center + Vector3(half.x, -half.y, half.z), center + Vector3(-half.x, -half.y, half.z),
                    center + Vector3(-half.x, half.y, -half.z), center + Vector3(half.x, half.y, -half.z),
                    center + Vector3(half.x, half.y, half.z), center + Vector3(-half.x, half.y, half.z)
                };
                constexpr int edges[12][2] = {
                    {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}
                };
                for (const auto& edge : edges)
                    scene->DrawDebugGizmoLine(corners[edge[0]], corners[edge[1]], boundsColor);
            }
        }

        for (const auto& [handle, runtime] : m_surfaces)
        {
            if (!runtime || !runtime->built || !_registry.valid(handle))
                continue;
            CloudNavSurface* component = _registry.try_get<CloudNavSurface>(handle);
            Entity* surfaceEntity = component != nullptr ? component->entity : nullptr;
            if (surfaceEntity == nullptr ||
                (_selectedSurface != nullptr && surfaceEntity != _selectedSurface) ||
                !component->showDebug)
                continue;

            const std::vector<AStarNode>& nodes = runtime->graph.GetNodes();
            for (unsigned int id = 1u; id < nodes.size(); ++id)
            {
                const Vector3 point = nodes[id].position;
                constexpr float marker = 0.09f;
                scene->DrawDebugGizmoLine(point - Vector3(marker, 0, 0), point + Vector3(marker, 0, 0), nodeColor);
                scene->DrawDebugGizmoLine(point - Vector3(0, marker, 0), point + Vector3(0, marker, 0), nodeColor);
                scene->DrawDebugGizmoLine(point - Vector3(0, 0, marker), point + Vector3(0, 0, marker), nodeColor);
                for (const unsigned int adjacent : nodes[id].adjacentPointIDs)
                {
                    if (adjacent > id && adjacent < nodes.size())
                        scene->DrawDebugGizmoLine(point, nodes[adjacent].position, edgeColor);
                }
            }
        }
    }

    bool CloudNavSystem::IsPointClear(const CloudNavSurface& _surface, const Vector3& _point) const
    {
        const float radius = std::max(0.05f, _surface.agentRadius);
        const Vector3 directions[6] = {
            Vector3(1,0,0), Vector3(-1,0,0), Vector3(0,1,0),
            Vector3(0,-1,0), Vector3(0,0,1), Vector3(0,0,-1)
        };
        for (const Vector3& direction : directions)
        {
            for (const RaycastHit& hit : scene->RaycastAll(_point, direction, radius, _surface.collisionMask))
            {
                if (IsBlockingHit(_surface, hit.entity) && hit.distance < radius)
                    return false;
            }
        }
        return true;
    }

    bool CloudNavSystem::HasClearance(
        const CloudNavSurface& _surface,
        const Vector3& _from,
        const Vector3& _to) const
    {
        Vector3 delta = _to - _from;
        const float distance = glm::length(delta);
        if (distance < 0.001f)
            return true;
        const Vector3 direction = delta / distance;
        const float radius = std::max(0.0f, _surface.agentRadius);
        const Vector3 offsets[7] = {
            Vector3(0), Vector3(radius,0,0), Vector3(-radius,0,0),
            Vector3(0,radius,0), Vector3(0,-radius,0),
            Vector3(0,0,radius), Vector3(0,0,-radius)
        };
        for (const Vector3& offset : offsets)
        {
            for (const RaycastHit& hit : scene->RaycastAll(
                _from + offset, direction, distance, _surface.collisionMask))
            {
                if (IsBlockingHit(_surface, hit.entity) && hit.distance < distance - 0.05f)
                    return false;
            }
        }
        return true;
    }

    bool CloudNavSystem::IsBlockingHit(
        const CloudNavSurface& _surface,
        const Entity* _entity) const
    {
        if (_entity == nullptr || _entity == _surface.entity || !_entity->Active() ||
            !_entity->HasComponent<Rigidbody>())
            return false;
        const Rigidbody& body = _entity->GetComponent<Rigidbody>();
        return body.active && body.motionType == RigidbodyMotionType::STATIC;
    }
}
