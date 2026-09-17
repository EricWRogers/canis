#include <Canis/Profiler.hpp>
#include <yaml-cpp/yaml.h>
#include <imgui.h>
#include <filesystem>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace Canis::Profiler;
static void Check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
int main()
{
    try {
        Recorder recorder;
        Check(!recorder.IsRecording(),"Recorder should start paused");
        recorder.BeginFrame(0);
        auto disabled=recorder.Begin("Disabled",Category::Editor,0);
        recorder.End(disabled,1000); recorder.EndFrame(2000);
        Check(disabled==-1 && recorder.Frames().empty(),"Disabled recorder captured samples or frames");
        recorder.SetRecording(true);
        recorder.BeginFrame(1000000);
        auto parent=recorder.Begin("Parent",Category::Editor,1000000);
        auto child=recorder.Begin("Child \"quoted\"",Category::Physics,2000000);
        recorder.End(child,4000000); recorder.End(parent,6000000); recorder.EndFrame(7000000);
        const auto first=recorder.Frames().back();
        Check(first.totalMs==6 && first.samples[0].totalMs==5 && first.samples[0].selfMs==3,"Inclusive/self timing wrong");
        Check(first.samples[1].parent==0 && first.samples[1].depth==1,"Nesting wrong");
        Check(first.categories[0]==1 && first.categories[2]==2 && first.categories[6]==3,"Category totals double-counted");
        auto path=std::filesystem::temp_directory_path()/("canis-profile-"+std::to_string(Now())+".json");
        Check(recorder.Export(path.string()),"Trace export failed");
        auto trace=YAML::LoadFile(path.string())["traceEvents"];
        Check(trace.size()==2 && trace[1]["name"].as<std::string>()=="Child \"quoted\"" && trace[1]["dur"].as<double>()==2000,"Trace units or escaping wrong");
        std::filesystem::remove(path);
        recorder.SetRecording(false); recorder.BeginFrame(8000000); recorder.EndFrame(9000000);
        Check(recorder.Frames().size()==1,"Paused recorder grew");
        recorder.SetRecording(true);
        for (size_t i=0;i<Recorder::MaxFrames+5;++i) { recorder.BeginFrame(i*10000000); recorder.EndFrame(i*10000000+1000); }
        Check(recorder.Frames().size()==Recorder::MaxFrames && first.samples[1].name=="Child \"quoted\"","History not bounded or snapshots invalidated");
        recorder.Clear(); Check(recorder.Frames().empty(),"Clear failed");
        recorder.BeginFrame(0);
        for (size_t i=0;i<Recorder::MaxSamples+12;++i) { int sample=recorder.Begin("Repeated",Category::Scripts,i); recorder.End(sample,i+1); }
        recorder.EndFrame(100000);
        Check(recorder.Frames().back().dropped==12,"Sample cap failed");
        recorder.BeginFrame(200000); auto open=recorder.Begin("Open",Category::Other,200000);
        recorder.Clear(); recorder.End(open,210000); recorder.EndFrame(220000);
        Check(recorder.Frames().size()==1 && recorder.Frames().back().samples[0].totalMs==.01,"Clear invalidated open scopes");

        ImGui::CreateContext();
        auto& io=ImGui::GetIO(); io.IniFilename=nullptr; io.DeltaTime=1.f/60;
        unsigned char* pixels; int width,height; io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
        Check(!Get().IsRecording(),"Global recorder should start paused");
        Get().SetRecording(true);
        Get().BeginFrame(0); int sample=Get().Begin("Scene.Update",Category::Physics,0); Get().End(sample,1000000); Get().EndFrame(2000000);
        for (float panelWidth : {320.f,1000.f}) {
            io.DisplaySize=ImVec2(panelWidth,700);
            ImGui::NewFrame(); ImGui::SetNextWindowSize(io.DisplaySize); ImGui::Begin("Profiler test");
            DrawPanel(); ImGui::End(); ImGui::Render();
            Check(ImGui::GetDrawData()->TotalVtxCount>0,"Profiler panel is blank");
        }
        ImGui::DestroyContext();
        std::cout<<"Profiler nesting, self time, categories, pause, bounded history, export and panel checks passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
