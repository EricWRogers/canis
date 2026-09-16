#include <Canis/ShaderGraph.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/Yaml.hpp>
#include <SDL3/SDL.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs=std::filesystem;
using namespace Canis;
void Check(bool value,const char* message) { if(!value)throw std::runtime_error(message); }
int main() {
    SDL_Init(0);
    auto folder=fs::temp_directory_path()/("canis-graph-test-"+std::to_string(SDL_GetTicksNS()));
    fs::create_directories(folder);
    try {
        auto path=(folder/"toon.shadergraph").string();
        ShaderGraphDocument graph;
        auto bands=CreateShaderGraphProperty(ShaderGraphPropertyType::FLOAT,1,"Bands");bands.floatValue=4;
        graph.properties.push_back(bands);
        auto property=CreateShaderGraphNode("Property",1,{0,0});property.propertyId=1;
        auto lighting=CreateShaderGraphNode("ToonLighting",2,{300,0});lighting.inputB={1,"value"};
        auto preview=CreateShaderGraphNode("Preview",3,{600,0});preview.inputA={2,"value"};
        graph.nodes={property,lighting,preview};graph.outputColor={2,"value"};graph.nextNodeId=4;graph.nextPropertyId=2;
        Check(SaveShaderGraphDocument(path,graph),"save failed");
        ShaderGraphDocument loaded;Check(LoadShaderGraphDocument(path,loaded),"load failed");
        Check(loaded.nodes[1].inputB.nodeId==1,"connection did not round trip");
        std::string vertex,fragment,error;
        BuildShaderGraphSources(loaded,vertex,fragment);
        Check(fragment.find("sgToonLighting(normalize(fragmentNormal), sgp_Bands_1)")!=std::string::npos,"lighting graph not compiled");
        Check(fragment.find("directionalShadowMap")!=std::string::npos,"shadow support missing");
        Check(GenerateShaderGraphAssets(path,loaded,&error),error.c_str());
        auto meta=YAML::LoadFile(path+".meta");auto uuid=meta["UUID"].as<uint64_t>();
        Check(meta["ShaderGraph"]["fragmentSource"].as<std::string>()==fragment,"metadata GLSL differs");
        Check(!fs::exists(GetShaderGraphGeneratedVertexPath(path)) && !fs::exists(GetShaderGraphGeneratedFragmentPath(path)),"loose shaders exported");
        auto material=YAML::LoadFile(GetShaderGraphGeneratedMaterialPath(path));
        Check(material["shader"]["path"].as<std::string>()==path,"material does not reference graph");
        material.remove("uniforms");
        auto defaults=ResolveShaderGraphMaterial(material);
        Check(defaults["uniforms"]["sgp_Bands_1"]["value"].as<float>()==4,"graph default missing");
        material["uniforms"]["sgp_Bands_1"]["type"]="float";
        material["uniforms"]["sgp_Bands_1"]["value"]=7;
        Check(ResolveShaderGraphMaterial(material)["uniforms"]["sgp_Bands_1"]["value"].as<float>()==7,"material override overwritten");
        AssetManager::GetMetaFile(path)->Save();
        Check(YAML::LoadFile(path+".meta")["ShaderGraph"]["fragmentSource"].as<std::string>()==fragment,"metadata refresh removed GLSL");
        loaded.nodes[1].floatValue=6;loaded.nodes[1].inputB={};
        Check(GenerateShaderGraphAssets(path,loaded,&error),error.c_str());
        Check(YAML::LoadFile(path+".meta")["UUID"].as<uint64_t>()==uuid,"graph identity changed");
        loaded.vertexPosition={2,"value"};
        Check(!GenerateShaderGraphAssets(path,loaded,&error) && error.find("fragment stage")!=std::string::npos,"invalid stage not rejected");
        loaded.vertexPosition={};
        loaded.nodes[1].type="Fresnel";BuildShaderGraphSources(loaded,vertex,fragment);
        Check(fragment.find("normalize(cameraPosition - fragmentWorldPos)")!=std::string::npos,"Fresnel missing");
        loaded.nodes[1].type="Posterize";BuildShaderGraphSources(loaded,vertex,fragment);
        Check(fragment.find("floor((0.5)")!=std::string::npos,"posterization missing");
        fs::remove_all(folder);SDL_Quit();
        std::cout<<"Shader graph links, metadata, material overrides, nodes and stage validation passed\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<"\n";SDL_Quit();return 1; }
}
