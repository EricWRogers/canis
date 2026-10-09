#include <Canis/Canis.hpp>
#include <Canis/Asset.hpp>
#include <Canis/OpenGL.hpp>
#include <Canis/PostProcessPipeline.hpp>
#include <Canis/Window.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <memory>
#include <cmath>
using namespace Canis;
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv)
{
    try
    {
        SetEditorRuntimeEnabled(false);
        Window window("Distance fog tests",128,64,true);
        const std::string shader=(std::filesystem::path(argv[1])/"assets/shaders/postprocess_distance_fog").string();
        const auto fixture=std::filesystem::temp_directory_path()/"canis-distance-fog-test.postprocess";
        auto asset=[&](float density,float blur)
        {
            std::ofstream(fixture)<<"passes:\n- shader: {path: "<<shader<<"}\n  settings:\n    fogClearDistance: 72\n    fogDensity: "<<density<<"\n    fogHeight: 12\n    fogHeightFalloff: 0.16\n    fogMaxOpacity: 0.8\n    fogColor: [0.26, 0.34, 0.38]\n    blurStartDistance: 72\n    blurEndDistance: 240\n    distanceBlurRadius: 24\n    distanceBlurStrength: "<<blur<<"\n";
            auto result=std::make_unique<PostProcessAsset>();Check(result->Load(fixture.string()),"Postprocess settings failed to load");return result;
        };
        RenderTarget source;EnsureRenderTarget(source,128,64);
        const Matrix4 projection=glm::perspective(glm::radians(60.0f),2.0f,0.1f,1000.0f);
        std::vector<float> pixels(128*64*4),depth(128*64);
        for(int y=0;y<64;++y)for(int x=0;x<128;++x)
        {
            size_t i=y*128+x;
            pixels[i*4]=pixels[i*4+1]=pixels[i*4+2]=(x%2)?1.0f:0.0f;pixels[i*4+3]=1;
            float distance=x<48?40.0f:200.0f;
            auto clip=projection*Vector4(0,0,-distance,1);
            depth[i]=(clip.z/clip.w)*0.5f+0.5f;
        }
        glBindTexture(GL_TEXTURE_2D,source.colorTexture);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,128,64,GL_RGBA,GL_FLOAT,pixels.data());
        glBindTexture(GL_TEXTURE_2D,source.depthTexture);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,128,64,GL_DEPTH_COMPONENT,GL_FLOAT,depth.data());
        auto render=[&](PostProcessAsset& post,float height)
        {
            const Matrix4 view=glm::lookAt(Vector3(0,height,0),Vector3(0,height,-1),Vector3(0,1,0));
            auto result=ApplyPostProcessChain(&post,source.framebuffer,source.colorTexture,source.depthTexture,128,64,projection,nullptr,view);
            std::vector<float> out(128*64*4);glBindFramebuffer(GL_READ_FRAMEBUFFER,result.framebuffer);glReadPixels(0,0,128,64,GL_RGBA,GL_FLOAT,out.data());return out;
        };
        auto disabled=asset(0,0);auto plain=render(*disabled,2);
        auto fog=asset(.009f,0);auto low=render(*fog,2);auto high=render(*fog,150);
        auto pixel=[](const std::vector<float>& p,int x){return p[(32*128+x)*4];};
        Check(std::abs(pixel(low,24)-pixel(plain,24))<.001f,"Fog changed the clear block");
        Check(pixel(low,88)>pixel(plain,88)+.1f,"Distant low surfaces did not gain fog");
        Check(std::abs(pixel(high,88)-pixel(plain,88))<.01f,"Fog followed camera height instead of world height");
        auto blurred=asset(0,1);auto soft=render(*blurred,2);
        Check(std::abs(pixel(soft,24)-pixel(plain,24))<.001f,"Distance blur changed nearby detail");
        Check(std::abs(pixel(soft,89)-pixel(soft,88))<std::abs(pixel(plain,89)-pixel(plain,88))*.9f,"Distant detail did not blur");
        Check(fog->GetPasses()[0].fogClearDistance==72&&fog->GetPasses()[0].fogColor==Vector3(.26f,.34f,.38f),"Fog inspector settings did not round trip");
        DestroyRenderTarget(source);std::filesystem::remove(fixture);
        std::cout<<"DISTANCE_FOG_PASS one block clear, far blur, height fog, adjustable settings\n";
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
