#include <Canis/GpuPreference.hpp>
#include <Canis/Debug.hpp>
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#ifdef __linux__
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#if __has_include(<libdrm/amdgpu_drm.h>)
#include <libdrm/amdgpu_drm.h>
#define CANIS_QUERY_AMDGPU 1
#endif
#endif

namespace Canis
{
    namespace { std::string startupWarning; }

    const std::string& GetGpuPreferenceWarning() { return startupWarning; }

    int FindPreferredGpu(const std::vector<GpuAdapter>& adapters,int preference)
    {
        preference=NormalizeGpuPreference(preference);
        if (preference==PROJECT_GPU_AUTOMATIC) return -1;
        for (size_t i=0;i<adapters.size();++i)
            if (adapters[i].type==preference) return static_cast<int>(i);
        return -1;
    }

    std::vector<GpuAdapter> DetectGpuAdapters()
    {
        std::vector<GpuAdapter> adapters;
#ifdef __linux__
        namespace fs=std::filesystem;
        std::error_code error;
        for (const auto& entry:fs::directory_iterator("/sys/class/drm",error)) {
            const auto name=entry.path().filename().string();
            if (name.rfind("renderD",0)!=0) continue;
            auto device=fs::canonical(entry.path()/"device",error);
            if (error) { error.clear(); continue; }
            unsigned vendor=0;
            std::ifstream(device/"vendor") >> std::hex >> vendor;
            if (!vendor) continue;
            const auto driverPath=fs::canonical(device/"driver",error);
            if (error) { error.clear(); continue; }
            GpuAdapter adapter{device.filename().string(),driverPath.filename().string()};
            if (vendor==0x10de) adapter.type=PROJECT_GPU_DEDICATED;
#ifdef CANIS_QUERY_AMDGPU
            if (adapter.driver=="amdgpu") {
                // The driver's APU flag distinguishes integrated AMD graphics from discrete VRAM GPUs.
                const int fd=open((fs::path("/dev/dri")/name).c_str(),O_RDWR|O_CLOEXEC);
                if (fd>=0) {
                    drm_amdgpu_info_device info{};
                    drm_amdgpu_info request{};
                    request.return_pointer=reinterpret_cast<uintptr_t>(&info);
                    request.return_size=sizeof(info);
                    request.query=AMDGPU_INFO_DEV_INFO;
                    if (ioctl(fd,DRM_IOCTL_AMDGPU_INFO,&request)==0)
                        adapter.type=(info.ids_flags & AMDGPU_IDS_FLAGS_FUSION) ? PROJECT_GPU_INTEGRATED : PROJECT_GPU_DEDICATED;
                    else Debug::Warning("GPU device query failed for %s: %s",adapter.pciAddress.c_str(),std::strerror(errno));
                    close(fd);
                }
                else Debug::Warning("Cannot open GPU %s: %s",adapter.pciAddress.c_str(),std::strerror(errno));
            }
#endif
            adapters.push_back(adapter);
        }
        std::sort(adapters.begin(),adapters.end(),[](const auto& a,const auto& b) { return a.pciAddress<b.pciAddress; });
#endif
        return adapters;
    }

    void ApplyGpuPreference(int preference)
    {
        startupWarning.clear();
        preference=NormalizeGpuPreference(preference);
        if (preference==PROJECT_GPU_AUTOMATIC) return;
#ifdef __linux__
        const auto adapters=DetectGpuAdapters();
        const int selected=FindPreferredGpu(adapters,preference);
        if (selected<0) {
            startupWarning="Preferred GPU unavailable; using system selection.";
            Debug::Warning("Preferred %s GPU unavailable or unclassified; keeping system GPU selection.",
                preference==PROJECT_GPU_INTEGRATED ? "integrated" : "dedicated");
            return;
        }
        const auto& adapter=adapters[selected];
        std::string pci=adapter.pciAddress;
        std::replace(pci.begin(),pci.end(),':','_');
        std::replace(pci.begin(),pci.end(),'.','_');
        setenv("DRI_PRIME",("pci-"+pci).c_str(),1);
        unsetenv("__NV_PRIME_RENDER_OFFLOAD_PROVIDER");
        if (adapter.driver=="nvidia") {
            setenv("__NV_PRIME_RENDER_OFFLOAD","1",1);
            setenv("__GLX_VENDOR_LIBRARY_NAME","nvidia",1);
        } else {
            unsetenv("__NV_PRIME_RENDER_OFFLOAD");
            // Explicitly avoid inherited NVIDIA GLX routing when returning to Mesa.
            setenv("__GLX_VENDOR_LIBRARY_NAME","mesa",1);
        }
        Debug::Log("GPU preference: %s; PCI %s (%s).",preference==PROJECT_GPU_INTEGRATED ? "Integrated" : "Dedicated",
            adapter.pciAddress.c_str(),adapter.driver.c_str());
#else
        startupWarning="GPU selection is managed by the OS on this platform.";
        Debug::Warning("GPU preference requires OS graphics settings on this OpenGL platform.");
#endif
    }
}
