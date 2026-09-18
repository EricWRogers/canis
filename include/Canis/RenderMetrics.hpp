#pragma once
#include <cstdint>
#include <Canis/Profiler.hpp>
#include <deque>
#include <string>
#include <vector>

namespace Canis::RenderMetrics
{
    struct Frame {
        uint64_t id=0, gpuFrame=0, draws=0, shadowDraws=0, instances=0, triangles=0, culled=0, shadowCulled=0;
        double cpuSubmitMs=0, presentMs=0, gpuMs=-1;
        uint64_t instanceUploadBytes=0, instanceCacheHits=0, batchBuilds=0;
        uint64_t roomCulledInstances=0;
    };
    const Frame& Latest();
    struct Pass { std::string name; double cpuMs=0, gpuMs=0, startMs=0; int depth=0; };
    struct PassFrame { uint64_t id=0; double gpuMs=0; std::vector<Pass> passes; size_t dropped=0; };
    const PassFrame& LatestPasses();
    const std::deque<PassFrame>& History();
    void ClearHistory();
    bool ExportPasses(const std::string& path);
    class Scope {
        Profiler::Scope cpu;
        uint64_t start=0, frame=0;
        int index=-1;
        bool editor=false;
    public:
        explicit Scope(const char* name, bool editorOnly=false);
        ~Scope();
        Scope(const Scope&)=delete;
        Scope& operator=(const Scope&)=delete;
    };
    void BeginFrame();
    void EndSubmit();
    void EndPresent();
    void Shutdown();
    void Draw(uint64_t triangles,uint64_t instances=1);
    void Cull(bool shadow);
    void ShadowPass(bool enabled);
    bool InstancingEnabled();
    bool CullingEnabled();
    bool BatchCacheEnabled();
    Profiler::Category ProfileCategory();
    void InstanceUpload(uint64_t bytes);
    void InstanceCacheHit();
    void BatchBuild();
    void RoomCull(uint64_t count);
}
