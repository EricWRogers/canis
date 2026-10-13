#include <Canis/StaticVisibility.hpp>
#include <Canis/Asset.hpp>
#include <Canis/Frustum.hpp>
#include <Canis/OpenGL.hpp>
#include <Canis/ShaderGraph.hpp>
#include <Canis/Window.hpp>
#include <Canis/RenderMetrics.hpp>
#include <Canis/Time.hpp>
#include <Canis/Scene.hpp>
#include <Canis/App.hpp>
#include <Canis/Scripting/SceneBindings.hpp>
#include <Canis/Scripting/ManagedComponents.hpp>
#include <Canis/Scripting/NativeBindings.hpp>
#include <Canis/ECS/Systems/ModelAnimation3DSystem.hpp>
#include <Canis/Components.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/RenderVisibility.hpp>
#include <Canis/PostProcessPipeline.hpp>
#include <Canis/ECS/Systems/MeshRenderer3DSystem.hpp>
#include <Canis/ECS/Systems/SpriteRenderer2DSystem.hpp>
#include <Canis/VFX/Particles.hpp>
#include <Canis/ECS/Systems/JoltPhysics3DSystem.hpp>
#include <SDL3/SDL.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace Canis;
static void Check(bool ok,const char* message) { if(!ok)throw std::runtime_error(message); }

// Model-relative and material-file references must share one GPU texture. External
// images stay lazy, while embedded images retain their decoded pixels/orientation.
static void ImportedTextureLoading() {
    using Canis::Scripting::ManagedJson;
    const auto root=std::filesystem::temp_directory_path()/("canis-texture-loading-"+std::to_string(SDL_GetTicksNS()));
    std::filesystem::create_directories(root/"nested");
    const unsigned char png[]={137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,2,0,0,0,2,8,6,0,0,0,114,182,13,36,0,0,0,20,73,68,65,84,120,156,99,248,207,192,240,31,4,89,24,193,52,3,3,0,62,255,6,0,250,92,19,176,0,0,0,0,73,69,78,68,174,66,96,130};
    std::ofstream image(root/"colors.png",std::ios::binary);image.write(reinterpret_cast<const char*>(png),sizeof(png));image.close();
    std::ofstream(root/"unused.png") << "Unused image must not be decoded";
    const float positions[]={0,0,0,1,0,0,0,1,0};
    std::ofstream binary(root/"mesh.bin",std::ios::binary);binary.write(reinterpret_cast<const char*>(positions),sizeof(positions));binary.close();
    auto fixture=ManagedJson::parse(R"({
      "asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],"nodes":[{"mesh":0}],
      "meshes":[{"primitives":[{"attributes":{"POSITION":0},"material":0}]}],
      "buffers":[{"uri":"mesh.bin","byteLength":36}],"bufferViews":[{"buffer":0,"byteLength":36}],
      "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]}],
      "materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],"textures":[{"source":0}],
      "images":[{"uri":"nested/../colors.png"},{"uri":"unused.png"}]
    })");
    std::ofstream(root/"external.gltf") << fixture.dump();
    const int shared=AssetManager::LoadTexture((root/"colors.png").string());
    const auto count=AssetManager::GetAssetLibrary().assets.size();
    ModelAsset external;
    Check(external.Load((root/"external.gltf").string()),"External glTF texture fixture failed");
    Check(AssetManager::GetAssetLibrary().assets.size()==count,"Relative texture path caused a duplicate GPU upload");
    const std::vector<unsigned char> expected={255,0,0,255,0,255,0,255,0,0,255,255,255,255,255,255};
    auto pixels=[&](int id) {
        auto texture=AssetManager::GetTexture(id)->GetGLTexture();
        Check(texture.width==2&&texture.height==2&&texture.id!=0,"Imported texture dimensions missing");
        std::vector<unsigned char> result(16);glBindTexture(GL_TEXTURE_2D,texture.id);
        glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,result.data());glBindTexture(GL_TEXTURE_2D,0);return result;
    };
    Check(pixels(shared)==expected,"External image orientation or colors changed");
    std::vector<std::string> preload;
    for(int i=0;i<9;i++)
    {
        const auto path=root/("worker-"+std::to_string(i)+".png");
        std::filesystem::copy_file(root/"colors.png",path);
        preload.push_back(path.string());
    }
    preload.push_back((root/"nested/../worker-0.png").string());
    preload.push_back((root/"unused.png").string());
    Check(AssetManager::PreloadTexturesParallel(preload)==9,"Parallel preload lost or duplicated textures");
    for(size_t i=0;i<9;i++)Check(pixels(AssetManager::LoadTexture(preload[i]))==expected,"Worker decode changed pixels or orientation");
    Check(AssetManager::PreloadTexturesParallel(preload)==0,"Repeated preload changed the shared cache");
    Check(AssetManager::LoadTexture(preload[0])==AssetManager::LoadTexture(preload[9]),"Normalized preload alias duplicated texture");
    Check(AssetManager::GetID((root/"unused.png").string())<0,"Failed background decode poisoned the asset cache");
    std::filesystem::copy_file(root/"colors.png",root/"unused.png",std::filesystem::copy_options::overwrite_existing);
    Check(AssetManager::PreloadTexturesParallel({(root/"unused.png").string()})==1,"Failed image could not be retried after repair");
    // Metadata parsing may run in parallel; UUID repair/registration must remain ordered.
    std::vector<std::string> metadata;
    for(int i=0;i<2;i++)
    {
        auto path=root/("worker-"+std::to_string(i)+".png");metadata.push_back(path.string());
        std::ofstream(path.string()+".meta") << "FileType: TEXTURE\nUUID: 792111222333444555\nname: worker\nextension: png\nsize: 0\nmodified: 0\n";
    }
    metadata.push_back(preload[2]);metadata.push_back(metadata[0]);
    AssetManager::LoadMetadataParallel(metadata);
    auto *first=AssetManager::GetMetaFile(metadata[0]),*second=AssetManager::GetMetaFile(metadata[1]);
    Check((uint64_t)first->uuid==792111222333444555ull && first->uuid!=second->uuid,"Parallel metadata changed deterministic UUID collision repair");
    Check(AssetManager::GetPath(second->uuid)==metadata[1],"Repaired metadata path missing from asset manager");
    Check(std::filesystem::exists(metadata[2]+".meta"),"Missing metadata was not generated");
    AssetManager::LoadMetadataParallel(metadata);
    Check(AssetManager::GetMetaFile(metadata[0])==first,"Repeated metadata scan replaced cached objects");
    fixture["images"]=ManagedJson::array({{{"name","colors"},{"uri","data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0kAAAAFElEQVR4nGP4z8DwHwRZGME0AwMAPv8GAPpcE7AAAAAASUVORK5CYII="}}});
    std::ofstream(root/"embedded.gltf") << fixture.dump();
    ModelAsset embedded;
    Check(embedded.Load((root/"embedded.gltf").string()),"Embedded image decoding failed");
    int extracted=AssetManager::GetID((root/"embedded_textures/colors.tga").string());
    Check(extracted>=0&&pixels(extracted)==expected,"Embedded image extraction changed orientation or colors");
    {
        std::ofstream skyFile(root/"orientation.skybox");
        for (const char* face : {"right","left","top","bottom","front","back"})
            skyFile << face << ":\n  path: " << (root/"colors.png").string()
                    << "\n  rotation: " << (std::string(face)=="top" ? 180 :
                        std::string(face)=="right" ? 90 : std::string(face)=="bottom" ? -90 : 0) << '\n';
        skyFile.close();
        SkyboxAsset sky;
        Check(sky.Load((root/"orientation.skybox").string()),"Skybox face orientation fixture failed");
        glBindTexture(GL_TEXTURE_CUBE_MAP,sky.GetTexture());
        const int order[6][4]={{2,0,3,1},{0,1,2,3},{3,2,1,0},{1,3,0,2},{0,1,2,3},{0,1,2,3}};
        for (int face=0;face<6;++face)
        {
            std::vector<unsigned char> actual(16);
            glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,0,GL_RGBA,GL_UNSIGNED_BYTE,actual.data());
            for(int pixel=0;pixel<4;++pixel)
                for(int channel=0;channel<4;++channel)
                    Check(actual[pixel*4+channel]==expected[order[face][pixel]*4+channel],
                          "Cube face rotation changed its corner colors or a neighboring face");
        }
        glBindTexture(GL_TEXTURE_CUBE_MAP,0);
        sky.Free();
    }
    external.Free();embedded.Free();std::filesystem::remove_all(root);
    std::cout << "Imported textures share GPU cache, ignore unused images, and retain external/embedded pixel orientation\n";
}

// Imported model queries must use parent transforms just as rendering does.
static void ImportedHierarchyGeometry() {
    const auto root=std::filesystem::temp_directory_path()/("canis-model-hierarchy-"+std::to_string(SDL_GetTicksNS()));
    std::filesystem::create_directories(root);
    const float positions[]={0,0,0, 1,0,0, 0,1,0};
    const uint16_t indices[]={0,1,2};
    {
        std::ofstream binary(root/"mesh.bin",std::ios::binary);
        binary.write(reinterpret_cast<const char*>(positions),sizeof(positions));
        binary.write(reinterpret_cast<const char*>(indices),sizeof(indices));
        std::ofstream(root/"hierarchy.gltf") << R"({
          "asset":{"version":"2.0"}, "scene":0, "scenes":[{"nodes":[0]}],
          "nodes":[
            {"translation":[10,2,-3],"rotation":[0,0.7071067811865476,0,0.7071067811865476],"scale":[2,3,4],"children":[1]},
            {"translation":[1,0,0],"mesh":0}],
          "meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}],
          "buffers":[{"uri":"mesh.bin","byteLength":42}],
          "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6}],
          "accessors":[
            {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},
            {"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}]
        })";
    }
    ModelAsset model;
    Check(model.Load((root/"hierarchy.gltf").string()),"Hierarchy fixture failed to load");
    std::vector<Vector3> vertices;
    std::vector<u32> triangles;
    Check(model.BuildTriangleMesh(vertices,triangles),"Imported collision mesh missing");
    Check(vertices.size()==3 && triangles==std::vector<u32>({0,1,2}),"Imported triangle topology changed");
    Check(glm::distance(vertices[0],Vector3(10,2,-5))<.0001f &&
          glm::distance(vertices[1],Vector3(10,2,-7))<.0001f &&
          glm::distance(vertices[2],Vector3(10,5,-5))<.0001f,
          "Collision geometry lost imported parent translation, rotation or scale");
    Vector3 low,high;
    Check(model.GetLocalBounds(low,high) && glm::distance(low,Vector3(10,2,-7))<.0001f &&
          glm::distance(high,Vector3(10,5,-5))<.0001f,"Imported model bounds lost hierarchy transforms");
    Check(model.BuildTriangleMesh(vertices,triangles,1,false) && glm::distance(vertices[1],Vector3(1,0,0))<.0001f,
          "Untransformed node extraction unexpectedly baked its parents");
    Check(model.BuildTriangleMesh(vertices,triangles,1,true) && glm::distance(vertices[1],Vector3(10,2,-7))<.0001f,
          "Filtered node extraction lost its parents");
    // Exercise the actual C# pose binding beneath a rotated, nonuniformly scaled parent.
    // A fallen bone needs shear in its local matrix to keep its world scale fixed.
    {
        using namespace Canis::Scripting;
        App app; app.scene.app = &app;
        RegisterSceneBindings(app);
        struct Cleanup { ~Cleanup() { UnregisterSceneBindings(); } } cleanup;
        auto entity = app.scene.CreateEntity("Pose scale regression");
        entity.AddComponent<Transform>();
        entity.AddComponent<Model>()->modelId = AssetManager::LoadModel((root/"hierarchy.gltf").string());
        auto* animation = entity.AddComponent<ModelAnimation>();
        auto invoke = [](const std::string& name, std::initializer_list<NativeValue> arguments) {
            return NativeBindingRegistry::Get().Invoke(name,std::span<const NativeValue>(arguments.begin(),arguments.size()));
        };
        auto handle = Decode<uint64_t>(invoke("Scene.Find",{Encode(std::string("Pose scale regression"))}));
        invoke("Skeleton.Sample",{Encode(handle),Encode(0.f)});
        auto* loaded = AssetManager::GetModel(entity.GetComponent<Model>().modelId);
        ModelAsset::Pose3D original; loaded->ResetPose(original);
        Vector3 scale(glm::length(Vector3(original.globalNodeMatrices[1][0])),
                      glm::length(Vector3(original.globalNodeMatrices[1][1])),
                      glm::length(Vector3(original.globalNodeMatrices[1][2])));
        ModelAnimation3DSystem evaluator;
        for (int frame = 0; frame < 720; ++frame) {
            // Launch/tumble, then a long stationary landing. Reapplying either must not grow the bone.
            auto rotation = glm::angleAxis(frame < 120 ? frame*.025f : 2.3f,glm::normalize(Vector3(1,2,3)));
            Vector3 position(1,.2f,3);
            auto payload = ManagedJson::array({{{"Index",1},{"Position",{position.x,position.y,position.z}},
                {"Rotation",{rotation.x,rotation.y,rotation.z,rotation.w}}}}).dump();
            invoke("Skeleton.Apply",{Encode(handle),Encode(payload)});
            evaluator.Update(app.scene.GetRegistry(),1.f/120);
            auto expected = glm::translate(Matrix4(1),position)*glm::mat4_cast(rotation)*glm::scale(Matrix4(1),scale);
            for (int column = 0; column < 4; ++column)
                Check(glm::length(animation->pose.globalNodeMatrices[1][column]-expected[column]) < .001f,
                    "Repeated procedural pose changed bone scale or world transform");
        }
        invoke("Skeleton.Clear",{Encode(handle)});
        evaluator.Update(app.scene.GetRegistry(),0.f);
        for (int column = 0; column < 4; ++column)
            Check(glm::length(animation->pose.globalNodeMatrices[1][column]-original.globalNodeMatrices[1][column])<.001f,
                "Clearing a procedural pose did not restore the imported scale");
        std::cout << "Procedural pose scale stays stable through 720 tumble/landing frames\n";
    }
    model.Free();
    std::filesystem::remove_all(root);
    std::cout << "Imported hierarchy collision geometry and bounds passed\n";
}

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



// Read opaque depth after it has been rendered, including a nonzero viewport
// origin (the right eye of a direct stereo mirror), without texture feedback.
static void SoftDepthIntersections(Window& window) {
    auto root=std::filesystem::temp_directory_path()/("canis-soft-depth-"+std::to_string(SDL_GetTicksNS()));
    std::filesystem::create_directories(root);
    auto vertex=R"([OPENGL VERSION]
layout(location=0) in vec3 vertexPosition;
uniform mat4 P,V,M;out float eyeDepth;
void main(){vec4 p=M*vec4(vertexPosition,1);eyeDepth=-(V*p).z;gl_Position=P*V*p;}
)";
    std::ofstream(root/"soft.vs")<<vertex;
    std::ofstream(root/"opaque.vs")<<vertex;
    std::ofstream(root/"opaque.fs")<<"[OPENGL VERSION]\nout vec4 color;void main(){color=vec4(0,0,0,1);}";
    std::ofstream(root/"soft.fs")<<R"([OPENGL VERSION]
in float eyeDepth;
uniform sampler2D softSceneDepth;uniform bool softDepthAvailable;
uniform vec4 softDepthViewport;uniform mat4 softInverseProjection;out vec4 color;
void main(){
 vec2 uv=(gl_FragCoord.xy-softDepthViewport.xy)/softDepthViewport.zw;
 vec4 p=softInverseProjection*vec4(uv*2.-1.,texture(softSceneDepth,uv).r*2.-1.,1.);
 float gap=clamp((-p.z/p.w-eyeDepth),0.,1.);
 color=softDepthAvailable?vec4(gap,gap,gap,1):vec4(1,0,1,1);
}
)";
    std::ofstream(root/"soft.material")<<"shader: {path: '"<<(root/"soft").string()<<"'}\ncolor: [1,1,1,0.5]\nbackFaceCulling: false\n";
    std::ofstream(root/"opaque.material")<<"shader: {path: '"<<(root/"opaque").string()<<"'}\ncolor: [1,1,1,1]\nbackFaceCulling: false\n";
    Scene scene;
    int modelId=AssetManager::CreateModel();ModelAsset::PrimitiveBuild3D quad;
    quad.vertices={{{-2,-2,0},{0,0,1},{}},{{2,-2,0},{0,0,1},{}},{{2,2,0},{0,0,1},{}},{{-2,2,0},{0,0,1},{}}};quad.indices={0,1,2,0,2,3};
    Check(AssetManager::GetModel(modelId)->SetRuntimePrimitives({quad}),"Soft depth fixture geometry failed");
    auto wall=scene.CreateEntity("wall");wall.AddComponent<Transform>()->position=Vector3(0,0,-3);
    wall.AddComponent<Model>()->modelId=modelId;wall.AddComponent<Material>()->materialId=AssetManager::LoadMaterial((root/"opaque.material").string());
    auto shaft=scene.CreateEntity("shaft");shaft.AddComponent<Transform>()->position=Vector3(0,0,-2);
    shaft.AddComponent<Model>()->modelId=modelId;shaft.AddComponent<Material>()->materialId=AssetManager::LoadMaterial((root/"soft.material").string());
    MeshRenderer3DSystem renderer;renderer.scene=&scene;renderer.window=&window;renderer.Create();
    RenderTarget target;EnsureRenderTarget(target,320,120);
    auto render=[&](float wallZ,int origin){
        wall.GetComponent<Transform>().position.z=wallZ;
        glBindFramebuffer(GL_FRAMEBUFFER,target.framebuffer);glDisable(GL_SCISSOR_TEST);glViewport(origin,0,160,120);
        glDepthMask(GL_TRUE);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        scene.SetVRCamera(Matrix4(1),glm::perspective(glm::radians(70.f),160.f/120.f,.1f,100.f),.1f,100.f);
        renderer.Update(scene.GetRegistry(),1.f/60.f);
        std::array<unsigned char,4> pixel{};glReadPixels(origin+80,60,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data());return pixel;
    };
    for(int origin:{0,160}) {
        const auto near=render(-2.01f,origin),far=render(-3.5f,origin);
        Check(near[0]<8 && near[1]<8 && near[2]<8,"Soft depth did not fade at the opaque intersection");
        Check(far[0]>240 && far[1]>240 && far[2]>240,"Soft depth used stale/wrong-eye depth or missing uniforms");
    }
    Check(glGetError()==GL_NO_ERROR,"OpenGL error during soft depth copying");
    renderer.OnDestroy();glBindFramebuffer(GL_FRAMEBUFFER,0);DestroyRenderTarget(target);AssetManager::FreeModel(modelId);
    std::filesystem::remove_all(root);
}

static void SinglePassPixels() {
    if(!GLEW_VERSION_4_1){std::cout<<"Single-pass test skipped: GL 4.1 unavailable\n";return;}
    Shader mono;
    mono.CompileSource(R"([OPENGL VERSION]
layout(location=0) in vec3 vertexPosition;
layout(location=3) in mat4 instanceM;
uniform mat4 P,V,M;uniform bool useInstanceMatrix;
out vec3 worldPosition;flat out float marker;
void main(){vec4 p=(useInstanceMatrix?instanceM:M)*vec4(vertexPosition,1);worldPosition=p.xyz;marker=1.;gl_Position=P*V*p;}
)",R"([OPENGL VERSION]
in vec3 worldPosition;flat in float marker;uniform vec3 cameraPosition;out vec4 color;
void main(){color=vec4(.4+worldPosition.x*.1+cameraPosition.x,.3+worldPosition.y*.1,.2*marker,1);}
)","stereo pixel fixture");mono.Link();
    auto* stereo=mono.StereoVariant();Check(stereo!=nullptr,"Stereo shader compilation failed");
    Check(stereo==mono.StereoVariant(),"Stereo variant was not cached");
    ModelAsset model;ModelAsset::PrimitiveBuild3D quad;
    quad.vertices={{{-.5f,-.5f,0},{0,0,1},{}},{{.5f,-.5f,0},{0,0,1},{}},{{.5f,.5f,0},{0,0,1},{}},{{-.5f,.5f,0},{0,0,1},{}}};
    quad.indices={0,1,2,0,2,3};Check(model.SetRuntimePrimitives({quad}),"Stereo geometry failed");
    std::vector<Matrix4> models={glm::translate(Matrix4(1),Vector3(-.7f,0,-3)),glm::scale(glm::translate(Matrix4(1),Vector3(.7f,.2f,-2)),Vector3(-.7f,1.1f,1))};
    RenderTarget target;EnsureRenderTarget(target,320,120);glBindFramebuffer(GL_FRAMEBUFFER,target.framebuffer);
    glDisable(GL_SCISSOR_TEST);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);
    Matrix4 views[2]={glm::translate(Matrix4(1),Vector3(.08f,0,0)),glm::translate(Matrix4(1),Vector3(-.08f,0,0))};
    Matrix4 projections[2]={glm::frustum(-.12f,.08f,-.075f,.075f,.1f,100.f),glm::frustum(-.08f,.12f,-.075f,.075f,.1f,100.f)};
    Vector3 eyes[2]={Vector3(-.08f,0,0),Vector3(.08f,0,0)};
    std::vector<unsigned char> reference(320*120*4),actual(reference.size());
    glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    for(int eye=0;eye<2;++eye){glViewport(eye*160,0,160,120);mono.Use();mono.SetMat4("P",projections[eye]);mono.SetMat4("V",views[eye]);mono.SetVec3("cameraPosition",eyes[eye]);model.DrawInstanced(mono,models);}
    glReadPixels(0,0,320,120,GL_RGBA,GL_UNSIGNED_BYTE,reference.data());
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    const GLfloat viewports[]={0,0,160,120,160,0,160,120};glViewportArrayv(0,2,viewports);
    stereo->Use();stereo->SetMat4("P",projections[0]);stereo->SetMat4("V",views[0]);stereo->SetMat4("stereoInverseClip",glm::inverse(projections[0]*views[0]));
    for(int eye=0;eye<2;++eye){stereo->SetMat4("stereoClip["+std::to_string(eye)+"]",projections[eye]*views[eye]);stereo->SetMat4("stereoView["+std::to_string(eye)+"]",views[eye]);stereo->SetMat4("stereoProjection["+std::to_string(eye)+"]",projections[eye]);stereo->SetVec3("stereoEyes["+std::to_string(eye)+"]",eyes[eye]);}
    model.DrawInstanced(*stereo,models);glReadPixels(0,0,320,120,GL_RGBA,GL_UNSIGNED_BYTE,actual.data());
    size_t lit[2]={},differences=0;
    for(size_t i=0;i<reference.size();i+=4){if(reference[i])++lit[(i/4)%320>=160];for(int c=0;c<3;++c)if(std::abs(int(reference[i+c])-int(actual[i+c]))>1)++differences;}
    Check(lit[0]>100&&lit[1]>100,"Blank stereo reference eye");Check(differences==0,"Single-pass stereo differs from asymmetric per-eye reference");
    Check(glGetError()==GL_NO_ERROR,"Single-pass GL error");
    stereo->UnUse();glBindFramebuffer(GL_FRAMEBUFFER,0);DestroyRenderTarget(target);model.Free();
    Shader unsupported;unsupported.CompileSource("[OPENGL VERSION]\nuniform vec3 cameraPosition;out vec3 p;void main(){p=cameraPosition;gl_Position=vec4(p,1);}","[OPENGL VERSION]\nin vec3 p;out vec4 color;void main(){color=vec4(p,1);}","unsupported view-dependent vertex");unsupported.Link();
    Check(unsupported.StereoVariant()==nullptr,"View-dependent vertices must fall back");
}

static void StereoPreparation(Window& window) {
    Scene scene;
    auto parent=scene.CreateEntity("Parent");parent.AddComponent<Transform>();
    auto other=scene.CreateEntity("Other parent");other.AddComponent<Transform>();
    auto child=scene.CreateEntity("Cached static model");
    auto& t=*child.AddComponent<Transform>();t.position=Vector3(0,0,-3);t.SetParent(&parent);
    int id=AssetManager::CreateModel();
    ModelAsset::PrimitiveBuild3D quad;
    quad.vertices={{{-1,-1,0},{0,0,1},{}},{{1,-1,0},{0,0,1},{}},{{1,1,0},{0,0,1},{}},{{-1,1,0},{0,0,1},{}}};
    quad.indices={0,1,2,0,2,3};
    Check(AssetManager::GetModel(id)->SetRuntimePrimitives({quad}),"Stereo fixture geometry failed");
    auto& model=*child.AddComponent<Model>();model.modelId=id;model.staticModel=true;
    MeshRenderer3DSystem cached;cached.scene=&scene;cached.window=&window;cached.Create();
    auto draw=[&](MeshRenderer3DSystem& renderer,bool stereo) {
        std::vector<unsigned char> pixels(320*240*4*2);
        if(stereo)renderer.BeginStereoFrame(Vector3(0));
        for(int eye=0;eye<2;++eye) {
            scene.SetVRCamera(glm::translate(Matrix4(1),Vector3(eye ? -.032f : .032f,0,0)),
                glm::perspective(glm::radians(70.f),320.f/240.f,.1f,100.f),.1f,100.f);
            glViewport(0,0,320,240);glDepthMask(GL_TRUE);glClearColor(0,0,0,1);
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            renderer.Update(scene.GetRegistry(),1.f/60.f);
            glReadPixels(0,0,320,240,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data()+eye*320*240*4);
        }
        if(stereo)renderer.EndStereoFrame();
        return pixels;
    };
    auto verify=[&]() {
        auto actual=draw(cached,true);
        MeshRenderer3DSystem reference;reference.scene=&scene;reference.window=&window;reference.Create();
        auto expected=draw(reference,false);reference.OnDestroy();
        Check(actual==expected,"Shared/persistent render preparation differs from fresh per-eye rendering");
        return actual;
    };
    auto initial=verify();verify();
    if(GLEW_VERSION_4_1) {
        RenderTarget target;EnsureRenderTarget(target,640,240);
        glBindFramebuffer(GL_FRAMEBUFFER,target.framebuffer);glViewport(0,0,320,240);
        glDepthMask(GL_TRUE);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        const Matrix4 p=glm::perspective(glm::radians(70.f),320.f/240.f,.1f,100.f);
        const Matrix4 left=glm::translate(Matrix4(1),Vector3(.032f,0,0)),right=glm::translate(Matrix4(1),Vector3(-.032f,0,0));
        cached.BeginStereoFrame(Vector3(0));cached.SetStereoViews(left,right,p,p,Vector3(-.032f,0,0),Vector3(.032f,0,0));
        scene.SetVRCamera(left,p,.1f,100.f);cached.SetStereoPass(1);cached.Update(scene.GetRegistry(),1.f/60.f);cached.EndStereoFrame();
        std::vector<unsigned char> actual(initial.size());
        for(int eye=0;eye<2;++eye)glReadPixels(eye*320,0,320,240,GL_RGBA,GL_UNSIGNED_BYTE,actual.data()+eye*320*240*4);
        size_t errors=0;for(size_t i=0;i<actual.size();++i)if(std::abs(int(actual[i])-int(initial[i]))>1)++errors;
        Check(errors==0,"Renderer single-pass viewport/pixels differ from per-eye reference");
        auto sun=scene.CreateEntity("Stereo shadow viewport regression");sun.AddComponent<Transform>();
        auto& light=*sun.AddComponent<DirectionalLight>();
        cached.BeginStereoFrame(Vector3(0));cached.SetStereoPass(1);
        cached.Update(scene.GetRegistry(),1.f/60.f);cached.EndStereoFrame();
        GLfloat rightViewport[4];glGetFloati_v(GL_VIEWPORT,1,rightViewport);
        Check(rightViewport[0]==320 && rightViewport[2]==320,"Shadow pass collapsed stereo viewports");
        light.enabled=false;
        glBindFramebuffer(GL_FRAMEBUFFER,0);DestroyRenderTarget(target);
    }
    parent.GetComponent<Transform>().position.x=.7f;
    Check(verify()!=initial,"Parent movement retained stale pixels");
    parent.GetComponent<Transform>().scale=Vector3(-.7f,1.2f,1);verify();
    parent.GetComponent<Transform>().rotation=glm::angleAxis(.3f,Vector3(0,0,1));verify();
    t.SetLocalMatrixPrefix(glm::translate(Matrix4(1),Vector3(.2f,.3f,0)));verify();
    t.ClearLocalMatrixPrefix();t.SetParent(&other);verify();
    t.active=false;auto hidden=verify();t.active=true;
    Check(verify()!=hidden,"Reactivated model missing from prepared candidates");
    cached.OnDestroy();AssetManager::FreeModel(id);
}

static void VertexColorsSurviveInstancing() {
    ModelAsset model;ModelAsset::PrimitiveBuild3D tri;
    tri.vertices={{{-.8f,-.8f,0},{0,0,1},{},Color(1,0,0,1)},{{.8f,-.8f,0},{0,0,1},{},Color(0,1,0,1)},{{0,.8f,0},{0,0,1},{},Color(0,0,1,1)}};
    tri.indices={0,1,2};Check(model.SetRuntimePrimitives({tri}),"Vertex color fixture failed");
    Shader shader;shader.CompileSource("[OPENGL VERSION]\nlayout(location=0)in vec3 p;layout(location=7)in vec4 c;out vec4 color;void main(){gl_Position=vec4(p,1);color=c;}",
        "[OPENGL VERSION]\nin vec4 color;out vec4 result;void main(){result=color;}","vertex colors after shadow instancing");shader.Link();shader.Use();
    glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,320,240);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
    std::vector<unsigned char> before(320*240*4),after(before.size());
    glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);model.Draw(shader,Matrix4(1));
    glReadPixels(0,0,320,240,GL_RGBA,GL_UNSIGNED_BYTE,before.data());
    model.DrawInstanced(shader,{Matrix4(1)});
    glClear(GL_COLOR_BUFFER_BIT);model.Draw(shader,Matrix4(1));
    glReadPixels(0,0,320,240,GL_RGBA,GL_UNSIGNED_BYTE,after.data());
    Check(before==after,"Instanced draw corrupted the baked vertex-color stream");
    model.Free();shader.UnUse();glEnable(GL_DEPTH_TEST);
}

static void PickingIdsIgnoreMaterialTint() {
    Shader shader("assets/shaders/editor_model_pick.vs","assets/shaders/editor_model_pick.fs");shader.Link();shader.Use();
    shader.SetMat4("P",Matrix4(1));shader.SetMat4("V",Matrix4(1));
    GLuint framebuffer,texture;glGenFramebuffers(1,&framebuffer);glGenTextures(1,&texture);
    glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,32,32,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
    Check(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Picking regression framebuffer is incomplete");
    const bool dither=glIsEnabled(GL_DITHER),srgb=glIsEnabled(GL_FRAMEBUFFER_SRGB);
    glViewport(0,0,32,32);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_DITHER);glDisable(GL_FRAMEBUFFER_SRGB);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    ModelAsset model;ModelAsset::PrimitiveBuild3D part;
    part.vertices={{{-1,-1,0},{0,0,1},{}},{{1,-1,0},{0,0,1},{}},{{1,1,0},{0,0,1},{}},{{-1,1,0},{0,0,1},{}}};part.indices={0,1,2,0,2,3};
    for(const Color tint:{Color(.926f,.926f,.926f,1),Color(.3f,.6f,.8f,.18f),Color(0,0,0,0)}) {
        part.baseColor=tint;Check(model.SetRuntimePrimitives({part}),"Picking regression mesh failed");
        for(uint32_t id:{1u,255u,256u,5107u,65537u,0x78563412u}) {
            const std::array<unsigned char,4> expected={static_cast<unsigned char>(id),static_cast<unsigned char>(id>>8),static_cast<unsigned char>(id>>16),static_cast<unsigned char>(id>>24)};
            shader.SetVec4("entityIdColor",Color(expected[0],expected[1],expected[2],expected[3])/255.f);
            glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);model.Draw(shader,Matrix4(1),nullptr,-1,Color(.4f,.7f,.2f,.5f));
            std::array<unsigned char,4> pixel{};glReadPixels(16,16,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data());
            Check(pixel==expected,"Imported material color or alpha corrupted the editor selection ID");
        }
    }
    Check(glGetError()==GL_NO_ERROR,"Picking regression produced an OpenGL error");
    model.Free();shader.UnUse();glBindFramebuffer(GL_FRAMEBUFFER,0);glDeleteFramebuffers(1,&framebuffer);glDeleteTextures(1,&texture);glViewport(0,0,320,240);
    if(dither)glEnable(GL_DITHER);if(srgb)glEnable(GL_FRAMEBUFFER_SRGB);
}

static void BakedVisibilitySafety() {
    BakedVisibility bake;
    bake.bounds.push_back({Vector3(-.2f,-.2f,-3),Vector3(.2f,.2f,-2)});
    bake.minimum=Vector3(-1);bake.cellSize=1;bake.dimensions=glm::ivec3(2);bake.hidden.resize(8);
    VisibilityView view;view.count=1;view.eye[0]=Vector3(-.032f,0,0);view.eye[1]=Vector3(.032f,0,0);
    view.clip[0]=view.clip[1]=glm::perspective(glm::radians(70.f),1.f,.1f,100.f);
    bake.hidden[bake.Cell(view.eye[0])]={0};
    Check(!bake.Visible(0,view,Matrix4(1)),"Baked occlusion ignored hidden primitive");
    view.occlusionEnabled=false;
    Check(bake.Visible(0,view,Matrix4(1)),"Scene occlusion switch did not disable PVS");
    view.occlusionEnabled=true;
    view.count=2;Check(bake.Visible(0,view,Matrix4(1)),"Left-eye bake incorrectly culled right-eye-visible primitive");
    bake.hidden[bake.Cell(view.eye[1])]={0};Check(!bake.Visible(0,view,Matrix4(1)),"Both-eye occlusion not applied");
    view.eye[1]=Vector3(100,0,0);Check(bake.Visible(0,view,Matrix4(1)),"Outside bake volume did not fail open");
    Check(bake.Visible(0,view,glm::scale(Matrix4(1),Vector3(0))),"Singular model matrix did not fail open");
    Matrix4 invalid(1);invalid[0][0]=std::numeric_limits<float>::quiet_NaN();
    Check(bake.Visible(0,view,invalid),"Invalid model matrix did not fail open");
    auto path=std::filesystem::temp_directory_path()/"canis-stale-visibility.json";
    std::ofstream(path)<<R"({"version":1,"crc32":2,"bounds":[],"hidden":[]})";
    Check(!bake.Load(path.string(),3,1) && bake.bounds.empty(),"Stale bake retained rejection data");
    using Canis::Scripting::ManagedJson;
    auto fixture=ManagedJson::parse(R"({"version":1,"crc32":7,"minimum":[-1,-1,-1],"dimensions":[1,1,1],"cellSize":2,
        "bounds":[[[-0.2,-0.2,-3],[0.2,0.2,-2]],[[20,0,-3],[21,1,-2]]],"hidden":[[]],
        "rooms":[{"name":"near","primitives":[0]},{"name":"distant","primitives":[1]}]})");
    auto load=[&]() {std::ofstream(path)<<fixture.dump();return bake.Load(path.string(),7,2);};
    Check(load() && bake.rooms.size()==2,"Room bake failed to load");
    for(int step=-20;step<=20;++step) {
        view.count=2;view.occlusionEnabled=false;
        for(int eye=0;eye<2;++eye)view.clip[eye]=glm::perspective(glm::radians(70.f),1.f,.1f,100.f)*
            glm::translate(Matrix4(1),Vector3(float(step)+(eye?.032f:-.032f),0,0));
        std::vector<unsigned char> grouped,flat;bake.Query(view,Matrix4(1),grouped);
        auto rooms=std::move(bake.rooms);bake.Query(view,Matrix4(1),flat);bake.rooms=std::move(rooms);
        Check(grouped==flat,"Room hierarchy differs from conservative per-primitive stereo culling");
    }
    fixture["rooms"][1]["primitives"]={0};Check(!load() && bake.rooms.empty(),"Duplicate room membership accepted");
    fixture["rooms"].erase(1);Check(!load(),"Missing room membership accepted");
    fixture.erase("rooms");Check(load() && bake.rooms.empty(),"Legacy bake without rooms rejected");
    std::filesystem::remove(path);
}

static void MaterialSlotCulling(Window& window) {
    const auto root=std::filesystem::temp_directory_path()/("canis-slot-culling-"+std::to_string(SDL_GetTicksNS()));
    std::filesystem::create_directories(root);
    for (auto name : {"red","green"})
        std::ofstream(root/(std::string(name)+".material")) << "shader: {path: assets/shaders/model3d}\ncolor: "
            << (std::string(name)=="red" ? "[1,0.1,0.1,1]" : "[0.1,1,0.1,1]") << "\nbackFaceCulling: false\n";
    const int red=AssetManager::LoadMaterial((root/"red.material").string());
    const int green=AssetManager::LoadMaterial((root/"green.material").string());
    int id=AssetManager::CreateModel();
    ModelAsset::PrimitiveBuild3D left,right;
    left.vertices={{{-1,-1,0},{0,0,1},{}},{{0,-1,0},{0,0,1},{}},{{0,1,0},{0,0,1},{}},{{-1,1,0},{0,0,1},{}}};
    left.indices={0,1,2,0,2,3};left.materialSlot=0;
    right=left;right.materialSlot=1;for(auto& v:right.vertices)v.position.x+=1;
    Check(AssetManager::GetModel(id)->SetRuntimePrimitives({left,right},{"left","right"}),"Slot culling geometry failed");
    Scene scene;
    auto make=[&](Vector3 position,bool stationary) {
        auto e=scene.CreateEntity("Slotted mesh");e.AddComponent<Transform>()->position=position;
        auto& m=*e.AddComponent<Model>();m.modelId=id;m.staticModel=stationary;
        e.AddComponent<Material>()->materialIds={red,green};return e;
    };
    auto visible=make({0,0,-3},true);
    std::vector<Entity> outside;
    for(int i=0;i<12;++i)outside.push_back(make({20.f+i*2,0,-3},true));
    auto parent=scene.CreateEntity("Moving door root");parent.AddComponent<Transform>()->position={20,0,0};
    auto moving=make({0,0,-2},false);moving.GetComponent<Transform>().SetParent(&parent);
    moving.GetComponent<Transform>().position={0,0,-2};
    auto& body=*moving.AddComponent<Rigidbody>();body.motionType=RigidbodyMotionType::KINEMATIC;
    MeshRenderer3DSystem renderer;renderer.scene=&scene;renderer.window=&window;renderer.Create();
    auto draw=[&]() {
        scene.SetVRCamera(Matrix4(1),glm::perspective(glm::radians(70.f),320.f/240.f,.1f,100.f),.1f,100.f);
        glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,320,240);glDepthMask(GL_TRUE);
        glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        RenderMetrics::BeginFrame();renderer.Update(scene.GetRegistry(),1.f/60);
        std::vector<unsigned char> pixels(320*240*4);glReadPixels(0,0,320,240,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        RenderMetrics::EndSubmit();glFinish();RenderMetrics::EndPresent();return pixels;
    };
    auto original=draw();
    std::cout << "Slot visibility fixture draws=" << RenderMetrics::Latest().draws
              << " culled=" << RenderMetrics::Latest().culled << '\n';
    Check(RenderMetrics::Latest().draws==2 && RenderMetrics::Latest().culled==13,
          "Safe material-slot meshes bypassed static/dynamic frustum culling");
    for(auto e:outside)e.GetComponent<Transform>().active=false;
    moving.GetComponent<Transform>().active=false;
    Check(original==draw(),"Culling changed the visible slotted mesh pixels");
    moving.GetComponent<Transform>().active=true;parent.GetComponent<Transform>().position={0,0,0};
    draw();Check(RenderMetrics::Latest().draws==4,"Moving a door parent left its slotted child incorrectly culled");
    parent.GetComponent<Transform>().position={20,0,0};
    outside[0].GetComponent<Transform>().active=true;outside[0].GetComponent<Transform>().position={0,0,-4};
    draw();Check(RenderMetrics::Latest().draws==4,"Editing a static mesh failed to update its visibility bounds");
    outside[0].GetComponent<Transform>().position={20,0,-2};
    // Unknown custom shaders can move vertices beyond the mesh's imported bounds.
    std::ofstream(root/"offset.vs") << "[OPENGL VERSION]\nlayout(location=0) in vec3 vertexPosition;uniform mat4 P,V,M;void main(){vec4 p=M*vec4(vertexPosition,1);p.x-=20.;gl_Position=P*V*p;}\n";
    std::ofstream(root/"offset.fs") << "[OPENGL VERSION]\nout vec4 color;void main(){color=vec4(1,0,1,1);}\n";
    std::ofstream(root/"offset.material") << "shader: {path: '"<<(root/"offset").string()<<"'}\nbackFaceCulling: false\n";
    const int offset=AssetManager::LoadMaterial((root/"offset.material").string());
    auto& material=outside[0].GetComponent<Material>();material.materialIds={red,offset};
    draw();Check(RenderMetrics::Latest().draws==4,"Unknown shader in a later material slot was incorrectly culled");
    material.materialIds={red,green};material.materialId=offset;
    auto deformed=draw();
    Check(RenderMetrics::Latest().draws==4 && deformed[(120*320+160)*4]>200 && deformed[(120*320+160)*4+2]>200,
          "Custom vertex displacement was culled using undeformed bounds");
    Check(glGetError()==GL_NO_ERROR,"GL error in material slot visibility test");
    renderer.OnDestroy();AssetManager::FreeModel(id);std::filesystem::remove_all(root);
    std::cout << "Material-slot culling preserves pixels, moving parents, static edits and unsafe shaders\n";
}

static void OverlayDepthIsolation(Window& window) {
    Scene scene;
    scene.Init(nullptr,&window,nullptr);
    auto camera=scene.CreateEntity("Camera");camera.AddComponent<Transform>();
    auto& view=*camera.AddComponent<Camera>();view.primary=true;view.nearClip=.1f;view.farClip=100;
    int id=AssetManager::CreateModel();ModelAsset::PrimitiveBuild3D quad;
    quad.vertices={{{-1,-1,0},{0,0,1},{}},{{1,-1,0},{0,0,1},{}},{{1,1,0},{0,0,1},{}},{{-1,1,0},{0,0,1},{}}};quad.indices={0,1,2,0,2,3};
    Check(AssetManager::GetModel(id)->SetRuntimePrimitives({quad}),"Overlay occluder geometry failed");
    auto occluder=scene.CreateEntity("Near cockpit wall");occluder.AddComponent<Transform>()->position={0,0,-.15f};
    occluder.AddComponent<Model>()->modelId=id;
    GLuint texture=0;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
    const unsigned char white[]={255,255,255,255};glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,white);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    auto canvas=scene.CreateEntity("Overlay");auto& canvasRect=*canvas.AddComponent<RectTransform>();
    canvasRect.anchorMin={0,0};canvasRect.anchorMax={1,1};canvasRect.size={0,0};canvasRect.depth=0;
    auto& settings=*canvas.AddComponent<Canvas>();settings.scaleMode=CanvasScaleMode::CONSTANT_PIXEL_SIZE;settings.screenSize={320,240};
    auto sprite=[&](const char* name,Entity parent,Vector2 size,Vector2 position,float depth,Color color) {
        auto e=scene.CreateEntity(name);auto& rect=*e.AddComponent<RectTransform>();rect.SetParent(&parent);
        rect.position=position;rect.size=size;rect.depth=depth;
        auto& image=*e.AddComponent<Sprite2D>();image.textureHandle.texture.id=texture;image.color=color;return e;
    };
    sprite("Menu background",canvas,{60,60},{-80,0},.1f,{1,0,0,1});
    sprite("Menu foreground",canvas,{20,20},{-80,0},.2f,{0,1,0,1});
    auto world=scene.CreateEntity("World canvas");world.AddComponent<Transform>()->position={.08f,0,-.3f};
    world.AddComponent<RectTransform>()->depth=0;world.AddComponent<Canvas>()->renderMode=CanvasRenderMode::WORLD_SPACE;
    sprite("World label behind wall",world,{.04f,.04f},{0,0},0,{0,0,1,1});
    MeshRenderer3DSystem meshes;meshes.scene=&scene;meshes.window=&window;meshes.Create();
    SpriteRenderer2DSystem ui;ui.scene=&scene;ui.window=&window;ui.Create();
    for(bool editor2D:{false,true}) {
        scene.ClearEditorCameraOverrides();
        if(editor2D)scene.SetEditorCamera2DOverride(glm::ortho(-160.f,160.f,-120.f,120.f,-100.f,100.f),{0,0});
        glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,320,240);glDepthMask(GL_TRUE);glClearDepth(1);
        glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        meshes.Update(scene.GetRegistry(),1.f/60);
        float sceneDepth=0,overlayDepth=0;glReadPixels(80,120,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&sceneDepth);
        ui.Update(scene.GetRegistry(),1.f/60);
        glReadPixels(80,120,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&overlayDepth);
        Check(std::abs(sceneDepth-overlayDepth)<.000001f,"Overlay UI changed scene depth needed by postprocessing");
        std::array<unsigned char,4> pixel{};glReadPixels(80,120,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data());
        Check(pixel[1]>240 && pixel[0]<10 && pixel[2]<10,
              "3D scene depth hid overlay UI or broke its foreground/background ordering");
        if(!editor2D) {
            glReadPixels(206,120,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data());
            Check(!(pixel[2]>240 && pixel[0]<10 && pixel[1]<10),"World-space UI lost occlusion behind scene geometry");
        }
    }
    window.SetRenderSize(320,120);
    scene.SetEditorCamera2DOverride(glm::ortho(-160.f,160.f,-60.f,60.f,-100.f,100.f),{0,0});
    Check(glm::distance(canvasRect.GetResolvedSize(),Vector2(160,120))<.001f,
          "Narrow Scene panel did not fit the full reference canvas");
    glViewport(0,0,320,120);glDepthMask(GL_TRUE);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    meshes.Update(scene.GetRegistry(),1.f/60);ui.Update(scene.GetRegistry(),1.f/60);
    std::array<unsigned char,4> fitted{};glReadPixels(120,60,1,1,GL_RGBA,GL_UNSIGNED_BYTE,fitted.data());
    Check(fitted[1]>240 && fitted[0]<10,"Reference canvas fitting broke overlay positions or layers");
    scene.SetEditorCamera2DOverride(glm::ortho(-160.f,160.f,-60.f,60.f,-100.f,100.f)*
                                    glm::translate(Matrix4(1),Vector3(-20,0,0)),{20,0});
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);meshes.Update(scene.GetRegistry(),1.f/60);ui.Update(scene.GetRegistry(),1.f/60);
    glReadPixels(100,60,1,1,GL_RGBA,GL_UNSIGNED_BYTE,fitted.data());
    Check(fitted[1]>240 && fitted[0]<10,"Scene camera panning did not move the overlay preview");
    window.SetRenderSize(320,240);glViewport(0,0,320,240);
    Check(glGetError()==GL_NO_ERROR,"Overlay depth isolation produced an OpenGL error");
    scene.ClearEditorCameraOverrides();ui.OnDestroy();meshes.OnDestroy();AssetManager::FreeModel(id);glDeleteTextures(1,&texture);
    std::cout << "Overlay UI survives near geometry in Game/2D Scene views and world UI retains occlusion\n";
}

static void ScreenUiAfterPostProcessing(Window& window) {
    auto root=std::filesystem::temp_directory_path()/("canis-post-ui-"+std::to_string(SDL_GetTicksNS()));
    std::filesystem::create_directories(root);
    std::filesystem::copy_file("assets/shaders/postprocess_color_correction.vs",root/"black.vs");
    std::ofstream(root/"black.fs") << "[OPENGL VERSION]\nout vec4 color;void main(){color=vec4(0,0,0,1);}\n";
    std::ofstream(root/"test.postprocess") << "passes:\n- shader: {path: '"<<(root/"black").string()<<"'}\n";
    PostProcessAsset effect;Check(effect.Load((root/"test.postprocess").string()),"Post UI fixture shader failed");
    Scene scene;scene.Init(nullptr,&window,nullptr);window.SetRenderSize(320,240);
    auto projection=glm::perspective(glm::radians(70.f),320.f/240.f,.1f,100.f);
    scene.SetLastRenderCamera(Matrix4(1),projection,{0,0,0},.1f,100.f);
    auto* ui=static_cast<SpriteRenderer2DSystem*>(scene.CreateRenderSystem(new SpriteRenderer2DSystem()));ui->Create();
    GLuint texture=0;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
    const unsigned char white[]={255,255,255,255};glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,white);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    auto canvas=[&](unsigned mode) {
        auto e=scene.CreateEntity("Canvas");auto& rect=*e.AddComponent<RectTransform>();
        rect.anchorMin={0,0};rect.anchorMax={1,1};rect.size={0,0};
        auto& c=*e.AddComponent<Canvas>();c.renderMode=mode;c.scaleMode=CanvasScaleMode::CONSTANT_PIXEL_SIZE;c.screenSize={320,240};return e;
    };
    auto sprite=[&](Entity parent,Vector2 position,Vector2 size,Color color) {
        auto e=scene.CreateEntity("Sprite");auto& rect=*e.AddComponent<RectTransform>();
        if(parent)rect.SetParent(parent.TryGet());rect.position=position;rect.size=size;
        auto& s=*e.AddComponent<Sprite2D>();s.textureHandle.texture.id=texture;s.color=color;return e;
    };
    auto overlay=canvas(CanvasRenderMode::SCREEN_SPACE_OVERLAY);sprite(overlay,{-80,0},{20,20},{0,1,0,1});
    auto cameraUi=canvas(CanvasRenderMode::SCREEN_SPACE_CAMERA);sprite(cameraUi,{-40,0},{20,20},{1,0,1,1});
    auto world=canvas(CanvasRenderMode::WORLD_SPACE);world.AddComponent<Transform>()->position={0,0,-1};
    sprite(world,{0,0},{.2f,.2f},{0,0,1,1});
    sprite({}, {80,0},{20,20},{1,1,0,1}); // Ordinary 2D scene sprite, not a UI canvas.
    RenderTarget raw,processed;EnsureRenderTarget(raw,320,240);
    glBindFramebuffer(GL_FRAMEBUFFER,raw.framebuffer);glViewport(0,0,320,240);glDepthMask(GL_TRUE);
    glClearColor(1,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    auto pixel=[&](int x){std::array<unsigned char,4> value{};glReadPixels(x,120,1,1,GL_RGBA,GL_UNSIGNED_BYTE,value.data());return value;};
    scene.Render(1.f/60,true);
    Check(pixel(80)[0]>240 && pixel(80)[1]<10 && pixel(120)[0]>240 && pixel(120)[2]<10,
          "Screen canvases were already baked into the world render before postprocessing");
    Check(pixel(160)[2]>240 && pixel(240)[0]>240 && pixel(240)[1]>240,
          "World UI or ordinary 2D sprites were incorrectly deferred with the HUD");
    auto output=ApplyPostProcessChain(&effect,raw.framebuffer,raw.colorTexture,raw.depthTexture,320,240,projection,&processed);
    scene.CompositeScreenSpaceUI(output.framebuffer,320,240);
    glBindFramebuffer(GL_FRAMEBUFFER,output.framebuffer);
    Check(pixel(80)[1]>240 && pixel(80)[0]<10 && pixel(120)[0]>240 && pixel(120)[2]>240,
          "Postprocessing modified or hid screen-space overlay/camera UI");
    Check(pixel(160)[0]<10 && pixel(160)[2]<10 && pixel(240)[0]<10 && pixel(240)[1]<10,
          "World-space UI or ordinary sprites incorrectly bypassed postprocessing");
    Check(glGetError()==GL_NO_ERROR,"Post UI composition produced an OpenGL error");
    glBindFramebuffer(GL_FRAMEBUFFER,0);scene.Unload();glDeleteTextures(1,&texture);
    DestroyRenderTarget(raw);DestroyRenderTarget(processed);effect.Free();std::filesystem::remove_all(root);
    std::cout << "Screen canvases bypass postprocessing; world UI and 2D scene sprites retain it\n";
}

static void CombinedColliderAndVehicle(Window& window) {
    Scene scene;scene.Init(nullptr,&window,nullptr);
    ModelAsset::PrimitiveBuild3D plane;
    plane.vertices={{{-15,0,-15},{0,1,0},{}},{{15,0,-15},{0,1,0},{}},{{15,0,15},{0,1,0},{}},{{-15,0,15},{0,1,0},{}}};
    plane.indices={0,2,1,0,3,2};
    int modelId=AssetManager::CreateModel();Check(AssetManager::GetModel(modelId)->SetRuntimePrimitives({plane}),"Combined collider fixture failed");
    auto ground=scene.CreateEntity("Gound");ground.AddComponent<Transform>();auto& collider=*ground.AddComponent<CombineMeshCollider>();
    std::vector<Entity> tiles;
    for(float z:{-15.f,15.f}) {
        auto tile=scene.CreateEntity("Tile");auto& t=*tile.AddComponent<Transform>();t.position={0,0,z};t.SetParent(ground.TryGet());
        tile.AddComponent<Model>()->modelId=modelId;tiles.push_back(tile);
    }
    auto parked=scene.CreateEntity("Edit mode dynamic body");parked.AddComponent<Transform>()->position={0,10,0};
    parked.AddComponent<Rigidbody>()->motionType=RigidbodyMotionType::DYNAMIC;parked.AddComponent<BoxCollider>();
    auto* physics=static_cast<JoltPhysics3DSystem*>(scene.CreateSystem(new JoltPhysics3DSystem()));physics->Create();
    scene.UpdateEditor();
    Check(collider.sourceModels==2&&collider.triangleCount==4&&collider.rebuildCount==1,
        "Edit mode failed to cook combined ground and populate Inspector diagnostics");
    for(int i=0;i<30;i++)scene.UpdateEditor();
    Check(physics->GetBodyCount()==1&&parked.GetComponent<Transform>().position.y==10&&collider.rebuildCount==1,
        "Combined editor preview advanced dynamic physics or rebuilt unchanged geometry");
    ++collider.rebuildVersion;scene.UpdateEditor();
    Check(collider.rebuildCount==2,"Editor rebuild request did not refresh combined collision");
    tiles[0].GetComponent<Transform>().position.y=.5f;scene.UpdateEditor();
    RaycastHit editHit;
    Check(physics->Raycast({0,5,-15},{0,-1,0},editHit,10)&&std::abs(editHit.point.y-.5f)<.001f,
        "Edit mode child transform did not refresh combined collision queries");
    tiles[0].GetComponent<Transform>().position.y=0;scene.UpdateEditor();
    scene.Destroy(parked);physics->Update(scene.GetRegistry(),1.f/60);
    Check(!ground.HasComponent<Rigidbody>()&&physics->GetBodyCount()==1,"Combined collider did not own one static body without per-tile bodies");
    for(float z:{-29.f,-.001f,0.f,.001f,29.f}) {
        RaycastHit hit;Check(physics->Raycast({0,5,z},{0,-1,0},hit,10)&&hit.entity==ground&&std::abs(hit.point.y)<.001f,"Combined ground has a hole or incorrect query owner at a tile seam");
    }
    auto builds=collider.rebuildCount;for(int i=0;i<30;i++)physics->Update(scene.GetRegistry(),1.f/60);
    Check(collider.rebuildCount==builds,"Unchanged combined ground rebuilt every frame");
    tiles[0].SetActive(false);physics->Update(scene.GetRegistry(),1.f/60);
    RaycastHit inactiveHit;Check(!physics->Raycast({0,5,-15},{0,-1,0},inactiveHit,10),"Inactive child remained in combined collision geometry");
    tiles[1].SetActive(false);physics->Update(scene.GetRegistry(),1.f/60);
    Check(physics->GetBodyCount()==0,"Empty combined ground retained a body");
    builds=collider.rebuildCount;
    for(int i=0;i<10;i++)physics->Update(scene.GetRegistry(),1.f/60);
    Check(collider.rebuildCount==builds,"Empty combined ground rebuilt every frame");
    tiles[0].SetActive(true);tiles[1].SetActive(true);physics->Update(scene.GetRegistry(),1.f/60);
    for(auto& vertex:plane.vertices)vertex.position.y=.25f;
    Check(AssetManager::GetModel(modelId)->SetRuntimePrimitives({plane}),"Combined collider geometry replacement failed");
    physics->Update(scene.GetRegistry(),1.f/60);
    Check(physics->Raycast({0,5,15},{0,-1,0},inactiveHit,10)&&std::abs(inactiveHit.point.y-.25f)<.001f,"Geometry revision did not invalidate combined collision shape");
    for(auto& vertex:plane.vertices)vertex.position.y=0;
    AssetManager::GetModel(modelId)->SetRuntimePrimitives({plane});
    tiles[0].GetComponent<Transform>().position.y=1;physics->Update(scene.GetRegistry(),1.f/60);
    RaycastHit hit;Check(physics->Raycast({0,5,-15},{0,-1,0},hit,10)&&std::abs(hit.point.y-1)<.001f,"Child transform edit failed to rebuild combined ground");
    tiles[0].GetComponent<Transform>().position.y=0;
    ground.GetComponent<Transform>().position={3,2,0};ground.GetComponent<Transform>().scale={-1,1,1};
    physics->Update(scene.GetRegistry(),1.f/60);
    Check(physics->Raycast({3,7,15},{0,-1,0},hit,10)&&std::abs(hit.point.y-2)<.001f,"Combined ground failed under translated/mirrored parent");
    ground.GetComponent<Transform>().position={0,0,0};ground.GetComponent<Transform>().scale={1,1,1};
    auto car=scene.CreateEntity("Rover");car.AddComponent<Transform>()->position={0,1.1f,8};
    auto& body=*car.AddComponent<Rigidbody>();body.motionType=RigidbodyMotionType::DYNAMIC;body.mass=1400;body.useGravity=true;
    car.AddComponent<BoxCollider>()->size={2,.5f,3.9f};auto& vehicle=*car.AddComponent<WheeledVehicle>();
    for(int i=0;i<180;i++)physics->Update(scene.GetRegistry(),1.f/60);
    Check(vehicle.groundedWheels==4&&car.GetComponent<Transform>().position.y>.5f,"Jolt vehicle suspension failed to settle on combined ground");
    float start=car.GetComponent<Transform>().position.z;vehicle.throttle=1;
    for(int i=0;i<120;i++)physics->Update(scene.GetRegistry(),1.f/60);
    Check(vehicle.speed>2&&car.GetComponent<Transform>().position.z<start-2,"Native AWD vehicle did not drive across combined ground");
    auto vehicleBuilds=vehicle.rebuildCount;
    vehicle.steeringResponse=1;vehicle.steering=1;
    vehicle.engineMaxRPM=7000;vehicle.brakeTorque=3000;vehicle.handbrakeTorque=6000;
    vehicle.longitudinalGrip=.8f;vehicle.lateralGrip=.9f;vehicle.antiRollStiffness=1000;
    physics->Update(scene.GetRegistry(),1.f/60);
    Check(vehicle.appliedSteering>0&&vehicle.appliedSteering<.05f,"Vehicle steering response failed to smooth a sudden input");
    Check(vehicle.rebuildCount==vehicleBuilds,"Live vehicle handling edits rebuilt the constraint");
    vehicle.steeringResponse=0;vehicle.highSpeedSteeringRatio=0;vehicle.steeringFadeSpeedKph=1;
    physics->Update(scene.GetRegistry(),1.f/60);
    Check(std::abs(vehicle.appliedSteering)<.001f,"Vehicle high-speed steering reduction did not apply");
    vehicle.throttle=0;vehicle.throttleResponse=1;
    physics->Update(scene.GetRegistry(),1.f/60);
    Check(vehicle.appliedThrottle>0&&vehicle.appliedThrottle<1,"Vehicle throttle response did not ease pedal release");
    vehicle.brake=1;physics->Update(scene.GetRegistry(),1.f/60);
    Check(vehicle.appliedThrottle==0,"Vehicle continued smoothed acceleration while braking");
    vehicle.brake=0;vehicle.throttle=1;vehicle.throttleResponse=0;
    vehicle.highSpeedSteeringRatio=1;vehicle.longitudinalGrip=vehicle.lateralGrip=1;vehicle.antiRollStiffness=0;
    vehicle.steering=1;float startX=car.GetComponent<Transform>().position.x;
    for(int i=0;i<45;i++)physics->Update(scene.GetRegistry(),1.f/60);
    Check(car.GetComponent<Transform>().position.x>startX+.2f,"Positive vehicle steering did not turn right with native -Z forward");
    vehicle.wheelWidth+=.02f;physics->Update(scene.GetRegistry(),1.f/60);
    Check(vehicle.rebuildCount==vehicleBuilds+1,"Wheel geometry edit did not rebuild native wheel collision");
    vehicle.steering=0;
    vehicle.throttle=0;vehicle.brake=1;for(int i=0;i<60;i++)physics->Update(scene.GetRegistry(),1.f/60);
    {
        App app;RegisterWheeledVehicle(app);
        auto* codec=app.GetComponentConf(WheeledVehicle::ScriptName);
        YAML::Node saved;codec->Encode(saved,car);
        auto copy=scene.CreateEntity("Reloaded vehicle settings");codec->Decode(saved,copy,false);
        const auto& restored=copy.GetComponent<WheeledVehicle>();
        Check(restored.engineMaxRPM==7000&&restored.brakeTorque==3000&&restored.handbrakeTorque==6000&&
            restored.steeringFadeSpeedKph==1&&restored.wheelWidth==vehicle.wheelWidth,
            "Vehicle tuning did not survive the native scene/prefab codec round trip");
        scene.Destroy(copy);
    }
    car.RemoveComponent<WheeledVehicle>();physics->Update(scene.GetRegistry(),1.f/60);
    collider.active=false;physics->Update(scene.GetRegistry(),1.f/60);Check(physics->GetBodyCount()==1,"Disabling combined collider left its body active");
    collider.active=true;scene.UpdateEditor();
    Check(physics->GetBodyCount()==2,"Editor preview did not restore re-enabled combined ground");
    ground.RemoveComponent<CombineMeshCollider>();scene.UpdateEditor();
    Check(physics->GetBodyCount()==1&&!physics->Raycast({0,5,15},{0,-1,0},hit,10),
        "Removing the combined component in edit mode retained stale collision geometry");
    scene.Unload();AssetManager::FreeModel(modelId);
    std::cout<<"Combined ground caches edits; native AWD, live handling, input response, steering and tuning persistence passed\n";
}

static void ParticleSpawnShapes(Window& window) {
    Scene scene;scene.Init(nullptr,&window,nullptr);
    auto parent=scene.CreateEntity("Emitter parent");auto& parentTransform=*parent.AddComponent<Transform>();
    parentTransform.position={3,4,-2};parentTransform.rotation=glm::angleAxis(.7f,Vector3(1,0,0));
    parentTransform.scale={2,3,4};
    auto source=scene.CreateEntity("Emitter");auto& transform=*source.AddComponent<Transform>();
    transform.position={1,2,3};transform.SetParent(parent.TryGet());
    auto& emitter=*source.AddComponent<ParticleEmitter>();emitter.entity=source.TryGet();
    emitter.modelPath=emitter.materialPath="";emitter.spawnExtents={-4,-3,-6};emitter.burstCount=256;
    emitter.destroyEntityWhenDone=false;emitter.speedMin=emitter.speedMax=0;emitter.gravity=emitter.drag=0;
    emitter.particleLifetime=2;emitter.useAgeUniforms=false;
    ParticleEmitterSystem system;system.scene=&scene;system.window=&window;system.Create();
    for(int shape:{0,1,2}) for(bool local:{false,true}) {
        emitter.spawnShape=shape;emitter.localSpace=local;emitter.Play();system.Update(scene.GetRegistry(),0);
        Check(emitter.particles.size()==256,"Particle shape did not emit the authored burst count");
        Vector3 range(0);auto inverse=glm::inverse(transform.GetModelMatrix());
        for(auto handle:emitter.particles) {
            auto offset=Vector3(inverse*Vector4(scene.GetRegistry().get<Transform>(handle).position,1));
            range=glm::max(range,glm::abs(offset));
            Check(std::abs(offset.x)<=4.001f&&std::abs(offset.y)<=3.001f&&std::abs(offset.z)<=6.001f,
                  "Spawn shape escaped its half extents under a rotated/scaled parent");
            if(shape==1)Check(glm::length(offset)<.001f,"Point emitter spread away from its origin");
            if(shape==2)Check(std::abs(offset.y)<.001f,"Plane emitter spawned outside local XZ plane");
        }
        if(shape!=1)Check(range.x>3.2f&&range.z>4.8f,"Plane/box emitter did not cover its width and depth");
        if(shape==0)Check(range.y>2.4f,"Legacy box emitter lost its height volume");
        system.Update(scene.GetRegistry(),3); // Expiry must clean registry handles in every mode.
        Check(emitter.particles.empty(),"Shape emitter retained expired particles");
    }
    std::cout << "Point, plane and legacy box particle shapes respect transformed local bounds and expiry\n";
}

static void ParticleBudgetsTimingAndCollisions(Window& window) {
    auto configure=[](Entity& e)->ParticleEmitter& {
        auto& emitter=*e.AddComponent<ParticleEmitter>();emitter.entity=e.TryGet();
        emitter.modelPath=emitter.materialPath="";emitter.destroyEntityWhenDone=false;
        emitter.randomSeed=123;emitter.useAgeUniforms=false;return emitter;
    };
    {
        Scene scene;scene.Init(nullptr,&window,nullptr);auto e=scene.CreateEntity("Budgeted emitter");e.AddComponent<Transform>();
        auto& emitter=configure(e);emitter.burstCount=100000;emitter.maxParticles=17;emitter.maxSpawnPerFrame=5;
        emitter.looping=true;emitter.particleLifetime=5;emitter.Play();
        ParticleEmitterSystem system;system.scene=&scene;system.Update(scene.GetRegistry(),0);
        Check(emitter.particles.size()==5&&emitter.totalDropped==99995,"Burst escaped per-frame spawn limit");
        for(int i=0;i<20;i++){system.Update(scene.GetRegistry(),.02f);Check(emitter.particles.size()<=17&&emitter.spawnedThisFrame<=5,"Loop escaped live/per-frame limits");}
        emitter.maxParticles=1;system.Update(scene.GetRegistry(),0);
        Check(emitter.particles.size()==1,"Reducing the live cap did not trim existing particles");emitter.maxParticles=17;
        emitter.burstMode=false;emitter.spawnCount=100000;emitter.spawnInterval=.001f;emitter.Play();
        system.Update(scene.GetRegistry(),30);Check(emitter.particles.size()<=17&&emitter.spawnedThisFrame<=5,"Long-frame catch-up escaped particle limits");
        system.Update(scene.GetRegistry(),std::numeric_limits<float>::quiet_NaN());system.Update(scene.GetRegistry(),-1);
        for(auto h:emitter.particles)Check(std::isfinite(scene.GetRegistry().get<Transform>(h).position.x),"Invalid delta poisoned particle simulation");
        e.RemoveComponent<ParticleEmitter>();system.Update(scene.GetRegistry(),0);
        auto remaining=scene.GetRegistry().view<Model,Material>();
        Check(remaining.begin()==remaining.end(),"Removing an emitter left orphaned particles");
    }
    auto simulate=[&](int steps) {
        Scene scene;scene.Init(nullptr,&window,nullptr);auto e=scene.CreateEntity("Seeded emitter");e.AddComponent<Transform>();
        auto& emitter=configure(e);emitter.burstMode=false;emitter.looping=true;emitter.spawnInterval=.01f;emitter.spawnCount=1;
        emitter.particleLifetime=3;emitter.gravity=-3;emitter.drag=.7f;emitter.Play();
        ParticleEmitterSystem system;system.scene=&scene;for(int i=0;i<steps;i++)system.Update(scene.GetRegistry(),1.f/steps);
        std::vector<Vector3> positions;for(auto h:emitter.particles)positions.push_back(scene.GetRegistry().get<Transform>(h).position);
        return positions;
    };
    auto slow=simulate(30),fast=simulate(120),repeat=simulate(30);
    Check(slow.size()==100&&fast.size()==slow.size()&&slow==repeat,"Seeded emission count or repeatability differs");
    for(size_t i=0;i<slow.size();i++)Check(glm::distance(slow[i],fast[i])<.002f,"Particle motion/emission depends on frame rate");
    {
        Scene scene;scene.Init(nullptr,&window,nullptr);auto parent=scene.CreateEntity("Parent");parent.AddComponent<Transform>();
        auto e=scene.CreateEntity("Finite emitter");e.AddComponent<Transform>()->SetParent(parent.TryGet());auto& emitter=configure(e);
        emitter.burstMode=false;emitter.spawnCount=3;emitter.spawnInterval=.02f;emitter.emissionDuration=.08f;emitter.particleLifetime=3;
        emitter.spawnShape=1;emitter.minVelocity=emitter.maxVelocity={1,0,0};emitter.speedMin=emitter.speedMax=2;emitter.drag=emitter.gravity=0;
        emitter.Play();ParticleEmitterSystem system;system.scene=&scene;system.Update(scene.GetRegistry(),1);
        Check(emitter.particles.size()==12&&!emitter.playing,"Finite emitter emitted beyond its duration during a long frame");
        for(auto h:emitter.particles)Check(scene.GetRegistry().get<Transform>(h).position.x>1.83f,"Catch-up particles ignored their actual birth times");
        emitter.looping=true;emitter.Play();parent.GetComponent<Transform>().active=false;system.Update(scene.GetRegistry(),.1f);
        Check(emitter.totalSpawned==0,"Inactive parent still emitted particles");
    }
    for(int collisionMode:{1,2}) {
        Scene scene;scene.Init(nullptr,&window,nullptr);
        if(collisionMode==2) {
            auto floor=scene.CreateEntity("Physics wall");floor.AddComponent<Transform>()->position={-.5f,0,0};
            floor.AddComponent<Rigidbody>()->motionType=RigidbodyMotionType::STATIC;floor.AddComponent<BoxCollider>()->size={1,10,10};
            auto* physics=static_cast<JoltPhysics3DSystem*>(scene.CreateSystem(new JoltPhysics3DSystem()));physics->Create();physics->Update(scene.GetRegistry(),1.f/60);
        }
        auto source=scene.CreateEntity("Rain");source.AddComponent<Transform>()->position={0,1,0};auto& rain=configure(source);
        auto target=scene.CreateEntity("Impact emitter");target.AddComponent<Transform>()->position={100,100,100};auto& splash=configure(target);
        splash.playOnStart=false;splash.burstCount=3;splash.maxParticles=5;splash.maxSpawnPerFrame=4;
        splash.spawnShape=1;splash.localSpace=true;splash.speedMin=splash.speedMax=2;splash.drag=splash.gravity=0;
        splash.minVelocity=splash.maxVelocity={0,1,0};
        rain.spawnShape=1;rain.burstCount=8;rain.particleLifetime=2;rain.minVelocity=rain.maxVelocity={0,-1,0};
        if(collisionMode==2){source.GetComponent<Transform>().position={1,0,0};rain.minVelocity=rain.maxVelocity={-1,0,0};}
        rain.speedMin=rain.speedMax=40;rain.gravity=rain.drag=0;rain.collisionMode=collisionMode;rain.impactEmitter=target;
        rain.Play();ParticleEmitterSystem system;system.scene=&scene;system.Update(scene.GetRegistry(),0);system.Update(scene.GetRegistry(),.1f);
        Check(rain.particles.empty()&&rain.totalImpacts==8,"Swept particles tunneled through their collision surface");
        Check(splash.particles.size()==4&&splash.totalDropped>0,"Impact bursts escaped the shared frame budget");
        Vector3 impactOrigin=collisionMode==1?Vector3(0,.015f,0):Vector3(.015f,0,0);
        for(auto h:splash.particles)Check(glm::distance(scene.GetRegistry().get<Transform>(h).position,impactOrigin)<.001f,"Impact emitter spawned at its authored origin instead of the hit");
        system.Update(scene.GetRegistry(),.1f);
        auto moved=impactOrigin+(collisionMode==1?Vector3(0,.2f,0):Vector3(.2f,0,0));
        for(auto h:splash.particles)Check(glm::distance(scene.GetRegistry().get<Transform>(h).position,moved)<.001f,"Impact particles followed the template transform or ignored the surface normal");
        scene.Unload();
    }
    std::cout<<"Particle budgets, seeds, frame-independent motion, finite duration, orphan cleanup and swept impact emitters passed\n";
}

static void ShadowTextureIsolation(Window& window) {
    auto root=std::filesystem::temp_directory_path()/("canis-shadow-samplers-"+std::to_string(SDL_GetTicksNS()));
    std::filesystem::create_directories(root);
    const float vertices[]={-1,-1,0,0,0,1,0,0, 1,-1,0,0,0,1,1,0, 1,1,0,0,0,1,1,1,
                            -1,-1,0,0,0,1,0,0, 1,1,0,0,0,1,1,1, -1,1,0,0,0,1,0,1};
    std::ofstream binary(root/"quad.bin",std::ios::binary);binary.write(reinterpret_cast<const char*>(vertices),sizeof(vertices));binary.close();
    std::filesystem::copy_file("assets/defaults/textures/square.png",root/"normal.png");
    std::ofstream(root/"quad.gltf") << R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],
      "nodes":[{"mesh":0}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"material":0}]}],
      "buffers":[{"uri":"quad.bin","byteLength":192}],"bufferViews":[{"buffer":0,"byteLength":192,"byteStride":32}],
      "accessors":[{"bufferView":0,"componentType":5126,"count":6,"type":"VEC3","min":[-1,-1,0],"max":[1,1,0]},
                   {"bufferView":0,"byteOffset":12,"componentType":5126,"count":6,"type":"VEC3"},
                   {"bufferView":0,"byteOffset":24,"componentType":5126,"count":6,"type":"VEC2"}],
      "materials":[{"normalTexture":{"index":0}}],"textures":[{"source":0}],"images":[{"uri":"normal.png"}]})";
    auto read=[](const char* path){std::ifstream f(path);return std::string(std::istreambuf_iterator<char>(f),{});};
    auto fragment=read("assets/shaders/model3d.fs");
    auto replace=[&](const std::string& before,const std::string& after){auto at=fragment.find(before);Check(at!=std::string::npos,"Shadow comparison fixture changed");fragment.replace(at,before.size(),after);};
    replace("uniform sampler2D directionalShadowMap;","uniform sampler2DShadow directionalShadowMap;");
    replace("float sampleDepth = texture(directionalShadowMap, projCoords.xy + offset).r;",
            "float visibility = texture(directionalShadowMap, vec3(projCoords.xy + offset, currentDepth - bias));");
    replace("shadow += (currentDepth - bias > sampleDepth) ? 1.0 : 0.0;","shadow += 1.0 - visibility;");
    std::ofstream(root/"comparison.vs") << read("assets/shaders/model3d.vs");
    std::ofstream(root/"comparison.fs") << fragment;
    std::ofstream(root/"comparison.material") << "shader: {path: '"<<(root/"comparison").string()<<"'}\ncolor: [1,1,1,1]\n";
    int id=AssetManager::LoadModel((root/"quad.gltf").string());
    int materialId=AssetManager::LoadMaterial((root/"comparison.material").string());
    Scene scene;auto sun=scene.CreateEntity("Sun");sun.AddComponent<Transform>();
    sun.AddComponent<DirectionalLight>()->direction=glm::normalize(Vector3(.7f,-.3f,-1));
    auto receiver=scene.CreateEntity("Normal-mapped receiver");receiver.AddComponent<Transform>()->position={0,0,-5};
    auto& receiverModel=*receiver.AddComponent<Model>();receiverModel.modelId=id;receiverModel.castShadow=false;
    auto caster=scene.CreateEntity("Normal-mapped caster");caster.AddComponent<Transform>()->position={-.5f,0,-3};
    caster.GetComponent<Transform>().scale=Vector3(.25f);caster.AddComponent<Model>()->modelId=id;
    MeshRenderer3DSystem renderer;renderer.scene=&scene;renderer.window=&window;renderer.Create();
    int rawId=AssetManager::LoadShader("assets/shaders/model3d");
    for(bool comparison:{false,true,false,true}) {
        if(comparison) {
            receiver.AddComponent<Material>()->materialId=materialId;
            caster.AddComponent<Material>()->materialId=materialId;
        } else {receiver.RemoveComponent<Material>();caster.RemoveComponent<Material>();}
        scene.SetVRCamera(Matrix4(1),glm::perspective(glm::radians(70.f),320.f/240.f,.1f,100.f),.1f,100.f);
        glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,320,240);glDepthMask(GL_TRUE);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        renderer.Update(scene.GetRegistry(),1.f/60);
        auto* shader=AssetManager::Get<ShaderAsset>(comparison?AssetManager::GetMaterial(materialId)->shaderId:rawId)->GetShader();
        Check(shader->GetUniformType("directionalShadowMap")==unsigned(comparison?GL_SAMPLER_2D_SHADOW:GL_SAMPLER_2D),"Incorrect cached shadow uniform type");
        GLint shadowUnit=-1,normalUnit=-1;
        glGetUniformiv(shader->GetProgramID(),shader->GetUniformLocation("directionalShadowMap",0),&shadowUnit);
        glGetUniformiv(shader->GetProgramID(),shader->GetUniformLocation("normalMap",0),&normalUnit);
        Check(shadowUnit!=normalUnit,"Normal map aliases directional shadow texture unit");
        glActiveTexture(GL_TEXTURE0+shadowUnit);GLint format=0,mode=0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_INTERNAL_FORMAT,&format);
        glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_MODE,&mode);
        Check(format==GL_DEPTH_COMPONENT24,"Imported normal map replaced the directional shadow depth texture");
        Check(mode==(comparison?GL_COMPARE_REF_TO_TEXTURE:GL_NONE),"Shadow sampler type and depth comparison state disagree");
        Check(glGetError()==GL_NO_ERROR,"Shadow/normal-map binding produced an OpenGL error");
    }
    // A magnified normal map must interpolate rather than produce square
    // reflection patches. Shared texture defaults remain suitable for pixel UI.
    auto normal=AssetManager::GetTexture(AssetManager::LoadTexture((root/"normal.png").string()))->GetGLTexture();
    const unsigned char colors[]={255,0,0,255,0,255,0,255,0,0,255,255,255,255,255,255};
    glBindTexture(GL_TEXTURE_2D,normal.id);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,colors);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glGenerateMipmap(GL_TEXTURE_2D);
    std::ofstream(root/"filter.vs") << read("assets/shaders/model3d.vs");
    std::ofstream(root/"filter.fs") << "[OPENGL VERSION]\nuniform sampler2D normalMap;uniform bool useNormalMap;out vec4 color;void main(){color=useNormalMap?texture(normalMap,vec2(.5,.5)):vec4(1,0,0,1);}\n";
    std::ofstream(root/"filter.material") << "shader: {path: '"<<(root/"filter").string()<<"'}\ncolor: [1,1,1,1]\n";
    int filtered=AssetManager::LoadMaterial((root/"filter.material").string());
    receiver.GetComponent<Material>().materialId=filtered;caster.GetComponent<Material>().materialId=filtered;
    glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,320,240);glDepthMask(GL_TRUE);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    renderer.Update(scene.GetRegistry(),1.f/60);
    std::array<unsigned char,4> pixel{};glReadPixels(160,120,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data());
    for(int c=0;c<3;c++)Check(pixel[c]>=125&&pixel[c]<=130,"Imported normal map magnification used nearest sampling");
    glActiveTexture(GL_TEXTURE5);glBindTexture(GL_TEXTURE_2D,normal.id);GLint filter=0;
    glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,&filter);
    Check(filter==GL_NEAREST,"3D normal filtering changed shared texture defaults used by UI");
    Check(glGetError()==GL_NO_ERROR,"Normal-map filtering produced an OpenGL error");
    renderer.OnDestroy();AssetManager::FreeModel(id);glActiveTexture(GL_TEXTURE0);std::filesystem::remove_all(root);
    std::cout << "Imported normal maps preserve shadows; raw and comparison shaders use matching depth sampling\n";
}

static void StaticShadowBaking(Window& window) {
    Scene scene;
    auto sun=scene.CreateEntity("Sun");sun.AddComponent<Transform>();auto& light=*sun.AddComponent<DirectionalLight>();
    light.direction=glm::normalize(Vector3(.7f,-.3f,-1));light.intensity=2;
    ModelAsset::PrimitiveBuild3D quad;
    quad.vertices={{{-1,-1,0},{0,0,1},{}},{{1,-1,0},{0,0,1},{}},{{1,1,0},{0,0,1},{}},{{-1,1,0},{0,0,1},{}}};quad.indices={0,1,2,0,2,3};
    int id=AssetManager::CreateModel();Check(AssetManager::GetModel(id)->SetRuntimePrimitives({quad}),"Shadow fixture geometry failed");
    auto make=[&](const char* name,Vector3 position,float size,bool stationary) {
        auto e=scene.CreateEntity(name);auto& t=*e.AddComponent<Transform>();t.position=position;t.scale=Vector3(size);
        auto& m=*e.AddComponent<Model>();m.modelId=id;m.staticModel=stationary;return e;
    };
    auto receiver=make("Receiver",Vector3(0,0,-5),3,true);
    auto wall=make("Static caster",Vector3(-.8f,0,-3),.35f,true);
    auto moving=make("Dynamic caster",Vector3(.4f,.4f,-4),.25f,false);
    MeshRenderer3DSystem renderer;renderer.scene=&scene;renderer.window=&window;renderer.Create();
    auto draw=[&](bool cached) {
        SDL_setenv_unsafe("CANIS_DISABLE_STATIC_SHADOW_CACHE",cached?"0":"1",1);
        scene.SetVRCamera(Matrix4(1),glm::perspective(glm::radians(70.f),320.f/240.f,.1f,100.f),.1f,100.f);
        glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,320,240);glDepthMask(GL_TRUE);
        glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        RenderMetrics::BeginFrame();renderer.Update(scene.GetRegistry(),1.f/60);
        std::vector<unsigned char> pixels(320*240*4);glReadPixels(0,0,320,240,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        RenderMetrics::EndSubmit();glFinish();RenderMetrics::EndPresent();return pixels;
    };
    auto original=draw(true);draw(true);const auto reused=RenderMetrics::Latest().shadowDraws;
    auto fresh=draw(false);const auto full=RenderMetrics::Latest().shadowDraws;
    Check(original==fresh && reused<full,"Static shadow reuse changed pixels or failed to reduce shadow draws");
    scene.GetBakingSettings().staticShadows=false;draw(true);
    Check(RenderMetrics::Latest().shadowDraws==full,"Scene shadow-cache switch did not force full shadow rendering");
    scene.GetBakingSettings().staticShadows=true;draw(true);renderer.InvalidateStaticShadowCache();
    Check(!renderer.HasStaticShadowCache(),"Explicit shadow rebuild did not invalidate cache");
    draw(true);Check(renderer.HasStaticShadowCache(),"Explicit shadow rebuild did not refill cache");
    auto verify=[&]() {auto a=draw(true);auto b=draw(false);Check(a==b,"Static shadow invalidation/composition differs from uncached rendering");};
    draw(true);moving.GetComponent<Transform>().position.x=-.2f;verify();
    draw(true);wall.GetComponent<Transform>().position.y=.6f;verify();
    draw(true);wall.GetComponent<Model>().castShadow=false;verify();
    draw(true);wall.GetComponent<Model>().castShadow=true;wall.GetComponent<Transform>().active=false;verify();
    wall.GetComponent<Transform>().active=true;draw(true);light.direction=glm::normalize(Vector3(-.6f,-.2f,-1));verify();
    draw(true);quad.vertices[0].position.y=-.7f;Check(AssetManager::GetModel(id)->SetRuntimePrimitives({quad}),"Shadow geometry edit failed");verify();
    Check(glGetError()==GL_NO_ERROR,"GL error in static shadow bake");
    SDL_unsetenv_unsafe("CANIS_DISABLE_STATIC_SHADOW_CACHE");renderer.OnDestroy();AssetManager::FreeModel(id);
}

// Dynamic rigid instances must keep the static cache warm, track current transforms,
// and match the conservative individual-draw fallback at the pixel level.
static void DynamicRigidBatches(Window& window)
{
    Scene scene;
    ModelAsset::PrimitiveBuild3D quad;
    quad.vertices={{{-.25f,-.25f,0},{0,0,1},{}},{{.25f,-.25f,0},{0,0,1},{}},
                   {{.25f,.25f,0},{0,0,1},{}},{{-.25f,.25f,0},{0,0,1},{}}};
    quad.indices={0,1,2,0,2,3};
    int id=AssetManager::CreateModel();
    Check(AssetManager::GetModel(id)->SetRuntimePrimitives({quad}),"Dynamic rigid fixture failed");
    std::vector<Entity> moving;
    for(int i=0;i<6;++i) {
        auto e=scene.CreateEntity("Rigid actor");
        auto& t=*e.AddComponent<Transform>();t.position=Vector3((i-2.5f)*.55f,0,-3);
        auto& m=*e.AddComponent<Model>();m.modelId=id;m.staticModel=false;m.castShadow=false;
        moving.push_back(e);
    }
    for(int i=0;i<2;++i) {
        auto e=scene.CreateEntity("Static neighbor");
        e.AddComponent<Transform>()->position=Vector3((i-.5f)*.6f,.8f,-4);
        auto& m=*e.AddComponent<Model>();m.modelId=id;m.staticModel=true;m.castShadow=false;
    }
    MeshRenderer3DSystem renderer;renderer.scene=&scene;renderer.window=&window;renderer.Create();
    auto draw=[&]() {
        scene.SetVRCamera(Matrix4(1),glm::perspective(glm::radians(70.f),320.f/240.f,.1f,100.f),.1f,100.f);
        glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,320,240);glDepthMask(GL_TRUE);
        glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        RenderMetrics::BeginFrame();renderer.Update(scene.GetRegistry(),1.f/60);
        std::vector<unsigned char> pixels(320*240*4);glReadPixels(0,0,320,240,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        RenderMetrics::EndSubmit();glFinish();RenderMetrics::EndPresent();return pixels;
    };
    auto baseline=draw();draw();
    Check(RenderMetrics::Latest().draws==2,"Rigid actors were not batched separately from static neighbors");
    moving[0].GetComponent<Transform>().position.y=.5f;
    moving[1].GetComponent<Transform>().scale=Vector3(-.8f,1.2f,1);
    // Legacy assets sometimes claim to be static while their body is kinematic.
    // Such actors still belong in transient batches and must not invalidate a bake.
    moving[2].GetComponent<Model>().staticModel=true;
    moving[2].AddComponent<Rigidbody>()->motionType=RigidbodyMotionType::KINEMATIC;
    auto updated=draw();
    Check(updated!=baseline && RenderMetrics::Latest().batchBuilds==0,"Moving actors invalidated static batches or retained stale matrices");
    // A per-entity field override forces the existing individual-draw path.
    for(auto e:moving)e.AddComponent<Material>()->materialFields.SetFloat("rigidParitySentinel",0);
    auto individual=draw();
    Check(individual==updated,"Dynamic batching changed pixels/normal transforms");
    for(auto e:moving)e.RemoveComponent<Material>();
    auto hidden=moving.back();hidden.GetComponent<Transform>().position=Vector3(100,0,-3);
    auto culled=draw();
    Check(RenderMetrics::Latest().culled>0 && RenderMetrics::Latest().instances==7,"Moving offscreen actor was not culled");
    hidden.Active()=false;Check(draw()==culled,"Dynamic culling removed visible geometry");
    hidden.Active()=true;hidden.GetComponent<Transform>().position=Vector3(1.2f,0,-3);
    Check(draw()!=culled,"Re-entering dynamic actor stayed culled");
    Check(glGetError()==GL_NO_ERROR,"GL error in dynamic rigid batching");
    renderer.OnDestroy();AssetManager::FreeModel(id);
}

static void MirroredNormalMapping() {
    auto read=[](const char* path){std::ifstream file(path);return std::string(std::istreambuf_iterator<char>(file),{});};
    Shader shader;shader.CompileSource(read("assets/shaders/model3d.vs"),read("assets/shaders/model3d.fs"),"mirrored normal mapping");shader.Link();shader.Use();
    const float vertices[]={-1,-1,0, 0,0,1, 0,0,  1,-1,0, 0,0,1, 1,0,  1,1,0, 0,0,1, 1,1,
                            -1,-1,0, 0,0,1, 0,0,  1,1,0, 0,0,1, 1,1, -1,1,0, 0,0,1, 0,1};
    GLuint vao,vbo,texture;glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);glGenTextures(1,&texture);
    glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_DYNAMIC_DRAW);
    for(int attribute=0;attribute<3;attribute++){glEnableVertexAttribArray(attribute);glVertexAttribPointer(attribute,attribute==2?2:3,GL_FLOAT,GL_FALSE,8*sizeof(float),reinterpret_cast<void*>(size_t(attribute==0?0:attribute==1?3:6)*sizeof(float)));}
    shader.SetMat4("P",Matrix4(1));shader.SetMat4("V",Matrix4(1));shader.SetMat4("M",Matrix4(1));shader.SetBool("useInstanceMatrix",false);
    shader.SetBool("useNormalMap",true);shader.SetInt("normalMap",5);shader.SetBool("useAlbedoMap",false);shader.SetVec4("albedoValue",Color(.6f,.6f,.6f,1));
    shader.SetVec3("cameraPosition",{0,0,3});shader.SetVec3("ambientLightColor",Vector3(0));shader.SetFloat("ambientLightIntensity",0);
    shader.SetBool("useDirectionalLight",true);shader.SetBool("useDirectionalShadow",false);shader.SetVec3("directionalLightDirection",{-1,0,-1});shader.SetVec3("directionalLightColor",Vector3(1));shader.SetFloat("directionalLightIntensity",1);
    shader.SetInt("pointLightCount",0);shader.SetFloat("specularValue",0);shader.SetFloat("roughnessValue",.5f);shader.SetFloat("metallicValue",0);
    shader.SetFloat("alphaCutoff",.5f);shader.SetBool("useBakedVertexLighting",false);shader.SetBool("useEmbeddedEmissionMap",false);shader.SetBool("canisProbeCapture",false);shader.SetInt("canisProbeCount",0);
    for(int i=0;i<MaxLightingProbes;i++)shader.SetInt("canisProbeMap"+std::to_string(i),8+i);
    glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,320,240);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);
    auto draw=[&](unsigned char red){
        const unsigned char pixel[]={red,128,230,255};glActiveTexture(GL_TEXTURE5);glBindSampler(5,0);glBindTexture(GL_TEXTURE_2D,texture);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,pixel);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glClear(GL_COLOR_BUFFER_BIT);glDrawArrays(GL_TRIANGLES,0,6);std::array<unsigned char,4> color;glReadPixels(160,120,1,1,GL_RGBA,GL_UNSIGNED_BYTE,color.data());return color;
    };
    auto ordinary=draw(204);std::vector<float> mirrored(std::begin(vertices),std::end(vertices));for(int i=0;i<6;i++)mirrored[i*8+6]=1-mirrored[i*8+6];glBufferSubData(GL_ARRAY_BUFFER,0,sizeof(vertices),mirrored.data());auto reversed=draw(51);
    Check(ordinary[0]>50,"Normal mapping fixture rendered blank");for(int c=0;c<3;c++)Check(std::abs(int(ordinary[c])-int(reversed[c]))<=1,"Mirrored UVs reversed the physical normal-map tangent basis");
    Check(glGetError()==GL_NO_ERROR,"Normal map regression produced an OpenGL error");glDeleteTextures(1,&texture);glDeleteBuffers(1,&vbo);glDeleteVertexArrays(1,&vao);shader.UnUse();glActiveTexture(GL_TEXTURE0);
}

static void LightingProbeRendering(Window& window) {
    // A red wall behind the camera must illuminate/reflect in a dynamic white receiver.
    const auto root=std::filesystem::temp_directory_path()/("canis-probes-"+std::to_string(SDL_GetTicksNS()));
    std::filesystem::create_directories(root);
    std::ofstream(root/"red.material")<<"color: [1,0,0,1]\nbackFaceCulling: false\n";
    Scene scene;scene.SetEnvironmentAmbientLight(Color(1,1,1,1));scene.SetEnvironmentAmbientLightIntensity(.3f);
    const int id=AssetManager::CreateModel();ModelAsset::PrimitiveBuild3D quad;
    quad.vertices={{{-2,-2,0},{0,0,1},{}},{{2,-2,0},{0,0,1},{}},{{2,2,0},{0,0,1},{}},{{-2,2,0},{0,0,1},{}}};
    quad.indices={0,1,2,0,2,3};
    Check(AssetManager::GetModel(id)->SetRuntimePrimitives({quad}),"Probe fixture geometry failed");
    auto source=scene.CreateEntity("red source");auto& t=*source.AddComponent<Transform>();
    t.position=Vector3(0,0,2);t.rotation.y=glm::pi<float>();t.scale=Vector3(3);
    auto& sourceModel=*source.AddComponent<Model>();sourceModel.modelId=id;sourceModel.staticModel=true;
    source.AddComponent<Material>()->materialId=AssetManager::LoadMaterial((root/"red.material").string());
    auto receiver=scene.CreateEntity("dynamic receiver");receiver.AddComponent<Transform>()->position=Vector3(0,0,-3);
    receiver.AddComponent<Model>()->modelId=id;
    MeshRenderer3DSystem renderer;renderer.scene=&scene;renderer.window=&window;renderer.Create();
    MeshRenderer3DSystem* activeRenderer=&renderer;
    auto draw=[&]() {
        scene.SetVRCamera(Matrix4(1),glm::perspective(glm::radians(70.f),320.f/240.f,.1f,100.f),.1f,100.f);
        glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,320,240);glDepthMask(GL_TRUE);
        glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        activeRenderer->Update(scene.GetRegistry(),0);
        std::array<unsigned char,4> pixel{};glReadPixels(160,120,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data());
        GLint viewport[4];glGetIntegerv(GL_VIEWPORT,viewport);
        Check(viewport[2]==320 && viewport[3]==240,"Probe capture leaked its viewport");
        Check(scene.GetLastRenderCameraPosition()==Vector3(0),"Probe capture leaked its camera");
        Check(glGetError()==GL_NO_ERROR,"OpenGL error during probe render");return pixel;
    };
    auto baseline=draw();LightingProbe probe;probe.halfExtents=Vector3(10);probe.specularIntensity=0;
    probe.cachePath=(root/"room.probe").string();
    scene.GetLightingProbes().push_back(probe);auto diffuse=draw();
    Check(renderer.ReadyLightingProbeCount()==1,"Probe was not captured");
    Check(diffuse[0]>baseline[0]+2 && diffuse[0]>diffuse[1]+2,"Captured red wall did not provide diffuse color bleeding");
    Check(draw()==diffuse,"Cached probe changed between frames");
    Check(std::filesystem::exists(probe.cachePath),"Reflection probe capture was not persisted");
    const auto savedTime=std::filesystem::last_write_time(probe.cachePath);
    MeshRenderer3DSystem reloaded;reloaded.scene=&scene;reloaded.window=&window;reloaded.Create();
    activeRenderer=&reloaded;
    Check(draw()==diffuse,"Persisted reflection capture changed on reload");
    Check(std::filesystem::last_write_time(probe.cachePath)==savedTime,"Valid reflection cache was recaptured on reload");
    reloaded.OnDestroy();activeRenderer=&renderer;
    auto& p=scene.GetLightingProbes()[0];p.diffuseIntensity=0;p.specularIntensity=4;auto reflection=draw();
    Check(reflection[0]>baseline[0]+2,"Probe reflection did not reach dynamic receiver");
    p.diffuseIntensity=1;p.specularIntensity=0;p.halfExtents.z=3;auto boundary=draw();
    Check(boundary[0]>baseline[0]+2,"Reflection/GI faded away on a room's projection-box surface");
    p.halfExtents.z=10;p.diffuseIntensity=0;p.specularIntensity=4;
    sourceModel.staticModel=false;renderer.InvalidateLightingProbes();auto empty=draw();
    Check(empty==baseline,"Dynamic objects leaked into static probe capture");
    p.enabled=false;Check(draw()==baseline && renderer.ReadyLightingProbeCount()==0,"Disabled probe retained lighting");
    scene.GetLightingProbes().clear();draw();renderer.OnDestroy();AssetManager::FreeModel(id);
    std::filesystem::remove_all(root);
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
        StaticVisibility spatial;
        std::vector<StaticVisibility::Bounds> boxes;
        for(size_t i=0;i<256;++i) {
            Vector3 p(float(i%16)*3-24,float(i/16%4)*2-4,-float(i/64)*12-3);
            boxes.push_back({i,p-Vector3(.5f),p+Vector3(.5f)});
        }
        auto checkSpatial=[&] {
            spatial.Update(boxes);
            for(int frame=0;frame<12;++frame) {
                Matrix4 clip=projection*glm::translate(Matrix4(1),Vector3(frame*2-12,0,0));
                std::vector<unsigned char> hidden;spatial.Query(clip,boxes.size(),hidden);
                size_t rejected=0;
                for(const auto& box:boxes)if(hidden[box.candidate]) {
                    ++rejected;
                    Check(BoundsOutsideFrustum(clip,box.minimum,box.maximum),"Spatial tree rejected visible bounds");
                }
                Check(rejected>0,"Spatial tree did not reject any offscreen clusters");
            }
        };
        checkSpatial();
        boxes[0].minimum=Vector3(-.2f,-.2f,-2);boxes[0].maximum=Vector3(.2f,.2f,-1);
        checkSpatial();
        spatial.Update({});std::vector<unsigned char> emptyVisibility;
        spatial.Query(projection,3,emptyVisibility);
        Check(emptyVisibility==std::vector<unsigned char>(3,0),"Cleared spatial tree retained stale rejection");
        Window window("Renderer tests",320,240,true);
        ImportedTextureLoading();
        ImportedHierarchyGeometry();
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
        StereoPreparation(window);
        SinglePassPixels();
        SoftDepthIntersections(window);
        VertexColorsSurviveInstancing();
        PickingIdsIgnoreMaterialTint();
        BakedVisibilitySafety();
        MaterialSlotCulling(window);
        OverlayDepthIsolation(window);
        ScreenUiAfterPostProcessing(window);
        StaticShadowBaking(window);
        ShadowTextureIsolation(window);
        ParticleSpawnShapes(window);
        CombinedColliderAndVehicle(window);
        ParticleBudgetsTimingAndCollisions(window);
        DynamicRigidBatches(window);
        MirroredNormalMapping();
        LightingProbeRendering(window);
        std::cout<<"Frustum, stereo, bounds invalidation and instanced pixel parity passed\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
