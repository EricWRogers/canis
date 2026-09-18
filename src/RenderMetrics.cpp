#include <Canis/RenderMetrics.hpp>
#include <Canis/OpenGL.hpp>
#include <SDL3/SDL.h>
#include <array>
#include <cstdlib>
#include <fstream>
#include <string>
#include <algorithm>
#include <filesystem>

namespace Canis::RenderMetrics
{
    namespace {
        Frame current,latest;
        uint64_t start=0,presentStart=0,nextFrame=0;
        bool shadow=false,active=false;
        constexpr size_t MaxPasses=128;
        struct Query {
            unsigned ids[2]{};
            std::array<unsigned,MaxPasses*2> passIds{};
            uint64_t frame=0, generation=0;
            bool pending=false, record=false;
            PassFrame result;
        };
        std::array<Query,8> queries{};
        Query* issued=nullptr;
        PassFrame latestPasses;
        std::deque<PassFrame> history;
        uint64_t generation=0;
        int depth=0;
        int editorDepth=0;
        std::ofstream output;
        bool outputOpened=false;
        bool Disabled(const char* name) { const char* value=std::getenv(name); return value && std::string(value)=="1"; }
    }
    bool InstancingEnabled() { static const bool enabled=!Disabled("CANIS_DISABLE_INSTANCING"); return enabled; }
    bool CullingEnabled() { static const bool enabled=!Disabled("CANIS_DISABLE_CULLING"); return enabled; }
    bool BatchCacheEnabled() { static const bool enabled=!Disabled("CANIS_DISABLE_BATCH_CACHE"); return enabled; }
    Profiler::Category ProfileCategory() { return editorDepth ? Profiler::Category::Editor : Profiler::Category::Rendering; }
    void InstanceUpload(uint64_t bytes) { if(active)current.instanceUploadBytes+=bytes; }
    void InstanceCacheHit() { if(active)++current.instanceCacheHits; }
    void BatchBuild() { if(active)++current.batchBuilds; }
    void RoomCull(uint64_t count) { if(active)current.roomCulledInstances+=count; }
    const Frame& Latest() { return latest; }
    const PassFrame& LatestPasses() { return latestPasses; }
    const std::deque<PassFrame>& History() { return history; }
    void ClearHistory() { history.clear(); ++generation; }
    bool ExportPasses(const std::string& path) {
        std::error_code error;
        const auto parent=std::filesystem::path(path).parent_path();
        if(!parent.empty())std::filesystem::create_directories(parent,error);
        std::ofstream file(path);
        file<<"frame,frame_gpu_ms,pass,depth,cpu_ms,gpu_ms,gpu_start_ms\n";
        for(const auto& frame:history)for(const auto& pass:frame.passes) {
            std::string escaped;
            for(char c:pass.name) { escaped+=c;if(c=='"')escaped+='"'; }
            file<<frame.id<<','<<frame.gpuMs<<",\""<<escaped<<"\","<<pass.depth<<','<<pass.cpuMs<<','<<pass.gpuMs<<','<<pass.startMs<<'\n';
        }
        file.flush();return bool(file);
    }
    Scope::Scope(const char* name, bool editorOnly):cpu(name,editorOnly || editorDepth ? Profiler::Category::Editor : Profiler::Category::Rendering),editor(editorOnly) {
        start=SDL_GetTicksNS();
        if(editor)++editorDepth;
#ifndef __EMSCRIPTEN__
        if(active && issued && (!editorDepth || Profiler::Get().ProfilesEditor())) {
            frame=issued->frame;
            if(issued->result.passes.size()<MaxPasses) {
                index=static_cast<int>(issued->result.passes.size());
                issued->result.passes.push_back({name,0,0,0,depth});
                auto* ids=&issued->passIds[index*2];
                if(!ids[0])glGenQueries(2,ids);
                glQueryCounter(ids[0],GL_TIMESTAMP);
            } else ++issued->result.dropped;
        }
#endif
        ++depth;
    }
    Scope::~Scope() {
        --depth;
        if(editor)--editorDepth;
#ifndef __EMSCRIPTEN__
        if(index>=0 && issued && issued->frame==frame) {
            glQueryCounter(issued->passIds[index*2+1],GL_TIMESTAMP);
            issued->result.passes[index].cpuMs=double(SDL_GetTicksNS()-start)/1e6;
        }
#endif
    }
    void BeginFrame() {
        current={};current.id=++nextFrame;current.gpuMs=-1;active=true;shadow=false;issued=nullptr;depth=0;
        start=SDL_GetTicksNS();
#ifndef __EMSCRIPTEN__
        if (GLEW_VERSION_3_3 || GLEW_ARB_timer_query) {
            std::vector<PassFrame> completed;
            for (auto& q:queries) if (q.pending) {
                GLint ready=0;glGetQueryObjectiv(q.ids[1],GL_QUERY_RESULT_AVAILABLE,&ready);
                if (!ready) continue;
                GLuint64 begin=0,end=0;
                glGetQueryObjectui64v(q.ids[0],GL_QUERY_RESULT,&begin);
                glGetQueryObjectui64v(q.ids[1],GL_QUERY_RESULT,&end);
                q.result.gpuMs=double(end-begin)/1e6;
                for(size_t i=0;i<q.result.passes.size();++i) {
                    GLuint64 a=0,b=0;
                    glGetQueryObjectui64v(q.passIds[i*2],GL_QUERY_RESULT,&a);
                    glGetQueryObjectui64v(q.passIds[i*2+1],GL_QUERY_RESULT,&b);
                    q.result.passes[i].gpuMs=double(b-a)/1e6;
                    q.result.passes[i].startMs=double(a-begin)/1e6;
                }
                if(q.frame>latestPasses.id)latestPasses=q.result;
                if(q.record && q.generation==generation)completed.push_back(q.result);
                if (q.frame>current.gpuFrame) { current.gpuFrame=q.frame;current.gpuMs=double(end-begin)/1e6; }
                q.pending=false;
            }
            std::sort(completed.begin(),completed.end(),[](const auto& a,const auto& b){return a.id<b.id;});
            for(auto& result:completed)history.push_back(std::move(result));
            while(history.size()>300)history.pop_front();
            for (auto& q:queries) if (!q.pending) {
                if (!q.ids[0]) glGenQueries(2,q.ids);
                q.frame=current.id;q.result={};q.result.id=q.frame;
                q.record=Profiler::Get().IsRecording();q.generation=generation;
                issued=&q;glQueryCounter(q.ids[0],GL_TIMESTAMP);break;
            }
        }
#endif
    }
    void EndSubmit() {
#ifndef __EMSCRIPTEN__
        if (issued) { glQueryCounter(issued->ids[1],GL_TIMESTAMP);issued->pending=true; }
#endif
        presentStart=SDL_GetTicksNS();current.cpuSubmitMs=double(presentStart-start)/1e6;
    }
    void EndPresent() {
        current.presentMs=double(SDL_GetTicksNS()-presentStart)/1e6;latest=current;active=false;
        if (!outputOpened) {
            outputOpened=true;
            if (const char* path=std::getenv("CANIS_RENDER_METRICS_OUTPUT")) {
                output.open(path);
                if (output) output<<"frame,cpu_submit_ms,present_ms,gpu_frame,gpu_ms,model_draws,shadow_draws,instances,triangles,culled,shadow_culled,instance_upload_bytes,instance_cache_hits,batch_builds,room_culled_instances\n";
            }
        }
        if (output.is_open() && output) output<<current.id<<','<<current.cpuSubmitMs<<','<<current.presentMs<<','<<current.gpuFrame<<','
            <<current.gpuMs<<','<<current.draws<<','<<current.shadowDraws<<','<<current.instances<<','<<current.triangles<<','
            <<current.culled<<','<<current.shadowCulled<<','<<current.instanceUploadBytes<<','<<current.instanceCacheHits<<','<<current.batchBuilds<<','<<current.roomCulledInstances<<'\n';
    }
    void Draw(uint64_t triangles,uint64_t instances) { if (active) { ++current.draws;if(shadow)++current.shadowDraws;current.instances+=instances;current.triangles+=triangles*instances; } }
    void Cull(bool isShadow) { if (active) { if(isShadow)++current.shadowCulled;else ++current.culled; } }
    void ShadowPass(bool enabled) { shadow=enabled; }
    void Shutdown() {
        if(const char* path=std::getenv("CANIS_GPU_PROFILE_OUTPUT"))ExportPasses(path);
#ifndef __EMSCRIPTEN__
        for (auto& q:queries) {
            if(q.ids[0])glDeleteQueries(2,q.ids);
            for(size_t i=0;i<MaxPasses;++i)if(q.passIds[i*2])glDeleteQueries(2,&q.passIds[i*2]);
            q={};
        }
#endif
        history.clear();latestPasses={};latest={};
        if(output.is_open())output.close();outputOpened=false;active=false;issued=nullptr;
    }
}
