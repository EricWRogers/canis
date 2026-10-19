#pragma once
namespace Canis {
// Expanded by Shader::CompileSource at the explicit [CANIS PROBE LIGHTING] marker.
inline constexpr const char* ProbeLightingGLSL=R"GLSL(
#ifndef CANIS_PROBE_ARRAYS
#define CANIS_PROBE_ARRAYS 0
#endif
uniform bool canisProbeCapture;
uniform int canisProbeCount;
#if CANIS_PROBE_ARRAYS
uniform samplerCubeArray canisProbeArray;
uniform int canisProbeLayerFrom,canisProbeLayerTo;
uniform float canisProbeBlend;
#else
uniform samplerCube canisProbeMap0,canisProbeMap1,canisProbeMap2,canisProbeMap3,canisProbeMap4,canisProbeMap5;
#endif
uniform vec3 canisProbePosition[6],canisProbeExtent[6],canisProbeParams[6];
uniform vec3 canisProbeSH[54];
vec3 CanisProbeLighting(vec3 p,vec3 n,vec3 v,vec3 albedo,float roughness,vec3 f0,float diffuseWeight) {
    if(canisProbeCapture || canisProbeCount==0)return vec3(0);
    float b[9];b[0]=.282095;b[1]=.488603*n.y;b[2]=.488603*n.z;b[3]=.488603*n.x;
    b[4]=1.092548*n.x*n.y;b[5]=1.092548*n.y*n.z;b[6]=.315392*(3.*n.z*n.z-1.);
    b[7]=1.092548*n.x*n.z;b[8]=.546274*(n.x*n.x-n.y*n.y);
    vec3 result=vec3(0);float total=0.;vec3 r=reflect(-v,n);
    vec3 fresnel=f0+(1.-f0)*pow(1.-clamp(dot(n,v),0.,1.),5.);
    for(int i=0;i<6;i++) {
        if(i>=canisProbeCount)break;
        vec3 local=p-canisProbePosition[i],extent=canisProbeExtent[i];
        // Fade beyond the projection box so its floor/wall surfaces retain
        // reflections. Fading inward made reflections vanish at those surfaces.
        vec3 edge=extent+vec3(canisProbeParams[i].x)-abs(local);
        float w=clamp(min(edge.x,min(edge.y,edge.z))/max(canisProbeParams[i].x,.001),0.,1.);
        if(w<=0.)continue;
        // Ray-box exit point corrects reflection parallax inside the influence box.
        vec3 safeR=mix(vec3(-1.),vec3(1.),greaterThanEqual(r,vec3(0)))*max(abs(r),vec3(.00001));
        vec3 distances=(mix(-extent,extent,greaterThanEqual(r,vec3(0)))-local)/safeR;
        float exitDistance=min(distances.x,min(distances.y,distances.z));
        // Reject a neighboring room on the opposite side of a floor or wall.
        if(exitDistance<=.0001)continue;
        vec3 direction=local+r*exitDistance;
        vec3 reflected=vec3(0);
        if(canisProbeParams[i].z>0.) {
            float lod=clamp(roughness,0.,1.)*6.;
#if CANIS_PROBE_ARRAYS
            if(canisProbeBlend<=0.)reflected=textureLod(canisProbeArray,vec4(direction,float(canisProbeLayerFrom+i)),lod).rgb;
            else if(canisProbeBlend>=1.)reflected=textureLod(canisProbeArray,vec4(direction,float(canisProbeLayerTo+i)),lod).rgb;
            else reflected=mix(textureLod(canisProbeArray,vec4(direction,float(canisProbeLayerFrom+i)),lod).rgb,
                textureLod(canisProbeArray,vec4(direction,float(canisProbeLayerTo+i)),lod).rgb,canisProbeBlend);
#else
            if(i==0)reflected=textureLod(canisProbeMap0,direction,lod).rgb;
            else if(i==1)reflected=textureLod(canisProbeMap1,direction,lod).rgb;
            else if(i==2)reflected=textureLod(canisProbeMap2,direction,lod).rgb;
            else if(i==3)reflected=textureLod(canisProbeMap3,direction,lod).rgb;
            else if(i==4)reflected=textureLod(canisProbeMap4,direction,lod).rgb;
            else reflected=textureLod(canisProbeMap5,direction,lod).rgb;
#endif
        }
        vec3 irradiance=vec3(0);
        for(int j=0;j<9;j++)irradiance+=canisProbeSH[i*9+j]*b[j];
        result+=w*(max(irradiance,vec3(0))*albedo*(1.-fresnel)*diffuseWeight*canisProbeParams[i].y
            +reflected*fresnel*canisProbeParams[i].z);
        total+=w;
    }
    return result/max(total,1.);
}
)GLSL";
}
