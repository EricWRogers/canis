#pragma once
#include <string>
#include <map>
#include <filesystem>

#include <Canis/Asset.hpp>
#include <Canis/Debug.hpp>
#include <Canis/AssetHandle.hpp>

namespace Canis
{
    namespace AssetManager
    {
        struct AssetLibrary
        {
            int nextId{0};
            std::map<int, void *> assets{};
            std::map<std::string, int> assetPath{};
            std::map<UUID, std::string> uuidAssetPath{};
        };

        AssetLibrary &GetAssetLibrary();

        bool Has(std::string _name);
        int LoadMetaFile(const std::string &_path);
        MetaFileAsset* GetMetaFile(const std::string &_path);
        MetaFileAsset* GetMetaFile(const int _metaID);

        template <typename T>
        T *Get(int id)
        {
            auto &assetLibrary = GetAssetLibrary();
            return (T *)assetLibrary.assets[id];
        }

        inline std::string GetPath(int _id)
        {
            auto &assetLibrary = GetAssetLibrary();

            for (const auto &[key, value] : assetLibrary.assetPath)
                if (value == _id)
                    return key;

            return std::string("Path was not found in AssetLibrary");
        }

        inline std::string GetPath(UUID _uuid)
        {
            auto &assetLibrary = GetAssetLibrary();

            if (assetLibrary.uuidAssetPath.contains(_uuid))
                return assetLibrary.uuidAssetPath[_uuid];

            /*for (const auto &[key, value] : assetLibrary.assetPath)
                if (value == _id)
                    return key;*/

            return std::string("Path was not found in AssetLibrary");
        }

        inline std::string ResolvePath(const SceneAssetHandle &_sceneAssetHandle)
        {
            if (_sceneAssetHandle.uuid != UUID(0))
            {
                const std::string resolvedPath = GetPath(_sceneAssetHandle.uuid);
                if (resolvedPath != "Path was not found in AssetLibrary")
                    return resolvedPath;

                // Fallback for UUID-only references that were not cached yet.
                std::error_code ec;
                if (std::filesystem::exists("assets", ec) && std::filesystem::is_directory("assets", ec))
                {
                    for (const auto& entry : std::filesystem::recursive_directory_iterator("assets", ec))
                    {
                        if (ec || !entry.is_regular_file())
                            continue;

                        if (entry.path().extension() == ".meta")
                            continue;

                        if (MetaFileAsset* meta = GetMetaFile(entry.path().generic_string()))
                        {
                            if (meta->uuid == _sceneAssetHandle.uuid)
                                return entry.path().generic_string();
                        }
                    }
                }
            }

            return _sceneAssetHandle.path;
        }

        inline std::string ResolvePath(const ShaderGraphAssetHandle &_shaderGraphAssetHandle)
        {
            if (_shaderGraphAssetHandle.uuid != UUID(0))
            {
                const std::string resolvedPath = GetPath(_shaderGraphAssetHandle.uuid);
                if (resolvedPath != "Path was not found in AssetLibrary")
                    return resolvedPath;

                std::error_code ec;
                if (std::filesystem::exists("assets", ec) && std::filesystem::is_directory("assets", ec))
                {
                    for (const auto &entry : std::filesystem::recursive_directory_iterator("assets", ec))
                    {
                        if (ec || !entry.is_regular_file())
                            continue;

                        if (entry.path().extension() == ".meta")
                            continue;

                        if (MetaFileAsset *meta = GetMetaFile(entry.path().generic_string()))
                        {
                            if (meta->uuid == _shaderGraphAssetHandle.uuid)
                                return entry.path().generic_string();
                        }
                    }
                }
            }

            return _shaderGraphAssetHandle.path;
        }

        inline int GetID(UUID _uuid)
        {
            auto &assetLibrary = GetAssetLibrary();

            i32 id = -1;

            std::string path = GetPath(_uuid);

            if (assetLibrary.assetPath.contains(path))
                id = assetLibrary.assetPath[path];

            return id;
        }

        inline int GetID(std::string _path)
        {
            auto &assetLibrary = GetAssetLibrary();

            i32 id = -1;

            if (assetLibrary.assetPath.contains(_path))
                id = assetLibrary.assetPath[_path];

            return id;
        }

        bool MoveAsset(std::string _source, std::string _target);
        bool DeleteAsset(std::string _path);

        template <typename T>
        void Free(std::string _name)
        {
            auto &assetLibrary = GetAssetLibrary();
            if (!Has(_name))
                return;

            int assetId = assetLibrary.assetPath[_name];

            {
                std::map<std::string, int>::iterator it;
                it = assetLibrary.assetPath.find(_name);

                assetLibrary.assetPath.erase(it);
            }

            if (!assetLibrary.assets.contains(assetId))
                return;

            ((T *)assetLibrary.assets[assetId])->Free();
            delete ((T *)assetLibrary.assets[assetId]);

            {
                std::map<int, void *>::iterator it;
                it = assetLibrary.assets.find(assetId);

                assetLibrary.assets.erase(it);
            }
        }

        int LoadTexture(const std::string &_path);
        TextureAsset *GetTexture(const std::string &_path);
        TextureAsset *GetTexture(const int _textureID);

        TextureHandle GetTextureHandle(const std::string &_path);
        TextureHandle GetTextureHandle(const int _textureID);

        int LoadText(const std::string &_path, unsigned int _fontSize);
        TextAsset *GetText(const std::string &_path, unsigned int _fontSize);
        TextAsset *GetText(i32 _textID);

        int LoadAudioClip(const std::string &_path);
        AudioClipAsset *GetAudioClip(const std::string &_path);
        AudioClipAsset *GetAudioClip(i32 _audioID);

        int LoadSound(const std::string &_path);
        SoundAsset *GetSound(const std::string &_path);
        SoundAsset *GetSound(i32 _soundID);

        int LoadMusic(const std::string &_path);
        MusicAsset *GetMusic(const std::string &_path);
        MusicAsset *GetMusic(i32 _musicID);

        int LoadShader(const std::string &_pathWithOutExtension);
        void ReloadLoadedShaders();

        int LoadSpriteAnimation(const std::string &_path);
        SpriteAnimationAsset* GetSpriteAnimation(const std::string &_path);
        SpriteAnimationAsset* GetSpriteAnimation(i32 _animationID);

        int LoadModel(const std::string &_path);
        int CreateModel();
        void FreeModel(i32 _modelID);
        ModelAsset* GetModel(const std::string &_path);
        ModelAsset* GetModel(i32 _modelID);

        int LoadMaterial(const std::string &_path);
        MaterialAsset* GetMaterial(const std::string &_path);
        MaterialAsset* GetMaterial(i32 _materialID);
        bool ReloadMaterial(const std::string &_path);

        int LoadSkybox(const std::string &_path);
        SkyboxAsset* GetSkybox(const std::string &_path);
        SkyboxAsset* GetSkybox(i32 _skyboxID);

        int LoadPostProcess(const std::string &_path);
        PostProcessAsset* GetPostProcess(const std::string &_path);
        PostProcessAsset* GetPostProcess(i32 _postProcessID);
    } // end of AssetManager namespace
} // end of Canis namespace
