#include <Canis/Canis.hpp>
#include <Canis/Yaml.hpp>
#include <SDL3/SDL.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace Canis;
namespace fs=std::filesystem;
static void Check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }

int main(int argc,char** argv)
{
    if (argc>1 && std::string(argv[1])=="--probe") {
        for (const auto& adapter:DetectGpuAdapters())
            std::cout<<adapter.pciAddress<<" "<<adapter.driver<<" type="<<adapter.type<<"\n";
        return 0;
    }
    const auto cwd=fs::current_path();
    const auto temp=fs::temp_directory_path()/("canis-gpu-"+std::to_string(SDL_GetTicksNS()));
    try {
        std::vector<GpuAdapter> adapters={{"0000:c4:00.0","amdgpu",PROJECT_GPU_DEDICATED},
                                         {"0000:c5:00.0","amdgpu",PROJECT_GPU_INTEGRATED}};
        Check(FindPreferredGpu(adapters,PROJECT_GPU_INTEGRATED)==1,"Integrated GPU assumed to be first");
        Check(FindPreferredGpu(adapters,PROJECT_GPU_DEDICATED)==0,"Dedicated GPU assumed to be secondary");
        Check(FindPreferredGpu(adapters,PROJECT_GPU_AUTOMATIC)==-1,"Automatic must retain system selection");
        Check(FindPreferredGpu(adapters,123)==-1,"Invalid preference not normalized");
        Check(FindPreferredGpu({adapters[0]},PROJECT_GPU_INTEGRATED)==-1,"Unavailable GPU must fall back");
        Check(FindPreferredGpu({{"0000:00:02.0","unknown"}},PROJECT_GPU_INTEGRATED)==-1,"Unknown GPU guessed");
        fs::create_directories(temp/"project/project_settings");
        fs::create_directories(temp/"project_settings");
        std::ofstream(temp/"CMakeLists.txt")<<"# isolated workspace\n";
        fs::current_path(temp/"project");
        for (int value:{PROJECT_GPU_AUTOMATIC,PROJECT_GPU_INTEGRATED,PROJECT_GPU_DEDICATED}) {
            GetProjectConfig()=ProjectConfig{};
            GetProjectConfig().gpuPreference=value;
            SetEditorRuntimeEnabled(true);
            Check(SaveProjectConfig(),"Project save failed");
            Check(YAML::LoadFile("../project_settings/project.canis")["gpuPreference"].as<int>()==value,"Source preference not saved");
            GetProjectConfig().gpuPreference=-1;
            Init();
            Check(GetProjectConfig().gpuPreference==value,"Preference did not survive restart");
        }
        std::ofstream("project_settings/project.canis")<<"gpuPreference: 999\n";
        Init(); Check(GetProjectConfig().gpuPreference==PROJECT_GPU_AUTOMATIC,"Invalid saved preference not normalized");
        std::ofstream("project_settings/project.canis")<<"editor: true\n";
        GetProjectConfig().gpuPreference=PROJECT_GPU_DEDICATED;
        Init(); Check(GetProjectConfig().gpuPreference==PROJECT_GPU_AUTOMATIC,"Legacy project not automatic");
        fs::current_path(cwd);fs::remove_all(temp);
        std::cout<<"GPU selection, fallback and project persistence passed\n";
        return 0;
    } catch (const std::exception& error) {
        fs::current_path(cwd);fs::remove_all(temp);
        std::cerr<<error.what()<<"\n";return 1;
    }
}
