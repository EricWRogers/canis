#include <Canis/Blockout.hpp>
#include <algorithm>
#include <cmath>
#include <map>
#include <queue>
#include <set>
#include <tuple>

namespace Canis
{
    namespace
    {
        using Edge = std::pair<u32, u32>;
        Edge Key(u32 a, u32 b)
        {
            return std::minmax(a, b);
        }
        Vector3 Normal(const EditableBlockoutMesh &m, const BlockoutFace &f)
        {
            Vector3 n(0);
            for (size_t i = 0; i < f.vertices.size(); ++i)
                n += glm::cross(m.vertices[f.vertices[i]],
                                m.vertices[f.vertices[(i + 1) % f.vertices.size()]]);
            return glm::length(n) > 1e-6f ? glm::normalize(n) : Vector3(0);
        }
    } // namespace

    bool ValidateBlockoutTopology(const EditableBlockoutMesh &m, std::string &error)
    {
        auto fail = [&](const char *s) {
            error = s;
            return false;
        };
        if (m.vertices.size() > 100000 || m.faces.size() > 100000)
            return fail("Mesh exceeds the editing limit.");
        std::map<Edge, std::vector<Edge>> edges;
        size_t count = 0;
        for (const auto &f : m.faces)
        {
            if (f.vertices.empty())
                continue;
            ++count;
            if (f.vertices.size() < 3 || f.vertices.size() != f.uv.size())
                return fail("Invalid face data.");
            std::set<u32> unique;
            for (size_t i = 0; i < f.vertices.size(); ++i)
            {
                u32 v = f.vertices[i];
                if (v >= m.vertices.size() || !unique.insert(v).second)
                    return fail("Invalid face vertex.");
                auto p = m.vertices[v];
                auto uv = f.uv[i];
                if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) ||
                    !std::isfinite(uv.x) || !std::isfinite(uv.y))
                    return fail("Non-finite mesh value.");
            }
            Vector3 n = Normal(m, f);
            if (glm::length(n) < 0.5f)
                return fail("Operation would collapse a face.");
            for (size_t i = 0; i < f.vertices.size(); ++i)
            {
                u32 a = f.vertices[i], b = f.vertices[(i + 1) % f.vertices.size()],
                    c = f.vertices[(i + 2) % f.vertices.size()];
                Vector3 ab = m.vertices[b] - m.vertices[a], bc = m.vertices[c] - m.vertices[b];
                if (glm::length(ab) < 1e-5f)
                    return fail("Operation would collapse an edge.");
                if (glm::dot(glm::cross(ab, bc), n) < -1e-5f)
                    return fail("Concave faces are not supported.");
                auto &uses = edges[Key(a, b)];
                if (uses.size() >= 2 || (!uses.empty() && uses[0] == Edge(a, b)))
                    return fail("Operation would create non-manifold or reversed faces.");
                uses.push_back({a, b});
            }
        }
        if (!count)
            return fail("A blockout must contain at least one face.");
        error.clear();
        return true;
    }

    bool ConvertBlockoutToEditable(BlockoutShape &shape, std::string &error)
    {
        if (shape.meshEdited)
            return ValidateBlockoutTopology(shape.editMesh, error);
        BlockoutMeshData render;
        if (!BuildBlockoutMesh(shape, render, &error))
            return false;
        if (shape.type == BlockoutShapeType::STAIRS)
        {
            // Convert the stair union, removing interior cell faces. The procedural
            // renderer uses overlapping boxes, which are not editable manifold topology.
            render = {};
            int steps = std::clamp(shape.stepCount, 1, 64);
            Vector3 size = glm::max(glm::abs(shape.size), Vector3(0.01f));
            for (int z = 0; z < steps; ++z)
                for (int y = 0; y <= z; ++y)
                {
                    BlockoutShape cell;
                    cell.size = {size.x, size.y / steps, size.z / steps};
                    cell.uvScale = shape.uvScale;
                    BlockoutMeshData data;
                    BuildBlockoutMesh(cell, data);
                    u32 base = static_cast<u32>(render.vertices.size());
                    for (auto v : data.vertices)
                    {
                        v.position += Vector3(0, y * cell.size.y, -size.z * 0.5f + (z + 0.5f) * cell.size.z);
                        render.vertices.push_back(v);
                    }
                    for (auto f : data.faces)
                    {
                        for (auto &v : f.vertices)
                            v += base;
                        render.faces.push_back(std::move(f));
                    }
                }
        }
        EditableBlockoutMesh mesh;
        std::vector<u32> remap;
        std::map<std::tuple<long long, long long, long long>, u32> welded;
        for (const auto &v : render.vertices)
        {
            auto key = std::make_tuple(std::llround(v.position.x * 10000), std::llround(v.position.y * 10000),
                                       std::llround(v.position.z * 10000));
            auto [it, added] = welded.emplace(key, static_cast<u32>(mesh.vertices.size()));
            u32 id = it->second;
            if (added)
                mesh.vertices.push_back(v.position);
            remap.push_back(id);
        }
        std::map<std::vector<u32>, u32> surfaces;
        for (auto f : render.faces)
        {
            for (auto &v : f.vertices)
                v = remap[v];
            if (shape.type == BlockoutShapeType::STAIRS)
            {
                auto key = f.vertices;
                std::sort(key.begin(), key.end());
                auto [it, added] = surfaces.emplace(key, static_cast<u32>(mesh.faces.size()));
                if (!added)
                {
                    mesh.faces[it->second] = {};
                    continue;
                }
            }
            mesh.faces.push_back(std::move(f));
        }
        if (!ValidateBlockoutTopology(mesh, error))
            return false;
        shape.editMesh = std::move(mesh);
        shape.meshEdited = true;
        shape.MarkDirty();
        return true;
    }

    bool ExtrudeBlockoutFaces(EditableBlockoutMesh &mesh, const std::vector<u32> &ids, float distance,
                              std::string &error)
    {
        if (ids.empty() || !std::isfinite(distance) || std::abs(distance) < 0.001f)
        {
            error = "Select faces and use a nonzero extrusion distance.";
            return false;
        }
        auto m = mesh;
        std::set<u32> selected(ids.begin(), ids.end());
        std::map<Edge, int> uses;
        std::map<u32, u32> copies;
        Vector3 normal(0);
        for (u32 id : selected)
        {
            if (id >= m.faces.size() || m.faces[id].vertices.empty())
            {
                error = "Invalid face selection.";
                return false;
            }
            const auto &f = m.faces[id];
            normal += Normal(m, f);
            for (size_t i = 0; i < f.vertices.size(); ++i)
                ++uses[Key(f.vertices[i], f.vertices[(i + 1) % f.vertices.size()])];
        }
        if (glm::length(normal) < 1e-5f)
        {
            error = "Selected faces have opposing normals.";
            return false;
        }
        normal = glm::normalize(normal);
        bool boundary = false;
        for (u32 id : selected)
        {
            auto f = m.faces[id];
            for (u32 v : f.vertices)
                if (!copies.count(v))
                {
                    copies[v] = static_cast<u32>(m.vertices.size());
                    m.vertices.push_back(m.vertices[v] + normal * distance);
                }
            for (size_t i = 0; i < f.vertices.size(); ++i)
            {
                u32 a = f.vertices[i], b = f.vertices[(i + 1) % f.vertices.size()];
                if (uses[Key(a, b)] != 1)
                    continue;
                boundary = true;
                float length = glm::length(m.vertices[b] - m.vertices[a]);
                m.faces.push_back(
                    {{a, b, copies[b], copies[a]},
                     {{0, 0}, {length, 0}, {length, std::abs(distance)}, {0, std::abs(distance)}}});
            }
            for (auto &v : f.vertices)
                v = copies[v];
            m.faces[id] = std::move(f);
        }
        if (!boundary)
        {
            error = "Select a face region with a boundary.";
            return false;
        }
        if (!ValidateBlockoutTopology(m, error))
            return false;
        mesh = std::move(m);
        return true;
    }

    bool CutBlockoutLoop(EditableBlockoutMesh &mesh, u32 a, u32 b, int cuts, std::string &error)
    {
        if (cuts < 1 || cuts > 32)
        {
            error = "Use 1 to 32 cuts.";
            return false;
        }
        std::map<Edge, std::vector<std::pair<u32, int>>> adjacency;
        for (u32 id = 0; id < mesh.faces.size(); ++id)
        {
            const auto &f = mesh.faces[id];
            for (int i = 0; i < static_cast<int>(f.vertices.size()); ++i)
                adjacency[Key(f.vertices[i], f.vertices[(i + 1) % f.vertices.size()])].push_back({id, i});
        }
        std::queue<Edge> pending;
        pending.push(Key(a, b));
        std::set<Edge> ring;
        std::map<u32, int> crossed;
        while (!pending.empty())
        {
            Edge edge = pending.front();
            pending.pop();
            if (!ring.insert(edge).second)
                continue;
            if (!adjacency.count(edge) || adjacency[edge].size() != 2)
            {
                error = "No continuous closed quad loop.";
                return false;
            }
            for (auto [id, i] : adjacency[edge])
            {
                const auto &f = mesh.faces[id];
                if (f.vertices.size() != 4)
                {
                    error = "Loop cuts require quad faces.";
                    return false;
                }
                if (crossed.count(id) && (crossed[id] % 2) != (i % 2))
                {
                    error = "Loop crosses itself.";
                    return false;
                }
                crossed[id] = i;
                pending.push(Key(f.vertices[(i + 2) % 4], f.vertices[(i + 3) % 4]));
            }
        }
        auto m = mesh;
        std::map<Edge, std::vector<u32>> split;
        for (Edge edge : ring)
        {
            auto &v = split[edge];
            v.push_back(edge.first);
            for (int i = 1; i <= cuts; ++i)
            {
                v.push_back(static_cast<u32>(m.vertices.size()));
                m.vertices.push_back(
                    glm::mix(m.vertices[edge.first], m.vertices[edge.second], float(i) / float(cuts + 1)));
            }
            v.push_back(edge.second);
        }
        auto ordered = [&](u32 x, u32 y) {
            auto v = split[Key(x, y)];
            if (x > y)
                std::reverse(v.begin(), v.end());
            return v;
        };
        for (auto [id, i] : crossed)
        {
            auto f = mesh.faces[id];
            auto left = ordered(f.vertices[i], f.vertices[(i + 1) % 4]);
            auto right = ordered(f.vertices[(i + 3) % 4], f.vertices[(i + 2) % 4]);
            m.faces[id] = {};
            for (int j = 0; j <= cuts; ++j)
            {
                float t = float(j) / float(cuts + 1), s = float(j + 1) / float(cuts + 1);
                BlockoutFace q{{left[j], left[j + 1], right[j + 1], right[j]},
                               {glm::mix(f.uv[i], f.uv[(i + 1) % 4], t),
                                glm::mix(f.uv[i], f.uv[(i + 1) % 4], s),
                                glm::mix(f.uv[(i + 3) % 4], f.uv[(i + 2) % 4], s),
                                glm::mix(f.uv[(i + 3) % 4], f.uv[(i + 2) % 4], t)}};
                if (j == 0)
                    m.faces[id] = q;
                else
                    m.faces.push_back(q);
            }
        }
        if (!ValidateBlockoutTopology(m, error))
            return false;
        mesh = std::move(m);
        return true;
    }

    bool DissolveBlockoutEdge(EditableBlockoutMesh &mesh, u32 a, u32 b, std::string &error)
    {
        std::vector<u32> faces;
        for (u32 id = 0; id < mesh.faces.size(); ++id)
        {
            const auto &f = mesh.faces[id];
            for (size_t i = 0; i < f.vertices.size(); ++i)
                if (Key(f.vertices[i], f.vertices[(i + 1) % f.vertices.size()]) == Key(a, b))
                    faces.push_back(id);
        }
        if (faces.size() != 2)
        {
            error = "Choose one edge between two coplanar faces.";
            return false;
        }
        if (glm::dot(Normal(mesh, mesh.faces[faces[0]]), Normal(mesh, mesh.faces[faces[1]])) < 0.9999f)
        {
            error = "Only coplanar faces can be dissolved.";
            return false;
        }
        std::map<u32, std::pair<u32, Vector2>> boundary;
        for (u32 id : faces)
        {
            const auto &f = mesh.faces[id];
            for (size_t i = 0; i < f.vertices.size(); ++i)
            {
                u32 x = f.vertices[i], y = f.vertices[(i + 1) % f.vertices.size()];
                if (Key(x, y) != Key(a, b))
                    boundary[x] = {y, f.uv[i]};
            }
        }
        BlockoutFace merged;
        u32 start = boundary.begin()->first, v = start;
        do
        {
            if (!boundary.count(v) || merged.vertices.size() > boundary.size())
            {
                error = "Invalid boundary.";
                return false;
            }
            merged.vertices.push_back(v);
            merged.uv.push_back(boundary[v].second);
            v = boundary[v].first;
        } while (v != start);
        const Vector3 planeNormal = Normal(mesh, mesh.faces[faces[0]]);
        const Vector3 planePoint = mesh.vertices[merged.vertices[0]];
        for (u32 id : merged.vertices)
            if (std::abs(glm::dot(mesh.vertices[id] - planePoint, planeNormal)) > 0.002f)
            {
                error = "Only coplanar faces can be dissolved.";
                return false;
            }
        auto m = mesh;
        m.faces[faces[0]] = merged;
        m.faces[faces[1]] = {};
        if (!ValidateBlockoutTopology(m, error))
            return false;
        mesh = std::move(m);
        return true;
    }
} // namespace Canis
