#pragma once
#include <glm/glm.hpp>
#include <yaml-cpp/yaml.h>

namespace YAML {
template <>
struct convert<glm::vec2> {
    inline static Node encode(const glm::vec2 &v) {
        Node node;
        node.push_back(v.x);
        node.push_back(v.y);
        node.SetStyle(EmitterStyle::Flow);
        return node;
    }

    inline static bool decode(const Node &node, glm::vec2 &v) {
        if (!node.IsSequence() || node.size() != 2) {
            return false;
        }
        v.x = node[0].as<float>();
        v.y = node[1].as<float>();
        return true;
    }
};
} // end of YAML namespace