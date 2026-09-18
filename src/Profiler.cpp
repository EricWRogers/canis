#include <Canis/Profiler.hpp>
#include <SDL3/SDL.h>
#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <filesystem>
#include <fstream>

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
