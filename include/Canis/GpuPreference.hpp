#pragma once
#include <string>
#include <vector>

namespace Canis
{
    enum ProjectGpuPreference : int {
        PROJECT_GPU_AUTOMATIC = 0,
        PROJECT_GPU_INTEGRATED = 1,
        PROJECT_GPU_DEDICATED = 2,
    };

    inline int NormalizeGpuPreference(int value) {
        return value == PROJECT_GPU_INTEGRATED || value == PROJECT_GPU_DEDICATED ? value : PROJECT_GPU_AUTOMATIC;
    }

    struct GpuAdapter {
        std::string pciAddress;
        std::string driver;
        int type = PROJECT_GPU_AUTOMATIC; // Unknown adapters are never guessed from enumeration order.
    };

    std::vector<GpuAdapter> DetectGpuAdapters();
    int FindPreferredGpu(const std::vector<GpuAdapter>& adapters, int preference);
    void ApplyGpuPreference(int preference);
    const std::string& GetGpuPreferenceWarning();
}
