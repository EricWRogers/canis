#include <Canis/RectLayoutSerialization.hpp>
#include <Canis/Yaml.hpp>
#include <Canis/Components.hpp>
#include <algorithm>

using namespace Canis;
namespace Canis
{
    bool MigrateRectLayoutEntity(YAML::Node& node)
    {
        const YAML::Node legacy = static_cast<const YAML::Node&>(node)["Canis::RectTransform"];
        if (!legacy || node[RectLayout::ScriptName]) return false;
        YAML::Node spatial = node[Transform::ScriptName] ? YAML::Clone(node[Transform::ScriptName]) : YAML::Node(YAML::NodeType::Map);
        const Vector2 xy = legacy["position"].as<Vector2>(Vector2(0));
        const Vector2 scale = legacy["scale"].as<Vector2>(Vector2(1));
        const float angle = legacy["rotation"].as<float>(0), depth = legacy["depth"].as<float>(.001f);
        const auto canvas = static_cast<const YAML::Node&>(node)[Canvas::ScriptName];
        const bool worldCanvas = canvas && canvas["renderMode"].as<int>(0) == CanvasRenderMode::WORLD_SPACE;
        const Vector3 oldPosition = spatial["position"].as<Vector3>(Vector3(0));
        const Quaternion oldRotation = spatial["rotation"].as<Quaternion>(Quaternion(Vector3(0)));
        const Vector3 oldScale = spatial["scale"].as<Vector3>(Vector3(1));
        if (worldCanvas)
        {
            // Canvas-local layout becomes identity; its former 2D pose is
            // incorporated into the existing physical canvas pose.
            spatial["position"] = oldPosition + oldRotation * (oldScale * Vector3(xy, depth));
            spatial["rotation"] = glm::normalize(oldRotation * Quaternion(Vector3(0, 0, angle)));
            spatial["scale"] = oldScale * Vector3(scale, 1);
        }
        else
        {
            spatial["position"] = Vector3(xy, depth);
            spatial["rotation"] = Quaternion(Vector3(0, 0, angle));
            spatial["scale"] = Vector3(scale, 1);
        }
        spatial["active"] = spatial["active"].as<bool>(true);
        const auto rectParent = legacy["parent"].as<uint64_t>(0);
        spatial["parent"] = rectParent ? rectParent : spatial["parent"].as<uint64_t>(0);
        YAML::Node children(YAML::NodeType::Sequence);
        std::vector<uint64_t> seen;
        for (const auto& data : {spatial["children"], legacy["children"]})
            if (data && data.IsSequence()) for (auto child : data) {
                const auto id = child.as<uint64_t>(0);
                if (id && std::find(seen.begin(), seen.end(), id) == seen.end()) { seen.push_back(id); children.push_back(id); }
            }
        spatial["children"] = children;
        YAML::Node layout(YAML::NodeType::Map);
        for (const char* field : {"active", "size", "anchorMin", "anchorMax", "pivot", "originOffset", "rotationOriginOffset"})
            if (legacy[field]) layout[field] = YAML::Clone(legacy[field]);
        node[Transform::ScriptName] = spatial;
        node[RectLayout::ScriptName] = layout;
        node.remove("Canis::RectTransform");
        return true;
    }
}
