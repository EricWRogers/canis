#pragma once
#include <Canis/Math.hpp>
#include <array>
#include <cmath>

namespace Canis {
// World-aligned influence/projection box. Captures are transient and rebuilt on load.
struct LightingProbe {
    bool enabled = true;
    Vector3 position{0}, halfExtents{5};
    float blendDistance = 1, diffuseIntensity = 1, specularIntensity = 1;
    float nearClip = .1f, farClip = 100;
    bool operator==(const LightingProbe&) const = default;
    void Sanitize() {
        auto finite=[](float x,float fallback) { return std::isfinite(x)?x:fallback; };
        for(int i=0;i<3;++i) {
            position[i]=finite(position[i],0);
            halfExtents[i]=glm::clamp(finite(halfExtents[i],5),.01f,10000.f);
        }
        blendDistance=glm::clamp(finite(blendDistance,1),.001f,10000.f);
        diffuseIntensity=glm::clamp(finite(diffuseIntensity,1),0.f,16.f);
        specularIntensity=glm::clamp(finite(specularIntensity,1),0.f,16.f);
        nearClip=glm::clamp(finite(nearClip,.1f),.01f,100.f);
        farClip=glm::clamp(finite(farClip,100),nearClip+.01f,20000.f);
    }
};
inline std::array<float,9> ProbeSHBasis(Vector3 n) {
    return {.282095f,.488603f*n.y,.488603f*n.z,.488603f*n.x,
        1.092548f*n.x*n.y,1.092548f*n.y*n.z,.315392f*(3*n.z*n.z-1),
        1.092548f*n.x*n.z,.546274f*(n.x*n.x-n.y*n.y)};
}
inline Vector3 ProbeTexelDirection(int face,float u,float v) {
    switch(face) {
        case 0:return glm::normalize(Vector3(1,-v,-u));
        case 1:return glm::normalize(Vector3(-1,-v,u));
        case 2:return glm::normalize(Vector3(u,1,v));
        case 3:return glm::normalize(Vector3(u,-1,-v));
        case 4:return glm::normalize(Vector3(u,-v,1));
        default:return glm::normalize(Vector3(-u,-v,-1));
    }
}
}
