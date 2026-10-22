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
        auto soft=loaded;
        auto softness=CreateShaderGraphNode("Float",4,{0,300});softness.floatValue=1;
        soft.nodes.push_back(softness);soft.nextNodeId=5;
        Check(GetShaderGraphNodeInputPins(soft.nodes[1]).size()==3,"point softness pin missing");
        Check(SetShaderGraphNodeInputLink(soft.nodes[1],"pointSoftness",{4,"value"}),"softness link rejected");
        Check(GetShaderGraphNodeInputLink(soft.nodes[1],"pointSoftness").nodeId==4,"softness link unreadable");
        Check(SaveShaderGraphDocument(path,soft),"soft lighting save failed");
        Check(LoadShaderGraphDocument(path,soft),"soft lighting load failed");
        Check(soft.nodes[1].inputT.nodeId==4,"softness did not round trip");
        std::string softVertex,softFragment;
        BuildShaderGraphSources(soft,softVertex,softFragment);
        Check(softFragment.find("sgp_Bands_1, 1.0000)")!=std::string::npos,"soft lighting input not compiled");
        Check(softFragment.find("clamp(pointSoftness,0.0,1.0)")!=std::string::npos,"softness not clamped");
        Check(fragment.find("return sgToonLighting(normal,bands,0.0);")!=std::string::npos,"legacy lighting changed");
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
        ShaderGraphDocument portal;
        auto local=CreateShaderGraphNode("LocalPosition",1,{0,0});
        auto length=CreateShaderGraphNode("Length",2,{200,0});length.inputA={1,"position"};
        auto screen=CreateShaderGraphNode("ScreenUV",3,{0,200});
        auto scene=CreateShaderGraphNode("SceneColor",4,{200,200});scene.inputUV={3,"uv"};
        auto depth=CreateShaderGraphNode("SceneDepthDifference",5,{0,400});
        auto divide=CreateShaderGraphNode("Divide",6,{200,400});divide.inputA={5,"value"};divide.inputB={2,"value"};
        auto saturate=CreateShaderGraphNode("Saturate",7,{400,400});saturate.inputA={6,"value"};
        auto inverse=CreateShaderGraphNode("OneMinus",8,{600,400});inverse.inputA={7,"value"};
        auto step=CreateShaderGraphNode("Step",9,{800,400});step.inputA={2,"value"};step.inputB={8,"value"};
        portal.nodes={local,length,screen,scene,depth,divide,saturate,inverse,step};
        portal.nextNodeId=10;portal.outputColor={4,"color"};portal.outputAlpha={9,"value"};
        Check(SaveShaderGraphDocument(path,portal),"portal graph save failed");
        Check(LoadShaderGraphDocument(path,portal),"portal graph load failed");
        BuildShaderGraphSources(portal,vertex,fragment);
        Check(vertex.find("fragmentLocalPos = sgPosition")!=std::string::npos,"local position varying missing");
        Check(fragment.find("sgSceneColor(sgScreenUV())")!=std::string::npos,"scene color link missing");
        Check(fragment.find("if (!softDepthAvailable) return 10000.0")!=std::string::npos,"safe depth fallback missing");
        Check(fragment.find("step(length(fragmentLocalPos)")!=std::string::npos,"reveal mask missing");
        Check(GenerateShaderGraphAssets(path,portal,&error),error.c_str());
        portal.vertexPosition={4,"color"};
        Check(!GenerateShaderGraphAssets(path,portal,&error) && error.find("fragment stage")!=std::string::npos,"scene sample allowed in vertex stage");
        ShaderGraphDocument bird;
        auto bp=CreateShaderGraphNode("LocalPosition",1,{0,0});
        auto axis=CreateShaderGraphNode("Vector3",2,{0,200});axis.vec3Value={1,0,0};
        auto dot=CreateShaderGraphNode("Dot",3,{200,0});dot.inputA={1,"position"};dot.inputB={2,"value"};
        auto lift=CreateShaderGraphNode("Vector3",4,{200,200});lift.vec3Value={0,1,0};
        auto multiply=CreateShaderGraphNode("Multiply",5,{400,0});multiply.inputA={3,"value"};multiply.inputB={4,"value"};
        auto add=CreateShaderGraphNode("Add",6,{600,0});add.inputA={1,"position"};add.inputB={5,"value"};
        auto world=CreateShaderGraphNode("ObjectToWorldPosition",7,{800,0});world.inputA={6,"value"};
        bird.nodes={bp,axis,dot,lift,multiply,add,world};bird.nextNodeId=8;bird.vertexPosition={7,"value"};
        Check(GetShaderGraphNodeOutputPins(dot)[0].type==ShaderGraphValueType::FLOAT,"Dot output must be scalar");
        Check(GetShaderGraphNodeInputPins(world)[0].type==ShaderGraphValueType::VEC3,"Local-to-world input must be a position");
        Check(SaveShaderGraphDocument(path,bird)&&LoadShaderGraphDocument(path,bird),"Bird coordinate graph round trip failed");
        BuildShaderGraphSources(bird,vertex,fragment);
        Check(vertex.find("dot(sgPosition, vec3(1.0000, 0.0000, 0.0000))")!=std::string::npos,"Wing span extraction missing");
        Check(vertex.find("(sgModel * vec4((sgPosition +")!=std::string::npos,"Local wing deformation did not use per-instance model matrix");
        Check(GenerateShaderGraphAssets(path,bird,&error),error.c_str());
        bird.vertexPosition={};bird.outputColor={7,"value"};
        Check(!GenerateShaderGraphAssets(path,bird,&error)&&error.find("vertex stage")!=std::string::npos,"Object-to-world vertex input allowed in fragment stage");
        fs::remove_all(folder);SDL_Quit();
        std::cout<<"Shader graph links, metadata, material overrides, nodes and stage validation passed\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<"\n";SDL_Quit();return 1; }
}
