#include <Canis/Profiler.hpp>
#include <SDL3/SDL.h>
#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cstdlib>

namespace Canis::Profiler
{
    uint64_t Now() { return SDL_GetTicksNS(); }
    Recorder& Get() { static thread_local Recorder recorder; return recorder; }
    const char* CategoryName(Category category)
    {
        static const char* names[] = {"Other", "Scripts", "Physics", "Rendering", "Animation", "Audio", "Editor", "Wait / Present"};
        return names[static_cast<size_t>(category)];
    }
    Category SystemCategory(std::string_view name)
    {
        if (name.find("Physics") != name.npos) return Category::Physics;
        if (name.find("Anim") != name.npos || name.find("Bone") != name.npos) return Category::Animation;
        if (name.find("Render") != name.npos || name.find("Trail") != name.npos || name.find("Particle") != name.npos) return Category::Rendering;
        return Category::Other;
    }
    void Recorder::BeginFrame(uint64_t now)
    {
        active = recording;
        current = {};
        stack.clear();
        if (!active) return;
        current.id = nextId++;
        current.startNs = now;
    }
    int Recorder::Begin(std::string_view name, Category category, uint64_t now)
    {
        if (!active) return -1;
        if (category == Category::Editor && !profileEditor) return -1;
        if (current.samples.size() >= MaxSamples || stack.size() >= 64) { ++current.dropped; return -1; }
        const int index = static_cast<int>(current.samples.size());
        current.samples.push_back({std::string(name),category,stack.empty() ? -1 : stack.back(),
            static_cast<int>(stack.size()),double(now-current.startNs)/1e6});
        stack.push_back(index);
        return index;
    }
    void Recorder::End(int index, uint64_t now)
    {
        if (!active || index < 0 || stack.empty() || stack.back() != index) return;
        auto& sample = current.samples[index];
        sample.totalMs = std::max(0.0,double(now-current.startNs)/1e6-sample.startMs);
        sample.selfMs += sample.totalMs;
        if (sample.parent >= 0) current.samples[sample.parent].selfMs -= sample.totalMs;
        stack.pop_back();
    }
    void Recorder::EndFrame(uint64_t now)
    {
        if (!active) return;
        while (!stack.empty()) End(stack.back(),now);
        current.totalMs = double(now-current.startNs)/1e6;
        double accounted = 0;
        for (auto& sample : current.samples) {
            sample.selfMs = std::max(0.0,sample.selfMs);
            current.categories[static_cast<size_t>(sample.category)] += sample.selfMs;
            accounted += sample.selfMs;
        }
        current.categories[0] += std::max(0.0,current.totalMs-accounted);
        // Optional bounded-memory, whole-session timing capture. Buffered CSV
        // avoids retaining an unbounded trace during headset playtests.
        static thread_local std::ofstream frameOutput;
        static thread_local bool outputInitialized=false;
        if(!outputInitialized) {
            outputInitialized=true;
            if(const char* path=std::getenv("CANIS_CPU_FRAME_OUTPUT")) {
                frameOutput.open(path);
                if(frameOutput)frameOutput<<"frame,start_ns,total_ms,scene_update_ms,game_update_ms,scripts_ms,physics_sync_ms,animation_ms,render_preparation_ms,xr_wait_ms,vr_eyes_ms,present_ms,limiter_ms,xr_swapchain_wait_ms,xr_submit_ms,body_rebuild_ms,body_rebuild_count,physics_ms,physics_step_ms,physics_vehicle_ms,physics_ragdoll_ms,physics_capture_ms,physics_interpolation_ms,physics_wheels_ms,physics_ragdoll_poses_ms,physics_contacts_ms,physics_steps,physics_inline_steps,physics_worker_steps\n";
            }
        }
        if(frameOutput.is_open() && frameOutput) {
            static constexpr const char* names[]={"Scene.Update","Game.Update","C# Scripts.Update",
                "Physics body synchronization","Canis::ModelAnimation3DSystem","Render frame preparation",
                "OpenXR frame wait","VR eye","Present","Frame limiter","OpenXR swapchain wait","OpenXR frame submission","Physics body rebuild"};
            double totals[13]{};
            size_t rebuildCount = 0;
            for(const auto& sample:current.samples)
                for(size_t i=0;i<13;++i)if(sample.name==names[i])totals[i]+=sample.totalMs;
            for(const auto& sample:current.samples) rebuildCount += sample.name == "Physics body rebuild";
            frameOutput<<current.id<<','<<current.startNs<<','<<current.totalMs;
            for(double total:totals)frameOutput<<','<<total;
            frameOutput<<','<<rebuildCount;
            static constexpr const char* physicsNames[]={"Canis::JoltPhysics3DSystem", "Physics simulation step",
                "Physics vehicle synchronization", "Physics ragdoll synchronization", "Physics pose capture",
                "Physics pose interpolation", "Physics wheel poses", "Physics ragdoll poses", "Physics contact events"};
            std::array<double, std::size(physicsNames)> physicsTotals{};
            size_t steps = 0, inlineSteps = 0, workerSteps = 0;
            for (const auto& sample : current.samples) {
                for (size_t i = 0; i < physicsTotals.size(); ++i)
                    if (sample.name == physicsNames[i]) physicsTotals[i] += sample.totalMs;
                steps += sample.name == "Physics simulation step";
                inlineSteps += sample.name == "Jolt inline jobs";
                workerSteps += sample.name == "Jolt worker jobs";
            }
            for (double total : physicsTotals) frameOutput<<','<<total;
            frameOutput<<','<<steps<<','<<inlineSteps<<','<<workerSteps<<'\n';
            if(current.id%90==0)frameOutput.flush();
        }
        if (recording) {
            if (frames.size() == MaxFrames) frames.pop_front();
            frames.push_back(std::move(current));
        }
        active = false;
    }
    bool Recorder::Export(const std::string& path) const
    {
        if (frames.empty() || path.empty()) return false;
        try {
            YAML::Emitter out;
            out.SetMapFormat(YAML::Flow); out.SetSeqFormat(YAML::Flow); out.SetStringFormat(YAML::DoubleQuoted);
            out << YAML::BeginMap << YAML::Key << "traceEvents" << YAML::Value << YAML::BeginSeq;
            for (const auto& frame : frames) for (const auto& sample : frame.samples) {
                out << YAML::BeginMap << YAML::Key << "name" << YAML::Value << sample.name
                    << YAML::Key << "cat" << YAML::Value << CategoryName(sample.category)
                    << YAML::Key << "ph" << YAML::Value << "X"
                    << YAML::Key << "pid" << YAML::Value << 1 << YAML::Key << "tid" << YAML::Value << 1
                    << YAML::Key << "ts" << YAML::Value << double(frame.startNs-frames.front().startNs)/1e3+sample.startMs*1000
                    << YAML::Key << "dur" << YAML::Value << sample.totalMs*1000 << YAML::EndMap;
            }
            out << YAML::EndSeq << YAML::EndMap;
            const auto parent = std::filesystem::path(path).parent_path();
            if (!parent.empty()) std::filesystem::create_directories(parent);
            std::ofstream file(path); file << out.c_str(); file.flush();
            return out.good() && file.good();
        } catch (...) { return false; }
    }
}
