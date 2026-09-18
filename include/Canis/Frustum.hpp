#pragma once
#include <Canis/Math.hpp>
#include <array>
#include <cmath>

namespace Canis
{
    // Homogeneous clip-space rejection is conservative under rotation, shear,
    // negative scale, near-plane crossings and asymmetric stereo projections.
    inline bool BoundsOutsideFrustum(const Matrix4& clipFromLocal,Vector3 minimum,Vector3 maximum)
    {
        std::array<Vector4,8> corners;
        for (int i=0;i<8;++i) {
            corners[i]=clipFromLocal*Vector4((i&1)?maximum.x:minimum.x,(i&2)?maximum.y:minimum.y,(i&4)?maximum.z:minimum.z,1);
            for(int j=0;j<4;++j) if(!std::isfinite(corners[i][j]))return false;
        }
        for(int axis=0;axis<3;++axis)for(float sign:{-1.f,1.f}) {
            bool outside=true;
            for(const auto& corner:corners)
                if(sign*corner[axis]<=corner.w+1e-4f) {outside=false;break;}
            if(outside)return true;
        }
        return false;
    }
}
