#include <Canis/Asset.hpp>
#include <Canis/Frustum.hpp>
#include <Canis/OpenGL.hpp>
#include <Canis/ShaderGraph.hpp>
#include <Canis/Window.hpp>
#include <Canis/RenderMetrics.hpp>
#include <Canis/Time.hpp>
#include <Canis/Scene.hpp>
#include <Canis/Components.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/RenderVisibility.hpp>
#include <Canis/ECS/Systems/MeshRenderer3DSystem.hpp>
#include <SDL3/SDL.h>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace Canis;
static void Check(bool ok,const char* message) { if(!ok)throw std::runtime_error(message); }

static void RoomOcclusion(Window& window) {
    Scene scene;
    scene.SetVRCamera(Matrix4(1),glm::perspective(glm::radians(70.f),320.f/240.f,.1f,100.f),.1f,100.f);
    MeshRenderer3DSystem renderer;
    renderer.scene=&scene;renderer.window=&window;renderer.Create();
    const int modelId=AssetManager::CreateModel();
    ModelAsset::PrimitiveBuild3D quad;
    quad.vertices={{{-1,-1,0},{0,0,1},{}},{{1,-1,0},{0,0,1},{}},{{1,1,0},{0,0,1},{}},{{-1,1,0},{0,0,1},{}}};
    quad.indices={0,1,2,0,2,3};
    Check(AssetManager::GetModel(modelId)->SetRuntimePrimitives({quad}),"Room fixture geometry failed");
    auto room=scene.CreateEntity("Hidden room");
    room.AddComponent<Transform>();room.AddComponent<RenderVisibilityGroup>();
    std::vector<Entity> walls;
    for(int i=0;i<4;++i) {
        auto entity=scene.CreateEntity("Quad");
        auto& t=*entity.AddComponent<Transform>();
        if(i<2) { t.position=Vector3(0,0,-2);t.scale=Vector3(2);walls.push_back(entity); }
        else { t.SetParent(&room);t.position=Vector3((i-2)*.3f,0,-5);t.scale=Vector3(.2f); }
        auto& model=*entity.AddComponent<Model>();model.modelId=modelId;model.staticModel=true;
    }
    auto draw=[&] {
        RenderMetrics::BeginFrame();glViewport(0,0,320,240);glDepthMask(GL_TRUE);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        renderer.Update(scene.GetRegistry(),1.f/60.f);
        RenderMetrics::EndSubmit();glFinish();RenderMetrics::EndPresent();
    };
    draw();Check(RenderMetrics::Latest().roomCulledInstances==0,"Room culled before a visibility result");
    for(int i=0;i<8;++i)draw();
    Check(RenderMetrics::Latest().roomCulledInstances==2,"Occluded room was not culled");
    scene.SetVRCamera(glm::translate(Matrix4(1),Vector3(.01f,0,0)),glm::perspective(glm::radians(70.f),320.f/240.f,.1f,100.f),.1f,100.f);
    draw();Check(RenderMetrics::Latest().roomCulledInstances==0,"Camera change reused stale occlusion");
    for(int i=0;i<8;++i)draw();
    Check(RenderMetrics::Latest().roomCulledInstances==2,"Room did not become occluded again");
    AssetManager::ReloadModel("unloaded-occlusion-fixture.glb");
    draw();Check(RenderMetrics::Latest().roomCulledInstances==0,"Asset reload reused stale occlusion");
    for(int i=0;i<8;++i)draw();
    Check(RenderMetrics::Latest().roomCulledInstances==2,"Room did not recover after reload invalidation");
    for(auto wall:walls)wall.GetComponent<Transform>().position.x=10;
    draw();Check(RenderMetrics::Latest().roomCulledInstances==0,"Moving an occluder left room hidden");
    for(int i=0;i<8;++i)draw();
    Check(RenderMetrics::Latest().roomCulledInstances==0,"Exposed room remained hidden");
    Check(glGetError()==GL_NO_ERROR,"Room queries corrupted GL state");
    renderer.OnDestroy();AssetManager::FreeModel(modelId);
}

int main(int argc,char** argv) {
    try {
        if(argc>1)std::filesystem::current_path(argv[1]);
        const Matrix4 projection=glm::perspective(glm::radians(70.f),1.f,.1f,100.f);
        const Vector3 lo(-.5f),hi(.5f);
        Check(!BoundsOutsideFrustum(projection*glm::translate(Matrix4(1),Vector3(0,0,-3)),lo,hi),"Visible bounds culled");
        Check(BoundsOutsideFrustum(projection*glm::translate(Matrix4(1),Vector3(20,0,-3)),lo,hi),"Offscreen bounds retained");
        Check(BoundsOutsideFrustum(projection*glm::translate(Matrix4(1),Vector3(0,0,3)),lo,hi),"Behind-camera bounds retained");
        Check(!BoundsOutsideFrustum(projection,lo,hi),"Near-plane crossing culled");
        const auto scaled=glm::scale(glm::rotate(glm::translate(Matrix4(1),Vector3(0,0,-3)),.7f,Vector3(0,1,0)),Vector3(-2,.1f,3));
        Check(!BoundsOutsideFrustum(projection*scaled,lo,hi),"Negative nonuniform scale culled");
        Matrix4 invalid(1);invalid[0][0]=std::numeric_limits<float>::quiet_NaN();
        Check(!BoundsOutsideFrustum(invalid,lo,hi),"Invalid bounds not conservative");
        const auto left=glm::frustum(-.12f,.08f,-.1f,.1f,.1f,100.f);
        const auto right=glm::frustum(-.08f,.12f,-.1f,.1f,.1f,100.f);
        Check(!BoundsOutsideFrustum(left*scaled,lo,hi) && !BoundsOutsideFrustum(right*scaled,lo,hi),"Stereo projection culled visible bounds");
        Window window("Renderer tests",320,240,true);
        Time::Init(120);
        Time::StartFrame();SDL_Delay(2200);Time::EndFrame();
        const auto resumed=SDL_GetTicks();
        Time::StartFrame();Time::EndFrame();
        Check(SDL_GetTicks()-resumed<1000,"Long frame corrupted limiter delay");
        Time::Quit();
        Profiler::Get().SetRecording(true);
        for(int i=0;i<4;++i) {
            RenderMetrics::BeginFrame();
            { RenderMetrics::Scope parent("Parent");
              { RenderMetrics::Scope child("Child");glClear(GL_COLOR_BUFFER_BIT); }
            }
            RenderMetrics::EndSubmit();glFinish();RenderMetrics::EndPresent();
        }
        const auto& gpu=RenderMetrics::LatestPasses();
        Check(gpu.id>0 && gpu.passes.size()==2,"GPU pass queries not resolved");
        Check(gpu.passes[0].depth==0 && gpu.passes[1].depth==1,"GPU pass nesting lost");
        Check(gpu.passes[0].gpuMs>=gpu.passes[1].gpuMs && gpu.passes[1].gpuMs>=0,"Invalid GPU pass duration");
        Check(!RenderMetrics::History().empty(),"GPU capture not recorded");
        Profiler::Get().SetProfileEditor(false);
        for(int i=0;i<4;++i) {
            RenderMetrics::BeginFrame();
            { RenderMetrics::Scope editor("Scene viewport",true);
              RenderMetrics::Scope nested("Editor geometry");glClear(GL_COLOR_BUFFER_BIT); }
            { RenderMetrics::Scope game("Game viewport");glClear(GL_COLOR_BUFFER_BIT); }
            RenderMetrics::EndSubmit();glFinish();RenderMetrics::EndPresent();
        }
        Check(RenderMetrics::LatestPasses().passes.size()==1 &&
              RenderMetrics::LatestPasses().passes[0].name=="Game viewport",
              "Editor GPU filtering removed game or retained editor passes");
        Profiler::Get().SetProfileEditor(true);
        RenderMetrics::BeginFrame();
        for(int i=0;i<140;++i) { RenderMetrics::Scope pass("Overflow"); }
        RenderMetrics::EndSubmit();glFinish();RenderMetrics::EndPresent();
        RenderMetrics::BeginFrame();RenderMetrics::EndSubmit();glFinish();RenderMetrics::EndPresent();
        Check(RenderMetrics::LatestPasses().passes.size()==128 && RenderMetrics::LatestPasses().dropped==12,
            "GPU pass cap not enforced");
        RenderMetrics::ClearHistory();Profiler::Get().SetRecording(false);
        RenderMetrics::BeginFrame();RenderMetrics::EndSubmit();glFinish();RenderMetrics::EndPresent();
        Check(RenderMetrics::History().empty(),"Cleared GPU history repopulated by pending queries");
        ModelAsset model;
        ModelAsset::PrimitiveBuild3D triangle;
        const auto normal=glm::normalize(Vector3(.2,.5,1));
        triangle.vertices={{{-.4f,-.4f,0},normal,{}},{{.4f,-.4f,0},normal,{}},{{0,.4f,0},normal,{}}};
        triangle.indices={0,1,2};
        Check(model.SetRuntimePrimitives({triangle}),"Runtime model failed");
        Vector3 minimum,maximum;
        Check(model.GetLocalBounds(minimum,maximum) && maximum.x==.4f,"Bounds failed");
        triangle.vertices[1].position.x=.5f;
        Check(model.SetRuntimePrimitives({triangle}),"Geometry edit failed");
        Check(model.GetLocalBounds(minimum,maximum) && maximum.x==.5f,"Cached bounds not invalidated");
        ShaderGraphDocument graph;
        graph.nodes={CreateShaderGraphNode("Normal",1,{})};graph.outputColor={1,"normal"};
        std::string vertex,fragment;BuildShaderGraphSources(graph,vertex,fragment);
        Shader shader;shader.CompileSource(vertex,fragment,"instancing test");shader.Link();shader.Use();
        shader.SetMat4("P",Matrix4(1));shader.SetMat4("V",Matrix4(1));
        std::vector<Matrix4> matrices;
        std::vector<Color> colors;
        for(int i=0;i<3;++i) {
            matrices.push_back(glm::scale(glm::rotate(glm::translate(Matrix4(1),Vector3((i-1)*.65f,0,0)),i*.2f,Vector3(0,1,0)),Vector3(i==2?-.6f:.6f,.8f,.3f)));
            colors.push_back(i==0?Color(1,.2,.3,1):i==1?Color(.2,1,.3,1):Color(.3,.2,1,1));
        }
        glViewport(0,0,320,240);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);
        glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
        for(size_t i=0;i<matrices.size();++i)model.Draw(shader,matrices[i],nullptr,-1,colors[i]);
        std::vector<unsigned char> single(320*240*4),instanced(single.size());
        glReadPixels(0,0,320,240,GL_RGBA,GL_UNSIGNED_BYTE,single.data());
        glClear(GL_COLOR_BUFFER_BIT);
        model.DrawInstanced(shader,matrices,-1,Color(1),-1,true,&colors);
        glReadPixels(0,0,320,240,GL_RGBA,GL_UNSIGNED_BYTE,instanced.data());
        size_t lit=0,differences=0;
        for(size_t i=0;i<single.size();i+=4) { if(single[i]||single[i+1]||single[i+2])++lit;
            for(size_t j=0;j<3;++j)if(std::abs(int(single[i+j])-int(instanced[i+j]))>1)++differences; }
        Check(lit>1000,"Blank reference image");Check(differences==0,"Instanced color/normal transforms differ");
        auto drawCached=[&]() {
            RenderMetrics::BeginFrame();
            glClear(GL_COLOR_BUFFER_BIT);
            model.DrawInstanced(shader,matrices,-1,Color(1),-1,true,&colors);
            glReadPixels(0,0,320,240,GL_RGBA,GL_UNSIGNED_BYTE,instanced.data());
            RenderMetrics::EndSubmit();glFinish();RenderMetrics::EndPresent();
        };
        drawCached();
        Check(RenderMetrics::Latest().instanceCacheHits==1 && RenderMetrics::Latest().instanceUploadBytes==0,
              "Unchanged instance data uploaded again");
        Check(single==instanced,"Cached draw changed pixels");
        matrices[0]=glm::translate(matrices[0],Vector3(.3f,0,0));
        colors[1]=Color(0,0,1,1);
        drawCached();
        Check(RenderMetrics::Latest().instanceUploadBytes>0,"Transform/color edit did not invalidate cache");
        glClear(GL_COLOR_BUFFER_BIT);
        for(size_t i=0;i<matrices.size();++i)model.Draw(shader,matrices[i],nullptr,-1,colors[i]);
        glReadPixels(0,0,320,240,GL_RGBA,GL_UNSIGNED_BYTE,single.data());
        Check(single==instanced,"Edited cached instances differ from individual draws");
        Check(model.SetRuntimePrimitives({triangle}),"Geometry replacement failed");
        drawCached();
        Check(RenderMetrics::Latest().instanceUploadBytes>0,"Geometry replacement retained stale instance buffers");
        Check(glGetError()==GL_NO_ERROR,"OpenGL error during instancing");
        model.Free();
        RoomOcclusion(window);
        std::cout<<"Frustum, stereo, bounds invalidation and instanced pixel parity passed\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
