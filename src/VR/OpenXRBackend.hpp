#pragma once
#include <Canis/VR/VRSystem.hpp>
#include <memory>
namespace Canis::VR
{
    // Private boundary keeps OpenXR/native headers out of gameplay and web builds.
    class OpenXRBackend
    {
    public:
        virtual ~OpenXRBackend() = default;
        virtual void Initialize(Window&, const Config&, Diagnostics&) = 0;
        virtual void Begin(State&) = 0;
        virtual void RenderEnd(State&, const System::RenderEye&, const Matrix4&, unsigned int mirror, int mirrorWidth, int mirrorHeight) = 0;
        virtual bool Haptic(unsigned, float, float) = 0;
    };
    std::unique_ptr<OpenXRBackend> CreateOpenXRBackend();
}
