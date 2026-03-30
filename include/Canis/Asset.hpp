#pragma once
#include <string>
#include <vector>

#include <Canis/Shader.hpp>
#include <Canis/Data/GLTexture.hpp>
#include <Canis/Data/Character.hpp>

#include <Canis/UUID.hpp>
#include <Canis/Data/Types.hpp>

namespace Canis
{
    typedef i32 AnimationClip2DID;
    typedef i32 ModelID;

    class Asset
    {
    public:
        Asset();
        ~Asset();

        Asset(const Asset &) = delete;
        Asset &operator=(const Asset &) = delete;

        virtual bool Load(std::string _path) = 0;
        virtual bool Free() = 0;
    };

    class TextureAsset : public Asset
    {
    private:
        GLTexture m_texture;
        std::string m_path = "";

    public:
        bool Load(std::string _path) override;
        bool Free() override;
        GLTexture GetGLTexture() { return m_texture; }
        GLTexture *GetPointerToTexture() { return &m_texture; }
        std::string GetPath() { return m_path; }
    };

    class SkyboxAsset : public Asset
    {
    private:
        unsigned int m_cubemapTexture = 0;
        bool m_loaded = false;

    public:
        bool Load(std::string _path) override;
        bool Free() override;
        unsigned int GetTexture() const { return m_cubemapTexture; }
        bool IsLoaded() const { return m_loaded; }
    };

    class ShaderAsset : public Asset
    {
    private:
        Canis::Shader *m_shader;

    public:
        ShaderAsset() { m_shader = new Canis::Shader(); }

        bool Load(std::string _path) override;
        bool Free() override;

        Canis::Shader *GetShader() { return m_shader; }
    };

    class TextAsset : public Asset
    {
    private:
        std::string m_path = "";
        unsigned int m_texture = 0;
        unsigned int m_vao = 0;
        unsigned int m_vbo = 0;
        unsigned int m_fontSize = 0;
        static const int atlasWidth = 1024;
        static const int atlasHeight = 1024;
        unsigned char m_atlasData[atlasWidth * atlasHeight * 4] = {};
    public:
        explicit TextAsset(unsigned int _fontSize) : m_fontSize(_fontSize) {}

        Character characters[127];

        bool Load(std::string _path) override;
        bool Free() override;
        unsigned int GetTexture() { return m_texture; }
        unsigned int GetVAO() { return m_vao; }
        unsigned int GetVBO() { return m_vbo; }
        unsigned int GetFontSize() { return m_fontSize; }
        std::string GetPath() { return m_path; }
    };

    class MetaFileAsset : public Asset
    {
    private:
    public:
        enum FileType {
            FILE_UNKNOWN,
            FRAGMENT,
            VERTEX,
            TEXTURE,
            SCENE,
            ANIMATIONCLIP2D,
            MODEL,
            MATERIAL,
            SKYBOX,
        };

        MetaFileAsset() {}

        void CreateMetaFile(std::string _path);
        void Save();

        bool Load(std::string _path) override;
        bool Free() override { return true; }

        FileType type = FILE_UNKNOWN;
        UUID uuid;
        std::string path = "";
        std::string name = "";
        std::string extension = "";
        u64 size = 0;
        // i64 created
        i64 modified = 0; // nanoseconds since the Unix epoch (Jan 1, 1970).
        // i64 accessed
    };

    struct SpriteFrame
    {
        f32 timeOnFrame = 0.0f;
        i32 textureId = 0u;
        u16 offsetX = 0u;
        u16 offsetY = 0u;
        u16 row = 0u;
        u16 col = 0u;
        u16 width = 0u;
        u16 height = 0u;
    };

    class SpriteAnimationAsset : public Asset
    {
    private:
    public:
        SpriteAnimationAsset() {}

        bool Load(std::string _path) override;
        bool Free() override;

        std::vector<SpriteFrame> frames = {};
    };

    enum MaterialInfo : u32
    {
        MATERIAL_HAS_SHADER = 1u << 0u,
        MATERIAL_HAS_ALBEDO = 1u << 1u,
        MATERIAL_HAS_SPECULAR = 1u << 2u,
        MATERIAL_HAS_EMISSION = 1u << 3u,
        MATERIAL_HAS_COLOR = 1u << 4u,
        MATERIAL_BACK_FACE_CULLING = 1u << 5u,
        MATERIAL_FRONT_FACE_CULLING = 1u << 6u,
        MATERIAL_HAS_ROUGHNESS = 1u << 7u,
        MATERIAL_HAS_METALLIC = 1u << 8u,
    };

    class MaterialFields
    {
    private:
        struct FloatUniformData
        {
            std::string name = "";
            float value = 0.0f;
        };

        std::vector<FloatUniformData> m_floatUniformData = {};

    public:
        void Use(Shader &_shader) const;
        void SetFloat(const std::string &_name, float _value);
    };

    struct MaterialAsset
    {
        u32 info = 0u;
        i32 shaderId = -1;
        i32 albedoId = -1;
        i32 specularId = -1;
        i32 roughnessId = -1;
        i32 metallicId = -1;
        i32 emissionId = -1;
        Color color = Color(1.0f);
        float specularValue = 0.5f;
        float roughnessValue = 0.5f;
        float metallicValue = 0.0f;
        MaterialFields materialFields = {};
    };

    class ModelAsset : public Asset
    {
    public:
        struct RenderVertex3D
        {
            Vector3 position = Vector3(0.0f);
            Vector3 normal = Vector3(0.0f, 1.0f, 0.0f);
            Vector2 uv = Vector2(0.0f);
        };

        struct SkinVertex3D
        {
            Vector4 joints = Vector4(0.0f);
            Vector4 weights = Vector4(0.0f);
        };

        struct PrimitiveBuild3D
        {
            std::vector<RenderVertex3D> vertices = {};
            std::vector<unsigned int> indices = {};
            std::vector<SkinVertex3D> skinVertices = {};
            i32 nodeIndex = -1;
            i32 skinIndex = -1;
            i32 materialSlot = -1;
            i32 textureId = -1;
            bool dynamicVertices = false;
        };

        struct Primitive3D
        {
            unsigned int vao = 0;
            unsigned int vbo = 0;
            unsigned int ebo = 0;
            std::vector<RenderVertex3D> bindVertices = {};
            std::vector<RenderVertex3D> skinnedVertices = {};
            std::vector<SkinVertex3D> skinVertices = {};
            std::vector<unsigned int> indices = {};
            i32 nodeIndex = -1;
            i32 skinIndex = -1;
            i32 materialSlot = -1;
            i32 textureId = -1;
            bool hasSkinning = false;
            bool dynamicVertices = false;
        };

        struct Node3D
        {
            i32 parent = -1;
            std::vector<i32> children = {};
            i32 mesh = -1;
            i32 skin = -1;
            Vector3 translation = Vector3(0.0f);
            Vector4 rotation = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
            Vector3 scale = Vector3(1.0f);
            Matrix4 localMatrix = Matrix4(1.0f);
            Matrix4 globalMatrix = Matrix4(1.0f);
            bool hasMatrix = false;
        };

        struct Skin3D
        {
            std::vector<i32> joints = {};
            std::vector<Matrix4> inverseBindMatrices = {};
        };

        enum AnimationPath3D
        {
            TRANSLATION = 0,
            ROTATION = 1,
            SCALE = 2,
        };

        struct AnimationSampler3D
        {
            std::vector<float> inputs = {};
            std::vector<Vector4> outputs = {};
            std::string interpolation = "LINEAR";
            bool cubicSpline = false;
        };

        struct AnimationChannel3D
        {
            i32 sampler = -1;
            i32 targetNode = -1;
            AnimationPath3D path = AnimationPath3D::TRANSLATION;
        };

        struct AnimationClip3D
        {
            std::string name = "";
            float duration = 0.0f;
            std::vector<AnimationSampler3D> samplers = {};
            std::vector<AnimationChannel3D> channels = {};
        };

        struct Pose3D
        {
            std::vector<Matrix4> localNodeMatrices = {};
            std::vector<Matrix4> globalNodeMatrices = {};
            std::vector<std::vector<RenderVertex3D>> skinnedVertices = {};
            std::vector<std::vector<Matrix4>> skinMatricesScratch = {};
            std::vector<Vector3> translationsScratch = {};
            std::vector<Vector4> rotationsScratch = {};
            std::vector<Vector3> scalesScratch = {};
            std::vector<bool> trsChangedScratch = {};
            std::vector<bool> visitedScratch = {};
        };

        bool Load(std::string _path) override;
        bool Free() override;
        bool SetRuntimePrimitives(
            const std::vector<PrimitiveBuild3D> &_primitives,
            const std::vector<std::string> &_materialSlotNames = {});

        bool UpdateAnimation(i32 _clipIndex, float _timeSeconds);
        bool UpdateAnimation(Pose3D &_pose, i32 _clipIndex, float _timeSeconds) const;
        void ResetPose();
        void ResetPose(Pose3D &_pose) const;
        void Draw(
            Shader &_shader,
            const Matrix4 &_modelMatrix,
            const Pose3D *_pose = nullptr,
            i32 _overrideTextureId = -1,
            const Color &_baseColor = Color(1.0f),
            const std::vector<MaterialAsset*> *_slotMaterialOverrides = nullptr);

        i32 GetAnimationCount() const { return (i32)m_animations.size(); }
        std::string GetAnimationName(i32 _index) const;
        float GetAnimationDuration(i32 _index) const;
        i32 GetMaterialSlotCount() const { return static_cast<i32>(m_materialSlotNames.size()); }
        std::string GetMaterialSlotName(i32 _index) const;

        std::string GetPath() const { return m_path; }
        u64 GetGeometryRevision() const { return m_geometryRevision; }
        bool BuildTriangleMesh(std::vector<Vector3> &_vertices, std::vector<u32> &_indices) const;

    private:
        std::string m_path = "";
        std::vector<Node3D> m_nodes = {};
        std::vector<i32> m_sceneRoots = {};
        std::vector<Skin3D> m_skins = {};
        std::vector<AnimationClip3D> m_animations = {};
        std::vector<Primitive3D> m_primitives = {};
        std::vector<std::string> m_materialSlotNames = {};
        std::vector<Vector3> m_bindTranslations = {};
        std::vector<Vector4> m_bindRotations = {};
        std::vector<Vector3> m_bindScales = {};
        std::vector<Matrix4> m_bindLocalMatrices = {};
        Pose3D m_sharedPose = {};
        u64 m_geometryRevision = 0u;

        void EnsurePose(Pose3D &_pose) const;
        void VisitNodeRecursive(i32 _nodeIndex, const Matrix4 &_parentMatrix, Pose3D &_pose, std::vector<bool> &_visited) const;
        void UpdateGlobalMatrices(Pose3D &_pose) const;
        void UpdateSkinning(Pose3D &_pose) const;
        void FreePrimitives();
        bool UploadPrimitive(Primitive3D &_primitive) const;
    };
} // end of Canis namespace
