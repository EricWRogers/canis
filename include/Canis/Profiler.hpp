#pragma once
#include <array>
#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

namespace Canis::Profiler
{
    enum class Category { Other, Scripts, Physics, Rendering, Animation, Audio, Editor, Wait, Count };
    constexpr size_t CategoryCount = static_cast<size_t>(Category::Count);
    const char* CategoryName(Category category);
    Category SystemCategory(std::string_view name);
    struct Sample
    {
        std::string name;
        Category category = Category::Other;
        int parent = -1;
        int depth = 0;
        double startMs = 0, totalMs = 0, selfMs = 0;
    };
    struct Frame
    {
        uint64_t id = 0, startNs = 0;
        double totalMs = 0;
        size_t dropped = 0;
        std::array<double, CategoryCount> categories{};
        std::vector<Sample> samples;
    };
    class Recorder
    {
        bool recording = false, active = false, profileEditor = true;
        uint64_t nextId = 1;
        Frame current;
        std::vector<int> stack;
        std::deque<Frame> frames;
    public:
        static constexpr size_t MaxFrames = 300, MaxSamples = 4096;
        bool IsRecording() const { return recording; }
        bool ProfilesEditor() const { return profileEditor; }
        void SetProfileEditor(bool value) { profileEditor = value; }
        void SetRecording(bool value) { recording = value; }
        void Clear() { frames.clear(); }
        const std::deque<Frame>& Frames() const { return frames; }
        void BeginFrame(uint64_t now);
        void EndFrame(uint64_t now);
        int Begin(std::string_view name, Category category, uint64_t now);
        void End(int sample, uint64_t now);
        bool Export(const std::string& path) const;
    };
    Recorder& Get();
    uint64_t Now();
    class Scope
    {
        int sample;
    public:
        Scope(std::string_view name, Category category) : sample(Get().Begin(name, category, Now())) {}
        ~Scope() { Get().End(sample, Now()); }
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
    };
    class FrameScope
    {
    public:
        FrameScope() { Get().BeginFrame(Now()); }
        ~FrameScope() { Get().EndFrame(Now()); }
        FrameScope(const FrameScope&) = delete;
        FrameScope& operator=(const FrameScope&) = delete;
    };
    void DrawPanel();
}
