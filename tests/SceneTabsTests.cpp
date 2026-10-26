#include <Canis/App.hpp>
#include <Canis/Editor.hpp>
#include <Canis/Canis.hpp>
#include <Canis/InputManager.hpp>
#include <Canis/Window.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/Time.hpp>
#include <Canis/OpenGL.hpp>
#include <Canis/Shader.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>
#include "../src/EditorHistoryEvent.hpp"
#include <SDL3/SDL.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <array>
#include <vector>
#include <cmath>
#include <cstdlib>

namespace fs = std::filesystem;
static void Check(bool value, const char* message)
{ if (!value) throw std::runtime_error(message); }

namespace Canis
{
    struct EditorSceneTabsTestAccess
    {
        struct PreviewGLState
        {
            GLint program=0, draw=0, read=0, active=0, depthFunc=0, cullMode=0, frontFace=0;
            GLint vao=0, buffer=0, renderbuffer=0;
            std::array<GLint,4> viewport{};
            std::array<GLfloat,4> clear{};
            std::array<GLfloat,2> depthRange{};
            GLfloat clearDepth=0;
            std::array<GLboolean,4> colorMask{};
            GLboolean depthMask=0, depth=0, blend=0, cull=0, scissor=0, stencil=0;
            std::array<std::array<GLint,3>,8> textures{};
            static PreviewGLState Read()
            {
                PreviewGLState s;
                glGetIntegerv(GL_CURRENT_PROGRAM,&s.program);
                glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&s.draw);
                glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&s.read);
                glGetIntegerv(GL_ACTIVE_TEXTURE,&s.active);
                glGetIntegerv(GL_DEPTH_FUNC,&s.depthFunc);
                glGetIntegerv(GL_CULL_FACE_MODE,&s.cullMode);
                glGetIntegerv(GL_FRONT_FACE,&s.frontFace);
                glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&s.vao);
                glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&s.buffer);
                glGetIntegerv(GL_RENDERBUFFER_BINDING,&s.renderbuffer);
                glGetIntegerv(GL_VIEWPORT,s.viewport.data());
                glGetFloatv(GL_COLOR_CLEAR_VALUE,s.clear.data());
                glGetFloatv(GL_DEPTH_RANGE,s.depthRange.data());
                glGetFloatv(GL_DEPTH_CLEAR_VALUE,&s.clearDepth);
                glGetBooleanv(GL_COLOR_WRITEMASK,s.colorMask.data());
                glGetBooleanv(GL_DEPTH_WRITEMASK,&s.depthMask);
                s.depth=glIsEnabled(GL_DEPTH_TEST);s.blend=glIsEnabled(GL_BLEND);
                s.cull=glIsEnabled(GL_CULL_FACE);s.scissor=glIsEnabled(GL_SCISSOR_TEST);
                s.stencil=glIsEnabled(GL_STENCIL_TEST);
                for(size_t i=0;i<s.textures.size();++i) {
                    glActiveTexture(GL_TEXTURE0+i);
                    glGetIntegerv(GL_TEXTURE_BINDING_2D,&s.textures[i][0]);
                    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP,&s.textures[i][1]);
                    glGetIntegerv(GL_SAMPLER_BINDING,&s.textures[i][2]);
                }
                glActiveTexture(s.active);
                return s;
            }
            bool operator==(const PreviewGLState&) const = default;
        };

        static void MaterialAndModelPreviews(Editor& editor)
        {
            const std::string materialPath="assets/brutal-city/materials/SideWalk.material";
            const std::string modelPath="assets/defaults/models/sphere.glb";
            auto contents=[](const std::string& path) {
                std::ifstream in(path);return std::string(std::istreambuf_iterator<char>(in),{});
            };
            const std::string originalMaterial=contents(materialPath);
            auto* material=AssetManager::GetMaterial(materialPath);
            auto* modelMaterial=AssetManager::GetMaterial("assets/defaults/materials/default.material");
            Check(material && modelMaterial,"Material preview fixtures could not load");
            Check(material->albedoId>=0 && material->roughnessId>=0 && material->normalId>=0 &&
                  std::abs(material->normalScale-.45f)<.0001f && material->roughnessChannel==1,
                  "SideWalk material did not load its own concrete color, roughness and normal maps");
            Check(modelMaterial->normalId<0 && modelMaterial->normalScale==1 &&
                  modelMaterial->roughnessChannel==0,
                  "Legacy materials did not retain default normal strength and red roughness sampling");
            for(const auto& uniform:material->materialFields.GetFloatUniforms())
                Check(uniform.name!="normalScale" && uniform.name!="roughnessChannel",
                      "First-class normal/roughness settings leaked into custom material uniforms");
            for(const auto& uniform:material->materialFields.GetIntUniforms())
                Check(uniform.name!="normalScale" && uniform.name!="roughnessChannel",
                      "First-class normal/roughness settings were reclassified as custom integer uniforms");

            Shader sentinel;
            sentinel.CompileSource("[OPENGL VERSION]\nlayout(location=0)in vec3 p;void main(){gl_Position=vec4(p,1);}",
                                   "[OPENGL VERSION]\nout vec4 c;void main(){c=vec4(1);}","Preview GL state sentinel");
            sentinel.Link();Check(sentinel.IsLinked(),"Preview sentinel shader failed");
            GLuint framebuffers[2]{},textures[2]{},sampler=0,vao=0,buffer=0,renderbuffer=0;
            glGenFramebuffers(2,framebuffers);glGenTextures(2,textures);glGenSamplers(1,&sampler);
            glGenVertexArrays(1,&vao);glGenBuffers(1,&buffer);glGenRenderbuffers(1,&renderbuffer);
            auto seedGL=[&] {
                sentinel.Use();
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER,framebuffers[0]);
                glBindFramebuffer(GL_READ_FRAMEBUFFER,framebuffers[1]);
                glViewport(7,11,73,61);glClearColor(.13f,.29f,.41f,.57f);
                glEnable(GL_DEPTH_TEST);glDepthFunc(GL_GEQUAL);glDepthMask(GL_FALSE);
                glDepthRange(.2,.8);glClearDepth(.4);glColorMask(GL_FALSE,GL_TRUE,GL_FALSE,GL_TRUE);
                glEnable(GL_BLEND);glEnable(GL_CULL_FACE);glCullFace(GL_FRONT);glFrontFace(GL_CW);
                glEnable(GL_SCISSOR_TEST);glScissor(0,0,1,1);
                glEnable(GL_STENCIL_TEST);glStencilFunc(GL_NEVER,1,0xff);
                glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,buffer);
                glBindRenderbuffer(GL_RENDERBUFFER,renderbuffer);
                for(int i=0;i<8;++i) {
                    glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,textures[0]);
                    glBindTexture(GL_TEXTURE_CUBE_MAP,textures[1]);glBindSampler(i,sampler);
                }
                glActiveTexture(GL_TEXTURE6);
            };
            auto image=[&](bool model, bool fromDisk=false) {
                if(fromDisk)editor.DestroyAssetPreviewCache();
                auto& cache=model?editor.m_modelPreviewCache[modelPath]:editor.m_materialPreviewCache[materialPath];
                cache.lastRenderSeconds=-1;
                cache.lastSourceCheckSeconds=-1;
                cache.diskCacheChecked=!fromDisk;
                seedGL();const auto before=PreviewGLState::Read();
                unsigned int texture=0;
                const bool rendered=model?editor.GetModelPreviewTexture(modelPath,texture)
                                         :editor.GetMaterialPreviewTexture(materialPath,texture);
                Check(rendered && texture,"Actual editor preview failed to render");
                Check(PreviewGLState::Read()==before,"Editor thumbnail leaked GL state into the caller");
                GLint oldRead=0;glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&oldRead);
                glBindFramebuffer(GL_READ_FRAMEBUFFER,cache.renderTarget.framebuffer);
                std::vector<unsigned char> pixels(size_t(cache.renderTarget.width)*cache.renderTarget.height*4);
                glReadPixels(0,0,cache.renderTarget.width,cache.renderTarget.height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
                glBindFramebuffer(GL_READ_FRAMEBUFFER,oldRead);
                Check(glGetError()==GL_NO_ERROR,"Editor thumbnail rendering generated an OpenGL error");
                return pixels;
            };
            auto shaded=[](const std::vector<unsigned char>& pixels) {
                const int size=int(std::sqrt(pixels.size()/4));
                int low=255,high=0;
                // Stay inside the sphere silhouette; background contrast alone
                // must not make an unlit, uniformly gray preview pass.
                for(int y=size*3/8;y<size*5/8;++y)for(int x=size*3/8;x<size*5/8;++x) {
                    const size_t offset=(size_t(y)*size+x)*4;
                    int luma=(int(pixels[offset])+pixels[offset+1]+pixels[offset+2])/3;
                    low=std::min(low,luma);high=std::max(high,luma);
                }
                Check(high-low>12,"Editor preview sphere is flat gray instead of visibly lit");
                for(int i=0;i<size;++i)
                    Check(pixels[size_t(i)*4]==pixels[0] &&
                          pixels[(size_t(size-1)*size+i)*4]==pixels[0],
                          "Editor preview sphere is cropped at the image border");
            };
            auto poison=[](MaterialAsset* asset) {
                auto* shader=AssetManager::Get<ShaderAsset>(asset->shaderId)->GetShader();shader->Use();
                shader->SetInt("directionalLightMode",2);shader->SetBool("canisDirectCapture",true);
                shader->SetBool("canisProbeCapture",true);shader->SetInt("canisProbeCount",1);
                shader->SetVec3("canisProbeExtent[0]",100,100,100);
                shader->SetVec3("canisProbeParams[0]",1,100,0);
                shader->SetVec3("canisProbeSH[0]",10,10,10);
                shader->SetBool("disableBakedLighting",false);shader->SetBool("useBakedVertexLighting",true);
                shader->SetBool("weatherSurface",true);
                shader->SetBool("weatherWetnessEnabled",true);shader->SetFloat("weatherWetness",1);
                shader->SetFloat("weatherDryRoughness",.01f);shader->SetFloat("weatherWetRoughness",.001f);
                shader->SetFloat("weatherDarkening",1);shader->SetFloat("softTime",913);
                shader->UnUse();
            };
            while(glGetError()!=GL_NO_ERROR){}
            const auto cleanMaterial=image(false);shaded(cleanMaterial);
            poison(material);
            Check(image(false)==cleanMaterial,"SideWalk preview inherited baked-light, probe-capture, or wet weather uniforms");
            auto visiblyDifferent=[](const std::vector<unsigned char>& a,const std::vector<unsigned char>& b) {
                Check(a.size()==b.size(),"Preview comparison dimensions changed");
                size_t changed=0,total=0;
                for(size_t i=0;i<a.size();i+=4)for(size_t channel=0;channel<3;++channel) {
                    const int delta=std::abs(int(a[i+channel])-int(b[i+channel]));
                    total+=delta;changed+=delta>=2;
                }
                return changed>32 && total>256;
            };
            const int albedoId=material->albedoId;
            material->albedoId=-1;
            Check(visiblyDifferent(image(false),cleanMaterial),
                  "Standalone material thumbnail ignored SideWalk's concrete albedo texture");
            material->albedoId=albedoId;
            const float normalScale=material->normalScale;
            material->normalScale=0;
            const auto flatNormal=image(false);
            material->normalScale=1;
            Check(visiblyDifferent(image(false),flatNormal),
                  "Standalone material normal map/strength did not change the sphere's shading");
            material->normalScale=normalScale;
            Check(image(false)==cleanMaterial,
                  "Changing normal strength or albedo left texture state on another material preview");
            const int roughnessChannel=material->roughnessChannel;
            material->roughnessChannel=0;
            const auto redRoughness=image(false);
            size_t roughnessChanges=0;
            for(size_t i=0;i<redRoughness.size();i+=4)
                for(size_t channel=0;channel<3;++channel)
                    roughnessChanges+=std::abs(int(redRoughness[i+channel])-int(cleanMaterial[i+channel]))>=1;
            Check(roughnessChanges>32,
                  "Concrete roughness ignored its packed green channel and sampled constant red instead");
            material->roughnessChannel=roughnessChannel;
            Check(image(false)==cleanMaterial,
                  "Restoring the material's packed roughness channel did not restore its preview");
            const auto cleanModel=image(true);shaded(cleanModel);
            poison(modelMaterial);
            Check(image(true)==cleanModel,"Model preview inherited baked-light, probe-capture, or weather uniforms");
            Check(image(false,true)==cleanMaterial,"Cached material preview changed pixels or vertical orientation");
            Check(image(true,true)==cleanModel,"Cached model preview changed pixels or vertical orientation");
            // Switching assets must neither rewrite the material's extensionless
            // shader reference nor turn an idle Inspector draw into an edit.
            ImGui::NewFrame();ImGui::Begin("Material preview inspector test");
            Check(editor.DrawMaterialAssetInspector(materialPath),"SideWalk Inspector did not recognize its material");
            ImGui::End();ImGui::Render();
            Check(contents(materialPath)==originalMaterial,"Opening the material Inspector rewrote its shader reference");
            if(const auto* destination=std::getenv("CANIS_PREVIEW_TEST_ARTIFACTS")) {
                fs::create_directories(destination);
                for(const auto& fixture:std::array<std::pair<const char*,const char*>,2>{{
                        {"cache/editor/material_previews","sidewalk.png"},
                        {"cache/editor/model_previews","sphere.png"}}}) {
                    for(const auto& entry:fs::recursive_directory_iterator(fixture.first))
                        if(entry.path().extension()==".png") {
                            fs::copy_file(entry.path(),fs::path(destination)/fixture.second,
                                          fs::copy_options::overwrite_existing);
                            break;
                        }
                }
            }

            glUseProgram(0);glBindFramebuffer(GL_FRAMEBUFFER,0);glBindVertexArray(0);
            glBindBuffer(GL_ARRAY_BUFFER,0);glBindRenderbuffer(GL_RENDERBUFFER,0);
            for(int i=0;i<8;++i) {glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,0);
                glBindTexture(GL_TEXTURE_CUBE_MAP,0);glBindSampler(i,0);}
            glActiveTexture(GL_TEXTURE0);glViewport(0,0,800,600);glClearColor(0,0,0,1);
            glDepthRange(0,1);glClearDepth(1);glDepthFunc(GL_LESS);glDepthMask(GL_TRUE);
            glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glCullFace(GL_BACK);glFrontFace(GL_CCW);
            glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
            glDisable(GL_SCISSOR_TEST);glDisable(GL_STENCIL_TEST);
            glDeleteFramebuffers(2,framebuffers);glDeleteTextures(2,textures);glDeleteSamplers(1,&sampler);
            glDeleteVertexArrays(1,&vao);glDeleteBuffers(1,&buffer);glDeleteRenderbuffers(1,&renderbuffer);
            std::cout << "Material/model thumbnails have concrete textures, material-owned normal strength, "
                         "studio shading, scene-uniform isolation and GL-state restoration\n";
        }

        static void LightingAnimationPreview(App& app, Editor& editor)
        {
            LightingScenario dusk, night;
            dusk.id = "preview-dusk"; dusk.name = "Dusk";
            night.id = "preview-night"; night.name = "Night";
            app.scene.GetLightingScenarios() = {dusk, night};
            Check(app.scene.SetInitialLightingScenario(night.id), "Preview fixture startup was rejected");
            auto& wetness = app.scene.GetSurfaceWetnessSettings();
            wetness.enabled = true;
            wetness.amount = .15f;
            auto root = app.scene.CreateEntity("Animation lighting fixture");
            float animatedWeight = 0;
            ScriptConf conf;
            conf.name = "AnimationLightingPreviewFixture";
            conf.Get = [&](Entity&) -> void* { return &animatedWeight; };
            conf.registry.animationGetters["weight"] = [](void* ptr) {
                return AnimationValue::Float(*static_cast<float*>(ptr));
            };
            conf.registry.animationSetters["weight"] = [&](void* ptr, const AnimationValue& value) {
                *static_cast<float*>(ptr) = value.AsFloat();
                app.scene.SetLightingScenarioPreview(dusk.id, night.id, value.AsFloat());
                app.scene.GetSurfaceWetnessSettings().amount = value.AsFloat();
            };
            app.RegisterScript(conf);
            auto capture = [&] {
                editor.CaptureAnimationLightingPreview();
                editor.m_animationPreviewTargetUUID = root.GetUUID();
                Editor::AnimationRestoreBinding binding;
                binding.component = conf.name;
                binding.property = "weight";
                binding.value = AnimationValue::Float(animatedWeight);
                editor.m_animationRestoreBindings = {binding};
            };
            auto sample = [&](float value) {
                conf.registry.animationSetters.at("weight")(&animatedWeight, AnimationValue::Float(value));
            };
            capture(); sample(.65f);
            // The Environment window must not erase a lighting preview owned by
            // the Animation window merely because its own checkbox is off.
            ImGui::NewFrame(); ImGui::Begin("Animation lighting preview test");
            editor.DrawLightingScenarios(); ImGui::End(); ImGui::Render();
            Check(app.scene.HasLightingScenarioPreview() &&
                  std::abs(app.scene.GetLightingScenarioBlend().blend - .65f) < .0001f &&
                  !app.scene.IsLightingScenarioBlendManual(),
                  "Environment erased animation lighting or Edit scrubbing changed runtime ownership");
            editor.RestoreAnimationPreviewState();
            const auto runtime = app.scene.GetLightingScenarioBlend();
            Check(animatedWeight == 0 && !app.scene.HasLightingScenarioPreview() &&
                  runtime.from->id == night.id && runtime.to->id == night.id &&
                  app.scene.GetInitialLightingScenario() == night.id &&
                  std::abs(app.scene.GetSurfaceWetnessSettings().amount - .15f) < .0001f,
                  "Ending animation preview changed the startup lighting or authored surface wetness");
            app.scene.SetLightingScenarioPreview(night.id, dusk.id, .23f);
            app.scene.GetSurfaceWetnessSettings().amount = .37f;
            capture(); sample(.8f);
            editor.RestoreAnimationPreviewState();
            const auto restored = app.scene.GetLightingScenarioBlend();
            Check(app.scene.HasLightingScenarioPreview() && restored.from->id == night.id &&
                  restored.to->id == dusk.id && std::abs(restored.blend - .23f) < .0001f &&
                  animatedWeight == 0 && !editor.m_animationLightingPreview.captured &&
                  std::abs(app.scene.GetSurfaceWetnessSettings().amount - .37f) < .0001f,
                  "Ending animation preview did not restore prior Environment lighting and wetness");
            // Repeated restore calls after panel closure must be harmless.
            editor.RestoreAnimationPreviewState();
            Check(app.scene.HasLightingScenarioPreview() &&
                  std::abs(app.scene.GetLightingScenarioBlend().blend - .23f) < .0001f,
                  "An idle hidden Animation panel erased the restored Environment preview");
            app.scene.ClearLightingScenarioPreview();
            AnimationClipAsset clip;
            clip.length = 40;
            AnimationTrack track;
            track.component = conf.name; track.property = "weight";
            track.type = AnimationValueType::FLOAT;
            track.keys = {{0, AnimationValue::Float(0)}, {40, AnimationValue::Float(1)}};
            clip.tracks = {track};
            Check(clip.Save("assets/weather-preview.animclip"), "Preview clip could not save");
            // Deleted remembered tabs must not discover every unrelated asset
            // again on each editor frame. UUID moves and path fallback still work.
            auto &remembered = GetEditorConfig().lastAnimationClip;
            const auto savedRemembered = remembered;
            Check(clip.Save("assets/unindexed-preview.animclip"), "Unindexed fixture could not save");
            remembered.uuid = UUID(991234567890123456ULL);
            remembered.path = "assets/deleted-pack/missing.animclip";
            const auto assetCount = AssetManager::GetAssetLibrary().assets.size();
            for (int i = 0; i < 20; ++i)
                Check(editor.ResolveRememberedAnimationClipPath().empty(), "Deleted remembered clip resolved");
            Check(AssetManager::GetAssetLibrary().assets.size() == assetCount &&
                  !fs::exists("assets/unindexed-preview.animclip.meta") &&
                  !fs::exists("assets/deleted-pack/missing.animclip.meta"),
                  "Remembered clip searched or registered unrelated/missing assets");
            auto *previewMeta = AssetManager::GetMetaFile("assets/weather-preview.animclip");
            remembered.uuid = previewMeta->uuid;
            Check(editor.ResolveRememberedAnimationClipPath() == previewMeta->path,
                  "Remembered UUID did not follow its indexed path after a move");
            remembered.uuid = UUID(0);
            remembered.path = "assets/weather-preview.animclip";
            Check(editor.ResolveRememberedAnimationClipPath() == previewMeta->path,
                  "Remembered path-only clip no longer resolves");
            remembered = savedRemembered;
            editor.m_animationClipStatePath = "assets/weather-preview.animclip";
            editor.m_animationTargetUUID = root.GetUUID();
            editor.m_animationTime = 40;
            editor.m_animationPlaying = false;
            editor.m_animationPreviewEnabled = true;
            ImGui::NewFrame(); editor.DrawAnimationWindow(1.f / 60); ImGui::Render();
            Check(animatedWeight == 1 && editor.m_animationTime == 40 &&
                  app.scene.GetLightingScenarioBlend().blend == 1,
                  "Stopped animation scrubbing wrapped the exact 40-second endpoint to the first key");
            editor.m_animationPreviewEnabled = false;
            ImGui::NewFrame(); editor.DrawAnimationWindow(1.f / 60); ImGui::Render();
            Check(animatedWeight == 0 && !app.scene.HasLightingScenarioPreview() &&
                  app.scene.GetLightingScenarioBlend().from->id == night.id &&
                  std::abs(app.scene.GetSurfaceWetnessSettings().amount - .37f) < .0001f,
                  "Disabling actual Animation window preview failed to restore lighting and wetness");
            editor.m_animationPreviewEnabled = true;
            editor.m_animationClipStatePath.clear();
            editor.m_animationTargetUUID = UUID(0);
            app.scene.GetLightingScenarios().clear();
            app.scene.SetInitialLightingScenario("");
            app.scene.GetSurfaceWetnessSettings() = {};
            root.Destroy();
            app.UnregisterScript(conf);
        }

        static void PostProcessColorSliders(Editor& editor)
        {
            const std::string path="color-controls.postprocess";
            std::ofstream(path) << R"(passes:
- name: Color correction
  enabled: true
  shader: {path: assets/shaders/postprocess_color_correction.vs}
  settings: {exposure: 1.2, contrast: 0.46, saturation: 0.82}
)";
            auto contents=[&]{std::ifstream in(path);return std::string(std::istreambuf_iterator<char>(in),{});};
            const auto original=contents();
            const std::array<const char*,3> names={"exposure","contrast","saturation"};
            std::array<ImGuiID,3> ids{};std::array<float,3> rows{};
            auto frame=[&](float x=-100,float y=-100,bool down=false)
            {
                auto& io=ImGui::GetIO();io.AddMousePosEvent(x,y);io.AddMouseButtonEvent(0,down);
                ImGui::NewFrame();
                ImGui::SetNextWindowPos(ImVec2(0,0));ImGui::SetNextWindowSize(ImVec2(700,580));
                ImGui::Begin("Post-process slider test",nullptr,ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoTitleBar);
                ImGui::PushID(0);for(size_t i=0;i<names.size();++i)ids[i]=ImGui::GetID(names[i]);ImGui::PopID();
                Check(editor.DrawPostProcessAssetInspector(path),"Post-process Inspector did not recognize the asset");
                const auto hovered=ImGui::GetCurrentContext()->HoveredId;
                ImGui::End();ImGui::Render();return hovered;
            };
            frame();frame();frame();
            for(float y=10;y<550;y+=3)
            {
                const auto hovered=frame(120,y);
                for(size_t i=0;i<ids.size();++i)if(hovered==ids[i])rows[i]=y;
            }
            Check(contents()==original,"Opening the post-process Inspector changed its settings");
            for(size_t i=0;i<names.size();++i)
            {
                Check(rows[i]>0,"Color correction slider was not exposed in the Inspector");
                const float before=YAML::LoadFile(path)["passes"][0]["settings"][names[i]].as<float>();
                frame(180,rows[i]);frame(180,rows[i],true);frame(180,rows[i]);
                const float after=YAML::LoadFile(path)["passes"][0]["settings"][names[i]].as<float>();
                Check(std::abs(after-before)>.01f,"Clicking a color correction slider did not save its value");
                auto* loaded=AssetManager::Get<PostProcessAsset>(AssetManager::GetID(path));
                Check(loaded && loaded->GetPasses().size()==1,"Slider edit did not reload the post-process asset");
                const auto& pass=loaded->GetPasses()[0];
                const float live=i==0?pass.exposure:i==1?pass.contrast:pass.saturation;
                Check(std::abs(live-after)<.00001f,"Saved slider value did not reach the live post-process pass");
            }
            frame();
            std::cout<<"Color correction Inspector sliders expose, save and reload exposure/contrast/saturation\n";
        }

        static void Run(App& app, Editor& editor, Window& window, const fs::path& executable)
        {
            InputManager input;
            app.scene.Init(&app, &window, &input);
            editor.m_scene = &app.scene;
            editor.m_app = &app;
            editor.m_window = &window;
            MaterialAndModelPreviews(editor);
            PostProcessColorSliders(editor);
            LightingAnimationPreview(app, editor);
            auto seed = [&](const char* path) {
                std::ofstream(path) << "Entities:\n- Entity: 101\n  Name: Original\n";
            };
            auto setup = [&] {
                seed("first.scene"); seed("second.scene");
                app.scene.Unload(); app.scene.Load("first.scene");
                Check(app.scene.GetSurfaceWetnessSettings() == SurfaceWetnessSettings{},
                      "A legacy scene inherited surface wetness from the previously loaded scene");
                editor.ResetSceneHistory(); editor.InitializeSceneTabs();
                editor.OpenSceneTab("second.scene");
            };
            auto dirtyFirst = [&] {
                editor.SwitchSceneTab(0);
                app.scene.GetEntities()[0]->SetName("Edited first");
                editor.ResetSceneHistory();
                editor.SwitchSceneTab(1);
            };
            auto diskName = [&] { return YAML::LoadFile("first.scene")["Entities"][0]["Name"].as<std::string>(); };
            auto& io = ImGui::GetIO();
            bool collapsed=false;
            auto frame = [&] {
                ImGui::NewFrame();
                ImGui::SetNextWindowPos(ImVec2(0,0));
                ImGui::SetNextWindowSize(ImVec2(800,600));
                ImGui::SetNextWindowCollapsed(collapsed,ImGuiCond_Always);
                ImGui::Begin("SceneTabsTest"); editor.DrawSceneTabs(); ImGui::End();
                ImGui::Render();
            };

            setup();
            const auto cleanRoundTrip = editor.CaptureSceneHistoryState();
            editor.RestoreSceneHistoryState(cleanRoundTrip);
            Check(editor.CaptureSceneHistoryState().sceneYaml == cleanRoundTrip.sceneYaml,
                  "An unedited scene changed its serialized history after load (including empty lighting sources)");
            // A changed sentinel distinguishes "no capture" from taking an
            // expensive snapshot and then discarding it as identical.
            app.scene.GetEntities()[0]->SetName("Scene edit sentinel");
            const auto originalHistory = editor.m_sceneHistoryCurrentState.sceneYaml;
            ImGui::NewFrame();
            editor.BeginSceneHistoryFrame();
            ImGui::Begin("Asset search");
            const auto searchId = ImGui::GetID("Search");
            ImGui::SetActiveID(searchId, ImGui::GetCurrentWindow());
            ImGui::MarkItemEdited(searchId);
            ImGui::ClearActiveID();
            ImGui::End();
            ImGui::Begin("Inspector");
            { SceneHistoryEditScope properties(editor.m_sceneHistoryEditWasActive); }
            ImGui::End();
            editor.EndSceneHistoryFrame();
            Check(editor.m_sceneHistoryCurrentState.sceneYaml == originalHistory &&
                  editor.m_sceneUndoStack.empty(), "Unrelated edit captured the scene");
            // Explicit scene commands capture once and remain undoable.
            editor.NotifySceneChanged();
            editor.EndSceneHistoryFrame();
            Check(editor.m_sceneUndoStack.size() == 1 &&
                  editor.m_sceneHistoryCurrentState.sceneYaml != originalHistory,
                  "Scene command did not record history");
            editor.EndSceneHistoryFrame();
            Check(editor.m_sceneUndoStack.size() == 1, "Idle end frame duplicated undo");
            ImGui::Render();
            editor.UndoSceneEdit();
            Check(app.scene.GetEntities()[0]->GetName() == "Original", "Scene command undo failed");
            editor.RedoSceneEdit();
            Check(app.scene.GetEntities()[0]->GetName() == "Scene edit sentinel", "Scene command redo failed");

            ImGui::NewFrame();
            editor.BeginSceneHistoryFrame();
            ImGui::Begin("Inspector");
            const auto propertyId = ImGui::GetID("Name");
            const auto beforeProperty = editor.m_sceneHistoryCurrentState.sceneYaml;
            {
                SceneHistoryEditScope properties(editor.m_sceneHistoryEditWasActive);
                ImGui::SetActiveID(propertyId, ImGui::GetCurrentWindow());
                app.scene.GetEntities()[0]->SetName("Property edit");
                ImGui::MarkItemEdited(propertyId);
            }
            editor.EndSceneHistoryFrame();
            Check(editor.m_sceneHistoryCurrentState.sceneYaml == beforeProperty,
                  "Property edit captured before the interaction finished");
            ImGui::ClearActiveID();
            editor.EndSceneHistoryFrame();
            Check(editor.m_sceneUndoStack.size() == 2,
                  "Completed property edit did not create one undo entry");
            ImGui::End(); ImGui::Render();
            editor.UndoSceneEdit();
            Check(app.scene.GetEntities()[0]->GetName() == "Scene edit sentinel",
                  "Property edit undo failed");

            setup();
            editor.SwitchSceneTab(0);
            for (int i=0;i<4;++i) frame();
            collapsed=true; frame();
            editor.OpenSceneTab("second.scene");
            for (int i=0;i<3;++i) frame();
            Check(editor.m_sceneTabSelectionRequest==1,"Hidden Scene panel discarded pending scene selection");
            collapsed=false;
            for (int i=0;i<4;++i) frame();
            Check(editor.m_activeSceneTab==1,"Showing Scene panel switched back to previous scene");
            editor.CloseSceneTab(0);
            Check(editor.m_sceneTabs.size()==1 && editor.m_activeSceneTab==0,
                  "Closing clean first tab corrupted the active index");
            setup(); dirtyFirst();
            for (int i=0; i<4; ++i) frame();
            auto* host=ImGui::FindWindowByName("SceneTabsTest");
            auto* bar=ImGui::GetCurrentContext()->TabBars.GetByKey(host->GetID("##SceneAssetTabs"));
            Check(bar && bar->Tabs.Size>=2, "Scene tab bar missing");
            const auto& tab=bar->Tabs[0];
            const ImVec2 close(bar->BarRect.Min.x+tab.Offset+tab.Width-ImGui::GetStyle().FramePadding.x-ImGui::GetFontSize()*.5f,
                               (bar->BarRect.Min.y+bar->BarRect.Max.y)*.5f);
            io.AddMousePosEvent(close.x,close.y); frame();
            io.AddMouseButtonEvent(0,true); frame();
            io.AddMouseButtonEvent(0,false); frame(); frame();
            Check(editor.m_pendingSceneTabClose==0 && editor.m_sceneTabs.size()==2,
                  "Dirty first-tab X did not open confirmation");
            Check(editor.m_activeSceneTab==1, "Closing inactive tab changed the active scene");
            auto* modal=ImGui::FindWindowByName("Unsaved Scene Changes");
            Check(modal && modal->WasActive, "Unsaved scene modal not visible");
            io.AddKeyEvent(ImGuiKey_Escape,true); frame();
            io.AddKeyEvent(ImGuiKey_Escape,false); frame();
            Check(editor.m_pendingSceneTabClose==-1 && editor.IsSceneTabDirty(0),
                  "Cancel lost edits or kept the close request");
            editor.CloseSceneTab(0);
            Check(editor.ResolveSceneTabClose(false), "Discard failed");
            Check(editor.m_sceneTabs.size()==1 && diskName()=="Original", "Discard saved changes to disk");

            setup(); dirtyFirst();
            editor.CloseSceneTab(0);
            Check(editor.ResolveSceneTabClose(true), "Save and close failed");
            Check(editor.m_sceneTabs.size()==1 && editor.m_activeSceneTab==0 &&
                  editor.m_sceneTabs[0].path=="second.scene" && diskName()=="Edited first",
                  "Saving inactive tab saved or closed the wrong scene");

            setup(); dirtyFirst();
            fs::remove("first.scene"); fs::create_directory("first.scene");
            editor.CloseSceneTab(0);
            Check(!editor.ResolveSceneTabClose(true), "Write failure reported success");
            Check(editor.m_sceneTabs.size()==2 && editor.IsSceneTabDirty(0) &&
                  editor.m_activeSceneTab==1 && !editor.m_sceneTabCloseError.empty(),
                  "Write failure closed the tab or lost edits");
            fs::remove("first.scene");
            editor.ResolveSceneTabClose(false);

            setup();
            app.scene.GetEntities()[0]->SetName("Edited second"); editor.ResetSceneHistory();
            editor.CloseSceneTab(1);
            Check(editor.ResolveSceneTabClose(false) && editor.m_activeSceneTab==0 &&
                  app.scene.GetPath()=="first.scene", "Closing active dirty tab did not restore the other scene");
            editor.CloseSceneTab(0);
            Check(editor.m_sceneTabs.size()==1, "Last scene tab unexpectedly closed");
            auto& baking=app.scene.GetBakingSettings();
            baking.lightmaps=false;baking.staticShadows=false;baking.occlusion=false;
            LightingProbe probe;probe.position=Vector3(3,4,5);probe.halfExtents=Vector3(8,9,10);
            probe.diffuseIntensity=.4f;probe.specularIntensity=.8f;
            for(int i=0;i<MaxLightingProbes;i++)app.scene.GetLightingProbes().push_back(probe);
            auto& wetness = app.scene.GetSurfaceWetnessSettings();
            wetness.enabled = true; wetness.amount = .6f;
            wetness.dryRoughness = .73f; wetness.wetRoughness = .16f; wetness.darkening = .32f;
            const auto authoredWetness = wetness;
            Check(app.scene.Save(),"Baking settings save failed");
            app.scene.Unload();
            Check(app.scene.GetSurfaceWetnessSettings() == SurfaceWetnessSettings{},
                  "Scene unload retained the previous scene's surface wetness");
            app.scene.Load("first.scene");
            Check(app.scene.GetSurfaceWetnessSettings() == authoredWetness,
                  "Surface wetness controls did not survive a real scene save and reload");
            Check(app.scene.GetLightingProbes().size()==MaxLightingProbes && app.scene.GetLightingProbes()[0]==probe,
                "Lighting probe settings did not survive scene reload");
            const auto& loaded=app.scene.GetBakingSettings();
            Check(!loaded.lightmaps && !loaded.staticShadows && !loaded.occlusion,
                "Environment baking settings did not survive scene reload");
            auto waitForBake=[&]() {
                const auto deadline=SDL_GetTicks()+5000;
                while(editor.m_environmentBakeRunning && SDL_GetTicks()<deadline) {
                    editor.UpdateEnvironmentBake();SDL_Delay(1);
                }
                Check(!editor.m_environmentBakeRunning,"Asynchronous baker failed to finish");
            };
            editor.StartEnvironmentBake();
            Check(editor.m_environmentBakeRunning,"Editor failed to launch bake process");
            auto running=editor.m_environmentBakeRunning;editor.StartEnvironmentBake();
            Check(running==editor.m_environmentBakeRunning,"Duplicate click launched another bake");
            waitForBake();Check(editor.m_environmentBakeStatus.find("Native bake finished")!=std::string::npos,"Successful bake reported failure");
            // A native snapshot job cannot be replaced by a second click.
            Check(editor.m_environmentBakeJob.valid()==false,"Completed native job retained a result");
            ImGui::NewFrame();ImGui::SetNextWindowSize(ImVec2(800,900));editor.DrawEnvironment();ImGui::Render();
            app.scene.Unload();
            auto canvas=app.scene.CreateEntity("Menu");
            canvas.AddComponent<Transform>();
            auto& root=*canvas.AddComponent<RectTransform>();root.anchorMin={0,0};root.anchorMax={1,1};root.size={0,0};root.depth=0;
            canvas.AddComponent<Canvas>()->screenSize={800,600};
            auto label=app.scene.CreateEntity("Menu item");
            label.AddComponent<Transform>();
            auto& rect=*label.AddComponent<RectTransform>();rect.SetParent(&canvas);rect.position={-200,0};rect.size={120,60};rect.depth=.3f;
            label.AddComponent<Text>()->text="Caption";
            auto group=app.scene.CreateEntity("Stretched grouping rect");
            auto& container=*group.AddComponent<RectTransform>();container.SetParent(&canvas);
            container.position={0,0};container.anchorMin={0,0};container.anchorMax={1,1};container.size={0,0};
            editor.m_sceneCameraMode=Editor::SceneCameraMode::SCENE_CAMERA_2D;
            editor.m_mode=EditorMode::EDIT;editor.m_editorCamera2DPosition={0,0};editor.m_editorCamera2DScale=1;
            editor.m_gameViewportWidth=editor.m_gameTextureWidth=800;
            editor.m_gameViewportHeight=editor.m_gameTextureHeight=200;
            editor.m_gameViewportPosX=editor.m_gameViewportPosY=0;
            editor.m_gameViewportDrawWidth=800;editor.m_gameViewportDrawHeight=200;
            editor.m_gameViewHovered=editor.m_sceneViewClicked=true;
            window.SetRenderSize(800,600);app.scene.ClearEditorCameraOverrides();
            editor.FocusEntity(canvas.TryGet());
            editor.m_app=&app;
            editor.m_selectedAssetPath="previously-selected.material";
            io.AddMousePosEvent(333,100);io.AddMouseButtonEvent(0,true);ImGui::NewFrame();editor.SelectSprite2D();ImGui::Render();
            Check(app.scene.GetEntities()[editor.m_index]->GetUUID()==label.GetUUID(),
                  "2D canvas preview click selected a grouping rect or used Game canvas dimensions");
            Check(editor.IsEntitySelected(label.TryGet()) && !editor.IsEntitySelected(canvas.TryGet()) &&
                  editor.GetSelectedEntities().size()==1 && editor.GetSelectedEntities()[0]->GetUUID()==label.GetUUID() &&
                  editor.m_selectedAssetPath.empty(),
                  "UI click updated an index but left hierarchy/inspector/gizmo selection on the old object");
            io.AddMouseButtonEvent(0,false);ImGui::NewFrame();
            ImGui::Begin("Selected UI");editor.DrawHierarchyPanel();editor.DrawInspectorPanel(false);ImGui::End();ImGui::Render();
            Check(ImGui::GetCurrentContext()->ErrorCountCurrentFrame == 0,
                  "Hierarchy/inspector font scopes crossed a window boundary");
            Check(editor.IsEntitySelected(label.TryGet()) && editor.GetSelectedEntities()[0]->GetUUID()==label.GetUUID(),
                  "UI selection did not survive hierarchy/inspector refresh after mouse release");
            auto gizmoFrame=[&](float x,bool down) {
                io.AddMousePosEvent(x,100);io.AddMouseButtonEvent(0,down);ImGui::NewFrame();
                ImGui::SetNextWindowPos(ImVec2(0,0));ImGui::SetNextWindowSize(ImVec2(800,200));
                ImGui::Begin("UI gizmo test",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove);
                editor.DrawSceneViewGizmo();bool usingGizmo=ImGuizmo::IsUsing();ImGui::End();ImGui::Render();return usingGizmo;
            };
            gizmoFrame(363,false);gizmoFrame(363,false);
            Check(gizmoFrame(363,true),"UI with both Transform and RectTransform did not expose its 2D gizmo");
            gizmoFrame(383,true);gizmoFrame(383,false);
            Check(rect.position.x>-180 && std::abs(rect.position.y)<.01f,
                  "Dragging the selected UI gizmo did not update its reference-canvas position");
            Check(window.GetScreenWidth()==800 && window.GetScreenHeight()==600 && !app.scene.HasEditorCamera2DOverride(),
                  "Scene UI picking leaked preview dimensions/camera into the Game view");

            // Exercise actual Scene flight input with a selected prefab root.
            // Its geometry is ten metres from the root, so pivot distance would
            // incorrectly keep full speed near the surface.
            app.scene.Unload();
            const int modelId = AssetManager::CreateModel();
            ModelAsset::PrimitiveBuild3D quad;
            quad.vertices = {{{-2,-2,0},{0,0,1},{}}, {{2,-2,0},{0,0,1},{}},
                             {{2,2,0},{0,0,1},{}}, {{-2,2,0},{0,0,1},{}}};
            quad.indices = {0,1,2,0,2,3};
            Check(AssetManager::GetModel(modelId)->SetRuntimePrimitives({quad}), "Navigation fixture model failed");
            auto selection = app.scene.CreateEntity("Group");selection.AddComponent<Transform>();
            auto child = app.scene.CreateEntity("Offset model");
            child.AddComponent<Transform>()->SetParent(&selection);
            child.GetComponent<Transform>().position.z = 10;
            child.AddComponent<Model>()->modelId = modelId;
            editor.FocusEntity(selection.TryGet());
            editor.m_sceneCameraMode = Editor::SCENE_CAMERA_3D;
            editor.m_editorCamera3DYaw = -90;editor.m_editorCamera3DPitch = 0;
            editor.m_gameViewHovered = true;
            SDL_Event mouseDown{};mouseDown.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
            mouseDown.button.windowID = SDL_GetWindowID(static_cast<SDL_Window*>(window.GetSDLWindow()));
            mouseDown.button.button = SDL_BUTTON_RIGHT;
            SDL_PushEvent(&mouseDown);input.Update(&window);input.SetSyntheticFocus(true);
            input.SetSyntheticKey(SDL_SCANCODE_W, true);
            Time::Init(60);Time::SetFixedDelta(1.0f / 60.0f);Time::StartFrame();
            auto flight = [&](float z) {
                editor.m_editorCamera3DPosition = Vector3(0,0,z);
                editor.ApplyInternalSceneCamera(1.0f / 60.0f);
                return z - editor.m_editorCamera3DPosition.z;
            };
            const float farStep = flight(30), nearStep = flight(11), insideStep = flight(10);
            Check(farStep > nearStep * 7.5f && nearStep > insideStep * 4.5f && insideStep > 0,
                  "Actual camera flight did not slow near selected child geometry or became stuck inside it");
            input.SetSyntheticKey(SDL_SCANCODE_LSHIFT, true);
            Check(std::abs(flight(11) - nearStep * 3) < .00001f, "Shift flight boost was lost near selection");
            input.SetSyntheticKey(SDL_SCANCODE_LSHIFT, false);editor.FocusEntity(nullptr);
            Check(std::abs(flight(11) - farStep) < .00001f, "Unselected flight unexpectedly slowed down");
            input.SetSyntheticKey(SDL_SCANCODE_W, false);Time::Quit();
            app.scene.Unload();AssetManager::FreeModel(modelId);
            editor.m_scene = nullptr;
        }
    };
}

int main(int argc, char** argv)
{
    if(argc==2 && std::string(argv[1])=="--fake-bake-success")return 0;
    if(argc==2 && std::string(argv[1])=="--fake-bake-failure")return 7;
    if (argc != 2) return 1;
    const auto executable=fs::absolute(argv[0]);
    const auto original=fs::current_path();
    const auto sourceAssets=fs::absolute(argv[1]).parent_path();
    auto temp=fs::temp_directory_path()/("canis-scene-tabs-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(temp); fs::current_path(temp);
    try {
        fs::create_directories("assets");
        fs::copy(argv[1], "assets/shaders", fs::copy_options::recursive);
        fs::create_directories("assets/defaults/models");
        fs::create_directories("assets/defaults/materials");
        fs::create_directories("assets/brutal-city/shaders");
        fs::create_directories("assets/brutal-city/materials");
        fs::create_directories("assets/brutal-city/source");
        fs::copy_file(sourceAssets/"defaults/models/sphere.glb","assets/defaults/models/sphere.glb");
        fs::copy_file(sourceAssets/"brutal-city/materials/SideWalk.material","assets/brutal-city/materials/SideWalk.material");
        for(const auto* filename:{"Concrete046_2K-JPG_Color.jpg","Concrete046_2K-JPG_NormalGL.jpg",
                                 "Concrete046_2K-JPG_Roughness.png"}) {
            const auto relative=fs::path("brutal-city/source")/filename;
            fs::copy_file(sourceAssets/relative,fs::path("assets")/relative);
            fs::copy_file(fs::path((sourceAssets/relative).string()+".meta"),
                          fs::path((fs::path("assets")/relative).string()+".meta"));
        }
        for(const auto* extension:{".vs",".fs"})
            fs::copy_file(sourceAssets/(std::string("brutal-city/shaders/wet")+extension),
                          std::string("assets/brutal-city/shaders/wet")+extension);
        std::ofstream("assets/defaults/materials/default.material")
            << "shader: {path: assets/shaders/model3d}\ncolor: [1, 1, 1, 1]\nroughnessValue: 0.5\nspecularValue: 0.5\n";
        Canis::Window window("Scene tab test",800,600,true);
        ImGui::CreateContext();
        auto& io=ImGui::GetIO(); io.IniFilename=nullptr; io.DisplaySize=ImVec2(800,600); io.DeltaTime=1.f/60;
        unsigned char* pixels; int width,height; io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
        {
            Canis::App app; Canis::Editor editor;
            Canis::EditorSceneTabsTestAccess::Run(app,editor,window,executable);
        }
        ImGui::DestroyContext();
        fs::current_path(original); fs::remove_all(temp);
        std::cout << "Scene tab X, cancel, discard, save, failed save and active-tab checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        fs::current_path(original);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
