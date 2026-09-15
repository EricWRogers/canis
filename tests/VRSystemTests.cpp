#include <Canis/VR/VRSystem.hpp>
#include <Canis/Window.hpp>
#include <Canis/OpenGL.hpp>
#include <iostream>
#include <stdexcept>

using namespace Canis;
void Require(bool value, const char* text) { if (!value) throw std::runtime_error(text); }
int main()
{
    try
    {
        Window window("VR lifecycle test", 320, 240, true);
        VR::System vr;
        VR::Config config; config.mode = VR::Mode::Simulated;
        config.simulationEyeWidth = 32; config.simulationEyeHeight = 32;
        std::string error;
        Require(vr.Initialize(window, config, error), error.c_str());
        glViewport(1,2,200,100);
        glEnable(GL_SCISSOR_TEST); glScissor(3,4,40,50);
        Require(vr.BeginFrame(error), "Begin simulation frame");
        Require(!vr.BeginFrame(error), "Reject double begin");
        unsigned eyes = 0;
        Require(vr.RenderAndEndFrame([&](const VR::Eye& eye, const Matrix4& view, unsigned int target) {
            Require(eye.pose.valid && target != 0, "Render receives valid eye target");
            auto center = view * Vector4(eye.pose.position, 1);
            Require(glm::length(Vector3(center)) < 0.0001f, "Predicted view matches eye");
            const unsigned eyeIndex = eyes++;
            glClearColor(eyeIndex == 0 ? 1 : 0, 0, eyeIndex == 1 ? 1 : 0, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }, error), error.c_str());
        Require(eyes == 2 && vr.GetDiagnostics().renderedEyes == 2, "Two eyes per frame");
        GLint viewport[4], scissor[4];
        glGetIntegerv(GL_VIEWPORT, viewport); glGetIntegerv(GL_SCISSOR_BOX, scissor);
        Require(viewport[0] == 1 && viewport[2] == 200 && scissor[0] == 3 && glIsEnabled(GL_SCISSOR_TEST), "Restore caller viewport/scissor");
        glBindFramebuffer(GL_FRAMEBUFFER, vr.MirrorFramebuffer());
        unsigned char left[4]{}, right[4]{};
        glReadPixels(16,16,1,1,GL_RGBA,GL_UNSIGNED_BYTE,left);
        glReadPixels(48,16,1,1,GL_RGBA,GL_UNSIGNED_BYTE,right);
        Require(left[0] == 255 && left[2] == 0 && right[0] == 0 && right[2] == 255, "Each eye occupies its own mirror half");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        for (float opacity : {0.5f,1.0f})
        {
            vr.SetComfortFade(opacity);
            Require(vr.BeginFrame(error), "Begin fade frame");
            Require(vr.RenderAndEndFrame([](auto&,auto&,auto){
                glClearColor(1,1,1,1); glClear(GL_COLOR_BUFFER_BIT);
            },error),error.c_str());
            glBindFramebuffer(GL_FRAMEBUFFER,vr.MirrorFramebuffer());
            glReadPixels(16,16,1,1,GL_RGBA,GL_UNSIGNED_BYTE,left);
            glReadPixels(48,16,1,1,GL_RGBA,GL_UNSIGNED_BYTE,right);
            int expected = opacity == 1 ? 0 : 128;
            Require(std::abs(int(left[0])-expected)<=1&&std::abs(int(right[0])-expected)<=1,"Fade covers both eyes at expected opacity");
            glBindFramebuffer(GL_FRAMEBUFFER,0);
        }
        vr.SetComfortFade(0);
        VR::Pose head{{0,1.7f,0}, {1,0,0,0}, true};
        std::array<VR::Hand,2> hands{}; hands[0].active = true; hands[0].squeeze = 1; hands[0].grip.valid = true;
        vr.SetSimulatedInput(head, hands, false);
        Require(vr.BeginFrame(error), "Begin unfocused frame");
        Require(!vr.GetState().hands[0].active && vr.GetState().hands[0].squeeze == 0, "Focus loss clears input");
        Require(vr.RenderAndEndFrame([](auto&, auto&, auto){}, error), "End unfocused frame");
        head.valid = false; vr.SetSimulatedInput(head, hands);
        Require(vr.BeginFrame(error), "Begin invalid tracking frame");
        bool called = false;
        Require(vr.RenderAndEndFrame([&](auto&, auto&, auto){called=true;}, error), "End skipped frame");
        Require(!called && vr.GetDiagnostics().skippedFrames == 1, "Invalid poses suppress rendering");
        head.valid = true; vr.SetSimulatedInput(head, hands);
        Require(vr.BeginFrame(error), "Begin failure test");
        Require(!vr.RenderAndEndFrame([](auto&,auto&,auto){throw std::runtime_error("test draw failure");}, error), "Report render exceptions");
        Require(vr.BeginFrame(error), "Can begin after handled render failure");
        Require(vr.RenderAndEndFrame([](auto&,auto&,auto){}, error), "End recovered frame");
        head.valid=false;vr.SetSimulatedInput(head,hands,false);
        vr.SetOrigin({2,3,4},1);
        vr.Shutdown(); vr.Shutdown();
        config.simulationEyeWidth = -1;
        Require(!vr.Initialize(window, config, error), "Reject invalid dimensions");
        config.simulationEyeWidth = 32;
        Require(vr.Initialize(window, config, error), "Can reinitialize after failure");
        Require(vr.BeginFrame(error),"Begin after stale simulation state reset");
        Require(vr.GetState().focused&&vr.GetState().head.valid&&glm::length(Vector3(vr.GetOrigin()[3]))==0,"Reinitialize resets tracking/focus/origin");
        Require(vr.RenderAndEndFrame([](auto&,auto&,auto){},error),"End reset frame");
        vr.Shutdown();
        return 0;
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
