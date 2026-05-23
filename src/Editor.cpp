#include <Canis/Editor.hpp>

#include <Canis/Canis.hpp>
#include <Canis/Debug.hpp>
#include <Canis/OpenGL.hpp>
#include <Canis/Window.hpp>
#include <Canis/Scene.hpp>
#include <Canis/Entity.hpp>
#include <Canis/App.hpp>
#include <Canis/Time.hpp>
#include <Canis/Shader.hpp>
#include <Canis/IOManager.hpp>
#include <Canis/InputManager.hpp>
#include <Canis/GameCodeObject.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/AudioManager.hpp>
#include <Canis/AnimationRuntime.hpp>
#include <Canis/ShaderGraph.hpp>
#include <Canis/ShaderGraphGraphEditor.hpp>
#include <Canis/Yaml.hpp>
#include <Canis/PostProcessPipeline.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_process.h>

#include <imgui.h>
#include <imgui_stdlib.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>

#include <ImGuizmo.h>
#include <ImSequencer.h>
#include <ImCurveEdit.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <Canis/External/tinygltf/tiny_gltf.h>

#include <filesystem>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <cctype>
#include <limits>
#include <sstream>
#include <utility>
#include <unordered_map>
#include <unordered_set>

#include <stb_image.h>

namespace Canis
{
    static void PushInspectorFieldID(const char *_label, const char *_idSuffix)
    {
        ImGui::PushID((_label != nullptr) ? _label : "");

        if (_idSuffix != nullptr && _idSuffix[0] != '\0')
            ImGui::PushID(_idSuffix);
    }

    static void PopInspectorFieldID(const char *_idSuffix)
    {
        if (_idSuffix != nullptr && _idSuffix[0] != '\0')
            ImGui::PopID();

        ImGui::PopID();
    }

    static bool ExportHierarchyRootsToPrefabAsset(
        Canis::Scene &_scene,
        const std::vector<Canis::Entity*> &_roots,
        const std::string &_prefabPath);
    static void AssignPrefabHandle(Canis::Entity *_entity, const SceneAssetHandle &_prefabHandle);

    namespace
    {
        YAML::Node g_lastPlaySceneNode;
        std::string g_lastPlayScenePath;
        constexpr int kEditorThemeDark = 0;
        constexpr int kEditorThemeLight = 1;

        int NormalizeEditorThemeSelection(int _themeSelection)
        {
            if (_themeSelection == kEditorThemeDark || _themeSelection == kEditorThemeLight)
                return _themeSelection;

            return kEditorThemeDark;
        }

        float NormalizeEditorFontScale(float _fontScale)
        {
            if (!std::isfinite(_fontScale))
                return 1.0f;

            return std::clamp(_fontScale, 0.5f, 2.5f);
        }

        void ApplyEditorThemeStyle(int _themeSelection, float _uiScale)
        {
            const float normalizedUiScale = (std::isfinite(_uiScale) && _uiScale > 0.0f) ? _uiScale : 1.0f;
            const int normalizedTheme = NormalizeEditorThemeSelection(_themeSelection);
            ImGuiStyle &style = ImGui::GetStyle();

            // Reset to the default ImGui metrics before applying theme/scale so
            // repeated theme toggles don't multiply spacing and paddings.
            style = ImGuiStyle();
            if (normalizedTheme == kEditorThemeLight)
                ImGui::StyleColorsLight(&style);
            else
                ImGui::StyleColorsDark(&style);
            style.ScaleAllSizes(normalizedUiScale);

            ImGuiIO &io = ImGui::GetIO();
            if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
            {
                style.WindowRounding = 0.0f;
                style.Colors[ImGuiCol_WindowBg].w = 1.0f;
            }
        }

        Matrix4 ComposeTransformMatrix(const Vector3 &_position, const Vector3 &_rotation, const Vector3 &_scale)
        {
            Matrix4 matrix = Matrix4(1.0f);
            matrix = glm::translate(matrix, _position);
            matrix = glm::rotate(matrix, _rotation.z, Vector3(0.0f, 0.0f, 1.0f));
            matrix = glm::rotate(matrix, _rotation.y, Vector3(0.0f, 1.0f, 0.0f));
            matrix = glm::rotate(matrix, _rotation.x, Vector3(1.0f, 0.0f, 0.0f));
            matrix = glm::scale(matrix, _scale);
            return matrix;
        }

        bool BuildEntityModelWorldVertices(
            Entity *_entity,
            const Matrix4 &_modelMatrix,
            std::vector<Vector3> &_outVertices,
            std::size_t _maxVertices)
        {
            if (_entity == nullptr ||
                !_entity->HasComponent<Transform>() ||
                !_entity->GetComponent<Transform>().IsActiveInHierarchy() ||
                !_entity->HasComponent<Model>())
                return false;

            Model &modelRenderer = _entity->GetComponent<Model>();
            if (modelRenderer.modelId < 0)
                return false;

            ModelAsset *model = AssetManager::GetModel(modelRenderer.modelId);
            if (model == nullptr)
                return false;

            std::vector<Vector3> localVertices = {};
            std::vector<u32> indices = {};
            if (!model->BuildTriangleMesh(localVertices, indices, modelRenderer.nodeIndex, modelRenderer.applyNodeTransform))
                return false;

            if (localVertices.empty())
                return false;

            _outVertices.clear();
            _outVertices.reserve(std::min(localVertices.size(), _maxVertices));

            const std::size_t stride = localVertices.size() > _maxVertices ?
                static_cast<std::size_t>(std::ceil(static_cast<float>(localVertices.size()) / static_cast<float>(_maxVertices))) :
                1u;

            for (std::size_t i = 0; i < localVertices.size() && _outVertices.size() < _maxVertices; i += stride)
            {
                const Vector3 &localVertex = localVertices[i];
                const Vector4 world = _modelMatrix * Vector4(localVertex.x, localVertex.y, localVertex.z, 1.0f);
                _outVertices.push_back(Vector3(world.x, world.y, world.z));
            }

            return true;
        }

        bool BuildEntityModelWorldBounds(
            Entity *_entity,
            const Matrix4 &_modelMatrix,
            Vector3 &_outMin,
            Vector3 &_outMax)
        {
            if (_entity == nullptr ||
                !_entity->HasComponent<Transform>() ||
                !_entity->GetComponent<Transform>().IsActiveInHierarchy() ||
                !_entity->HasComponent<Model>())
                return false;

            Model &modelRenderer = _entity->GetComponent<Model>();
            if (modelRenderer.modelId < 0)
                return false;

            ModelAsset *model = AssetManager::GetModel(modelRenderer.modelId);
            if (model == nullptr)
                return false;

            Vector3 localMin(0.0f);
            Vector3 localMax(0.0f);
            if (!model->GetLocalBounds(localMin, localMax, modelRenderer.nodeIndex, modelRenderer.applyNodeTransform))
                return false;

            const Vector3 corners[8] = {
                Vector3(localMin.x, localMin.y, localMin.z),
                Vector3(localMax.x, localMin.y, localMin.z),
                Vector3(localMin.x, localMax.y, localMin.z),
                Vector3(localMax.x, localMax.y, localMin.z),
                Vector3(localMin.x, localMin.y, localMax.z),
                Vector3(localMax.x, localMin.y, localMax.z),
                Vector3(localMin.x, localMax.y, localMax.z),
                Vector3(localMax.x, localMax.y, localMax.z),
            };

            bool initialized = false;
            for (const Vector3 &corner : corners)
            {
                const Vector4 world = _modelMatrix * Vector4(corner.x, corner.y, corner.z, 1.0f);
                const Vector3 worldCorner(world.x, world.y, world.z);
                if (!initialized)
                {
                    _outMin = worldCorner;
                    _outMax = worldCorner;
                    initialized = true;
                    continue;
                }

                _outMin = glm::min(_outMin, worldCorner);
                _outMax = glm::max(_outMax, worldCorner);
            }

            return initialized;
        }

        bool RangesOverlap(float _minA, float _maxA, float _minB, float _maxB, float _padding)
        {
            return _maxA + _padding >= _minB && _maxB + _padding >= _minA;
        }

        void RefreshMetaFileTimestamp(const std::string &_path)
        {
            MetaFileAsset *meta = AssetManager::GetMetaFile(_path);
            if (meta == nullptr)
                return;

            SDL_PathInfo info;
            if (!SDL_GetPathInfo(_path.c_str(), &info))
                return;

            meta->path = _path;
            meta->name = GetFileName(_path);
            meta->extension = GetFileExtension(_path);
            std::transform(meta->extension.begin(), meta->extension.end(), meta->extension.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });
            meta->size = info.size;
            meta->modified = info.modify_time;
            meta->Save();
        }

        bool IsHotReloadEligibleAsset(const std::string &_path)
        {
            if (_path.empty() || std::filesystem::path(_path).extension() == ".meta")
                return false;

            MetaFileAsset *meta = AssetManager::GetMetaFile(_path);
            if (meta == nullptr)
                return false;

            switch (meta->type)
            {
                case MetaFileAsset::FileType::TEXTURE:
                case MetaFileAsset::FileType::AUDIO:
                case MetaFileAsset::FileType::ANIMATIONCLIP:
                case MetaFileAsset::FileType::ANIMATORCONTROLLER:
                case MetaFileAsset::FileType::MODEL:
                case MetaFileAsset::FileType::MATERIAL:
                case MetaFileAsset::FileType::SKYBOX:
                case MetaFileAsset::FileType::POSTPROCESS:
                case MetaFileAsset::FileType::VERTEX:
                case MetaFileAsset::FileType::FRAGMENT:
                    return true;
                default:
                    return false;
            }
        }

        float GetEditorToolbarHeight()
        {
            const ImGuiStyle &style = ImGui::GetStyle();
            return ImGui::GetFrameHeightWithSpacing() + (style.WindowPadding.y * 2.0f);
        }

        std::filesystem::path GetEditorRuntimeBasePath()
        {
            const char* basePath = SDL_GetBasePath();
            if (basePath != nullptr)
                return std::filesystem::path(basePath);

            return std::filesystem::current_path();
        }

        std::filesystem::path GetDefaultStandaloneExecutablePath()
        {
#if defined(_WIN32)
            return GetEditorRuntimeBasePath() / "c-engine.exe";
#else
            return GetEditorRuntimeBasePath() / "c-engine";
#endif
        }

        bool IsRegularFilePath(const std::filesystem::path &_path)
        {
            std::error_code ec;
            return !_path.empty() && std::filesystem::exists(_path, ec) && std::filesystem::is_regular_file(_path, ec);
        }

        bool IsDirectoryPath(const std::filesystem::path &_path)
        {
            std::error_code ec;
            return !_path.empty() && std::filesystem::exists(_path, ec) && std::filesystem::is_directory(_path, ec);
        }

        std::vector<std::filesystem::path> BuildPathCandidates(const std::string &_configuredPath)
        {
            namespace fs = std::filesystem;

            std::vector<fs::path> candidates = {};
            if (_configuredPath.empty())
                return candidates;

            const fs::path rawPath(_configuredPath);
            if (rawPath.is_absolute())
            {
                candidates.push_back(rawPath);
                return candidates;
            }

            candidates.push_back(rawPath);
            candidates.push_back(fs::current_path() / rawPath);
            candidates.push_back(GetEditorRuntimeBasePath() / rawPath);
            candidates.push_back(GetEditorRuntimeBasePath() / "project" / rawPath);
            return candidates;
        }

        std::filesystem::path ResolveLaunchExecutablePath(const std::string &_configuredPath)
        {
            namespace fs = std::filesystem;

            std::vector<fs::path> candidates = BuildPathCandidates(_configuredPath);
            if (_configuredPath.empty())
                candidates.push_back(GetDefaultStandaloneExecutablePath());

#if defined(_WIN32)
            const std::size_t originalCount = candidates.size();
            for (std::size_t i = 0; i < originalCount; ++i)
            {
                if (!candidates[i].has_extension())
                    candidates.push_back(candidates[i].string() + ".exe");
            }
#endif

            for (const fs::path &candidate : candidates)
            {
                if (IsRegularFilePath(candidate))
                    return candidate.lexically_normal();
            }

            return {};
        }

        std::filesystem::path ResolveLaunchWorkingDirectory(const std::string &_configuredPath, const std::filesystem::path &_executablePath)
        {
            namespace fs = std::filesystem;

            if (_configuredPath.empty())
            {
                const fs::path currentDirectory = fs::current_path();
                if (IsDirectoryPath(currentDirectory))
                    return currentDirectory;

                if (!_executablePath.empty() && IsDirectoryPath(_executablePath.parent_path()))
                    return _executablePath.parent_path();

                return GetEditorRuntimeBasePath();
            }

            for (const fs::path &candidate : BuildPathCandidates(_configuredPath))
            {
                if (IsDirectoryPath(candidate))
                    return candidate.lexically_normal();
            }

            return {};
        }

        std::vector<std::string> TokenizeLaunchArguments(const std::string &_arguments)
        {
            std::vector<std::string> tokens = {};
            std::string current = "";
            bool inQuotes = false;
            char quoteChar = '\0';
            bool escapeNext = false;

            for (char c : _arguments)
            {
                if (escapeNext)
                {
                    current.push_back(c);
                    escapeNext = false;
                    continue;
                }

                if (c == '\\' && inQuotes)
                {
                    escapeNext = true;
                    continue;
                }

                if (c == '"' || c == '\'')
                {
                    if (inQuotes && c == quoteChar)
                    {
                        inQuotes = false;
                        quoteChar = '\0';
                    }
                    else if (!inQuotes)
                    {
                        inQuotes = true;
                        quoteChar = c;
                    }
                    else
                    {
                        current.push_back(c);
                    }
                    continue;
                }

                if (!inQuotes && std::isspace(static_cast<unsigned char>(c)))
                {
                    if (!current.empty())
                    {
                        tokens.push_back(current);
                        current.clear();
                    }
                    continue;
                }

                current.push_back(c);
            }

            if (escapeNext)
                current.push_back('\\');

            if (!current.empty())
                tokens.push_back(current);

            return tokens;
        }

        bool LaunchStandaloneGameFromProjectConfig(std::string &_outMessage)
        {
            const ProjectConfig &projectConfig = GetProjectConfig();
            const std::filesystem::path executablePath = ResolveLaunchExecutablePath(projectConfig.launchExecutablePath);
            if (executablePath.empty())
            {
                _outMessage = "Launch failed. Set a valid launch executable in Project Settings or place c-engine next to the editor executable.";
                return false;
            }

            const std::filesystem::path workingDirectory = ResolveLaunchWorkingDirectory(projectConfig.launchWorkingDirectory, executablePath);
            if (workingDirectory.empty())
            {
                _outMessage = "Launch failed. The configured working directory does not exist.";
                return false;
            }

            std::vector<std::string> argvStorage = {};
            argvStorage.push_back(executablePath.string());
            const std::vector<std::string> extraArguments = TokenizeLaunchArguments(projectConfig.launchArguments);
            argvStorage.insert(argvStorage.end(), extraArguments.begin(), extraArguments.end());

            std::vector<const char*> argv = {};
            argv.reserve(argvStorage.size() + 1u);
            for (const std::string &argument : argvStorage)
                argv.push_back(argument.c_str());
            argv.push_back(nullptr);

            SDL_Environment *launchEnvironment = SDL_CreateEnvironment(true);
            if (launchEnvironment == nullptr)
            {
                _outMessage = "Launch failed. Unable to create process environment: " + std::string(SDL_GetError());
                return false;
            }

            bool success = SDL_SetEnvironmentVariable(launchEnvironment, "CANIS_EDITOR_RUNTIME", "0", true);
            success = success && SDL_SetEnvironmentVariable(launchEnvironment, "CANIS_EDITOR", "0", true);
            if (!success)
            {
                _outMessage = "Launch failed. Unable to set no-editor environment override: " + std::string(SDL_GetError());
                SDL_DestroyEnvironment(launchEnvironment);
                return false;
            }

            SDL_PropertiesID launchProperties = SDL_CreateProperties();
            if (launchProperties == 0)
            {
                _outMessage = "Launch failed. Unable to create process properties: " + std::string(SDL_GetError());
                SDL_DestroyEnvironment(launchEnvironment);
                return false;
            }

            success = SDL_SetPointerProperty(
                launchProperties,
                SDL_PROP_PROCESS_CREATE_ARGS_POINTER,
                const_cast<const char**>(argv.data()));
            success = success && SDL_SetPointerProperty(
                launchProperties,
                SDL_PROP_PROCESS_CREATE_ENVIRONMENT_POINTER,
                launchEnvironment);
            success = success && SDL_SetStringProperty(
                launchProperties,
                SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING,
                workingDirectory.string().c_str());
            success = success && SDL_SetBooleanProperty(
                launchProperties,
                SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN,
                true);
            success = success && SDL_SetNumberProperty(
                launchProperties,
                SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER,
                SDL_PROCESS_STDIO_INHERITED);
            success = success && SDL_SetNumberProperty(
                launchProperties,
                SDL_PROP_PROCESS_CREATE_STDERR_NUMBER,
                SDL_PROCESS_STDIO_INHERITED);

            if (!success)
            {
                _outMessage = "Launch failed. Unable to configure process properties: " + std::string(SDL_GetError());
                SDL_DestroyProperties(launchProperties);
                SDL_DestroyEnvironment(launchEnvironment);
                return false;
            }

            SDL_Process *process = SDL_CreateProcessWithProperties(launchProperties);
            SDL_DestroyProperties(launchProperties);
            SDL_DestroyEnvironment(launchEnvironment);

            if (process == nullptr)
            {
                _outMessage = "Launch failed. SDL could not create the process: " + std::string(SDL_GetError());
                return false;
            }

            SDL_DestroyProcess(process);
            _outMessage = "Launched standalone game: " + executablePath.generic_string();
            return true;
        }

        bool HasSupportedFontExtension(const std::filesystem::path &_path)
        {
            std::string extension = _path.extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });

            return extension == ".ttf" || extension == ".otf";
        }

        std::filesystem::path ResolveEditorFontPath(const std::string &_fontPath)
        {
            namespace fs = std::filesystem;

            if (_fontPath.empty())
                return {};

            std::error_code ec;
            const fs::path rawPath = fs::path(_fontPath);
            if (rawPath.is_absolute())
            {
                if (fs::exists(rawPath, ec))
                    return rawPath;

                return {};
            }

            std::vector<fs::path> candidates =
            {
                rawPath,
                GetEditorRuntimeBasePath() / rawPath,
                GetEditorRuntimeBasePath() / "project" / rawPath
            };

            for (const fs::path &candidate : candidates)
            {
                ec.clear();
                if (fs::exists(candidate, ec))
                    return candidate;
            }

            return {};
        }

        std::filesystem::path BuildDuplicateAssetPath(const std::filesystem::path &_sourcePath)
        {
            namespace fs = std::filesystem;

            const fs::path parentPath = _sourcePath.parent_path();
            const std::string stem = _sourcePath.stem().string();
            const std::string extension = _sourcePath.extension().string();

            fs::path candidatePath = parentPath / (stem + "_copy" + extension);
            int index = 1;
            while (fs::exists(candidatePath) || fs::exists(candidatePath.string() + ".meta"))
            {
                candidatePath = parentPath / (stem + "_copy_" + std::to_string(index) + extension);
                ++index;
            }

            return candidatePath;
        }

        std::filesystem::path BuildUniqueAssetPath(
            const std::filesystem::path &_folderPath,
            const std::string &_stem,
            const std::string &_extension)
        {
            namespace fs = std::filesystem;

            fs::path candidatePath = _folderPath / (_stem + _extension);
            int index = 1;
            while (fs::exists(candidatePath) || fs::exists(candidatePath.string() + ".meta"))
            {
                candidatePath = _folderPath / (_stem + "_" + std::to_string(index) + _extension);
                ++index;
            }

            return candidatePath;
        }

        bool IsPathAtOrInsidePrefix(const std::string &_path, const std::string &_prefix)
        {
            if (_path == _prefix)
                return true;

            return _path.rfind(_prefix + "/", 0) == 0;
        }

        std::string RemapPathPrefix(
            const std::string &_path,
            const std::string &_oldPrefix,
            const std::string &_newPrefix)
        {
            if (!IsPathAtOrInsidePrefix(_path, _oldPrefix))
                return _path;

            return _newPrefix + _path.substr(_oldPrefix.size());
        }

        bool RefreshDuplicatedMetaFile(const std::string &_assetPath)
        {
            const std::string metaPath = _assetPath + ".meta";
            if (!FileExists(metaPath.c_str()))
                return false;

            MetaFileAsset meta;
            meta.Load(_assetPath);
            meta.uuid = UUID();
            meta.path = _assetPath;
            meta.name = GetFileName(_assetPath);

            SDL_PathInfo info;
            if (SDL_GetPathInfo(_assetPath.c_str(), &info))
            {
                meta.size = info.size;
                meta.modified = info.modify_time;
            }

            meta.Save();
            return true;
        }

        bool IsValidCppIdentifier(const std::string &_value)
        {
            if (_value.empty())
                return false;

            const unsigned char first = static_cast<unsigned char>(_value.front());
            if (!(std::isalpha(first) || _value.front() == '_'))
                return false;

            for (char c : _value)
            {
                const unsigned char value = static_cast<unsigned char>(c);
                if (!(std::isalnum(value) || c == '_'))
                    return false;
            }

            return true;
        }

        std::string NormalizeScriptTarget(const std::string &_rawTarget)
        {
            std::string normalized = _rawTarget;

            size_t position = 0;
            while ((position = normalized.find("::", position)) != std::string::npos)
                normalized.replace(position, 2, "/");

            std::replace(normalized.begin(), normalized.end(), '\\', '/');

            while (!normalized.empty() && normalized.front() == '/')
                normalized.erase(normalized.begin());

            while (!normalized.empty() && normalized.back() == '/')
                normalized.pop_back();

            return normalized;
        }

        std::vector<std::string> SplitScriptTarget(const std::string &_normalizedTarget)
        {
            std::vector<std::string> parts = {};
            std::stringstream stream(_normalizedTarget);
            std::string part = "";

            while (std::getline(stream, part, '/'))
            {
                if (!part.empty())
                    parts.push_back(part);
            }

            return parts;
        }

        std::string JoinStringParts(const std::vector<std::string> &_parts, const std::string &_separator)
        {
            std::string value = "";
            for (size_t i = 0; i < _parts.size(); ++i)
            {
                if (i > 0)
                    value += _separator;

                value += _parts[i];
            }

            return value;
        }

        std::string MakeIndent(int _amount)
        {
            return std::string(static_cast<size_t>(std::max(_amount, 0)), ' ');
        }

        void AppendNamespaceOpens(std::ostringstream &_stream, const std::vector<std::string> &_namespaceParts)
        {
            int indentLevel = 0;
            for (const std::string &namespaceName : _namespaceParts)
            {
                const std::string indent = MakeIndent(indentLevel);
                _stream << indent << "namespace " << namespaceName << "\n";
                _stream << indent << "{\n";
                indentLevel += 4;
            }
        }

        void AppendNamespaceCloses(std::ostringstream &_stream, const std::vector<std::string> &_namespaceParts)
        {
            int indentLevel = static_cast<int>(_namespaceParts.size()) * 4;
            for (size_t index = _namespaceParts.size(); index > 0; --index)
            {
                indentLevel -= 4;
                _stream << MakeIndent(indentLevel) << "}\n";
            }
        }

        enum class GameScriptType
        {
            ScriptableEntity = 0,
            Component = 1,
            System = 2
        };

        constexpr const char* kGameScriptTypeLabels[] =
        {
            "ScriptableEntity",
            "Component",
            "System"
        };

        GameScriptType GetGameScriptTypeFromSelection(int _selection)
        {
            if (_selection < 0 || _selection >= static_cast<int>(IM_ARRAYSIZE(kGameScriptTypeLabels)))
                return GameScriptType::ScriptableEntity;

            return static_cast<GameScriptType>(_selection);
        }

        bool CreateGameScriptFiles(
            const std::filesystem::path &_repoRoot,
            const std::string &_rawTarget,
            GameScriptType _scriptType,
            const std::vector<std::string> &_requiredComponents,
            bool _forceOverwrite,
            std::string &_outHeaderPath,
            std::string &_outSourcePath,
            std::string &_outError)
        {
            namespace fs = std::filesystem;

            const std::string normalizedTarget = NormalizeScriptTarget(_rawTarget);
            if (normalizedTarget.empty())
            {
                _outError = "Script name cannot be empty.";
                return false;
            }

            std::vector<std::string> pathParts = SplitScriptTarget(normalizedTarget);
            if (pathParts.empty())
            {
                _outError = "Invalid script target.";
                return false;
            }

            const std::string className = pathParts.back();
            pathParts.pop_back();

            if (!IsValidCppIdentifier(className))
            {
                _outError = "Script class name must be a valid C++ identifier.";
                return false;
            }

            for (const std::string &namespacePart : pathParts)
            {
                if (!IsValidCppIdentifier(namespacePart))
                {
                    _outError = "Namespace segments must be valid C++ identifiers.";
                    return false;
                }
            }

            const std::string relativeDir = JoinStringParts(pathParts, "/");
            const std::string scriptSymbolName = pathParts.empty() ? className : JoinStringParts(pathParts, "::") + "::" + className;
            const std::string relativeHeader = relativeDir.empty() ? className + ".hpp" : relativeDir + "/" + className + ".hpp";
            const std::string relativeSource = relativeDir.empty() ? className + ".cpp" : relativeDir + "/" + className + ".cpp";

            const fs::path headerPath = _repoRoot / "game" / "include" / relativeHeader;
            const fs::path sourcePath = _repoRoot / "game" / "src" / relativeSource;

            if ((fs::exists(headerPath) || fs::exists(sourcePath)) && !_forceOverwrite)
            {
                _outError = "Target script files already exist.";
                return false;
            }

            std::error_code ec;
            fs::create_directories(headerPath.parent_path(), ec);
            if (ec)
            {
                _outError = "Failed to create header directory: " + ec.message();
                return false;
            }

            fs::create_directories(sourcePath.parent_path(), ec);
            if (ec)
            {
                _outError = "Failed to create source directory: " + ec.message();
                return false;
            }

            std::ostringstream header;
            header << "#pragma once\n\n";
            if (_scriptType == GameScriptType::System)
                header << "#include <Canis/System.hpp>\n\n";
            else
                header << "#include <Canis/Entity.hpp>\n\n";
            header << "namespace Canis\n{\n    class App;\n}\n";

            if (!pathParts.empty())
                header << "\n";

            AppendNamespaceOpens(header, pathParts);

            const int classIndentLevel = static_cast<int>(pathParts.size()) * 4;
            const std::string classIndent = MakeIndent(classIndentLevel);
            const std::string bodyIndent = MakeIndent(classIndentLevel + 4);

            if (_scriptType == GameScriptType::ScriptableEntity)
            {
                header << classIndent << "class " << className << " : public Canis::ScriptableEntity\n";
                header << classIndent << "{\n";
                header << classIndent << "public:\n";
                header << bodyIndent << "static constexpr const char* ScriptName = \"" << scriptSymbolName << "\";\n\n";
                header << bodyIndent << "explicit " << className << "(Canis::Entity& _entity) : Canis::ScriptableEntity(_entity) {}\n\n";
                header << bodyIndent << "void Create() override;\n";
                header << bodyIndent << "void Ready() override;\n";
                header << bodyIndent << "void Destroy() override;\n";
                header << bodyIndent << "void Update(float _dt) override;\n";
                header << classIndent << "};\n\n";
                header << classIndent << "void Register" << className << "Script(Canis::App& _app);\n";
                header << classIndent << "void UnRegister" << className << "Script(Canis::App& _app);\n";
            }
            else if (_scriptType == GameScriptType::Component)
            {
                header << classIndent << "struct " << className << "\n";
                header << classIndent << "{\n";
                header << classIndent << "public:\n";
                header << bodyIndent << "static constexpr const char* ScriptName = \"" << scriptSymbolName << "\";\n\n";
                header << bodyIndent << className << "() = default;\n";
                header << bodyIndent << "explicit " << className << "(Canis::Entity& _entity) : entity(&_entity) {}\n\n";
                header << bodyIndent << "void Create() {}\n";
                header << bodyIndent << "Canis::Entity* entity = nullptr;\n";
                header << bodyIndent << "bool active = true;\n";
                header << classIndent << "};\n\n";
                header << classIndent << "void Register" << className << "Component(Canis::App& _app);\n";
                header << classIndent << "void UnRegister" << className << "Component(Canis::App& _app);\n";
            }
            else
            {
                header << classIndent << "class " << className << " : public Canis::System\n";
                header << classIndent << "{\n";
                header << classIndent << "public:\n";
                header << bodyIndent << "static constexpr const char* ScriptName = \"" << scriptSymbolName << "\";\n\n";
                header << bodyIndent << className << "() : Canis::System() { m_name = type_name<" << className << ">(); }\n\n";
                header << bodyIndent << "void Create() override;\n";
                header << bodyIndent << "void Ready() override;\n";
                header << bodyIndent << "void Update(entt::registry &_registry, float _deltaTime) override;\n";
                header << bodyIndent << "void OnDestroy() override;\n";
                header << classIndent << "};\n\n";
                header << classIndent << "void Register" << className << "System(Canis::App& _app);\n";
                header << classIndent << "void UnRegister" << className << "System(Canis::App& _app);\n";
            }

            if (!pathParts.empty())
                AppendNamespaceCloses(header, pathParts);

            std::ostringstream source;
            source << "#include <" << relativeHeader << ">\n\n";
            source << "#include <Canis/App.hpp>\n";
            source << "#include <Canis/ConfigHelper.hpp>\n\n";

            AppendNamespaceOpens(source, pathParts);

            const std::string sourceIndent = MakeIndent(classIndentLevel);
            const std::string blockIndent = MakeIndent(classIndentLevel + 4);

            source << sourceIndent << "namespace\n";
            source << sourceIndent << "{\n";
            if (_scriptType == GameScriptType::ScriptableEntity)
                source << blockIndent << "Canis::ScriptConf scriptConf = {};\n";
            else if (_scriptType == GameScriptType::Component)
                source << blockIndent << "Canis::ComponentConf componentConf = {};\n";
            else
                source << blockIndent << "Canis::SystemConf systemConf = {};\n";
            source << sourceIndent << "}\n\n";
            if (_scriptType == GameScriptType::ScriptableEntity)
            {
                std::string configMacro = "DEFAULT_CONFIG(scriptConf, " + scriptSymbolName + ");";
                if (!_requiredComponents.empty())
                    configMacro = "DEFAULT_CONFIG_AND_REQUIRED(scriptConf, " + scriptSymbolName + ", " + JoinStringParts(_requiredComponents, ", ") + ");";

                source << sourceIndent << "void Register" << className << "Script(Canis::App& _app)\n";
                source << sourceIndent << "{\n";
                source << blockIndent << "// REGISTER_PROPERTY(scriptConf, " << scriptSymbolName << ", exampleProperty);\n\n";
                source << blockIndent << configMacro << "\n\n";
                source << blockIndent << "scriptConf.DEFAULT_DRAW_INSPECTOR(" << scriptSymbolName << ");\n\n";
                source << blockIndent << "_app.RegisterScript(scriptConf);\n";
                source << sourceIndent << "}\n\n";
                source << sourceIndent << "DEFAULT_UNREGISTER_SCRIPT(scriptConf, " << className << ")\n\n";
                source << sourceIndent << "void " << className << "::Create() {}\n\n";
                source << sourceIndent << "void " << className << "::Ready() {}\n\n";
                source << sourceIndent << "void " << className << "::Destroy() {}\n\n";
                source << sourceIndent << "void " << className << "::Update(float _dt) {}\n";
            }
            else if (_scriptType == GameScriptType::Component)
            {
                std::string configMacro = "DEFAULT_COMPONENT_CONFIG(componentConf, " + scriptSymbolName + ");";
                if (!_requiredComponents.empty())
                    configMacro = "DEFAULT_COMPONENT_CONFIG_AND_REQUIRED(componentConf, " + scriptSymbolName + ", " + JoinStringParts(_requiredComponents, ", ") + ");";

                source << sourceIndent << "void Register" << className << "Component(Canis::App& _app)\n";
                source << sourceIndent << "{\n";
                source << blockIndent << "// REGISTER_PROPERTY(componentConf, " << scriptSymbolName << ", exampleProperty);\n\n";
                source << blockIndent << configMacro << "\n\n";
                source << blockIndent << "componentConf.DEFAULT_DRAW_COMPONENT_INSPECTOR(" << scriptSymbolName << ");\n\n";
                source << blockIndent << "_app.RegisterComponent(componentConf);\n";
                source << sourceIndent << "}\n\n";
                source << sourceIndent << "DEFAULT_UNREGISTER_COMPONENT(componentConf, " << className << ")\n";
            }
            else
            {
                source << sourceIndent << "void Register" << className << "System(Canis::App& _app)\n";
                source << sourceIndent << "{\n";
                source << blockIndent << "DEFAULT_SYSTEM_CONFIG(systemConf, " << scriptSymbolName << ", Canis::SystemPipeline::Update);\n";
                source << blockIndent << "_app.RegisterSystem(systemConf);\n";
                source << sourceIndent << "}\n\n";
                source << sourceIndent << "DEFAULT_UNREGISTER_SYSTEM(systemConf, " << className << ")\n\n";
                source << sourceIndent << "void " << className << "::Create() {}\n\n";
                source << sourceIndent << "void " << className << "::Ready() {}\n\n";
                source << sourceIndent << "void " << className << "::Update(entt::registry &, float) {}\n\n";
                source << sourceIndent << "void " << className << "::OnDestroy() {}\n";
            }

            if (!pathParts.empty())
                AppendNamespaceCloses(source, pathParts);

            {
                std::ofstream headerFile(headerPath);
                if (!headerFile.is_open())
                {
                    _outError = "Failed to write header file.";
                    return false;
                }
                headerFile << header.str();
            }

            {
                std::ofstream sourceFile(sourcePath);
                if (!sourceFile.is_open())
                {
                    _outError = "Failed to write source file.";
                    return false;
                }
                sourceFile << source.str();
            }

            _outHeaderPath = headerPath.string();
            _outSourcePath = sourcePath.string();
            return true;
        }

        std::filesystem::path FindGameCodeRoot()
        {
            namespace fs = std::filesystem;

            auto isGameCodeRoot = [](const fs::path &_path) -> bool
            {
                std::error_code ec;
                return fs::is_directory(_path / "game" / "include", ec) &&
                    fs::is_directory(_path / "game" / "src", ec);
            };

            std::vector<fs::path> startPaths = { fs::current_path() };
            if (const char *basePath = SDL_GetBasePath())
                startPaths.emplace_back(basePath);

            for (fs::path currentPath : startPaths)
            {
                while (!currentPath.empty())
                {
                    if (isGameCodeRoot(currentPath))
                        return currentPath;

                    if (currentPath == currentPath.root_path())
                        break;

                    currentPath = currentPath.parent_path();
                }
            }

            return {};
        }

        struct GameCodeBuildConfigInfo
        {
            std::string singleConfigType = "";
            std::string multiConfigTypes = "";
        };

        GameCodeBuildConfigInfo ReadGameCodeBuildConfigInfo(const std::filesystem::path &_buildDir)
        {
            namespace fs = std::filesystem;

            GameCodeBuildConfigInfo info = {};
            const fs::path cachePath = _buildDir / "CMakeCache.txt";
            std::ifstream cache(cachePath);
            if (!cache.is_open())
                return info;

            std::string line = "";
            while (std::getline(cache, line))
            {
                if (line.rfind("CMAKE_BUILD_TYPE:STRING=", 0) == 0)
                {
                    info.singleConfigType = line.substr(std::string("CMAKE_BUILD_TYPE:STRING=").size());
                }
                else if (line.rfind("CMAKE_CONFIGURATION_TYPES:STRING=", 0) == 0)
                {
                    info.multiConfigTypes = line.substr(std::string("CMAKE_CONFIGURATION_TYPES:STRING=").size());
                }
            }

            return info;
        }

        bool BuildGameCodeForReload(
            const std::filesystem::path &_buildDir,
            std::string &_outCommand,
            std::string &_outError,
            int &_outExitCode,
            const std::function<void(const std::string&)> &_onOutput = nullptr)
        {
#if defined(__EMSCRIPTEN__)
            (void)_buildDir;
            (void)_outCommand;
            (void)_outError;
            (void)_outExitCode;
            (void)_onOutput;
            return true;
#else
            _outCommand = "cmake --build \"" + _buildDir.generic_string() + "\" --target GameCode --parallel --";
            _outExitCode = -1;

            if (_onOutput != nullptr)
            {
                const std::string pipedCommand = _outCommand + " 2>&1";
    #if defined(_WIN32)
                FILE *pipe = _popen(pipedCommand.c_str(), "r");
    #else
                FILE *pipe = popen(pipedCommand.c_str(), "r");
    #endif
                if (pipe == nullptr)
                {
                    _outError = "Failed to start build command.";
                    return false;
                }

                char buffer[1024] = {};
                while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
                    _onOutput(std::string(buffer));

    #if defined(_WIN32)
                _outExitCode = _pclose(pipe);
    #else
                _outExitCode = pclose(pipe);
    #endif
            }
            else
            {
                _outExitCode = std::system(_outCommand.c_str());
            }

            if (_outExitCode != 0)
            {
                _outError = "Build command failed with exit code " + std::to_string(_outExitCode) + ".";
                return false;
            }

            return true;
#endif
        }

        bool UnloadGameCodeForReload(GameCodeObject *_gameCodeObject, App *_app, std::string &_outError, std::string &_outBackupPath)
        {
#if defined(__EMSCRIPTEN__)
            (void)_gameCodeObject;
            (void)_app;
            (void)_outError;
            (void)_outBackupPath;
            return true;
#else
            _outBackupPath.clear();

            if (_gameCodeObject == nullptr)
            {
                _outError = "GameCodeObject was null.";
                return false;
            }

            GameCodeObjectShutdownFunction(_gameCodeObject, _app);

            if (_gameCodeObject->sharedObjectHandle != nullptr)
            {
                SDL_UnloadObject(_gameCodeObject->sharedObjectHandle);
                _gameCodeObject->sharedObjectHandle = nullptr;
            }

            if (_gameCodeObject->path != nullptr && _gameCodeObject->path[0] != '\0')
            {
                std::error_code ec;
                const std::filesystem::path sharedObjectPath(_gameCodeObject->path);
                if (std::filesystem::exists(sharedObjectPath))
                {
                    const std::filesystem::path backupPath = sharedObjectPath.string() + ".reload.bak";
                    std::filesystem::remove(backupPath, ec);
                    ec.clear();
                    std::filesystem::rename(sharedObjectPath, backupPath, ec);
                    if (ec)
                    {
                        _outError = "Failed to move old game shared library aside before rebuild: " + ec.message();
                        return false;
                    }

                    _outBackupPath = backupPath.string();
                }
            }

            _gameCodeObject->gameData = nullptr;
            _gameCodeObject->GameInitFunction = nullptr;
            _gameCodeObject->GameUpdateFunction = nullptr;
            _gameCodeObject->GameShutdownFunction = nullptr;
            return true;
#endif
        }

        bool LoadGameCodeAfterReload(GameCodeObject *_gameCodeObject, App *_app, std::string &_outError)
        {
#if defined(__EMSCRIPTEN__)
            (void)_gameCodeObject;
            (void)_app;
            (void)_outError;
            return true;
#else
            if (_gameCodeObject == nullptr)
            {
                _outError = "GameCodeObject was null.";
                return false;
            }

            const char *path = _gameCodeObject->path;
            if (path == nullptr || path[0] == '\0')
            {
                _outError = "Game code shared library path is empty.";
                return false;
            }

            *_gameCodeObject = GameCodeObjectInit(path);
            if (_gameCodeObject->sharedObjectHandle == nullptr)
            {
                _outError = SDL_GetError();
                if (_outError.empty())
                    _outError = "Failed to reload game shared library.";
                return false;
            }

            _app->BeginGameCodeRegistration();
            GameCodeObjectInitFunction(_gameCodeObject, _app);
            _app->EndGameCodeRegistration();
            return true;
#endif
        }

        std::filesystem::path MakeUniqueDirectoryPath(const std::filesystem::path &_parentPath, const std::string &_baseName)
        {
            namespace fs = std::filesystem;

            fs::path candidatePath = _parentPath / _baseName;
            int index = 1;
            while (fs::exists(candidatePath))
            {
                candidatePath = _parentPath / (_baseName + "_" + std::to_string(index));
                ++index;
            }

            return candidatePath;
        }

        bool ReplaceAll(std::string &_value, const std::string &_from, const std::string &_to)
        {
            if (_from.empty())
                return false;

            bool replaced = false;
            size_t position = 0;
            while ((position = _value.find(_from, position)) != std::string::npos)
            {
                _value.replace(position, _from.length(), _to);
                position += _to.length();
                replaced = true;
            }

            return replaced;
        }

        bool IsGameCodeFilePath(const std::filesystem::path &_path)
        {
            std::string extension = _path.extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });

            return extension == ".hpp" ||
                extension == ".h" ||
                extension == ".cpp" ||
                extension == ".cc" ||
                extension == ".cxx";
        }

        bool TryMakeLexicalRelative(
            const std::filesystem::path &_path,
            const std::filesystem::path &_basePath,
            std::filesystem::path &_outRelativePath)
        {
            namespace fs = std::filesystem;

            const fs::path relativePath = _path.lexically_normal().lexically_relative(_basePath.lexically_normal());
            if (relativePath.empty())
                return false;

            for (const fs::path &part : relativePath)
            {
                if (part == "..")
                    return false;
            }

            _outRelativePath = relativePath;
            return true;
        }

        bool UpdateScriptIncludePathPrefixes(
            const std::filesystem::path &_includeRoot,
            const std::filesystem::path &_sourceRoot,
            const std::filesystem::path &_oldRelativeDir,
            const std::filesystem::path &_newRelativeDir,
            std::string &_outError)
        {
            namespace fs = std::filesystem;

            const std::string oldPrefix = _oldRelativeDir.generic_string() + "/";
            const std::string newPrefix = _newRelativeDir.generic_string() + "/";

            const std::vector<std::pair<std::string, std::string>> replacements =
            {
                { "#include <" + oldPrefix, "#include <" + newPrefix },
                { "#include \"" + oldPrefix, "#include \"" + newPrefix }
            };

            const std::vector<fs::path> roots = { _includeRoot, _sourceRoot };
            for (const fs::path &root : roots)
            {
                std::error_code iterEc;
                fs::recursive_directory_iterator it(root, iterEc);
                const fs::recursive_directory_iterator end;
                if (iterEc)
                {
                    _outError = iterEc.message();
                    return false;
                }

                while (it != end)
                {
                    const fs::path path = it->path();
                    if (it->is_regular_file(iterEc) && IsGameCodeFilePath(path))
                    {
                        std::ifstream input(path, std::ios::binary);
                        if (!input.is_open())
                        {
                            _outError = "Failed to read " + path.string();
                            return false;
                        }

                        std::ostringstream stream;
                        stream << input.rdbuf();
                        std::string contents = stream.str();

                        bool changed = false;
                        for (const auto &replacement : replacements)
                            changed |= ReplaceAll(contents, replacement.first, replacement.second);

                        if (changed)
                        {
                            std::ofstream output(path, std::ios::binary | std::ios::trunc);
                            if (!output.is_open())
                            {
                                _outError = "Failed to write " + path.string();
                                return false;
                            }

                            output << contents;
                        }
                    }

                    iterEc.clear();
                    it.increment(iterEc);
                    if (iterEc)
                    {
                        _outError = iterEc.message();
                        return false;
                    }
                }
            }

            return true;
        }

        bool RenameScriptFolderPair(
            const std::filesystem::path &_includeRoot,
            const std::filesystem::path &_sourceRoot,
            const std::filesystem::path &_includeFolderPath,
            const std::string &_newFolderName,
            std::filesystem::path &_outNewIncludeFolderPath,
            std::string &_outError)
        {
            namespace fs = std::filesystem;

            if (!IsValidCppIdentifier(_newFolderName))
            {
                _outError = "Folder name must be a valid C++ identifier.";
                return false;
            }

            std::error_code ec;
            if (!fs::is_directory(_includeFolderPath, ec))
            {
                _outError = "Script folder does not exist.";
                return false;
            }

            const fs::path oldIncludeFolderPath = _includeFolderPath;
            const fs::path newIncludeFolderPath = oldIncludeFolderPath.parent_path() / _newFolderName;
            if (newIncludeFolderPath == oldIncludeFolderPath)
            {
                _outNewIncludeFolderPath = oldIncludeFolderPath;
                return true;
            }

            if (fs::exists(newIncludeFolderPath, ec))
            {
                _outError = "A script folder with that name already exists.";
                return false;
            }

            std::error_code relativeEc;
            const fs::path oldRelativeDir = fs::relative(oldIncludeFolderPath, _includeRoot, relativeEc);
            if (relativeEc || oldRelativeDir.empty() || oldRelativeDir == ".")
            {
                _outError = "Could not resolve script folder relative path.";
                return false;
            }

            const fs::path newRelativeDir = oldRelativeDir.parent_path() / _newFolderName;
            const fs::path oldSourceFolderPath = _sourceRoot / oldRelativeDir;
            const fs::path newSourceFolderPath = _sourceRoot / newRelativeDir;

            const bool hasSourceFolder = fs::is_directory(oldSourceFolderPath, ec);
            if (hasSourceFolder && fs::exists(newSourceFolderPath, ec))
            {
                _outError = "A matching source folder with that name already exists.";
                return false;
            }

            ec.clear();
            fs::rename(oldIncludeFolderPath, newIncludeFolderPath, ec);
            if (ec)
            {
                _outError = "Failed to rename script folder: " + ec.message();
                return false;
            }

            if (hasSourceFolder)
            {
                ec.clear();
                fs::rename(oldSourceFolderPath, newSourceFolderPath, ec);
                if (ec)
                {
                    std::error_code rollbackEc;
                    fs::rename(newIncludeFolderPath, oldIncludeFolderPath, rollbackEc);
                    _outError = "Failed to rename matching source folder: " + ec.message();
                    if (rollbackEc)
                        _outError += " Also failed to restore include folder: " + rollbackEc.message();
                    return false;
                }
            }

            std::string includeUpdateError = "";
            if (!UpdateScriptIncludePathPrefixes(_includeRoot, _sourceRoot, oldRelativeDir, newRelativeDir, includeUpdateError))
                Debug::Warning("Renamed script folder, but failed to update include paths: %s", includeUpdateError.c_str());

            _outNewIncludeFolderPath = newIncludeFolderPath;
            return true;
        }

        std::string MakeUniqueScriptTarget(
            const std::filesystem::path &_includeRoot,
            const std::filesystem::path &_currentDir,
            const std::filesystem::path &_sourceRoot,
            const std::string &_baseClassName)
        {
            namespace fs = std::filesystem;

            std::error_code ec;
            fs::path relativeDir = fs::relative(_currentDir, _includeRoot, ec);
            if (ec)
                relativeDir.clear();

            std::string className = _baseClassName;
            int index = 1;

            while (true)
            {
                const fs::path headerPath = _currentDir / (className + ".hpp");
                const fs::path sourcePath = _sourceRoot / relativeDir / (className + ".cpp");

                if (!fs::exists(headerPath) && !fs::exists(sourcePath))
                    break;

                className = _baseClassName + "_" + std::to_string(index);
                ++index;
            }

            if (relativeDir.empty() || relativeDir == ".")
                return className;

            return relativeDir.generic_string() + "/" + className;
        }

        int PlaceRenameCursorAtEnd(ImGuiInputTextCallbackData *_data)
        {
            bool *shouldPlaceCursor = static_cast<bool *>(_data->UserData);
            if (shouldPlaceCursor != nullptr && *shouldPlaceCursor)
            {
                _data->CursorPos = _data->BufTextLen;
                _data->SelectionStart = _data->BufTextLen;
                _data->SelectionEnd = _data->BufTextLen;
                *shouldPlaceCursor = false;
            }

            return 0;
        }

        bool ShouldHideScriptBrowserEntry(const std::filesystem::path &_path)
        {
            const std::string filename = _path.filename().string();
            return filename == ".DS_Store" ||
                filename == "GamePCH.hpp" ||
                filename == "RegisterScripts.generated.hpp";
        }

        std::filesystem::path GetPairedScriptPath(
            const std::filesystem::path &_includeRoot,
            const std::filesystem::path &_sourceRoot,
            const std::filesystem::path &_filePath)
        {
            namespace fs = std::filesystem;

            fs::path pairedPath = {};
            std::string extension = _filePath.extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });

            if (extension == ".hpp")
            {
                std::error_code relativeEc;
                const fs::path relativeHeaderPath = fs::relative(_filePath, _includeRoot, relativeEc);
                if (!relativeEc && !relativeHeaderPath.empty())
                {
                    pairedPath = _sourceRoot / relativeHeaderPath.parent_path() /
                        (relativeHeaderPath.stem().string() + ".cpp");
                }
            }
            else if (extension == ".cpp")
            {
                std::error_code relativeEc;
                const fs::path relativeSourcePath = fs::relative(_filePath, _sourceRoot, relativeEc);
                if (!relativeEc && !relativeSourcePath.empty())
                {
                    pairedPath = _includeRoot / relativeSourcePath.parent_path() /
                        (relativeSourcePath.stem().string() + ".hpp");
                }
            }

            return pairedPath;
        }

        std::filesystem::path GetScriptHeaderPathFromName(
            const std::filesystem::path &_includeRoot,
            const std::string &_scriptName)
        {
            namespace fs = std::filesystem;

            if (_scriptName.empty())
                return {};

            const std::string normalizedName = NormalizeScriptTarget(_scriptName);
            std::vector<std::string> nameParts = SplitScriptTarget(normalizedName);
            if (nameParts.empty())
                return {};

            fs::path relativePath;
            for (std::size_t i = 0; i + 1 < nameParts.size(); ++i)
                relativePath /= nameParts[i];

            relativePath /= nameParts.back() + ".hpp";
            return _includeRoot / relativePath;
        }

        bool OpenScriptFromInspector(const ScriptConf &_conf)
        {
            namespace fs = std::filesystem;

            const fs::path gameCodeRoot = FindGameCodeRoot();
            if (gameCodeRoot.empty())
                return false;

            const fs::path includeRoot = gameCodeRoot / "game" / "include";
            const fs::path sourceRoot = gameCodeRoot / "game" / "src";
            const fs::path headerPath = GetScriptHeaderPathFromName(includeRoot, _conf.name);
            if (headerPath.empty() || !fs::exists(headerPath))
                return false;

            OpenInVSCode(headerPath.string());

            const fs::path pairedPath = GetPairedScriptPath(includeRoot, sourceRoot, headerPath);
            if (!pairedPath.empty() && pairedPath != headerPath && fs::exists(pairedPath))
                OpenInVSCode(pairedPath.string());

            return true;
        }

        bool DeleteScriptPair(
            const std::filesystem::path &_includeRoot,
            const std::filesystem::path &_sourceRoot,
            const std::filesystem::path &_filePath,
            std::string &_error)
        {
            namespace fs = std::filesystem;

            std::vector<fs::path> pathsToDelete = { _filePath };
            const fs::path pairedPath = GetPairedScriptPath(_includeRoot, _sourceRoot, _filePath);

            if (!pairedPath.empty() && pairedPath != _filePath && fs::exists(pairedPath))
                pathsToDelete.push_back(pairedPath);

            for (const fs::path &pathToDelete : pathsToDelete)
            {
                std::error_code removeEc;
                const bool removed = fs::remove(pathToDelete, removeEc);
                if (removeEc || !removed)
                {
                    _error = removeEc ? removeEc.message() : "File could not be removed.";
                    return false;
                }
            }

            return true;
        }

        Shader &GetDebugLineShader()
        {
            static Shader debugLineShader("assets/shaders/debug_line.vs", "assets/shaders/debug_line.fs");
            return debugLineShader;
        }

        Shader &GetModelPickingShader()
        {
            static Shader modelPickingShader("assets/shaders/editor_model_pick.vs", "assets/shaders/editor_model_pick.fs");
            return modelPickingShader;
        }

        Color EncodeEntityIdColor(const uint32_t _entityId)
        {
            return Color(
                static_cast<float>(_entityId & 0xFFu) / 255.0f,
                static_cast<float>((_entityId >> 8u) & 0xFFu) / 255.0f,
                static_cast<float>((_entityId >> 16u) & 0xFFu) / 255.0f,
                static_cast<float>((_entityId >> 24u) & 0xFFu) / 255.0f);
        }

        uint32_t DecodeEntityIdColor(const unsigned char _pixel[4])
        {
            return static_cast<uint32_t>(_pixel[0]) |
                (static_cast<uint32_t>(_pixel[1]) << 8u) |
                (static_cast<uint32_t>(_pixel[2]) << 16u) |
                (static_cast<uint32_t>(_pixel[3]) << 24u);
        }

        void ReloadEditorShaders()
        {
            AssetManager::ReloadLoadedShaders();

            Shader &debugLineShader = GetDebugLineShader();
            debugLineShader.Compile("assets/shaders/debug_line.vs", "assets/shaders/debug_line.fs");
            debugLineShader.Link();

            Shader &modelPickingShader = GetModelPickingShader();
            modelPickingShader.Compile("assets/shaders/editor_model_pick.vs", "assets/shaders/editor_model_pick.fs");
            modelPickingShader.Link();
        }

        PostProcessResult ApplyScenePostProcess(
            Scene *_scene,
            unsigned int _sourceFramebuffer,
            unsigned int _sourceColorTexture,
            unsigned int _sourceDepthTexture,
            int _width,
            int _height,
            const Matrix4 &_projection,
            RenderTarget *_outputTarget)
        {
            const PostProcessAsset *postProcess = nullptr;
            if (_scene != nullptr)
            {
                const UUID postProcessUUID = _scene->GetEnvironmentPostProcessUUID();
                if ((uint64_t)postProcessUUID != 0)
                {
                    const std::string postProcessPath = AssetManager::GetPath(postProcessUUID);
                    if (postProcessPath != "Path was not found in AssetLibrary")
                        postProcess = AssetManager::GetPostProcess(postProcessPath);
                }
            }

            return ApplyPostProcessChain(
                postProcess,
                _sourceFramebuffer,
                _sourceColorTexture,
                _sourceDepthTexture,
                _width,
                _height,
                _projection,
                _outputTarget);
        }

        constexpr const char* kDefaultImguiIniContents = R"([Window][DockSpaceViewport_11111111]
Size=1920,1057
Collapsed=0

[Window][Debug##Default]
Pos=60,60
Size=400,400
Collapsed=0

[Window][Dear ImGui Demo]
Pos=650,20
Size=550,680
Collapsed=0

[Window][Hello, world!]
Pos=30,465
Size=339,180
Collapsed=0

[Window][Another Window]
Pos=70,430
Size=198,71
Collapsed=0

[Window][Dear ImGui Style Editor]
Pos=60,60
Size=353,794
Collapsed=0

[Window][Example: Log]
Pos=60,60
Size=500,400
Collapsed=0

[Window][Example: Simple layout]
Pos=60,60
Size=500,440
Collapsed=0

[Window][MainMenuBar]
ViewportPos=0,0
ViewportId=0xDFA0D2E0
Size=1920,24
Collapsed=0

[Window][Game]
Pos=0,44
Size=687,320
Collapsed=0
DockId=0x0000000A,0

[Window][Hierarchy]
Pos=1329,0
Size=275,617
Collapsed=0
DockId=0x00000004,0

[Window][Inspector]
Pos=897,44
Size=383,365
Collapsed=0
DockId=0x0000000C,0

[Window][Assets]
Pos=689,411
Size=591,309
Collapsed=0
DockId=0x00000008,0

[Window][Systems]
Pos=689,411
Size=591,309
Collapsed=0
DockId=0x00000008,1

[Window][Environment]
Pos=897,44
Size=383,365
Collapsed=0
DockId=0x0000000C,1

[Window][ProjectSettings]
Pos=897,44
Size=383,365
Collapsed=0
DockId=0x0000000C,2

[Window][Canis Component Registry]
Pos=320,74
Size=1280,800
Collapsed=0

[Window][Canis Console]
Pos=0,948
Size=1597,109
Collapsed=0
DockId=0x11111111,1

[Window][Text Texture Preview]
Pos=534,157
Size=851,743
Collapsed=0

[Window][texture]
Pos=34,209
Size=1819,619
Collapsed=0

[Window][MainDockspace]
Pos=0,44
Size=1280,676
Collapsed=0

[Window][Scene]
Pos=0,366
Size=687,354
Collapsed=0
DockId=0x00000007,0

[Window][ShaderGraph]
Pos=0,44
Size=687,320
Collapsed=0
DockId=0x0000000A,1

[Window][Canis Editor]
Size=1327,52
Collapsed=0
DockId=0x00000009,0

[Window][###Hierarchy]
Pos=689,44
Size=206,365
Collapsed=0
DockId=0x0000000B,0

[Window][Animation]
Pos=0,44
Size=687,320
Collapsed=0
DockId=0x0000000A,3

[Window][Animator]
Pos=0,44
Size=687,320
Collapsed=0
DockId=0x0000000A,2

[Window][Scripts]
Pos=689,411
Size=591,309
Collapsed=0
DockId=0x00000008,2

[Table][0xA0A1C938,2]
RefScale=18
Column 0  Width=132
Column 1  Weight=1.0000

[Table][0x80D23677,2]
RefScale=18
Column 0  Width=178
Column 1  Weight=1.0000

[Table][0x80EEBDF8,4]
Column 0  Weight=1.0000
Column 1  Weight=1.0000
Column 2  Weight=1.0000
Column 3  Weight=1.0000

[Docking][Data]
DockSpace         ID=0x11111111 Pos=0,25 Size=1920,1032 CentralNode=1 Selected=0x79A00B04
DockSpace         ID=0x49B9F6FE Window=0x1C358F53 Pos=0,44 Size=1280,676 Split=X Selected=0xE601B12F
  DockNode        ID=0x00000001 Parent=0x49B9F6FE SizeRef=1327,1142 Split=Y Selected=0xE601B12F
    DockNode      ID=0x00000006 Parent=0x00000001 SizeRef=1327,320 Split=Y Selected=0xE601B12F
      DockNode    ID=0x00000009 Parent=0x00000006 SizeRef=1327,52 Selected=0xAD07D0E7
      DockNode    ID=0x0000000A Parent=0x00000006 SizeRef=1327,517 CentralNode=1 Selected=0xD1EB2482
    DockNode      ID=0x00000007 Parent=0x00000001 SizeRef=1327,354 Selected=0xE601B12F
  DockNode        ID=0x00000002 Parent=0x49B9F6FE SizeRef=591,1142 Split=Y Selected=0x73E3D51F
    DockNode      ID=0x00000003 Parent=0x00000002 SizeRef=319,617 Split=X Selected=0x73E3D51F
      DockNode    ID=0x00000004 Parent=0x00000003 SizeRef=275,617 Selected=0xBABDAE5E
      DockNode    ID=0x00000005 Parent=0x00000003 SizeRef=314,617 Split=X Selected=0x13B2DC32
        DockNode  ID=0x0000000B Parent=0x00000005 SizeRef=206,365 Selected=0x42E466D2
        DockNode  ID=0x0000000C Parent=0x00000005 SizeRef=383,365 Selected=0x36DC96AB
    DockNode      ID=0x00000008 Parent=0x00000002 SizeRef=319,523 Selected=0x42C24103
)";

        void EnsureDefaultImguiIniFile(const std::filesystem::path& _path)
        {
            namespace fs = std::filesystem;

            std::error_code ec;
            if (fs::exists(_path, ec))
                return;

            if (_path.has_parent_path())
                fs::create_directories(_path.parent_path(), ec);

            if (ec)
            {
                Debug::Warning("Failed to create ImGui user settings directory: %s", _path.parent_path().string().c_str());
                return;
            }

            std::ofstream file(_path);
            if (!file.is_open())
            {
                Debug::Warning("Failed to create default ImGui settings file: %s", _path.string().c_str());
                return;
            }

            file << kDefaultImguiIniContents;
        }

        SceneAssetHandle MakeSceneAssetHandleFromPath(const std::string& _path)
        {
            SceneAssetHandle handle = {};
            if (_path.empty())
                return handle;

            handle.path = _path;

            if (MetaFileAsset* meta = AssetManager::GetMetaFile(_path))
            {
                if (meta->type == MetaFileAsset::FileType::SCENE)
                    handle.uuid = meta->uuid;
            }

            return handle;
        }

        ShaderGraphAssetHandle MakeShaderGraphAssetHandleFromPath(const std::string& _path)
        {
            ShaderGraphAssetHandle handle = {};
            if (_path.empty())
                return handle;

            if (MetaFileAsset* meta = AssetManager::GetMetaFile(_path))
            {
                if (meta->type == MetaFileAsset::FileType::SHADERGRAPH)
                {
                    handle.path = meta->path;
                    handle.uuid = meta->uuid;
                }
            }

            return handle;
        }

        AnimationClipAssetHandle MakeAnimationClipAssetHandleFromPath(const std::string& _path)
        {
            AnimationClipAssetHandle handle = {};
            if (_path.empty())
                return handle;

            if (MetaFileAsset* meta = AssetManager::GetMetaFile(_path))
            {
                if (meta->type == MetaFileAsset::FileType::ANIMATIONCLIP)
                {
                    handle.path = meta->path;
                    handle.uuid = meta->uuid;
                }
            }

            return handle;
        }

        AnimatorControllerAssetHandle MakeAnimatorControllerAssetHandleFromPath(const std::string& _path)
        {
            AnimatorControllerAssetHandle handle = {};
            if (_path.empty())
                return handle;

            if (MetaFileAsset* meta = AssetManager::GetMetaFile(_path))
            {
                if (meta->type == MetaFileAsset::FileType::ANIMATORCONTROLLER)
                {
                    handle.path = meta->path;
                    handle.uuid = meta->uuid;
                }
            }

            return handle;
        }

        bool SceneAssetHandleChanged(const SceneAssetHandle& _left, const SceneAssetHandle& _right)
        {
            return _left.uuid != _right.uuid || _left.path != _right.path;
        }

        bool ShaderGraphAssetHandleChanged(const ShaderGraphAssetHandle& _left, const ShaderGraphAssetHandle& _right)
        {
            return _left.uuid != _right.uuid || _left.path != _right.path;
        }

        bool AnimationClipAssetHandleChanged(const AnimationClipAssetHandle& _left, const AnimationClipAssetHandle& _right)
        {
            return _left.uuid != _right.uuid || _left.path != _right.path;
        }

        bool AnimatorControllerAssetHandleChanged(const AnimatorControllerAssetHandle& _left, const AnimatorControllerAssetHandle& _right)
        {
            return _left.uuid != _right.uuid || _left.path != _right.path;
        }

        constexpr float kAnimationEditorFramesPerSecond = 60.0f;
        constexpr float kAnimationEditorKeyTimeEpsilon = 0.0001f;

        int AnimationFrameFromTime(float _time)
        {
            return std::max(0, static_cast<int>(std::round(std::max(_time, 0.0f) * kAnimationEditorFramesPerSecond)));
        }

        float AnimationTimeFromFrame(int _frame)
        {
            return std::max(_frame, 0) / kAnimationEditorFramesPerSecond;
        }

        float NormalizeAnimationSampleTime(float _time, float _length, bool _loop)
        {
            if (_length <= 0.0f)
                return 0.0f;

            float time = _time;
            if (_loop)
            {
                while (time < 0.0f)
                    time += _length;
                while (time >= _length)
                    time -= _length;
                return time;
            }

            return std::clamp(time, 0.0f, _length);
        }

        AnimationValue MakeDefaultAnimationValue(AnimationValueType _type)
        {
            switch (_type)
            {
                case AnimationValueType::FLOAT: return AnimationValue::Float(0.0f);
                case AnimationValueType::INT: return AnimationValue::Int(0);
                case AnimationValueType::BOOL: return AnimationValue::Bool(false);
                case AnimationValueType::VEC2: return AnimationValue::Vec2(Vector2(0.0f));
                case AnimationValueType::VEC3: return AnimationValue::Vec3(Vector3(0.0f));
                case AnimationValueType::VEC4: return AnimationValue::Vec4(Vector4(0.0f));
                case AnimationValueType::NONE:
                default:
                    return {};
            }
        }

        AnimationValue MakeDefaultAnimatorParameterValue(AnimatorParameterType _type)
        {
            switch (_type)
            {
                case AnimatorParameterType::INT: return AnimationValue::Int(0);
                case AnimatorParameterType::BOOL:
                case AnimatorParameterType::TRIGGER: return AnimationValue::Bool(false);
                case AnimatorParameterType::FLOAT:
                default: return AnimationValue::Float(0.0f);
            }
        }

        float GetAnimationValueComponent(const AnimationValue& _value, int _componentIndex)
        {
            switch (_value.type)
            {
                case AnimationValueType::FLOAT:
                    return _value.AsFloat();
                case AnimationValueType::INT:
                    return static_cast<float>(_value.AsInt());
                case AnimationValueType::BOOL:
                    return _value.AsBool() ? 1.0f : 0.0f;
                case AnimationValueType::VEC2:
                case AnimationValueType::VEC3:
                case AnimationValueType::VEC4:
                {
                    const int index = std::clamp(_componentIndex, 0, 3);
                    return _value.vector[index];
                }
                case AnimationValueType::NONE:
                default:
                    return 0.0f;
            }
        }

        void SetAnimationValueComponent(AnimationValue& _value, int _componentIndex, float _componentValue)
        {
            switch (_value.type)
            {
                case AnimationValueType::FLOAT:
                    _value = AnimationValue::Float(_componentValue);
                    break;
                case AnimationValueType::INT:
                    _value = AnimationValue::Int(static_cast<int>(std::round(_componentValue)));
                    break;
                case AnimationValueType::BOOL:
                    _value = AnimationValue::Bool(_componentValue >= 0.5f);
                    break;
                case AnimationValueType::VEC2:
                case AnimationValueType::VEC3:
                case AnimationValueType::VEC4:
                {
                    const int index = std::clamp(_componentIndex, 0, 3);
                    _value.vector[index] = _componentValue;
                    break;
                }
                case AnimationValueType::NONE:
                default:
                    break;
            }
        }

        const char* GetAnimationValueComponentName(AnimationValueType _type, int _componentIndex)
        {
            static const char* scalarComponent = "value";
            static const char* xyzwComponents[] = {"x", "y", "z", "w"};

            if (_type == AnimationValueType::FLOAT || _type == AnimationValueType::INT || _type == AnimationValueType::BOOL)
                return scalarComponent;

            if (_componentIndex >= 0 && _componentIndex < 4)
                return xyzwComponents[_componentIndex];

            return scalarComponent;
        }

        ImU32 GetAnimationCurveColor(int _componentIndex)
        {
            static constexpr ImU32 colors[] = {
                IM_COL32(255, 96, 96, 255),
                IM_COL32(96, 220, 96, 255),
                IM_COL32(96, 160, 255, 255),
                IM_COL32(240, 210, 96, 255),
            };

            return colors[std::clamp(_componentIndex, 0, 3)];
        }

        void SortAnimationTrackKeys(AnimationTrack &_track)
        {
            std::sort(_track.keys.begin(), _track.keys.end(), [](const AnimationKeyframe &_left, const AnimationKeyframe &_right)
            {
                return _left.time < _right.time;
            });
        }

        void RefreshAnimationClipLength(AnimationClipAsset &_clip)
        {
            float maxTrackTime = 0.0f;
            for (const AnimationTrack &track : _clip.tracks)
            {
                if (!track.keys.empty())
                    maxTrackTime = std::max(maxTrackTime, track.keys.back().time);
            }

            float maxEventTime = 0.0f;
            for (const AnimationEvent &event : _clip.events)
                maxEventTime = std::max(maxEventTime, event.time);

            _clip.length = std::max({_clip.length, maxTrackTime, maxEventTime, 0.0f});
        }

        AnimationTrack* FindAnimationTrack(
            AnimationClipAsset &_clip,
            const std::string &_path,
            const std::string &_component,
            const std::string &_property)
        {
            for (AnimationTrack &track : _clip.tracks)
            {
                if (track.path == _path &&
                    track.component == _component &&
                    track.property == _property)
                {
                    return &track;
                }
            }

            return nullptr;
        }

        AnimationTrack* FindOrCreateAnimationTrack(
            AnimationClipAsset &_clip,
            const std::string &_path,
            const std::string &_component,
            const std::string &_property,
            AnimationValueType _type,
            AnimationInterpolation _interpolation)
        {
            if (AnimationTrack *track = FindAnimationTrack(_clip, _path, _component, _property))
            {
                track->type = _type;
                track->interpolation = _interpolation;
                return track;
            }

            AnimationTrack track = {};
            track.path = _path;
            track.component = _component;
            track.property = _property;
            track.type = _type;
            track.interpolation = _interpolation;
            _clip.tracks.push_back(track);
            return &_clip.tracks.back();
        }

        std::string BuildAnimationTrackLabel(const AnimationTrack &_track)
        {
            std::string label = _track.component + "." + _track.property;
            if (!_track.path.empty())
                label += "  [" + _track.path + "]";
            return label;
        }

        struct AnimationPropertyCandidate
        {
            Entity* entity = nullptr;
            std::string path = "";
            std::string component = "";
            std::string property = "";
            AnimationValueType type = AnimationValueType::NONE;
            AnimationInterpolation interpolation = AnimationInterpolation::LINEAR;
            std::string label = "";
        };

        void CollectAnimationPropertyCandidatesRecursive(
            App &_app,
            Entity &_root,
            Entity &_current,
            std::vector<AnimationPropertyCandidate> &_outCandidates)
        {
            std::string relativePath = {};
            if (&_root != &_current && !BuildAnimationRelativePath(_root, _current, relativePath))
                return;

            const std::string entityLabel = relativePath.empty() ? _current.name : relativePath;
            for (ScriptConf &conf : _app.GetScriptRegistry())
            {
                if (conf.Has == nullptr || conf.Get == nullptr || conf.registry.animationGetters.empty())
                    continue;

                if (!conf.Has(_current))
                    continue;

                for (const std::string &propertyName : conf.registry.propertyOrder)
                {
                    auto typeIt = conf.registry.animationTypes.find(propertyName);
                    auto interpolationIt = conf.registry.animationInterpolations.find(propertyName);
                    if (typeIt == conf.registry.animationTypes.end() ||
                        interpolationIt == conf.registry.animationInterpolations.end())
                    {
                        continue;
                    }

                    AnimationPropertyCandidate candidate = {};
                    candidate.entity = &_current;
                    candidate.path = relativePath;
                    candidate.component = conf.name;
                    candidate.property = propertyName;
                    candidate.type = typeIt->second;
                    candidate.interpolation = interpolationIt->second;
                    candidate.label = entityLabel + " / " + conf.name + "." + propertyName;
                    _outCandidates.push_back(candidate);
                }
            }

            for (Entity *child : GetAnimationChildren(_current))
            {
                if (child != nullptr)
                    CollectAnimationPropertyCandidatesRecursive(_app, _root, *child, _outCandidates);
            }
        }

        namespace GraphEditor = CanisGraphEditor;

        constexpr GraphEditor::NodeIndex kInvalidAnimatorGraphNodeIndex = static_cast<GraphEditor::NodeIndex>(-1);
        constexpr float kAnimatorGraphNodeWidth = 260.0f;
        constexpr float kAnimatorGraphNodeHeight = 126.0f;

        enum AnimatorGraphTemplateIndex : GraphEditor::TemplateIndex
        {
            AnimatorGraphTemplate_State = 0,
            AnimatorGraphTemplate_EntryState,
            AnimatorGraphTemplate_ActiveState,
            AnimatorGraphTemplate_ActiveEntryState,
            AnimatorGraphTemplate_Count
        };

        struct AnimatorGraphLinkInfo
        {
            GraphEditor::Link graphLink = {};
            int fromStateIndex = -1;
            int transitionIndex = -1;
        };

        struct AnimatorGraphViewState
        {
            GraphEditor::Options options = {};
            GraphEditor::ViewState viewState = {};
            ImVec2 contextMousePos = ImVec2(0.0f, 0.0f);
        };

        std::unordered_map<std::string, AnimatorGraphViewState> g_animatorGraphViewStates = {};

        bool IsAnimatorControllerAssetPath(const std::string &_path, MetaFileAsset **_outMeta = nullptr)
        {
            MetaFileAsset *meta = AssetManager::GetMetaFile(_path);
            if (meta == nullptr || meta->type != MetaFileAsset::FileType::ANIMATORCONTROLLER)
                return false;

            if (_outMeta != nullptr)
                *_outMeta = meta;
            return true;
        }

        int FindAnimatorStateIndex(const AnimatorControllerAsset &_controller, const std::string &_stateName)
        {
            for (std::size_t i = 0; i < _controller.states.size(); ++i)
            {
                if (_controller.states[i].name == _stateName)
                    return static_cast<int>(i);
            }

            return -1;
        }

        std::string MakeUniqueAnimatorStateName(const AnimatorControllerAsset &_controller, const std::string &_baseName)
        {
            std::string baseName = _baseName.empty() ? "State" : _baseName;
            std::string candidate = baseName;
            int suffix = 1;
            while (FindAnimatorStateIndex(_controller, candidate) >= 0)
            {
                candidate = baseName + " " + std::to_string(suffix);
                ++suffix;
            }

            return candidate;
        }

        Vector2 GetDefaultAnimatorStatePosition(std::size_t _index)
        {
            const float column = static_cast<float>(_index % 3);
            const float row = static_cast<float>(_index / 3);
            return Vector2(70.0f + (column * 320.0f), 90.0f + (row * 190.0f));
        }

        Vector2 GetNextAnimatorStatePosition(const AnimatorControllerAsset &_controller)
        {
            if (_controller.states.empty())
                return GetDefaultAnimatorStatePosition(0);

            float maxX = _controller.states.front().editorPosition.x;
            float minY = _controller.states.front().editorPosition.y;
            for (const AnimatorState &state : _controller.states)
            {
                maxX = std::max(maxX, state.editorPosition.x);
                minY = std::min(minY, state.editorPosition.y);
            }

            return Vector2(maxX + 320.0f, minY);
        }

        bool EnsureAnimatorStateLayout(AnimatorControllerAsset &_controller)
        {
            if (_controller.states.empty())
                return false;

            bool hasMeaningfulPosition = false;
            for (const AnimatorState &state : _controller.states)
            {
                if (std::fabs(state.editorPosition.x) > 0.001f || std::fabs(state.editorPosition.y) > 0.001f)
                {
                    hasMeaningfulPosition = true;
                    break;
                }
            }

            if (hasMeaningfulPosition)
                return false;

            for (std::size_t i = 0; i < _controller.states.size(); ++i)
                _controller.states[i].editorPosition = GetDefaultAnimatorStatePosition(i);

            return true;
        }

        void RenameAnimatorParameterReferences(
            AnimatorControllerAsset &_controller,
            const std::string &_oldName,
            const std::string &_newName)
        {
            if (_oldName.empty() || _oldName == _newName)
                return;

            for (AnimatorState &state : _controller.states)
            {
                for (AnimatorTransition &transition : state.transitions)
                {
                    for (AnimatorTransitionCondition &condition : transition.conditions)
                    {
                        if (condition.parameter == _oldName)
                            condition.parameter = _newName;
                    }
                }
            }
        }

        void ResetAnimatorParameterReferenceValues(
            AnimatorControllerAsset &_controller,
            const std::string &_parameterName,
            AnimatorParameterType _parameterType)
        {
            for (AnimatorState &state : _controller.states)
            {
                for (AnimatorTransition &transition : state.transitions)
                {
                    for (AnimatorTransitionCondition &condition : transition.conditions)
                    {
                        if (condition.parameter == _parameterName)
                            condition.value = MakeDefaultAnimatorParameterValue(_parameterType);
                    }
                }
            }
        }

        void RemoveAnimatorParameterReferences(
            AnimatorControllerAsset &_controller,
            const std::string &_parameterName)
        {
            for (AnimatorState &state : _controller.states)
            {
                for (AnimatorTransition &transition : state.transitions)
                {
                    transition.conditions.erase(
                        std::remove_if(
                            transition.conditions.begin(),
                            transition.conditions.end(),
                            [&](const AnimatorTransitionCondition &_condition)
                            {
                                return _condition.parameter == _parameterName;
                            }),
                        transition.conditions.end());
                }
            }
        }

        void RenameAnimatorStateReferences(
            AnimatorControllerAsset &_controller,
            const std::string &_oldName,
            const std::string &_newName)
        {
            if (_oldName.empty() || _oldName == _newName)
                return;

            if (_controller.entryState == _oldName)
                _controller.entryState = _newName;

            for (AnimatorState &state : _controller.states)
            {
                for (AnimatorTransition &transition : state.transitions)
                {
                    if (transition.toState == _oldName)
                        transition.toState = _newName;
                }
            }
        }

        void RemoveAnimatorStateReferences(
            AnimatorControllerAsset &_controller,
            const std::string &_removedStateName)
        {
            if (_controller.entryState == _removedStateName)
                _controller.entryState.clear();

            for (AnimatorState &state : _controller.states)
            {
                state.transitions.erase(
                    std::remove_if(
                        state.transitions.begin(),
                        state.transitions.end(),
                        [&](const AnimatorTransition &_transition)
                        {
                            return _transition.toState == _removedStateName;
                        }),
                    state.transitions.end());
            }
        }

        AnimatorParameterType FindAnimatorParameterType(
            const AnimatorControllerAsset &_controller,
            const std::string &_parameterName)
        {
            for (const AnimatorParameterDefinition &parameter : _controller.parameters)
            {
                if (parameter.name == _parameterName)
                    return parameter.type;
            }

            return AnimatorParameterType::FLOAT;
        }

        GraphEditor::Template GetAnimatorGraphTemplate(GraphEditor::TemplateIndex _index)
        {
            static const char *inputNames[] = {"In"};
            static const char *outputNames[] = {"Out"};
            static ImU32 inputColors[] = {IM_COL32(188, 194, 204, 255)};
            static ImU32 outputColors[] = {IM_COL32(188, 194, 204, 255)};

            GraphEditor::Template graphTemplate{};
            graphTemplate.mInputCount = 1u;
            graphTemplate.mInputNames = inputNames;
            graphTemplate.mInputColors = inputColors;
            graphTemplate.mOutputCount = 1u;
            graphTemplate.mOutputNames = outputNames;
            graphTemplate.mOutputColors = outputColors;

            switch (static_cast<AnimatorGraphTemplateIndex>(_index))
            {
                case AnimatorGraphTemplate_EntryState:
                    graphTemplate.mHeaderColor = IM_COL32(114, 190, 145, 255);
                    graphTemplate.mBackgroundColor = IM_COL32(46, 67, 58, 235);
                    graphTemplate.mBackgroundColorOver = IM_COL32(56, 79, 68, 245);
                    return graphTemplate;
                case AnimatorGraphTemplate_ActiveState:
                    graphTemplate.mHeaderColor = IM_COL32(255, 198, 92, 255);
                    graphTemplate.mBackgroundColor = IM_COL32(76, 62, 38, 235);
                    graphTemplate.mBackgroundColorOver = IM_COL32(90, 72, 43, 245);
                    return graphTemplate;
                case AnimatorGraphTemplate_ActiveEntryState:
                    graphTemplate.mHeaderColor = IM_COL32(120, 214, 176, 255);
                    graphTemplate.mBackgroundColor = IM_COL32(54, 76, 66, 235);
                    graphTemplate.mBackgroundColorOver = IM_COL32(64, 90, 77, 245);
                    return graphTemplate;
                case AnimatorGraphTemplate_State:
                default:
                    graphTemplate.mHeaderColor = IM_COL32(126, 167, 222, 255);
                    graphTemplate.mBackgroundColor = IM_COL32(48, 56, 70, 235);
                    graphTemplate.mBackgroundColorOver = IM_COL32(58, 67, 84, 245);
                    return graphTemplate;
            }
        }

        Vector2 ScreenToAnimatorGraphPosition(
            const ImVec2 &_screenPos,
            const ImVec2 &_canvasScreenPos,
            const GraphEditor::ViewState &_viewState)
        {
            const float localX = (_screenPos.x - _canvasScreenPos.x) / _viewState.mFactor;
            const float localY = (_screenPos.y - _canvasScreenPos.y) / _viewState.mFactor;
            return Vector2(localX - _viewState.mPosition.x, localY - _viewState.mPosition.y);
        }

        class AnimatorGraphDelegate final : public GraphEditor::Delegate
        {
        public:
            AnimatorGraphDelegate(
                AnimatorControllerAsset &_controller,
                int &_selectedStateIndex,
                bool &_controllerChanged,
                const std::string &_activeStateName)
                : m_controller(_controller)
                , m_selectedStateIndex(_selectedStateIndex)
                , m_controllerChanged(_controllerChanged)
                , m_activeStateName(_activeStateName)
            {
            }

            bool AllowedLink(GraphEditor::NodeIndex _from, GraphEditor::NodeIndex _to) override
            {
                return _from != _to;
            }

            void SelectNode(GraphEditor::NodeIndex _nodeIndex, bool _selected) override
            {
                if (_selected)
                {
                    m_selectedStateIndex = static_cast<int>(_nodeIndex);
                }
                else if (m_selectedStateIndex == static_cast<int>(_nodeIndex))
                {
                    m_selectedStateIndex = -1;
                }
            }

            void MoveSelectedNodes(const ImVec2 _delta) override
            {
                if (m_selectedStateIndex < 0 || m_selectedStateIndex >= static_cast<int>(m_controller.states.size()))
                    return;

                AnimatorState &state = m_controller.states[static_cast<std::size_t>(m_selectedStateIndex)];
                state.editorPosition.x += _delta.x;
                state.editorPosition.y += _delta.y;
                m_controllerChanged = true;
            }

            void AddLink(
                GraphEditor::NodeIndex _sourceNodeIndex,
                GraphEditor::SlotIndex,
                GraphEditor::NodeIndex _targetNodeIndex,
                GraphEditor::SlotIndex) override
            {
                if (_sourceNodeIndex >= m_controller.states.size() ||
                    _targetNodeIndex >= m_controller.states.size() ||
                    _sourceNodeIndex == _targetNodeIndex)
                {
                    return;
                }

                AnimatorState &sourceState = m_controller.states[_sourceNodeIndex];
                const std::string targetStateName = m_controller.states[_targetNodeIndex].name;
                for (const AnimatorTransition &transition : sourceState.transitions)
                {
                    if (transition.toState == targetStateName)
                        return;
                }

                AnimatorTransition transition = {};
                transition.toState = targetStateName;
                sourceState.transitions.push_back(transition);
                m_selectedStateIndex = static_cast<int>(_sourceNodeIndex);
                m_controllerChanged = true;
            }

            void DelLink(GraphEditor::LinkIndex _linkIndex) override
            {
                const std::vector<AnimatorGraphLinkInfo> links = BuildLinks();
                if (_linkIndex >= links.size())
                    return;

                const AnimatorGraphLinkInfo &linkInfo = links[_linkIndex];
                if (linkInfo.fromStateIndex < 0 || linkInfo.transitionIndex < 0)
                    return;

                if (linkInfo.fromStateIndex >= static_cast<int>(m_controller.states.size()))
                    return;

                AnimatorState &state = m_controller.states[static_cast<std::size_t>(linkInfo.fromStateIndex)];
                if (linkInfo.transitionIndex >= static_cast<int>(state.transitions.size()))
                    return;

                state.transitions.erase(state.transitions.begin() + linkInfo.transitionIndex);
                m_controllerChanged = true;
            }

            void CustomDraw(ImDrawList *_drawList, ImRect _rectangle, GraphEditor::NodeIndex _nodeIndex, float) override
            {
                if (_nodeIndex >= m_controller.states.size())
                    return;

                const AnimatorState &state = m_controller.states[_nodeIndex];
                const std::string clipPath = AssetManager::ResolvePath(state.clip);
                std::string clipLabel = clipPath.empty()
                    ? "[ no clip ]"
                    : std::filesystem::path(clipPath).stem().string();
                if (!clipPath.empty())
                {
                    if (MetaFileAsset *meta = AssetManager::GetMetaFile(clipPath))
                        clipLabel = meta->name;
                }

                const bool isEntryState = m_controller.entryState == state.name;
                const bool isActiveState = !m_activeStateName.empty() && m_activeStateName == state.name;

                ImVec2 cursor(_rectangle.Min.x + 10.0f, _rectangle.Min.y + 8.0f);
                const ImVec4 clipRect(_rectangle.Min.x, _rectangle.Min.y, _rectangle.Max.x, _rectangle.Max.y);
                auto drawLine = [&](const std::string &_text, ImU32 _color) -> void
                {
                    const ImVec2 shadowPos(cursor.x + 1.0f, cursor.y + 1.0f);
                    _drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), shadowPos, IM_COL32(0, 0, 0, 180), _text.c_str(), nullptr, 0.0f, &clipRect);
                    _drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), cursor, _color, _text.c_str(), nullptr, 0.0f, &clipRect);
                    cursor.y += ImGui::GetFontSize() + 4.0f;
                };

                drawLine(clipLabel, IM_COL32(236, 238, 244, 255));
                drawLine(std::string("Speed ") + std::to_string(state.speed).substr(0, 4) + (state.loop ? "  Loop" : "  Once"), IM_COL32(186, 198, 215, 255));
                if (isEntryState)
                    drawLine("Entry State", IM_COL32(186, 255, 208, 255));
                if (isActiveState)
                    drawLine("Live State", IM_COL32(255, 230, 174, 255));
                drawLine(std::to_string(state.transitions.size()) + " transition(s)", IM_COL32(166, 174, 188, 255));
            }

            const char *GetNodeTitleTooltip(GraphEditor::NodeIndex _nodeIndex) override
            {
                if (_nodeIndex >= m_controller.states.size())
                    return nullptr;

                const AnimatorState &state = m_controller.states[_nodeIndex];
                const std::string clipPath = AssetManager::ResolvePath(state.clip);
                m_tooltipCache = state.name;
                if (!clipPath.empty())
                    m_tooltipCache += "\nClip: " + clipPath;
                if (!state.transitions.empty())
                    m_tooltipCache += "\nTransitions: " + std::to_string(state.transitions.size());
                return m_tooltipCache.c_str();
            }

            void RightClick(
                GraphEditor::NodeIndex _nodeIndex,
                GraphEditor::SlotIndex,
                GraphEditor::SlotIndex) override
            {
                m_contextNodeIndex = _nodeIndex;
                m_contextMenuRequested = true;
                m_contextMenuScreenPos = ImGui::GetMousePos();
            }

            const size_t GetTemplateCount() override
            {
                return AnimatorGraphTemplate_Count;
            }

            const GraphEditor::Template GetTemplate(GraphEditor::TemplateIndex _index) override
            {
                return GetAnimatorGraphTemplate(_index);
            }

            const size_t GetNodeCount() override
            {
                return m_controller.states.size();
            }

            const GraphEditor::Node GetNode(GraphEditor::NodeIndex _index) override
            {
                if (_index >= m_controller.states.size())
                    return GraphEditor::Node{"Invalid", AnimatorGraphTemplate_State, ImRect(ImVec2(0.0f, 0.0f), ImVec2(kAnimatorGraphNodeWidth, kAnimatorGraphNodeHeight)), false};

                const AnimatorState &state = m_controller.states[_index];
                const bool isEntryState = m_controller.entryState == state.name;
                const bool isActiveState = !m_activeStateName.empty() && m_activeStateName == state.name;

                GraphEditor::TemplateIndex templateIndex = AnimatorGraphTemplate_State;
                if (isEntryState && isActiveState)
                    templateIndex = AnimatorGraphTemplate_ActiveEntryState;
                else if (isEntryState)
                    templateIndex = AnimatorGraphTemplate_EntryState;
                else if (isActiveState)
                    templateIndex = AnimatorGraphTemplate_ActiveState;

                return GraphEditor::Node{
                    state.name.c_str(),
                    templateIndex,
                    ImRect(
                        ImVec2(state.editorPosition.x, state.editorPosition.y),
                        ImVec2(state.editorPosition.x + kAnimatorGraphNodeWidth, state.editorPosition.y + kAnimatorGraphNodeHeight)),
                    m_selectedStateIndex == static_cast<int>(_index)};
            }

            const size_t GetLinkCount() override
            {
                return BuildLinks().size();
            }

            const GraphEditor::Link GetLink(GraphEditor::LinkIndex _index) override
            {
                return BuildLinks()[_index].graphLink;
            }

            bool ConsumeContextMenuRequest(ImVec2 &_screenPos)
            {
                if (!m_contextMenuRequested)
                    return false;

                _screenPos = m_contextMenuScreenPos;
                m_contextMenuRequested = false;
                return true;
            }

            GraphEditor::NodeIndex GetContextNodeIndex() const
            {
                return m_contextNodeIndex;
            }

        private:
            std::vector<AnimatorGraphLinkInfo> BuildLinks() const
            {
                std::vector<AnimatorGraphLinkInfo> links = {};
                for (std::size_t stateIndex = 0; stateIndex < m_controller.states.size(); ++stateIndex)
                {
                    const AnimatorState &state = m_controller.states[stateIndex];
                    for (std::size_t transitionIndex = 0; transitionIndex < state.transitions.size(); ++transitionIndex)
                    {
                        const AnimatorTransition &transition = state.transitions[transitionIndex];
                        const int targetStateIndex = FindAnimatorStateIndex(m_controller, transition.toState);
                        if (targetStateIndex < 0)
                            continue;

                        AnimatorGraphLinkInfo linkInfo = {};
                        linkInfo.graphLink = GraphEditor::Link{
                            stateIndex,
                            0u,
                            static_cast<GraphEditor::NodeIndex>(targetStateIndex),
                            0u};
                        linkInfo.fromStateIndex = static_cast<int>(stateIndex);
                        linkInfo.transitionIndex = static_cast<int>(transitionIndex);
                        links.push_back(linkInfo);
                    }
                }

                return links;
            }

            AnimatorControllerAsset &m_controller;
            int &m_selectedStateIndex;
            bool &m_controllerChanged;
            const std::string &m_activeStateName;
            GraphEditor::NodeIndex m_contextNodeIndex = kInvalidAnimatorGraphNodeIndex;
            bool m_contextMenuRequested = false;
            ImVec2 m_contextMenuScreenPos = ImVec2(0.0f, 0.0f);
            mutable std::string m_tooltipCache = {};
        };

        bool DrawAnimationValueEditor(const char *_label, AnimationValue &_value)
        {
            switch (_value.type)
            {
                case AnimationValueType::FLOAT:
                {
                    float floatValue = _value.AsFloat();
                    if (ImGui::InputFloat(_label, &floatValue, 0.0f, 0.0f, "%.3f"))
                    {
                        _value = AnimationValue::Float(floatValue);
                        return true;
                    }
                    return false;
                }
                case AnimationValueType::INT:
                {
                    int intValue = _value.AsInt();
                    if (ImGui::InputInt(_label, &intValue))
                    {
                        _value = AnimationValue::Int(intValue);
                        return true;
                    }
                    return false;
                }
                case AnimationValueType::BOOL:
                {
                    bool boolValue = _value.AsBool();
                    if (ImGui::Checkbox(_label, &boolValue))
                    {
                        _value = AnimationValue::Bool(boolValue);
                        return true;
                    }
                    return false;
                }
                case AnimationValueType::VEC2:
                {
                    Vector2 vecValue = _value.AsVec2();
                    if (ImGui::InputFloat2(_label, &vecValue.x, "%.3f"))
                    {
                        _value = AnimationValue::Vec2(vecValue);
                        return true;
                    }
                    return false;
                }
                case AnimationValueType::VEC3:
                {
                    Vector3 vecValue = _value.AsVec3();
                    if (ImGui::InputFloat3(_label, &vecValue.x, "%.3f"))
                    {
                        _value = AnimationValue::Vec3(vecValue);
                        return true;
                    }
                    return false;
                }
                case AnimationValueType::VEC4:
                {
                    Vector4 vecValue = _value.AsVec4();
                    if (ImGui::InputFloat4(_label, &vecValue.x, "%.3f"))
                    {
                        _value = AnimationValue::Vec4(vecValue);
                        return true;
                    }
                    return false;
                }
                case AnimationValueType::NONE:
                default:
                    ImGui::TextDisabled("%s", _label);
                    return false;
            }
        }

        enum class HierarchyCreateType
        {
            Empty,
            Empty3D,
            Empty2D,
            Canvas,
            Text,
            Button,
            Image,
            Cube,
            Sphere,
            Capsule,
            DirectionalLight,
            PointLight,
        };

        struct AddComponentEntry
        {
            std::string componentName = "";
            std::string displayName = "";
        };

        std::string GetAddComponentDisplayName(const std::string &_componentName)
        {
            if (_componentName == DirectionalLight::ScriptName)
                return "3D/Directional Light";

            if (_componentName == PointLight::ScriptName)
                return "3D/Point Light";

            return _componentName;
        }

        struct RectTransformRenderBounds
        {
            Vector2 min = Vector2(0.0f);
            Vector2 size = Vector2(0.0f);
            Vector2 rotationPivot = Vector2(0.0f);
        };

        RectTransformRenderBounds GetRenderBounds(const Entity& _entity, const RectTransform& _transform)
        {
            RectTransformRenderBounds bounds = {};
            bounds.min = _transform.GetRectMin() + _transform.originOffset;
            bounds.size = _transform.GetResolvedSize();
            bounds.rotationPivot = _transform.GetPosition();

            if (_entity.HasComponent<Text>())
                bounds.rotationPivot += _transform.rotationOriginOffset;

            return bounds;
        }

        bool SceneHasEntityWithName(Scene &_scene, const std::string &_name)
        {
            for (Entity *entity : _scene.GetEntities())
            {
                if (entity != nullptr && entity->name == _name)
                    return true;
            }

            return false;
        }

        std::string MakeUniqueEntityName(Scene &_scene, const std::string &_baseName)
        {
            if (!SceneHasEntityWithName(_scene, _baseName))
                return _baseName;

            for (int suffix = 2;; ++suffix)
            {
                const std::string candidate = _baseName + " " + std::to_string(suffix);
                if (!SceneHasEntityWithName(_scene, candidate))
                    return candidate;
            }
        }

        void ParentNewHierarchyEntity(Entity *_entity, Entity *_parent)
        {
            if (_entity == nullptr || _parent == nullptr)
                return;

            if (_entity->HasComponent<RectTransform>() && _parent->HasComponent<RectTransform>())
            {
                RectTransform &rect = _entity->GetComponent<RectTransform>();
                rect.SetParent(_parent);
                rect.position = Vector2(0.0f);
                return;
            }

            if (_entity->HasComponent<Transform>() && _parent->HasComponent<Transform>())
            {
                Transform &transform = _entity->GetComponent<Transform>();
                transform.SetParent(_parent);
                transform.position = Vector3(0.0f);
                transform.rotation = Vector3(0.0f);
            }
        }

        Entity *CreateHierarchyEntity(Editor &_editor, App &_app, Scene &_scene, HierarchyCreateType _type, Entity *_parent)
        {
            auto addRequired = [&](Entity &_entity, const char *_scriptName) -> void
            {
                (void)_app.AddRequiredScript(_entity, _scriptName);
            };

            std::string baseName = "Entity";
            switch (_type)
            {
            case HierarchyCreateType::Canvas:
                baseName = "Canvas";
                break;
            case HierarchyCreateType::Empty3D:
                baseName = "3D Empty";
                break;
            case HierarchyCreateType::Empty2D:
                baseName = "2D Empty";
                break;
            case HierarchyCreateType::Text:
                baseName = "Text";
                break;
            case HierarchyCreateType::Button:
                baseName = "Button";
                break;
            case HierarchyCreateType::Image:
                baseName = "Image";
                break;
            case HierarchyCreateType::Cube:
                baseName = "Cube";
                break;
            case HierarchyCreateType::Sphere:
                baseName = "Sphere";
                break;
            case HierarchyCreateType::Capsule:
                baseName = "Capsule";
                break;
            case HierarchyCreateType::DirectionalLight:
                baseName = "Directional Light";
                break;
            case HierarchyCreateType::PointLight:
                baseName = "Point Light";
                break;
            case HierarchyCreateType::Empty:
            default:
                break;
            }

            Entity *entity = _scene.CreateEntity(MakeUniqueEntityName(_scene, baseName));
            if (entity == nullptr)
                return nullptr;

            switch (_type)
            {
            case HierarchyCreateType::Empty:
            {
                if (_parent != nullptr)
                {
                    if (_parent->HasComponent<RectTransform>())
                        addRequired(*entity, RectTransform::ScriptName);
                    else if (_parent->HasComponent<Transform>())
                        addRequired(*entity, Transform::ScriptName);
                }
                break;
            }
            case HierarchyCreateType::Empty3D:
            {
                addRequired(*entity, Transform::ScriptName);
                break;
            }
            case HierarchyCreateType::Empty2D:
            {
                addRequired(*entity, RectTransform::ScriptName);
                break;
            }
            case HierarchyCreateType::Canvas:
            {
                addRequired(*entity, Canvas::ScriptName);
                RectTransform &rect = entity->GetComponent<RectTransform>();
                rect.position = Vector2(0.0f);
                rect.size = Vector2(0.0f);
                rect.anchorMin = Vector2(0.0f);
                rect.anchorMax = Vector2(1.0f);
                rect.pivot = Vector2(0.5f);
                break;
            }
            case HierarchyCreateType::Text:
            {
                addRequired(*entity, Text::ScriptName);
                RectTransform &rect = entity->GetComponent<RectTransform>();
                rect.position = Vector2(0.0f);
                rect.size = Vector2(0.0f);
                Text &text = entity->GetComponent<Text>();
                text.SetText("Text");
                text.alignment = TextAlignment::CENTER;
                break;
            }
            case HierarchyCreateType::Button:
            {
                addRequired(*entity, UIButton::ScriptName);
                addRequired(*entity, Sprite2D::ScriptName);

                RectTransform &rect = entity->GetComponent<RectTransform>();
                rect.position = Vector2(0.0f);
                rect.size = Vector2(160.0f, 48.0f);

                const Color baseColor = Color(0.14f, 0.18f, 0.24f, 0.98f);
                const Color hoverColor = Color(0.19f, 0.25f, 0.34f, 1.0f);
                const Color pressedColor = Color(0.10f, 0.14f, 0.18f, 1.0f);

                Sprite2D &sprite = entity->GetComponent<Sprite2D>();
                sprite.color = baseColor;

                UIButton &button = entity->GetComponent<UIButton>();
                button.baseColor = baseColor;
                button.hoverColor = hoverColor;
                button.pressedColor = pressedColor;

                ParentNewHierarchyEntity(entity, _parent);

                Entity *label = _scene.CreateEntity(MakeUniqueEntityName(_scene, entity->name + " Label"));
                if (label != nullptr)
                {
                    addRequired(*label, Text::ScriptName);

                    RectTransform &labelRect = label->GetComponent<RectTransform>();
                    labelRect.position = Vector2(0.0f);
                    labelRect.size = Vector2(0.0f);
                    labelRect.anchorMin = Vector2(0.5f);
                    labelRect.anchorMax = Vector2(0.5f);
                    labelRect.pivot = Vector2(0.5f);
                    labelRect.depth = rect.depth - 0.0001f;

                    Text &labelText = label->GetComponent<Text>();
                    labelText.SetText("Button");
                    labelText.alignment = TextAlignment::CENTER;

                    ParentNewHierarchyEntity(label, entity);
                }

                _editor.FocusEntity(entity);
                return entity;
            }
            case HierarchyCreateType::Image:
            {
                addRequired(*entity, RectTransform::ScriptName);
                addRequired(*entity, Sprite2D::ScriptName);

                RectTransform &rect = entity->GetComponent<RectTransform>();
                rect.position = Vector2(0.0f);
                rect.size = Vector2(96.0f, 96.0f);
                break;
            }
            case HierarchyCreateType::Cube:
            case HierarchyCreateType::Sphere:
            case HierarchyCreateType::Capsule:
            {
                addRequired(*entity, Model::ScriptName);

                Transform &transform = entity->GetComponent<Transform>();
                transform.position = Vector3(0.0f);

                Model &model = entity->GetComponent<Model>();
                switch (_type)
                {
                case HierarchyCreateType::Sphere:
                    model.modelId = AssetManager::LoadModel("assets/defaults/models/sphere.glb");
                    break;
                case HierarchyCreateType::Capsule:
                    model.modelId = AssetManager::LoadModel("assets/defaults/models/capsule.glb");
                    break;
                case HierarchyCreateType::Cube:
                default:
                    model.modelId = AssetManager::LoadModel("assets/defaults/models/cube.glb");
                    break;
                }
                break;
            }
            case HierarchyCreateType::DirectionalLight:
            {
                addRequired(*entity, DirectionalLight::ScriptName);
                break;
            }
            case HierarchyCreateType::PointLight:
            {
                addRequired(*entity, PointLight::ScriptName);

                if (entity->HasComponent<Transform>())
                {
                    Transform &transform = entity->GetComponent<Transform>();
                    transform.position = Vector3(2.0f, 2.5f, 2.0f);
                }
                break;
            }
            }

            ParentNewHierarchyEntity(entity, _parent);
            _editor.FocusEntity(entity);
            return entity;
        }

        bool DrawHierarchyCreateMenu(Editor &_editor, App &_app, Scene &_scene, Entity *_parent, bool &_refresh)
        {
            bool created = false;

            if (ImGui::BeginMenu("Create"))
            {
                auto create = [&](HierarchyCreateType _type) -> void
                {
                    if (CreateHierarchyEntity(_editor, _app, _scene, _type, _parent) != nullptr)
                    {
                        _refresh = true;
                        created = true;
                    }
                };

                if (ImGui::MenuItem("Empty"))
                    create(HierarchyCreateType::Empty);

                ImGui::Separator();

                if (ImGui::BeginMenu("2D"))
                {
                    if (ImGui::MenuItem("Empty"))
                        create(HierarchyCreateType::Empty2D);
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("UI"))
                {
                    if (ImGui::MenuItem("Canvas"))
                        create(HierarchyCreateType::Canvas);
                    if (ImGui::MenuItem("Text"))
                        create(HierarchyCreateType::Text);
                    if (ImGui::MenuItem("Button"))
                        create(HierarchyCreateType::Button);
                    if (ImGui::MenuItem("Image"))
                        create(HierarchyCreateType::Image);
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("3D"))
                {
                    if (ImGui::MenuItem("Empty"))
                        create(HierarchyCreateType::Empty3D);
                    ImGui::Separator();
                    if (ImGui::MenuItem("Cube"))
                        create(HierarchyCreateType::Cube);
                    if (ImGui::MenuItem("Sphere"))
                        create(HierarchyCreateType::Sphere);
                    if (ImGui::MenuItem("Capsule"))
                        create(HierarchyCreateType::Capsule);
                    if (ImGui::MenuItem("Directional Light"))
                        create(HierarchyCreateType::DirectionalLight);
                    if (ImGui::MenuItem("Point Light"))
                        create(HierarchyCreateType::PointLight);
                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            return created;
        }
    }

    static std::string ResolveAssetRefPath(const YAML::Node &_node)
    {
        if (!_node)
            return "";

        if (_node.IsMap())
        {
            if (YAML::Node uuidNode = _node["uuid"])
            {
                UUID uuid = uuidNode.as<uint64_t>(0);
                if ((uint64_t)uuid != 0)
                {
                    std::string path = AssetManager::GetPath(uuid);
                    if (path != "Path was not found in AssetLibrary")
                        return path;
                }
            }

            if (YAML::Node pathNode = _node["path"])
                return pathNode.as<std::string>("");

            return "";
        }

        if (_node.IsScalar())
        {
            std::string raw = _node.as<std::string>("");
            if (raw.empty())
                return "";

            bool isNumeric = std::all_of(raw.begin(), raw.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
            if (isNumeric)
            {
                UUID uuid = (UUID)std::stoull(raw);
                std::string path = AssetManager::GetPath(uuid);
                if (path != "Path was not found in AssetLibrary")
                    return path;
            }

            return raw;
        }

        return "";
    }

    static std::string NormalizeShaderAssetPath(std::string _path)
    {
        if (_path.size() > 3 && _path.ends_with(".vs"))
            return _path.substr(0, _path.size() - 3);

        if (_path.size() > 3 && _path.ends_with(".fs"))
            return _path.substr(0, _path.size() - 3);

        return _path;
    }

    static std::string ToLowerCopy(std::string _value)
    {
        std::transform(_value.begin(), _value.end(), _value.begin(), [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });

        return _value;
    }

    static std::string NormalizeAssetSearchQuery(const std::string &_query)
    {
        if (_query.find_first_not_of(" \t\r\n") == std::string::npos)
            return {};

        return ToLowerCopy(_query);
    }

    static bool ShouldHideAssetBrowserPath(const std::filesystem::path &_path)
    {
        return _path.filename() == ".DS_Store" || _path.extension() == ".meta";
    }

    static bool AssetPathMatchesSearch(const std::filesystem::path &_path, const std::string &_lowerQuery)
    {
        if (_lowerQuery.empty())
            return true;

        const std::string path = ToLowerCopy(_path.generic_string());
        const std::string name = ToLowerCopy(_path.filename().string());
        return path.find(_lowerQuery) != std::string::npos || name.find(_lowerQuery) != std::string::npos;
    }

    static bool ConsoleLogLevelEnabled(
        Canis::Debug::LogLevel _level,
        bool _showLogs,
        bool _showWarnings,
        bool _showErrors,
        bool _showFatal)
    {
        switch (_level)
        {
            case Canis::Debug::LogLevel::Warning: return _showWarnings;
            case Canis::Debug::LogLevel::Error: return _showErrors;
            case Canis::Debug::LogLevel::Fatal: return _showFatal;
            case Canis::Debug::LogLevel::Log:
            default: return _showLogs;
        }
    }

    static bool ConsoleEntryMatchesSearch(const Canis::Debug::LogEntry &_entry, const std::string &_lowerQuery)
    {
        if (_lowerQuery.empty())
            return true;

        const std::string level = ToLowerCopy(Canis::Debug::LogLevelName(_entry.level));
        const std::string message = ToLowerCopy(_entry.message);
        const std::string file = ToLowerCopy(_entry.file);
        return level.find(_lowerQuery) != std::string::npos ||
            message.find(_lowerQuery) != std::string::npos ||
            file.find(_lowerQuery) != std::string::npos;
    }

    static ImVec4 ConsoleLogLevelColor(Canis::Debug::LogLevel _level)
    {
        switch (_level)
        {
            case Canis::Debug::LogLevel::Warning: return ImVec4(1.0f, 0.78f, 0.24f, 1.0f);
            case Canis::Debug::LogLevel::Error: return ImVec4(1.0f, 0.38f, 0.36f, 1.0f);
            case Canis::Debug::LogLevel::Fatal: return ImVec4(1.0f, 0.18f, 0.18f, 1.0f);
            case Canis::Debug::LogLevel::Log:
            default: return ImVec4(0.72f, 0.92f, 0.72f, 1.0f);
        }
    }

    struct ConsoleDisplayEntry
    {
        Canis::Debug::LogEntry entry = {};
        int count = 1;
    };

    static bool ConsoleEntriesAreDuplicates(const Canis::Debug::LogEntry &_left, const Canis::Debug::LogEntry &_right)
    {
        return _left.level == _right.level &&
            _left.message == _right.message &&
            _left.file == _right.file &&
            _left.line == _right.line;
    }

    static std::string MakeConsoleSourceDisplayPath(const std::string &_file)
    {
        if (_file.empty())
            return "";

        namespace fs = std::filesystem;
        const fs::path filePath(_file);
        if (!filePath.is_absolute())
            return filePath.generic_string();

        std::error_code ec;
        const fs::path currentPath = fs::current_path(ec);
        if (!ec && !currentPath.empty())
        {
            ec.clear();
            const fs::path relativePath = fs::relative(filePath, currentPath, ec);
            if (!ec && !relativePath.empty() && relativePath.begin()->string() != "..")
                return relativePath.generic_string();
        }

        return filePath.filename().generic_string();
    }

    static std::string MakeConsoleOpenTarget(const Canis::Debug::LogEntry &_entry)
    {
        if (_entry.file.empty())
            return "";

        namespace fs = std::filesystem;
        fs::path filePath(_entry.file);

        std::error_code ec;
        if (!filePath.is_absolute() && !fs::exists(filePath, ec))
        {
            const fs::path currentCandidate = fs::current_path(ec) / filePath;
            if (fs::exists(currentCandidate, ec))
                filePath = currentCandidate;
            else
            {
                const fs::path runtimeCandidate = GetEditorRuntimeBasePath() / filePath;
                if (fs::exists(runtimeCandidate, ec))
                    filePath = runtimeCandidate;
            }
        }

        std::string target = filePath.generic_string();
        if (_entry.line > 0)
            target += ":" + std::to_string(_entry.line);
        return target;
    }

    static bool AssetDirectoryContainsSearchMatch(const std::filesystem::path &_path, const std::string &_lowerQuery)
    {
        if (_lowerQuery.empty() || AssetPathMatchesSearch(_path, _lowerQuery))
            return true;

        std::error_code ec;
        std::filesystem::recursive_directory_iterator it(
            _path,
            std::filesystem::directory_options::skip_permission_denied,
            ec);
        const std::filesystem::recursive_directory_iterator end = {};

        while (!ec && it != end)
        {
            const std::filesystem::path currentPath = it->path();
            if (!ShouldHideAssetBrowserPath(currentPath) && AssetPathMatchesSearch(currentPath, _lowerQuery))
                return true;

            it.increment(ec);
        }

        return false;
    }

    static bool IsReservedMaterialKey(const std::string &_key)
    {
        return _key == "shader" || _key == "albedo" || _key == "specular" || _key == "roughness" || _key == "metallic" ||
               _key == "emission" || _key == "color" || _key == "specularValue" || _key == "roughnessValue" || _key == "metallicValue" ||
               _key == "backFaceCulling" || _key == "frontFaceCulling" || _key == "uniforms";
    }

    static YAML::Node MakeAssetRefNode(const std::string &_path)
    {
        YAML::Node node(YAML::NodeType::Map);
        if (MetaFileAsset *meta = AssetManager::GetMetaFile(_path))
        {
            node["uuid"] = (uint64_t)meta->uuid;
            node["path"] = _path;
        }
        return node;
    }

    static bool ApplyTypedMaterialUniform(MaterialFields &_fields, const std::string &_uniformName, const std::string &_type, const YAML::Node &_valueNode)
    {
        const std::string type = ToLowerCopy(_type);
        try
        {
            if (type == "int" || type == "integer")
            {
                _fields.SetInt(_uniformName, _valueNode.as<int>(0));
                return true;
            }

            if (type == "float")
            {
                _fields.SetFloat(_uniformName, _valueNode.as<float>(0.0f));
                return true;
            }

            if (type == "vector2" || type == "vec2")
            {
                _fields.SetVec2(_uniformName, _valueNode.as<Vector2>(Vector2(0.0f)));
                return true;
            }

            if (type == "vector3" || type == "vec3")
            {
                _fields.SetVec3(_uniformName, _valueNode.as<Vector3>(Vector3(0.0f)));
                return true;
            }

            if (type == "vector4" || type == "vec4")
            {
                _fields.SetVec4(_uniformName, _valueNode.as<Vector4>(Vector4(0.0f)));
                return true;
            }

            if (type == "color")
            {
                _fields.SetColor(_uniformName, _valueNode.as<Color>(Color(1.0f)));
                return true;
            }

            if (type == "texture" || type == "sampler2d")
            {
                const std::string texturePath = ResolveAssetRefPath(_valueNode);
                i32 textureId = -1;
                if (!texturePath.empty())
                    textureId = AssetManager::LoadTexture(texturePath);
                _fields.SetTexture(_uniformName, textureId);
                return true;
            }
        }
        catch (const YAML::Exception &)
        {
        }

        return false;
    }

    static bool ApplyInferredMaterialUniform(MaterialFields &_fields, const std::string &_uniformName, const YAML::Node &_valueNode)
    {
        if (!_valueNode)
            return false;

        if (_valueNode.IsMap())
        {
            const std::string texturePath = ResolveAssetRefPath(_valueNode);
            i32 textureId = -1;
            if (!texturePath.empty())
                textureId = AssetManager::LoadTexture(texturePath);
            _fields.SetTexture(_uniformName, textureId);
            return true;
        }

        if (_valueNode.IsSequence())
        {
            try
            {
                if (_valueNode.size() == 2u)
                {
                    _fields.SetVec2(_uniformName, _valueNode.as<Vector2>(Vector2(0.0f)));
                    return true;
                }

                if (_valueNode.size() == 3u)
                {
                    _fields.SetVec3(_uniformName, _valueNode.as<Vector3>(Vector3(0.0f)));
                    return true;
                }

                if (_valueNode.size() == 4u)
                {
                    _fields.SetVec4(_uniformName, _valueNode.as<Vector4>(Vector4(0.0f)));
                    return true;
                }
            }
            catch (const YAML::Exception &)
            {
            }
        }

        if (_valueNode.IsScalar())
        {
            try
            {
                _fields.SetFloat(_uniformName, _valueNode.as<float>());
                return true;
            }
            catch (const YAML::Exception &)
            {
            }
        }

        return false;
    }

    static void ReadMaterialUniformsFromNode(const YAML::Node &_root, MaterialFields &_fields)
    {
        _fields.Clear();

        if (YAML::Node uniformsNode = _root["uniforms"]; uniformsNode && uniformsNode.IsMap())
        {
            for (const auto &uniformEntry : uniformsNode)
            {
                const std::string uniformName = uniformEntry.first.as<std::string>("");
                if (uniformName.empty())
                    continue;

                const YAML::Node uniformNode = uniformEntry.second;
                if (!uniformNode)
                    continue;

                bool loaded = false;
                if (uniformNode.IsMap())
                {
                    const std::string type = uniformNode["type"].as<std::string>("");
                    YAML::Node valueNode = uniformNode["value"];
                    if (!valueNode && uniformNode["texture"])
                        valueNode = uniformNode["texture"];
                    if (!valueNode && uniformNode["data"])
                        valueNode = uniformNode["data"];
                    if (!valueNode)
                        valueNode = uniformNode;

                    if (!type.empty())
                        loaded = ApplyTypedMaterialUniform(_fields, uniformName, type, valueNode);

                    if (!loaded)
                        loaded = ApplyInferredMaterialUniform(_fields, uniformName, valueNode);
                }
                else
                {
                    loaded = ApplyInferredMaterialUniform(_fields, uniformName, uniformNode);
                }

                if (!loaded && uniformNode.IsScalar())
                {
                    try
                    {
                        _fields.SetFloat(uniformName, uniformNode.as<float>());
                    }
                    catch (const YAML::Exception &)
                    {
                    }
                }
            }
        }

        // Backward compatibility for old material files with root-level float uniforms.
        for (const auto &entry : _root)
        {
            const std::string key = entry.first.as<std::string>("");
            if (IsReservedMaterialKey(key) || !entry.second.IsScalar())
                continue;

            if (YAML::Node uniformsNode = _root["uniforms"]; uniformsNode && uniformsNode.IsMap() && uniformsNode[key])
                continue;

            try
            {
                _fields.SetFloat(key, entry.second.as<float>());
            }
            catch (const YAML::Exception &)
            {
            }
        }
    }

    static void WriteMaterialUniformsToNode(YAML::Node &_root, const MaterialFields &_fields)
    {
        std::vector<std::string> legacyKeysToRemove = {};
        for (const auto &entry : _root)
        {
            const std::string key = entry.first.as<std::string>("");
            if (IsReservedMaterialKey(key))
                continue;

            if (entry.second.IsScalar())
                legacyKeysToRemove.push_back(key);
        }

        for (const std::string &key : legacyKeysToRemove)
            _root.remove(key);

        YAML::Node uniformsNode(YAML::NodeType::Map);

        for (const MaterialFields::IntUniformData &uniform : _fields.GetIntUniforms())
        {
            YAML::Node uniformNode(YAML::NodeType::Map);
            uniformNode["type"] = "int";
            uniformNode["value"] = uniform.value;
            uniformsNode[uniform.name] = uniformNode;
        }

        for (const MaterialFields::FloatUniformData &uniform : _fields.GetFloatUniforms())
        {
            YAML::Node uniformNode(YAML::NodeType::Map);
            uniformNode["type"] = "float";
            uniformNode["value"] = uniform.value;
            uniformsNode[uniform.name] = uniformNode;
        }

        for (const MaterialFields::Vec2UniformData &uniform : _fields.GetVec2Uniforms())
        {
            YAML::Node uniformNode(YAML::NodeType::Map);
            uniformNode["type"] = "vector2";
            uniformNode["value"] = uniform.value;
            uniformsNode[uniform.name] = uniformNode;
        }

        for (const MaterialFields::Vec3UniformData &uniform : _fields.GetVec3Uniforms())
        {
            YAML::Node uniformNode(YAML::NodeType::Map);
            uniformNode["type"] = "vector3";
            uniformNode["value"] = uniform.value;
            uniformsNode[uniform.name] = uniformNode;
        }

        for (const MaterialFields::Vec4UniformData &uniform : _fields.GetVec4Uniforms())
        {
            YAML::Node uniformNode(YAML::NodeType::Map);
            uniformNode["type"] = "vector4";
            uniformNode["value"] = uniform.value;
            uniformsNode[uniform.name] = uniformNode;
        }

        for (const MaterialFields::ColorUniformData &uniform : _fields.GetColorUniforms())
        {
            YAML::Node uniformNode(YAML::NodeType::Map);
            uniformNode["type"] = "color";
            uniformNode["value"] = uniform.value;
            uniformsNode[uniform.name] = uniformNode;
        }

        for (const MaterialFields::TextureUniformData &uniform : _fields.GetTextureUniforms())
        {
            YAML::Node uniformNode(YAML::NodeType::Map);
            uniformNode["type"] = "texture";
            if (uniform.textureId >= 0)
            {
                const std::string texturePath = AssetManager::GetPath(uniform.textureId);
                if (texturePath.rfind("Path was not found", 0) != 0)
                    uniformNode["value"] = MakeAssetRefNode(texturePath);
                else
                    uniformNode["value"] = YAML::Node();
            }
            else
            {
                uniformNode["value"] = YAML::Node();
            }
            uniformsNode[uniform.name] = uniformNode;
        }

        if (uniformsNode.size() > 0u)
            _root["uniforms"] = uniformsNode;
        else
            _root.remove("uniforms");
    }

    static void SetAssetRefUUID(YAML::Node &_root, const std::string &_key, const std::string &_path)
    {
        YAML::Node node(YAML::NodeType::Map);
        if (MetaFileAsset *meta = AssetManager::GetMetaFile(_path))
        {
            node["uuid"] = (uint64_t)meta->uuid;
            node["path"] = _path;
        }
        _root[_key] = node;
    }

    static bool LoadTinyGLTFImageDataForEditor(
        tinygltf::Image *_image,
        const int /*_imageIndex*/,
        std::string *_error,
        std::string* /*_warning*/,
        int /*_reqWidth*/,
        int /*_reqHeight*/,
        const unsigned char *_bytes,
        int _size,
        void* /*_userPointer*/)
    {
        if (_image == nullptr || _bytes == nullptr || _size <= 0)
        {
            if (_error != nullptr)
                *_error += "Invalid glTF image payload.\n";
            return false;
        }

        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_uc *decoded = stbi_load_from_memory(_bytes, _size, &width, &height, &channels, 4);
        if (decoded == nullptr)
        {
            if (_error != nullptr)
            {
                *_error += "Failed to decode glTF image data";
                if (const char *reason = stbi_failure_reason())
                {
                    *_error += ": ";
                    *_error += reason;
                }
                *_error += "\n";
            }
            return false;
        }

        _image->width = width;
        _image->height = height;
        _image->component = 4;
        _image->bits = 8;
        _image->pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
        _image->image.assign(decoded, decoded + (width * height * 4));

        stbi_image_free(decoded);
        return true;
    }

    static bool IsGltfModelAssetPath(const std::filesystem::path &_path)
    {
        const std::string extension = ToLowerCopy(_path.extension().string());
        return extension == ".glb" || extension == ".gltf";
    }

    static std::string SanitizeAssetFileStem(std::string _name, const std::string &_fallback)
    {
        if (_name.empty())
            _name = _fallback.empty() ? "asset" : _fallback;

        for (char &c : _name)
        {
            const bool valid =
                (c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') ||
                c == '_' ||
                c == '-';

            if (!valid)
                c = '_';
        }

        if (_name.empty())
            return _fallback.empty() ? "asset" : _fallback;

        return _name;
    }

    static std::filesystem::path MakeUniqueFilePath(const std::filesystem::path &_desiredPath)
    {
        namespace fs = std::filesystem;

        if (!fs::exists(_desiredPath))
            return _desiredPath;

        const fs::path parent = _desiredPath.parent_path();
        const std::string stem = _desiredPath.stem().string();
        const std::string extension = _desiredPath.extension().string();

        int index = 1;
        fs::path candidate;
        do
        {
            candidate = parent / (stem + "_" + std::to_string(index) + extension);
            ++index;
        } while (fs::exists(candidate));

        return candidate;
    }

    static bool AreEquivalentPaths(const std::filesystem::path &_a, const std::filesystem::path &_b)
    {
        std::error_code ec;
        const bool equivalent = std::filesystem::equivalent(_a, _b, ec);
        return !ec && equivalent;
    }

    static std::string MakeEditorAssetPath(const std::filesystem::path &_path)
    {
        namespace fs = std::filesystem;

        fs::path normalized = _path.lexically_normal();
        if (!normalized.is_absolute())
            return normalized.generic_string();

        std::error_code ec;
        const fs::path runtimeBasePath = GetEditorRuntimeBasePath();
        const fs::path currentPath = fs::current_path(ec);
        const std::vector<fs::path> roots =
        {
            runtimeBasePath,
            runtimeBasePath / "project",
            currentPath,
            currentPath / "project"
        };

        for (const fs::path &root : roots)
        {
            if (root.empty())
                continue;

            ec.clear();
            fs::path relativePath = fs::relative(normalized, root, ec);
            if (ec || relativePath.empty())
                continue;

            const auto firstPart = relativePath.begin();
            if (firstPart != relativePath.end() && firstPart->string() != "..")
                return relativePath.lexically_normal().generic_string();
        }

        return normalized.generic_string();
    }

    static bool WriteEditorRgbaTga(
        const std::filesystem::path &_path,
        int _width,
        int _height,
        const std::vector<unsigned char> &_rgbaPixels)
    {
        if (_width <= 0 || _height <= 0 || _rgbaPixels.size() < static_cast<std::size_t>(_width * _height * 4))
            return false;

        std::ofstream out(_path, std::ios::binary);
        if (!out.is_open())
            return false;

        unsigned char header[18] = {};
        header[2] = 2;
        header[12] = static_cast<unsigned char>(_width & 0xFF);
        header[13] = static_cast<unsigned char>((_width >> 8) & 0xFF);
        header[14] = static_cast<unsigned char>(_height & 0xFF);
        header[15] = static_cast<unsigned char>((_height >> 8) & 0xFF);
        header[16] = 32;
        header[17] = 0x20;
        out.write(reinterpret_cast<const char *>(header), sizeof(header));

        for (int y = 0; y < _height; ++y)
        {
            for (int x = 0; x < _width; ++x)
            {
                const std::size_t offset = static_cast<std::size_t>((y * _width + x) * 4);
                const unsigned char bgra[4] =
                {
                    _rgbaPixels[offset + 2],
                    _rgbaPixels[offset + 1],
                    _rgbaPixels[offset + 0],
                    _rgbaPixels[offset + 3]
                };
                out.write(reinterpret_cast<const char *>(bgra), sizeof(bgra));
            }
        }

        return out.good();
    }

    static bool ExportGltfImageToFolder(
        const tinygltf::Image &_image,
        int _imageIndex,
        const std::filesystem::path &_modelPath,
        const std::filesystem::path &_targetFolder,
        std::string &_outAssetPath,
        std::string &_outError)
    {
        namespace fs = std::filesystem;

        _outAssetPath.clear();

        std::string textureName = _image.name;
        fs::path sourcePath;
        std::string extension = ".tga";

        if (!_image.uri.empty() && _image.uri.rfind("data:", 0) != 0)
        {
            sourcePath = (_modelPath.parent_path() / _image.uri).lexically_normal();
            const fs::path uriPath(_image.uri);
            if (textureName.empty())
                textureName = uriPath.stem().string();
            if (!uriPath.extension().empty())
                extension = uriPath.extension().string();
        }
        else if (_image.mimeType == "image/png")
        {
            extension = ".png";
        }
        else if (_image.mimeType == "image/jpeg" || _image.mimeType == "image/jpg")
        {
            extension = ".jpg";
        }

        textureName = SanitizeAssetFileStem(textureName, "texture_" + std::to_string(_imageIndex));

        std::error_code ec;
        if (!sourcePath.empty() && fs::exists(sourcePath, ec) && !ec)
        {
            fs::path targetPath = (_targetFolder / (textureName + extension)).lexically_normal();
            if (!AreEquivalentPaths(sourcePath, targetPath))
                targetPath = MakeUniqueFilePath(targetPath);

            if (!AreEquivalentPaths(sourcePath, targetPath))
            {
                ec.clear();
                fs::copy_file(sourcePath, targetPath, ec);
                if (ec)
                {
                    _outError = "Failed to copy texture '" + sourcePath.generic_string() + "': " + ec.message();
                    return false;
                }
            }

            _outAssetPath = MakeEditorAssetPath(targetPath);
            (void)AssetManager::GetMetaFile(_outAssetPath);
            return true;
        }

        if (_image.image.empty() || _image.width <= 0 || _image.height <= 0)
        {
            _outError = "Texture image data was empty.";
            return false;
        }

        fs::path targetPath = MakeUniqueFilePath((_targetFolder / (textureName + ".tga")).lexically_normal());
        if (!WriteEditorRgbaTga(targetPath, _image.width, _image.height, _image.image))
        {
            _outError = "Failed to write embedded texture '" + targetPath.generic_string() + "'.";
            return false;
        }

        _outAssetPath = MakeEditorAssetPath(targetPath);
        (void)AssetManager::GetMetaFile(_outAssetPath);
        return true;
    }

    static bool ExportGltfTextureToFolder(
        const tinygltf::Model &_gltfModel,
        int _textureIndex,
        const std::filesystem::path &_modelPath,
        const std::filesystem::path &_targetFolder,
        std::unordered_map<int, std::string> &_exportedImagePaths,
        std::string &_outAssetPath,
        int &_textureExportCount,
        std::string &_outError)
    {
        _outAssetPath.clear();

        if (_textureIndex < 0 || _textureIndex >= static_cast<int>(_gltfModel.textures.size()))
            return false;

        const tinygltf::Texture &texture = _gltfModel.textures[_textureIndex];
        if (texture.source < 0 || texture.source >= static_cast<int>(_gltfModel.images.size()))
            return false;

        auto existing = _exportedImagePaths.find(texture.source);
        if (existing != _exportedImagePaths.end())
        {
            _outAssetPath = existing->second;
            return !_outAssetPath.empty();
        }

        if (!ExportGltfImageToFolder(_gltfModel.images[texture.source], texture.source, _modelPath, _targetFolder, _outAssetPath, _outError))
            return false;

        _exportedImagePaths[texture.source] = _outAssetPath;
        ++_textureExportCount;
        return true;
    }

    struct ModelMaterialExportResult
    {
        int materialCount = 0;
        int textureCount = 0;
        std::string error = {};
    };

    static ModelMaterialExportResult ExportModelMaterialsAndTextures(
        const std::string &_modelPath,
        const std::string &_targetFolder)
    {
        namespace fs = std::filesystem;

        ModelMaterialExportResult result = {};
        const fs::path modelPath(_modelPath);
        const fs::path targetFolder(_targetFolder);

        std::error_code ec;
        fs::create_directories(targetFolder, ec);
        if (ec)
        {
            result.error = "Failed to create export folder '" + targetFolder.generic_string() + "': " + ec.message();
            return result;
        }

        tinygltf::TinyGLTF loader;
        loader.SetImageLoader(LoadTinyGLTFImageDataForEditor, nullptr);

        tinygltf::Model gltfModel;
        std::string error;
        std::string warning;
        bool loaded = false;
        const std::string extension = ToLowerCopy(modelPath.extension().string());
        if (extension == ".glb")
            loaded = loader.LoadBinaryFromFile(&gltfModel, &error, &warning, modelPath.string());
        else if (extension == ".gltf")
            loaded = loader.LoadASCIIFromFile(&gltfModel, &error, &warning, modelPath.string());
        else
        {
            result.error = "Only .glb and .gltf model assets can export materials.";
            return result;
        }

        if (!warning.empty())
            Debug::Warning("tinygltf: %s", warning.c_str());

        if (!loaded)
        {
            result.error = error.empty() ? "Failed to load glTF model." : error;
            return result;
        }

        if (gltfModel.materials.empty())
        {
            result.error = "Model has no glTF materials to export.";
            return result;
        }

        std::unordered_map<int, std::string> exportedImagePaths = {};
        const std::string modelStem = SanitizeAssetFileStem(modelPath.stem().string(), "model");

        for (std::size_t materialIndex = 0; materialIndex < gltfModel.materials.size(); ++materialIndex)
        {
            const tinygltf::Material &gltfMaterial = gltfModel.materials[materialIndex];
            const tinygltf::PbrMetallicRoughness &pbr = gltfMaterial.pbrMetallicRoughness;

            const std::string materialName = SanitizeAssetFileStem(
                gltfMaterial.name,
                modelStem + "_material_" + std::to_string(materialIndex));
            const fs::path materialPath = MakeUniqueFilePath((targetFolder / (materialName + ".material")).lexically_normal());

            YAML::Node root(YAML::NodeType::Map);
            if (MetaFileAsset *shaderMeta = AssetManager::GetMetaFile("assets/shaders/model3d.vs"))
            {
                YAML::Node shaderNode(YAML::NodeType::Map);
                shaderNode["uuid"] = (uint64_t)shaderMeta->uuid;
                shaderNode["path"] = shaderMeta->path;
                root["shader"] = shaderNode;
            }

            Color color(1.0f);
            if (pbr.baseColorFactor.size() == 4u)
            {
                color = Color(
                    static_cast<float>(pbr.baseColorFactor[0]),
                    static_cast<float>(pbr.baseColorFactor[1]),
                    static_cast<float>(pbr.baseColorFactor[2]),
                    static_cast<float>(pbr.baseColorFactor[3]));
            }
            root["color"] = color;
            root["specularValue"] = 0.5f;
            root["roughnessValue"] = static_cast<float>(pbr.roughnessFactor);
            root["metallicValue"] = static_cast<float>(pbr.metallicFactor);
            root["backFaceCulling"] = !gltfMaterial.doubleSided;

            std::string texturePath;
            std::string textureError;
            if (ExportGltfTextureToFolder(gltfModel, pbr.baseColorTexture.index, modelPath, targetFolder, exportedImagePaths, texturePath, result.textureCount, textureError))
                root["albedo"] = MakeAssetRefNode(texturePath);
            else if (!textureError.empty())
                Debug::Warning("%s", textureError.c_str());

            textureError.clear();
            if (ExportGltfTextureToFolder(gltfModel, pbr.metallicRoughnessTexture.index, modelPath, targetFolder, exportedImagePaths, texturePath, result.textureCount, textureError))
            {
                root["roughness"] = MakeAssetRefNode(texturePath);
                root["metallic"] = MakeAssetRefNode(texturePath);
            }
            else if (!textureError.empty())
            {
                Debug::Warning("%s", textureError.c_str());
            }

            textureError.clear();
            if (ExportGltfTextureToFolder(gltfModel, gltfMaterial.emissiveTexture.index, modelPath, targetFolder, exportedImagePaths, texturePath, result.textureCount, textureError))
                root["emission"] = MakeAssetRefNode(texturePath);
            else if (!textureError.empty())
                Debug::Warning("%s", textureError.c_str());

            auto setTextureUniform = [&](const char *_uniformName, const std::string &_texturePath) -> void
            {
                YAML::Node uniformNode(YAML::NodeType::Map);
                uniformNode["type"] = "texture";
                uniformNode["value"] = MakeAssetRefNode(_texturePath);
                root["uniforms"][_uniformName] = uniformNode;
            };

            textureError.clear();
            if (ExportGltfTextureToFolder(gltfModel, gltfMaterial.normalTexture.index, modelPath, targetFolder, exportedImagePaths, texturePath, result.textureCount, textureError))
                setTextureUniform("normalTexture", texturePath);
            else if (!textureError.empty())
                Debug::Warning("%s", textureError.c_str());

            textureError.clear();
            if (ExportGltfTextureToFolder(gltfModel, gltfMaterial.occlusionTexture.index, modelPath, targetFolder, exportedImagePaths, texturePath, result.textureCount, textureError))
                setTextureUniform("occlusionTexture", texturePath);
            else if (!textureError.empty())
                Debug::Warning("%s", textureError.c_str());

            std::ofstream out(materialPath);
            if (!out.is_open())
            {
                result.error = "Failed to create material '" + materialPath.generic_string() + "'.";
                return result;
            }

            out << root;
            if (!out.good())
            {
                result.error = "Failed to write material '" + materialPath.generic_string() + "'.";
                return result;
            }

            const std::string materialAssetPath = MakeEditorAssetPath(materialPath);
            (void)AssetManager::GetMetaFile(materialAssetPath);
            ++result.materialCount;
        }

        return result;
    }

    static void ApplyMaterialNodeToAsset(const YAML::Node &_root, MaterialAsset *_material)
    {
        if (_material == nullptr)
            return;

        _material->info = 0u;
        _material->shaderId = -1;
        _material->albedoId = -1;
        _material->specularId = -1;
        _material->roughnessId = -1;
        _material->metallicId = -1;
        _material->emissionId = -1;
        _material->color = Color(1.0f);
        _material->specularValue = 0.5f;
        _material->roughnessValue = 0.5f;
        _material->metallicValue = 0.0f;
        _material->materialFields = MaterialFields();

        if (YAML::Node shaderNode = _root["shader"])
        {
            std::string shaderPath = ResolveAssetRefPath(shaderNode);
            if (!shaderPath.empty())
            {
                if (shaderPath.size() > 3 && shaderPath.ends_with(".vs"))
                    shaderPath = shaderPath.substr(0, shaderPath.size() - 3);
                else if (shaderPath.size() > 3 && shaderPath.ends_with(".fs"))
                    shaderPath = shaderPath.substr(0, shaderPath.size() - 3);

                _material->shaderId = AssetManager::LoadShader(shaderPath);
                if (_material->shaderId >= 0)
                    _material->info |= MATERIAL_HAS_SHADER;
            }
        }

        if (YAML::Node albedoNode = _root["albedo"])
        {
            std::string path = ResolveAssetRefPath(albedoNode);
            if (!path.empty())
            {
                _material->albedoId = AssetManager::LoadTexture(path);
                if (_material->albedoId >= 0)
                    _material->info |= MATERIAL_HAS_ALBEDO;
            }
        }

        if (YAML::Node specularNode = _root["specular"])
        {
            std::string path = ResolveAssetRefPath(specularNode);
            if (!path.empty())
            {
                _material->specularId = AssetManager::LoadTexture(path);
                if (_material->specularId >= 0)
                    _material->info |= MATERIAL_HAS_SPECULAR;
            }
        }

        if (YAML::Node roughnessNode = _root["roughness"])
        {
            std::string path = ResolveAssetRefPath(roughnessNode);
            if (!path.empty())
            {
                _material->roughnessId = AssetManager::LoadTexture(path);
                if (_material->roughnessId >= 0)
                    _material->info |= MATERIAL_HAS_ROUGHNESS;
            }
        }

        if (YAML::Node metallicNode = _root["metallic"])
        {
            std::string path = ResolveAssetRefPath(metallicNode);
            if (!path.empty())
            {
                _material->metallicId = AssetManager::LoadTexture(path);
                if (_material->metallicId >= 0)
                    _material->info |= MATERIAL_HAS_METALLIC;
            }
        }

        if (YAML::Node emissionNode = _root["emission"])
        {
            std::string path = ResolveAssetRefPath(emissionNode);
            if (!path.empty())
            {
                _material->emissionId = AssetManager::LoadTexture(path);
                if (_material->emissionId >= 0)
                    _material->info |= MATERIAL_HAS_EMISSION;
            }
        }

        if (YAML::Node colorNode = _root["color"])
        {
            _material->color = colorNode.as<Color>(Color(1.0f));
            _material->info |= MATERIAL_HAS_COLOR;
        }

        _material->specularValue = _root["specularValue"].as<float>(0.5f);
        _material->roughnessValue = _root["roughnessValue"].as<float>(0.5f);
        _material->metallicValue = _root["metallicValue"].as<float>(0.0f);

        if (YAML::Node cullNode = _root["backFaceCulling"]; cullNode.as<bool>(false))
            _material->info |= MATERIAL_BACK_FACE_CULLING;

        if (YAML::Node cullNode = _root["frontFaceCulling"]; cullNode.as<bool>(false))
            _material->info |= MATERIAL_FRONT_FACE_CULLING;

        ReadMaterialUniformsFromNode(_root, _material->materialFields);
    }

    std::vector<AddComponentEntry> BuildAddComponentEntries(App &_app, Entity &_entity)
    {
        std::vector<AddComponentEntry> entries = {};
        for (ScriptConf &conf : _app.GetScriptRegistry())
        {
            if (conf.name == PrefabInstance::ScriptName)
                continue;

            if (conf.Has(_entity))
                continue;

            entries.push_back({
                .componentName = conf.name,
                .displayName = GetAddComponentDisplayName(conf.name),
            });
        }
        return entries;
    }

    void Editor::Init(Window *_window)
    {
#if CANIS_EDITOR
        // if (GetProjectConfig().editor == false)
        //     return;
        //{

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        (void)io;
        //ImGui::LoadIniSettingsFromMemory("");
        static std::string imguiIniPath = []() -> std::string
        {
            namespace fs = std::filesystem;

            const char* basePath = SDL_GetBasePath();
            const fs::path runtimeBasePath = basePath != nullptr ? fs::path(basePath) : fs::current_path();

            const fs::path userSettingsDir = runtimeBasePath / "user_settings";
            std::error_code ec;
            fs::create_directories(userSettingsDir, ec);
            const fs::path imguiPath = userSettingsDir / "imgui.ini";
            EnsureDefaultImguiIniFile(imguiPath);
            return imguiPath.string();
        }();
        io.IniFilename = imguiIniPath.c_str();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking
        io.ConfigWindowsMoveFromTitleBarOnly = true;

#ifdef __EMSCRIPTEN__

#else
        // Keep editor windows docked inside the main SDL window.
        io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
        io.ConfigViewportsNoAutoMerge = false;
        io.ConfigViewportsNoTaskBarIcon = true;
#endif

        // Setup Dear ImGui style and scaling.
        m_editorUiScale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
        if (m_editorUiScale <= 0.0f)
            m_editorUiScale = 1.0f;

        m_editorThemeSelection = NormalizeEditorThemeSelection(Canis::GetEditorConfig().theme);
        Canis::GetEditorConfig().theme = m_editorThemeSelection;
        m_editorFontScale = NormalizeEditorFontScale(Canis::GetEditorConfig().fontScale);
        Canis::GetEditorConfig().fontScale = m_editorFontScale;
        m_reloadBuildAutoCloseOnSuccess = Canis::GetEditorConfig().reloadBuildAutoCloseOnSuccess;
        ApplyEditorThemeStyle(m_editorThemeSelection, m_editorUiScale);
        RefreshEditorFontOptions();

        ImGuiStyle &style = ImGui::GetStyle();
        style.FontScaleDpi = m_editorUiScale; // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)
        io.ConfigDpiScaleFonts = true;     // [Experimental] Automatically overwrite style.FontScaleDpi in Begin() when Monitor DPI changes. This will scale fonts but _NOT_ scale sizes/padding for now.
        io.ConfigDpiScaleViewports = true; // [Experimental] Scale Dear ImGui and Platform Windows when Monitor DPI changes.

        // Setup Platform/Renderer backends
        ImGui_ImplSDL3_InitForOpenGL((SDL_Window *)_window->GetSDLWindow(), (SDL_GLContext)_window->GetGLContext());
        ImGui_ImplOpenGL3_Init(OPENGLVERSION);

        const std::string initialFontPath =
            (m_editorFontSelection > 0 && m_editorFontSelection < static_cast<int>(m_editorFontPaths.size())) ?
            m_editorFontPaths[m_editorFontSelection] : std::string();
        ApplyEditorFont(initialFontPath);

        m_assetPaths = FindFilesInFolder("assets", "");
        PrimeAssetHotReloadState();

        m_gameViewportWidth = _window->GetWindowWidth();
        m_gameViewportHeight = _window->GetWindowHeight();
        EnsureGameRenderTarget(m_gameViewportWidth, m_gameViewportHeight);
        m_playViewportWidth = _window->GetWindowWidth();
        m_playViewportHeight = _window->GetWindowHeight();
        EnsurePlayRenderTarget(m_playViewportWidth, m_playViewportHeight);
#endif
    }

    Editor::~Editor()
    {
        if (m_reloadBuildThread.joinable())
            m_reloadBuildThread.join();

        DestroyGameRenderTarget();
        DestroyGamePickingRenderTarget();
        DestroyPlayRenderTarget();
        DestroyRenderTarget(m_gameViewPostProcessTarget);
        DestroyRenderTarget(m_playViewPostProcessTarget);
    }

    void Editor::BeginGameRender(Window* _window)
    {
#if CANIS_EDITOR
        int targetWidth = (m_gameViewportWidth > 0) ? m_gameViewportWidth : _window->GetWindowWidth();
        int targetHeight = (m_gameViewportHeight > 0) ? m_gameViewportHeight : _window->GetWindowHeight();

        EnsureGameRenderTarget(targetWidth, targetHeight);
        if (m_gameFramebuffer == 0)
            return;

        _window->SetRenderSize(targetWidth, targetHeight);

        glBindFramebuffer(GL_FRAMEBUFFER, m_gameFramebuffer);
        glViewport(0, 0, targetWidth, targetHeight);

        Color clear = _window->GetClearColor();
        glClearColor(clear.r, clear.g, clear.b, clear.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#endif
    }

    void Editor::BeginPlayRender(Window* _window)
    {
#if CANIS_EDITOR
        int targetWidth = (m_playViewportWidth > 0) ? m_playViewportWidth : _window->GetWindowWidth();
        int targetHeight = (m_playViewportHeight > 0) ? m_playViewportHeight : _window->GetWindowHeight();

        EnsurePlayRenderTarget(targetWidth, targetHeight);
        if (m_playFramebuffer == 0)
            return;

        _window->SetRenderSize(targetWidth, targetHeight);

        glBindFramebuffer(GL_FRAMEBUFFER, m_playFramebuffer);
        glViewport(0, 0, targetWidth, targetHeight);

        Color clear = _window->GetClearColor();
        glClearColor(clear.r, clear.g, clear.b, clear.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#endif
    }

    void Editor::EndGameRender(Window* _window)
    {
#if CANIS_EDITOR
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, _window->GetWindowWidth(), _window->GetWindowHeight());
#endif
    }

    void Editor::Draw(Scene *_scene, Window *_window, App *_app, GameCodeObject *_gameSharedLib, float _deltaTime)
    {
#if CANIS_EDITOR
        // if (GetProjectConfig().editor)
        //{
        const bool sceneChanged = m_scene != _scene;
        if (sceneChanged)
        {
            Debug::Log("new scene");
            m_hierarchyRootOrder.clear();
            m_animationRestoreBindings.clear();
            m_animationPreviewTargetUUID = UUID(0);
            m_animationPreviewClipPath.clear();
        }
        m_app = _app;
        m_scene = _scene;
        m_window = _window;
        m_gameSharedLib = _gameSharedLib;
        m_gameInputWindowID = SDL_GetWindowID((SDL_Window *)m_window->GetSDLWindow());
        if (sceneChanged)
            ResetSceneHistory();
        PollAssetHotReload(_deltaTime);
        ProcessModelMaterialExportDialog();

        // Pass 1: runtime/game camera (used by Game panel).
        m_scene->ClearEditorCameraOverrides();
        BeginPlayRender(m_window);
        m_scene->Render(_deltaTime);
        m_playRenderProjection = m_scene->GetLastRenderProjection();
        EndGameRender(m_window);

        // Pass 2: editor scene camera (used by Scene panel + gizmos).
        ApplyInternalSceneCamera(_deltaTime);
        BeginGameRender(m_window);
        m_scene->Render(_deltaTime);
        m_gameRenderProjection = m_scene->GetLastRenderProjection();
        RenderGameDebug();
        EndGameRender(m_window);
        m_scene->ClearEditorCameraOverrides();

        // Keep logical gameplay size set to Game panel size for scripts/input math between frames.
        const int gameplayWidth = (m_playViewportWidth > 0) ? m_playViewportWidth : m_window->GetWindowWidth();
        const int gameplayHeight = (m_playViewportHeight > 0) ? m_playViewportHeight : m_window->GetWindowHeight();
        m_window->SetRenderSize(gameplayWidth, gameplayHeight);

        BeginSceneHistoryFrame();

        if (m_editorFontApplyQueued)
        {
            ApplyEditorFont(m_queuedEditorFontPath);
            m_editorFontApplyQueued = false;

            if (m_editorFontApplyShouldSaveConfig)
            {
                Canis::SaveEditorConfig();
                m_editorFontApplyShouldSaveConfig = false;
            }
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        DrawMainDockspace();

        bool refresh = false;
        if (m_showHierarchyPanel)
            refresh = DrawHierarchyPanel();
        if (m_showAnimationPanel)
            DrawAnimationWindow(_deltaTime);
        if (m_showAnimatorPanel)
            DrawAnimatorWindow();
        if (m_showInspectorPanel)
            DrawInspectorPanel(refresh);
        if (m_showEnvironmentPanel)
            DrawEnvironment();
        if (m_showSystemsPanel)
            DrawSystemPanel();
        if (m_showAssetsPanel)
            DrawAssetsPanel();
        if (m_showShaderGraphPanel)
            DrawShaderGraphWindow();
        if (m_showScriptsPanel)
            DrawScriptsPanel();
        if (m_showProjectSettingsPanel)
            DrawProjectSettings();
        if (m_showConsolePanel)
            DrawConsolePanel();
        if (m_showScenePanel)
            DrawSceneView();
        else
        {
            m_gameViewHovered = false;
            m_sceneViewClicked = false;
        }

        if (m_showGamePanel)
            DrawGameView();
        else
        {
            m_playViewHovered = false;
            if (m_scene != nullptr)
                m_scene->GetInputManager().ClearGameMouseViewport();
        }

        UpdatePlayMouseCapture();
        DrawEditorPanel(); // draw last
        ProcessQueuedPrefabRebuilds();

        if (m_sceneCameraMode == SceneCameraMode::SCENE_CAMERA_3D)
            SelectModel3D();
        else
            SelectSprite2D();

        EndSceneHistoryFrame();

        // find camera and verfy target entity
        m_debugDraw = DebugDraw::NONE;
        Camera2D *camera2D = nullptr;

        if (m_index > -1 && m_index < m_scene->GetEntities().size() && m_scene->GetEntities()[m_index] != nullptr)
        {
            Entity &entity = *m_scene->GetEntities()[m_index];

            std::vector<Entity *> &entities = m_scene->GetEntities();

            for (Entity *entity : entities)
            {
                if (entity == nullptr)
                    continue;

                Camera2D *camera = (entity != nullptr && entity->HasComponent<Camera2D>() ? &entity->GetComponent<Camera2D>() : nullptr);

                if (camera == nullptr)
                    continue;

                camera2D = camera;
            }

            if (entity.HasComponent<RectTransform>() && camera2D)
            {
                m_debugDraw = DebugDraw::RECT;
            }
        }

        // rendering
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        ImGuiIO &io = ImGui::GetIO();
        (void)io;

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call SDL_GL_MakeCurrent(window, gl_context) directly)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            SDL_Window *backup_current_window = SDL_GL_GetCurrentWindow();
            SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
        }

#endif
    }

    void Editor::DrawMainDockspace()
    {
        ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_None;
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking;

        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImVec2 dockspacePos = viewport->WorkPos;
        ImVec2 dockspaceSize = viewport->WorkSize;
        const float toolbarHeight = GetEditorToolbarHeight();
        dockspacePos.y += toolbarHeight;
        dockspaceSize.y = std::max(0.0f, dockspaceSize.y - toolbarHeight);
        ImGui::SetNextWindowPos(dockspacePos);
        ImGui::SetNextWindowSize(dockspaceSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        windowFlags |= ImGuiWindowFlags_NoTitleBar;
        windowFlags |= ImGuiWindowFlags_NoCollapse;
        windowFlags |= ImGuiWindowFlags_NoResize;
        windowFlags |= ImGuiWindowFlags_NoMove;
        windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
        windowFlags |= ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("MainDockspace", nullptr, windowFlags);
        ImGui::PopStyleVar(3);

        m_mainDockspaceID = ImGui::GetID("MainDockspaceID");
        ImGui::DockSpace(m_mainDockspaceID, ImVec2(0.0f, 0.0f), dockspaceFlags);
        ImGui::End();
    }

    void Editor::ApplyInternalSceneCamera(float _deltaTime)
    {
        (void)_deltaTime;

        if (m_scene == nullptr || m_window == nullptr)
            return;

        m_scene->ClearEditorCameraOverrides();

        // Scene view should always use the editor camera.
        // Game panel already renders with runtime/game cameras in pass 1.
        if (m_mode == EditorMode::HIDDEN)
            return;

        InputManager& input = m_scene->GetInputManager();
        const bool rightClickNavigation = m_gameViewHovered && input.GetRightClick();

        const int renderWidth = std::max(1, (m_gameViewportWidth > 0) ? m_gameViewportWidth : m_window->GetWindowWidth());
        const int renderHeight = std::max(1, (m_gameViewportHeight > 0) ? m_gameViewportHeight : m_window->GetWindowHeight());

        if (m_sceneCameraMode == SceneCameraMode::SCENE_CAMERA_3D)
        {
            if (rightClickNavigation)
            {
                m_editorCamera3DYaw += input.mouseRel.x * m_editorCamera3DLookSensitivity;
                m_editorCamera3DPitch -= input.mouseRel.y * m_editorCamera3DLookSensitivity;
                m_editorCamera3DPitch = std::clamp(m_editorCamera3DPitch, -89.0f, 89.0f);
            }

            const float yaw = DEG2RAD * m_editorCamera3DYaw;
            const float pitch = DEG2RAD * m_editorCamera3DPitch;

            Vector3 forward = Vector3(
                std::cos(pitch) * std::cos(yaw),
                std::sin(pitch),
                std::cos(pitch) * std::sin(yaw));
            forward = glm::normalize(forward);

            const Vector3 worldUp = Vector3(0.0f, 1.0f, 0.0f);
            Vector3 right = glm::normalize(glm::cross(forward, worldUp));
            Vector3 up = glm::normalize(glm::cross(right, forward));

            if (rightClickNavigation)
            {
                float moveSpeed = m_editorCamera3DMoveSpeed * Time::UnscaledDeltaTime();
                if (input.GetKey(Canis::Key::LSHIFT) || input.GetKey(Canis::Key::RSHIFT))
                    moveSpeed *= 3.0f;

                if (input.GetKey(Canis::Key::W))
                    m_editorCamera3DPosition += forward * moveSpeed;
                if (input.GetKey(Canis::Key::S))
                    m_editorCamera3DPosition -= forward * moveSpeed;
                if (input.GetKey(Canis::Key::A))
                    m_editorCamera3DPosition -= right * moveSpeed;
                if (input.GetKey(Canis::Key::D))
                    m_editorCamera3DPosition += right * moveSpeed;
                if (input.GetKey(Canis::Key::Q))
                    m_editorCamera3DPosition -= worldUp * moveSpeed;
                if (input.GetKey(Canis::Key::E))
                    m_editorCamera3DPosition += worldUp * moveSpeed;
            }

            const Matrix4 view = glm::lookAt(m_editorCamera3DPosition, m_editorCamera3DPosition + forward, up);
            const float aspect = static_cast<float>(renderWidth) / static_cast<float>(renderHeight);
            const Matrix4 projection = glm::perspective(DEG2RAD * m_editorCamera3DFovDegrees, aspect, 0.05f, 2000.0f);

            m_scene->SetEditorCamera3DOverride(view, projection);
        }
        else
        {
            if (rightClickNavigation)
            {
                m_editorCamera2DPosition.x -= input.mouseRel.x;
                m_editorCamera2DPosition.y += input.mouseRel.y;
            }

            Matrix4 projection = glm::ortho(0.0f, static_cast<float>(renderWidth), 0.0f,
                                            static_cast<float>(renderHeight), 0.0f, 100.0f);
            Matrix4 view = Matrix4(1.0f);
            view = glm::translate(view, Vector3(-m_editorCamera2DPosition.x + renderWidth * 0.5f,
                                                -m_editorCamera2DPosition.y + renderHeight * 0.5f, 0.0f));
            view = glm::scale(view, Vector3(m_editorCamera2DScale, m_editorCamera2DScale, 0.0f));
            m_scene->SetEditorCamera2DOverride(projection * view, m_editorCamera2DPosition);
        }
    }

    void Editor::RenderGameDebug()
    {
#if CANIS_EDITOR
        if (!m_scene)
            return;

        Camera2D *camera2D = nullptr;
        std::vector<Entity *> &entities = m_scene->GetEntities();

        for (Entity *entity : entities)
        {
            if (entity == nullptr)
                continue;

            Camera2D *camera = (entity != nullptr && entity->HasComponent<Camera2D>() ? &entity->GetComponent<Camera2D>() : nullptr);
            if (camera)
            {
                camera2D = camera;
                break;
            }
        }

        if (!camera2D)
            return;

        DrawSelectionMouseDebug(camera2D);

        if (m_index >= 0 && m_index < m_scene->GetEntities().size() && m_scene->GetEntities()[m_index] != nullptr)
        {
            Entity &selected = *m_scene->GetEntities()[m_index];
            if (selected.HasComponent<RectTransform>())
                DrawBoundingBox(camera2D);
        }
        
        //ImGui::Render();
        //ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#endif
    }

    void Editor::StopPlayMode()
    {
        if (m_mode != EditorMode::PLAY && m_mode != EditorMode::PAUSE)
            return;

        ReleasePlayMouseCapture();

        AudioManager::StopMusic();
        AudioManager::StopAllSounds();

        if (!g_lastPlaySceneNode || m_scene == nullptr || m_app == nullptr)
            return;

        m_stopPlayModeRequested = false;
        Time::SetTargetFPS(Canis::GetProjectConfig().frameLimitEditor + 0.0f);
        Time::SetTimeScale(1.0f);
        m_scene->SetPaused(false);
        m_mode = EditorMode::EDIT;
        m_scene->Unload();
        m_scene->m_path = g_lastPlayScenePath;
        m_scene->LoadSceneNode(g_lastPlaySceneNode);
        ResetSceneHistory();
    }

    void Editor::ReleasePlayMouseCapture()
    {
        m_playMouseCaptured = false;

        if (m_window != nullptr && m_window->IsMouseLocked())
            m_window->LockMouse(false);
    }

    void Editor::UpdatePlayMouseCapture()
    {
        if (m_window == nullptr)
            return;

        if (m_mode != EditorMode::PLAY || !m_showGamePanel)
        {
            ReleasePlayMouseCapture();
            return;
        }

        if (!m_window->IsMouseLocked())
        {
            m_playMouseCaptured = false;
            return;
        }

        m_playMouseCaptured = true;
    }

    void Editor::ResetVertexSnapDrag()
    {
        m_vertexSnapDragActive = false;
        m_vertexSnapAxesMask = 0;
        m_vertexSnapDragDirection = Vector3(0.0f);
    }

    bool Editor::TryApplyVertexSnap(Entity *_selected, const Matrix4 &_selectedWorldMatrix, Vector3 &_worldPosition, const Vector3 &_dragDelta)
    {
        if (_selected == nullptr || m_scene == nullptr)
            return false;

        static constexpr std::size_t kMaxSnapVerticesPerModel = 1024u;
        static constexpr std::size_t kMaxSnapPairChecks = 2000000u;
        static constexpr float kMaxSnapDistance = 0.75f;

        const float maxDragComponent = std::max({ std::abs(_dragDelta.x), std::abs(_dragDelta.y), std::abs(_dragDelta.z) });
        if (maxDragComponent <= 0.00001f)
            return false;

        const float axisThreshold = std::max(0.0005f, maxDragComponent * 0.15f);
        const int detectedAxesMask =
            (std::abs(_dragDelta.x) >= axisThreshold ? 1 : 0) |
            (std::abs(_dragDelta.y) >= axisThreshold ? 2 : 0) |
            (std::abs(_dragDelta.z) >= axisThreshold ? 4 : 0);
        if (detectedAxesMask == 0)
            return false;

        if (!m_vertexSnapDragActive)
        {
            m_vertexSnapDragActive = true;
            m_vertexSnapAxesMask = detectedAxesMask;
            m_vertexSnapDragDirection = Vector3(
                (detectedAxesMask & 1) != 0 ? (_dragDelta.x < 0.0f ? -1.0f : 1.0f) : 0.0f,
                (detectedAxesMask & 2) != 0 ? (_dragDelta.y < 0.0f ? -1.0f : 1.0f) : 0.0f,
                (detectedAxesMask & 4) != 0 ? (_dragDelta.z < 0.0f ? -1.0f : 1.0f) : 0.0f);
        }

        const int axesMask = (m_vertexSnapAxesMask != 0) ? m_vertexSnapAxesMask : detectedAxesMask;
        const bool useX = (axesMask & 1) != 0;
        const bool useY = (axesMask & 2) != 0;
        const bool useZ = (axesMask & 4) != 0;
        const int activeAxisCount = (useX ? 1 : 0) + (useY ? 1 : 0) + (useZ ? 1 : 0);
        auto lockedDirection = [&](int _axis) -> float
        {
            const float direction = m_vertexSnapDragDirection[_axis];
            if (direction != 0.0f)
                return direction < 0.0f ? -1.0f : 1.0f;

            return _dragDelta[_axis] < 0.0f ? -1.0f : 1.0f;
        };

        if (activeAxisCount == 1)
        {
            const int axis = useX ? 0 : (useY ? 1 : 2);
            static constexpr float kBoundsOverlapPadding = 0.05f;
            static constexpr float kOppositeFaceTolerance = 0.05f;

            Vector3 selectedMin(0.0f);
            Vector3 selectedMax(0.0f);
            if (BuildEntityModelWorldBounds(_selected, _selectedWorldMatrix, selectedMin, selectedMax))
            {
                const float direction = lockedDirection(axis);
                const bool movingNegative = direction < 0.0f;
                float bestDistance = kMaxSnapDistance;
                float bestAxisDelta = 0.0f;
                bool foundBoundsSnap = false;

                for (Entity *target : m_scene->GetEntities())
                {
                    if (target == nullptr || target == _selected || !target->HasComponents<Transform, Model>())
                        continue;

                    Vector3 targetMin(0.0f);
                    Vector3 targetMax(0.0f);
                    Transform &targetTransform = target->GetComponent<Transform>();
                    if (!targetTransform.IsActiveInHierarchy())
                        continue;

                    const Matrix4 targetMatrix = targetTransform.GetModelMatrix();
                    if (!BuildEntityModelWorldBounds(target, targetMatrix, targetMin, targetMax))
                        continue;

                    bool overlapsInactiveAxes = true;
                    for (int inactiveAxis = 0; inactiveAxis < 3; ++inactiveAxis)
                    {
                        if (inactiveAxis == axis)
                            continue;

                        if (!RangesOverlap(
                            selectedMin[inactiveAxis],
                            selectedMax[inactiveAxis],
                            targetMin[inactiveAxis],
                            targetMax[inactiveAxis],
                            kBoundsOverlapPadding))
                        {
                            overlapsInactiveAxes = false;
                            break;
                        }
                    }

                    if (!overlapsInactiveAxes)
                        continue;

                    const float selectedSurface = movingNegative ? selectedMin[axis] : selectedMax[axis];
                    const float targetSurface = movingNegative ? targetMax[axis] : targetMin[axis];
                    const float axisDelta = targetSurface - selectedSurface;
                    if (axisDelta * direction < -kOppositeFaceTolerance)
                        continue;

                    const float distance = std::abs(axisDelta);
                    if (distance < bestDistance)
                    {
                        bestDistance = distance;
                        bestAxisDelta = axisDelta;
                        foundBoundsSnap = true;
                    }
                }

                if (foundBoundsSnap)
                {
                    _worldPosition[axis] += bestAxisDelta;
                    return true;
                }
            }
        }

        std::vector<Vector3> selectedVertices = {};
        if (!BuildEntityModelWorldVertices(_selected, _selectedWorldMatrix, selectedVertices, kMaxSnapVerticesPerModel))
            return false;

        float bestDistanceSquared = kMaxSnapDistance * kMaxSnapDistance;
        Vector3 bestConstrainedDelta = Vector3(0.0f);
        bool found = false;

        std::vector<Entity *> &entities = m_scene->GetEntities();
        for (Entity *target : entities)
        {
            if (target == nullptr || target == _selected || !target->HasComponents<Transform, Model>())
                continue;

            std::vector<Vector3> targetVertices = {};
            Transform &targetTransform = target->GetComponent<Transform>();
            if (!targetTransform.IsActiveInHierarchy())
                continue;

            const Matrix4 targetMatrix = targetTransform.GetModelMatrix();
            if (!BuildEntityModelWorldVertices(target, targetMatrix, targetVertices, kMaxSnapVerticesPerModel))
                continue;

            if (selectedVertices.size() * targetVertices.size() > kMaxSnapPairChecks)
                continue;

            for (const Vector3 &selectedVertex : selectedVertices)
            {
                for (const Vector3 &targetVertex : targetVertices)
                {
                    const Vector3 delta = targetVertex - selectedVertex;
                    const Vector3 constrainedDelta(
                        useX ? delta.x : 0.0f,
                        useY ? delta.y : 0.0f,
                        useZ ? delta.z : 0.0f);
                    const float distanceSquared = glm::dot(constrainedDelta, constrainedDelta);
                    if (distanceSquared < bestDistanceSquared)
                    {
                        bestDistanceSquared = distanceSquared;
                        bestConstrainedDelta = constrainedDelta;
                        found = true;
                    }
                }
            }
        }

        if (!found)
            return false;

        if (glm::dot(bestConstrainedDelta, bestConstrainedDelta) <= 0.0000001f)
            return false;

        _worldPosition += bestConstrainedDelta;
        return true;
    }

    void Editor::PrimeAssetHotReloadState()
    {
        m_assetHotReloadWriteTimes.clear();
        m_pendingHotReloadAssets.clear();
        m_assetHotReloadPollTimer = 0.0f;

        m_assetPaths = FindFilesInFolder("assets", "");
        for (const std::string &path : m_assetPaths)
        {
            if (!IsHotReloadEligibleAsset(path))
                continue;

            std::error_code ec;
            const auto writeTime = std::filesystem::last_write_time(path, ec);
            if (!ec)
                m_assetHotReloadWriteTimes[path] = writeTime;
        }
    }

    void Editor::PollAssetHotReload(float _deltaTime)
    {
        if (!m_hotReloadAssets)
            return;

        for (auto it = m_pendingHotReloadAssets.begin(); it != m_pendingHotReloadAssets.end();)
        {
            std::error_code ec;
            const auto currentWriteTime = std::filesystem::last_write_time(it->first, ec);
            if (ec)
            {
                it = m_pendingHotReloadAssets.erase(it);
                continue;
            }

            if (currentWriteTime != it->second.writeTime)
            {
                it->second.writeTime = currentWriteTime;
                it->second.debounceSeconds = 0.35f;
                ++it;
                continue;
            }

            it->second.debounceSeconds -= std::max(0.0f, _deltaTime);
            if (it->second.debounceSeconds > 0.0f)
            {
                ++it;
                continue;
            }

            const std::string path = it->first;
            const bool reloaded = AssetManager::ReloadAsset(path);
            if (reloaded)
            {
                RefreshMetaFileTimestamp(path);
                Debug::Log("Hot reloaded asset: %s", path.c_str());
            }
            else
            {
                Debug::Warning("Failed to hot reload asset: %s", path.c_str());
            }

            m_assetHotReloadWriteTimes[path] = currentWriteTime;
            it = m_pendingHotReloadAssets.erase(it);
        }

        m_assetHotReloadPollTimer -= std::max(0.0f, _deltaTime);
        if (m_assetHotReloadPollTimer > 0.0f)
            return;

        m_assetHotReloadPollTimer = 0.5f;
        m_assetPaths = FindFilesInFolder("assets", "");

        std::unordered_set<std::string> currentPaths = {};
        currentPaths.reserve(m_assetPaths.size());
        for (const std::string &path : m_assetPaths)
        {
            currentPaths.insert(path);
            if (!IsHotReloadEligibleAsset(path))
                continue;

            std::error_code ec;
            const auto writeTime = std::filesystem::last_write_time(path, ec);
            if (ec)
                continue;

            auto knownIt = m_assetHotReloadWriteTimes.find(path);
            if (knownIt == m_assetHotReloadWriteTimes.end())
            {
                m_assetHotReloadWriteTimes[path] = writeTime;
                continue;
            }

            if (knownIt->second != writeTime)
            {
                PendingHotReloadAsset pending = {};
                pending.writeTime = writeTime;
                pending.debounceSeconds = 0.35f;
                m_pendingHotReloadAssets[path] = pending;
            }
        }

        for (auto it = m_assetHotReloadWriteTimes.begin(); it != m_assetHotReloadWriteTimes.end();)
        {
            if (currentPaths.contains(it->first))
            {
                ++it;
                continue;
            }

            m_pendingHotReloadAssets.erase(it->first);
            it = m_assetHotReloadWriteTimes.erase(it);
        }
    }

    void Editor::RequestModelMaterialExport(const std::string &_modelPath)
    {
        if (m_window == nullptr)
            return;

        {
            std::lock_guard<std::mutex> lock(m_modelMaterialExportDialog.mutex);
            if (m_modelMaterialExportDialog.active)
            {
                Debug::Warning("A model material export folder picker is already open.");
                return;
            }

            namespace fs = std::filesystem;
            fs::path modelPath(_modelPath);
            if (!modelPath.is_absolute())
                modelPath = fs::absolute(modelPath);

            m_modelMaterialExportDialog.modelPath = _modelPath;
            m_modelMaterialExportDialog.defaultLocation = modelPath.parent_path().string();
            m_modelMaterialExportDialog.selectedFolder.clear();
            m_modelMaterialExportDialog.error.clear();
            m_modelMaterialExportDialog.pending = false;
            m_modelMaterialExportDialog.canceled = false;
            m_modelMaterialExportDialog.active = true;
        }

        auto onFolderSelected = [](void *_userdata, const char * const *_filelist, int)
        {
            ModelMaterialExportDialogState *dialog = static_cast<ModelMaterialExportDialogState*>(_userdata);
            if (dialog == nullptr)
                return;

            std::lock_guard<std::mutex> lock(dialog->mutex);
            dialog->selectedFolder.clear();
            dialog->error.clear();
            dialog->canceled = false;
            dialog->pending = true;
            dialog->active = false;

            if (_filelist == nullptr)
            {
                dialog->error = SDL_GetError();
                if (dialog->error.empty())
                    dialog->error = "Could not open folder dialog.";
                return;
            }

            if (_filelist[0] == nullptr)
            {
                dialog->canceled = true;
                return;
            }

            dialog->selectedFolder = _filelist[0];
        };

        std::string defaultLocationStorage;
        {
            std::lock_guard<std::mutex> lock(m_modelMaterialExportDialog.mutex);
            defaultLocationStorage = m_modelMaterialExportDialog.defaultLocation;
        }
        const char *defaultLocation = defaultLocationStorage.empty() ? nullptr : defaultLocationStorage.c_str();

        SDL_ShowOpenFolderDialog(
            onFolderSelected,
            &m_modelMaterialExportDialog,
            static_cast<SDL_Window*>(m_window->GetSDLWindow()),
            defaultLocation,
            false);
    }

    void Editor::ProcessModelMaterialExportDialog()
    {
        std::string modelPath;
        std::string selectedFolder;
        std::string error;
        bool pending = false;
        bool canceled = false;

        {
            std::lock_guard<std::mutex> lock(m_modelMaterialExportDialog.mutex);
            pending = m_modelMaterialExportDialog.pending;
            if (!pending)
                return;

            modelPath = m_modelMaterialExportDialog.modelPath;
            selectedFolder = m_modelMaterialExportDialog.selectedFolder;
            error = m_modelMaterialExportDialog.error;
            canceled = m_modelMaterialExportDialog.canceled;

            m_modelMaterialExportDialog.pending = false;
            m_modelMaterialExportDialog.modelPath.clear();
            m_modelMaterialExportDialog.selectedFolder.clear();
            m_modelMaterialExportDialog.error.clear();
            m_modelMaterialExportDialog.canceled = false;
        }

        if (!error.empty())
        {
            Debug::Warning("Model material export failed: %s", error.c_str());
            return;
        }

        if (canceled || selectedFolder.empty())
            return;

        const ModelMaterialExportResult result = ExportModelMaterialsAndTextures(modelPath, selectedFolder);
        if (!result.error.empty())
        {
            Debug::Warning("Model material export failed: %s", result.error.c_str());
            return;
        }

        Debug::Log(
            "Exported %d material(s) and %d texture(s) from '%s' to '%s'.",
            result.materialCount,
            result.textureCount,
            modelPath.c_str(),
            selectedFolder.c_str());

        m_assetPaths = FindFilesInFolder("assets", "");
    }

    bool Editor::CanTrackSceneHistory() const
    {
        return m_scene != nullptr &&
            m_mode == EditorMode::EDIT &&
            !m_sceneHistoryRestoring;
    }

    Editor::SceneHistoryState Editor::CaptureSceneHistoryState() const
    {
        SceneHistoryState state = {};
        if (m_scene == nullptr)
            return state;

        YAML::Emitter out;
        out << m_scene->EncodeScene();
        state.sceneYaml = out.c_str();
        state.hierarchyRootOrder = m_hierarchyRootOrder;

        const std::vector<Entity*> &entities = m_scene->GetEntities();
        if (m_index >= 0 && m_index < static_cast<int>(entities.size()) && entities[m_index] != nullptr)
            state.selectedEntityUUID = entities[m_index]->uuid;

        return state;
    }

    bool Editor::SceneHistoryContentEquals(const SceneHistoryState &_left, const SceneHistoryState &_right) const
    {
        return _left.sceneYaml == _right.sceneYaml &&
            _left.hierarchyRootOrder == _right.hierarchyRootOrder;
    }

    void Editor::ResetSceneHistory()
    {
        m_sceneUndoStack.clear();
        m_sceneRedoStack.clear();
        m_sceneHistoryPendingBeforeState = {};
        m_hasSceneHistoryPendingBeforeState = false;
        m_sceneHistoryEditWasActive = false;
        m_sceneHistoryCurrentState = CaptureSceneHistoryState();
        m_hasSceneHistoryCurrentState = m_scene != nullptr && !m_sceneHistoryCurrentState.sceneYaml.empty();
    }

    void Editor::PushSceneUndoState(const SceneHistoryState &_state)
    {
        if (_state.sceneYaml.empty())
            return;

        if (!m_sceneUndoStack.empty() && SceneHistoryContentEquals(m_sceneUndoStack.back(), _state))
            return;

        constexpr size_t maxSceneHistoryStates = 100;
        m_sceneUndoStack.push_back(_state);
        if (m_sceneUndoStack.size() > maxSceneHistoryStates)
            m_sceneUndoStack.erase(m_sceneUndoStack.begin());
    }

    bool Editor::IsSceneHistoryEditInProgress() const
    {
        return ImGuizmo::IsUsing() || ImGui::IsAnyItemActive() ||
            ImGui::IsMouseDragging(ImGuiMouseButton_Left) ||
            ImGui::IsMouseDragging(ImGuiMouseButton_Right);
    }

    void Editor::CommitSceneHistoryPendingChange()
    {
        if (!m_hasSceneHistoryPendingBeforeState)
            return;

        if (!SceneHistoryContentEquals(m_sceneHistoryPendingBeforeState, m_sceneHistoryCurrentState))
        {
            PushSceneUndoState(m_sceneHistoryPendingBeforeState);
            m_sceneRedoStack.clear();
        }

        m_sceneHistoryPendingBeforeState = {};
        m_hasSceneHistoryPendingBeforeState = false;
    }

    void Editor::CommitSceneHistoryImmediateChange(const SceneHistoryState &_beforeState)
    {
        if (!CanTrackSceneHistory() || _beforeState.sceneYaml.empty())
            return;

        SceneHistoryState afterState = CaptureSceneHistoryState();
        if (SceneHistoryContentEquals(_beforeState, afterState))
            return;

        PushSceneUndoState(_beforeState);
        m_sceneRedoStack.clear();
        m_sceneHistoryCurrentState = afterState;
        m_hasSceneHistoryCurrentState = true;
        m_sceneHistoryPendingBeforeState = {};
        m_hasSceneHistoryPendingBeforeState = false;
        m_sceneHistoryEditWasActive = false;
    }

    void Editor::BeginSceneHistoryFrame()
    {
        if (!CanTrackSceneHistory())
            return;

        if (!m_hasSceneHistoryCurrentState)
        {
            m_sceneHistoryCurrentState = CaptureSceneHistoryState();
            m_hasSceneHistoryCurrentState = !m_sceneHistoryCurrentState.sceneYaml.empty();
        }
    }

    void Editor::EndSceneHistoryFrame()
    {
        if (!CanTrackSceneHistory())
            return;

        if (!m_hasSceneHistoryCurrentState)
        {
            BeginSceneHistoryFrame();
            return;
        }

        const bool editActive = IsSceneHistoryEditInProgress();
        if (editActive)
        {
            m_sceneHistoryEditWasActive = true;
            return;
        }

        ImGuiContext *imguiContext = ImGui::GetCurrentContext();
        const bool editedItemDeactivated =
            imguiContext != nullptr &&
            imguiContext->ActiveId == 0 &&
            imguiContext->DeactivatedItemData.HasBeenEditedBefore;

        if (m_sceneHistoryEditWasActive || editedItemDeactivated)
        {
            m_sceneHistoryEditWasActive = false;
            FlushSceneHistoryPendingChange();
        }
    }

    void Editor::FlushSceneHistoryPendingChange()
    {
        if (!CanTrackSceneHistory())
            return;

        if (!m_hasSceneHistoryCurrentState)
            BeginSceneHistoryFrame();

        SceneHistoryState afterState = CaptureSceneHistoryState();
        if (!SceneHistoryContentEquals(afterState, m_sceneHistoryCurrentState))
        {
            if (!m_hasSceneHistoryPendingBeforeState)
            {
                m_sceneHistoryPendingBeforeState = m_sceneHistoryCurrentState;
                m_hasSceneHistoryPendingBeforeState = true;
            }

            m_sceneHistoryCurrentState = afterState;
        }
        else
        {
            m_sceneHistoryCurrentState.selectedEntityUUID = afterState.selectedEntityUUID;
        }

        CommitSceneHistoryPendingChange();
    }

    void Editor::RestoreSceneHistoryState(const SceneHistoryState &_state)
    {
        if (m_scene == nullptr || _state.sceneYaml.empty())
            return;

        YAML::Node sceneRoot;
        try
        {
            sceneRoot = YAML::Load(_state.sceneYaml);
        }
        catch (const YAML::Exception &_exception)
        {
            Debug::Warning("Failed to restore editor undo state: %s", _exception.what());
            return;
        }

        const std::string scenePath = m_scene->m_path;
        m_sceneHistoryRestoring = true;
        m_scene->Unload();
        m_scene->m_path = scenePath;
        m_scene->LoadSceneNode(sceneRoot);
        m_sceneHistoryRestoring = false;

        m_hierarchyRootOrder = _state.hierarchyRootOrder;
        m_queuedPrefabInstanceRebuilds.clear();
        m_rebuildAllPrefabInstancesRequested = false;
        m_hierarchyRevealTargetUUID = UUID(0);
        m_hierarchyRevealPath.clear();
        m_selectedAssetPath.clear();
        m_selectedScriptPath.clear();

        m_index = -1;
        std::vector<Entity*> &entities = m_scene->GetEntities();
        if ((uint64_t)_state.selectedEntityUUID != 0)
        {
            for (int i = 0; i < static_cast<int>(entities.size()); ++i)
            {
                if (entities[i] != nullptr && entities[i]->uuid == _state.selectedEntityUUID)
                {
                    m_index = i;
                    break;
                }
            }
        }

        if (m_index < 0)
        {
            for (int i = 0; i < static_cast<int>(entities.size()); ++i)
            {
                if (entities[i] != nullptr)
                {
                    m_index = i;
                    break;
                }
            }
        }

        m_sceneHistoryCurrentState = CaptureSceneHistoryState();
        m_hasSceneHistoryCurrentState = !m_sceneHistoryCurrentState.sceneYaml.empty();
        m_sceneHistoryPendingBeforeState = {};
        m_hasSceneHistoryPendingBeforeState = false;
        m_forceRefresh = true;
    }

    bool Editor::CanUndoSceneEdit() const
    {
        return m_hasSceneHistoryPendingBeforeState || !m_sceneUndoStack.empty();
    }

    bool Editor::CanRedoSceneEdit() const
    {
        return !m_hasSceneHistoryPendingBeforeState && !m_sceneRedoStack.empty();
    }

    void Editor::UndoSceneEdit()
    {
        if (!CanTrackSceneHistory())
            return;

        FlushSceneHistoryPendingChange();
        if (m_sceneUndoStack.empty() || !m_hasSceneHistoryCurrentState)
            return;

        SceneHistoryState targetState = m_sceneUndoStack.back();
        m_sceneUndoStack.pop_back();
        m_sceneRedoStack.push_back(m_sceneHistoryCurrentState);
        RestoreSceneHistoryState(targetState);
    }

    void Editor::RedoSceneEdit()
    {
        if (!CanTrackSceneHistory())
            return;

        if (m_sceneRedoStack.empty() || !m_hasSceneHistoryCurrentState)
            return;

        SceneHistoryState targetState = m_sceneRedoStack.back();
        m_sceneRedoStack.pop_back();
        PushSceneUndoState(m_sceneHistoryCurrentState);
        RestoreSceneHistoryState(targetState);
    }

    bool Editor::HandleSceneHistoryShortcuts(float &_hotKeyCoolDown, float _hotKeyReset)
    {
        if (!CanTrackSceneHistory() || _hotKeyCoolDown >= 0.0f)
            return false;

        ImGuiIO &io = ImGui::GetIO();
        if (io.WantTextInput || !io.KeyCtrl)
            return false;

        if (ImGui::IsKeyPressed(ImGuiKey_Z))
        {
            _hotKeyCoolDown = _hotKeyReset;
            if (io.KeyShift)
                RedoSceneEdit();
            else
                UndoSceneEdit();
            return true;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Y))
        {
            _hotKeyCoolDown = _hotKeyReset;
            RedoSceneEdit();
            return true;
        }

        return false;
    }

    void Editor::DrawSceneView()
    {
        ImGui::Begin("Scene", &m_showScenePanel);
        m_sceneViewClicked = false;

        ImVec2 avail = ImGui::GetContentRegionAvail();
        int nextWidth = static_cast<int>(avail.x);
        int nextHeight = static_cast<int>(avail.y);
        bool hovered = false;

        if (nextWidth > 0 && nextHeight > 0)
        {
            ImVec2 cursorScreen = ImGui::GetCursorScreenPos();
            ImGuiViewport *viewport = ImGui::GetWindowViewport();

            if (viewport)
                m_gameViewportId = viewport->ID;

            if (nextWidth != m_gameViewportWidth || nextHeight != m_gameViewportHeight)
            {
                m_gameViewportWidth = nextWidth;
                m_gameViewportHeight = nextHeight;
            }

            if (m_gameColorTexture != 0)
            {
                PostProcessResult renderResult = ApplyScenePostProcess(
                    m_scene,
                    m_gameFramebuffer,
                    m_gameColorTexture,
                    m_gameDepthRbo,
                    m_gameTextureWidth,
                    m_gameTextureHeight,
                    m_gameRenderProjection,
                    &m_gameViewPostProcessTarget);

                float targetW = static_cast<float>((m_gameTextureWidth > 0) ? m_gameTextureWidth : 1);
                float targetH = static_cast<float>((m_gameTextureHeight > 0) ? m_gameTextureHeight : 1);
                float targetAspect = targetW / targetH;
                float availAspect = (avail.y > 0.0f) ? (avail.x / avail.y) : targetAspect;

                ImVec2 drawSize = avail;
                if (availAspect > targetAspect) {
                    drawSize.x = avail.y * targetAspect; // letterbox left/right
                } else {
                    drawSize.y = avail.x / targetAspect; // letterbox top/bottom
                }

                ImVec2 cursor = ImGui::GetCursorPos();
                ImVec2 offset((avail.x - drawSize.x) * 0.5f, (avail.y - drawSize.y) * 0.5f);
                m_gameViewportPosX = cursorScreen.x + offset.x;
                m_gameViewportPosY = cursorScreen.y + offset.y;
                m_gameViewportDrawWidth = drawSize.x;
                m_gameViewportDrawHeight = drawSize.y;
                ImGui::SetCursorPos(ImVec2(cursor.x + offset.x, cursor.y + offset.y));

                ImGui::Image(
                    (ImTextureID)(intptr_t)renderResult.colorTexture,
                    drawSize,
                    ImVec2(0.0f, 1.0f),
                    ImVec2(1.0f, 0.0f));
                hovered = ImGui::IsItemHovered();
                m_sceneViewClicked = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
                DrawSceneViewGizmo();
            }
            else
            {
                m_gameViewportPosX = 0.0f;
                m_gameViewportPosY = 0.0f;
                m_gameViewportDrawWidth = 0.0f;
                m_gameViewportDrawHeight = 0.0f;
                ImGui::Text("Game view unavailable.");
            }
        }
        else
        {
            m_gameViewportPosX = 0.0f;
            m_gameViewportPosY = 0.0f;
            m_gameViewportDrawWidth = 0.0f;
            m_gameViewportDrawHeight = 0.0f;
            m_gameViewportWidth = 0;
            m_gameViewportHeight = 0;
        }

        m_gameViewHovered = hovered;
        ImGui::End();
    }

    void Editor::DrawGameView()
    {
        ImGui::Begin("Game", &m_showGamePanel);

        m_playViewportPosX = 0.0f;
        m_playViewportPosY = 0.0f;
        m_playViewportDrawWidth = 0.0f;
        m_playViewportDrawHeight = 0.0f;
        m_playViewHovered = false;

        if (ImGuiViewport *viewport = ImGui::GetWindowViewport())
        {
            if (viewport->PlatformHandle != nullptr)
            {
                SDL_Window *viewportWindow = static_cast<SDL_Window *>(viewport->PlatformHandle);
                if (viewportWindow != nullptr)
                    m_gameInputWindowID = SDL_GetWindowID(viewportWindow);
            }
        }

        ImVec2 avail = ImGui::GetContentRegionAvail();
        int nextWidth = static_cast<int>(avail.x);
        int nextHeight = static_cast<int>(avail.y);

        if (nextWidth != m_playViewportWidth || nextHeight != m_playViewportHeight)
        {
            m_playViewportWidth = std::max(0, nextWidth);
            m_playViewportHeight = std::max(0, nextHeight);
        }

        if (nextWidth > 0 && nextHeight > 0)
        {
            if (m_playColorTexture != 0)
            {
                PostProcessResult renderResult = ApplyScenePostProcess(
                    m_scene,
                    m_playFramebuffer,
                    m_playColorTexture,
                    m_playDepthRbo,
                    m_playTextureWidth,
                    m_playTextureHeight,
                    m_playRenderProjection,
                    &m_playViewPostProcessTarget);

                ImVec2 cursorScreen = ImGui::GetCursorScreenPos();
                float targetW = static_cast<float>((m_playTextureWidth > 0) ? m_playTextureWidth : 1);
                float targetH = static_cast<float>((m_playTextureHeight > 0) ? m_playTextureHeight : 1);
                float targetAspect = targetW / targetH;
                float availAspect = (avail.y > 0.0f) ? (avail.x / avail.y) : targetAspect;

                ImVec2 drawSize = avail;
                if (availAspect > targetAspect) {
                    drawSize.x = avail.y * targetAspect;
                } else {
                    drawSize.y = avail.x / targetAspect;
                }

                ImVec2 cursor = ImGui::GetCursorPos();
                ImVec2 offset((avail.x - drawSize.x) * 0.5f, (avail.y - drawSize.y) * 0.5f);
                m_playViewportPosX = cursorScreen.x + offset.x;
                m_playViewportPosY = cursorScreen.y + offset.y;
                m_playViewportDrawWidth = drawSize.x;
                m_playViewportDrawHeight = drawSize.y;
                ImGui::SetCursorPos(ImVec2(cursor.x + offset.x, cursor.y + offset.y));

                ImGui::Image(
                    (ImTextureID)(intptr_t)renderResult.colorTexture,
                    drawSize,
                    ImVec2(0.0f, 1.0f),
                    ImVec2(1.0f, 0.0f));
                m_playViewHovered = ImGui::IsItemHovered();

                if (m_scene != nullptr)
                {
                    float logicalWidth = static_cast<float>((m_playTextureWidth > 0) ? m_playTextureWidth : m_playViewportWidth);
                    float logicalHeight = static_cast<float>((m_playTextureHeight > 0) ? m_playTextureHeight : m_playViewportHeight);
                    logicalWidth = std::max(1.0f, logicalWidth);
                    logicalHeight = std::max(1.0f, logicalHeight);

                    float localViewportPosX = m_playViewportPosX;
                    float localViewportPosY = m_playViewportPosY;
                    if (ImGuiViewport *view = ImGui::GetWindowViewport())
                    {
                        localViewportPosX -= view->Pos.x;
                        localViewportPosY -= view->Pos.y;
                    }

                    m_scene->GetInputManager().SetGameMouseViewport(
                        localViewportPosX,
                        localViewportPosY,
                        m_playViewportDrawWidth,
                        m_playViewportDrawHeight,
                        logicalWidth,
                        logicalHeight);
                }
            }
            else
            {
                if (m_scene != nullptr)
                    m_scene->GetInputManager().ClearGameMouseViewport();
                ImGui::Text("Game view unavailable.");
            }
        }
        else if (m_scene != nullptr)
        {
            m_scene->GetInputManager().ClearGameMouseViewport();
        }

        ImGui::End();
    }

    void Editor::DrawSceneViewGizmo()
    {
        if (m_gameViewportWidth <= 0 || m_gameViewportHeight <= 0)
        {
            ResetVertexSnapDrag();
            return;
        }

        if (m_index < 0 || m_index >= m_scene->GetEntities().size())
        {
            ResetVertexSnapDrag();
            return;
        }

        Entity *selected = m_scene->GetEntities()[m_index];
        if (!selected)
        {
            ResetVertexSnapDrag();
            return;
        }

        float rectW = (m_gameViewportDrawWidth > 0.0f) ? m_gameViewportDrawWidth : static_cast<float>(m_gameViewportWidth);
        float rectH = (m_gameViewportDrawHeight > 0.0f) ? m_gameViewportDrawHeight : static_cast<float>(m_gameViewportHeight);

        // Keep ImGuizmo's internal helper window attached to the Scene viewport.
        // This preserves gizmo interaction while preventing it from showing up as its own platform window.
        ImGuiWindow* sceneWindow = ImGui::GetCurrentWindow();
        if (sceneWindow != nullptr && sceneWindow->Viewport != nullptr)
            ImGui::SetNextWindowViewport(sceneWindow->Viewport->ID);

        ImGuiWindowClass gizmoWindowClass = {};
        gizmoWindowClass.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoTaskBarIcon;
        gizmoWindowClass.ViewportFlagsOverrideClear = ImGuiViewportFlags_NoAutoMerge;
        ImGui::SetNextWindowClass(&gizmoWindowClass);
        ImGuizmo::BeginFrame();

        ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
        ImGuizmo::SetAlternativeWindow(ImGui::GetCurrentWindow());
        ImGuizmo::SetRect(m_gameViewportPosX, m_gameViewportPosY, rectW, rectH);
        ImGuizmo::Enable(true);

        if (Transform *transform3D = (selected != nullptr && selected->HasComponent<Transform>() ? &selected->GetComponent<Transform>() : nullptr))
        {
            const bool useEditorSceneCamera =
                m_mode != EditorMode::HIDDEN &&
                m_sceneCameraMode == SceneCameraMode::SCENE_CAMERA_3D;

            if (!useEditorSceneCamera)
            {
                ResetVertexSnapDrag();
                return;
            }

            // Overrides are cleared before UI draw; keep using the last scene-camera matrices.
            Matrix4 projection = m_scene->GetEditorCamera3DProjection();
            Matrix4 view = m_scene->GetEditorCamera3DView();

            Matrix4 model = transform3D->GetModelMatrix();
            const Vector3 previousWorldPosition = transform3D->GetGlobalPosition();

            ImGuizmo::SetOrthographic(false);
            static ImGuizmo::OPERATION operation3D = ImGuizmo::TRANSLATE;
            if (!ImGui::GetIO().WantTextInput)
            {
                if (ImGui::IsKeyPressed(ImGuiKey_W))
                    operation3D = ImGuizmo::TRANSLATE;
                if (ImGui::IsKeyPressed(ImGuiKey_E))
                    operation3D = ImGuizmo::ROTATE;
                if (ImGui::IsKeyPressed(ImGuiKey_R))
                    operation3D = ImGuizmo::SCALE;
            }

            ImGuizmo::Manipulate(
                glm::value_ptr(view),
                glm::value_ptr(projection),
                operation3D,
                (ImGuizmo::MODE)m_guizmoMode,
                glm::value_ptr(model));

            if (ImGuizmo::IsUsing())
            {
                float t[3], r[3], s[3];
                ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(model), t, r, s);

                Vector3 worldPosition(t[0], t[1], t[2]);
                const Vector3 worldRotation(DEG2RAD * r[0], DEG2RAD * r[1], DEG2RAD * r[2]);
                const Vector3 worldScale(s[0], s[1], s[2]);
                const bool vertexSnapActive =
                    operation3D == ImGuizmo::TRANSLATE &&
                    (m_vertexSnappingEnabled || ImGui::IsKeyDown(ImGuiKey_V));

                if (vertexSnapActive)
                {
                    const Matrix4 snapModel = ComposeTransformMatrix(worldPosition, worldRotation, worldScale);
                    (void)TryApplyVertexSnap(selected, snapModel, worldPosition, worldPosition - previousWorldPosition);
                }
                else
                {
                    ResetVertexSnapDrag();
                }

                if (transform3D->parent != nullptr)
                {
                    if (transform3D->parent->HasComponent<Transform>())
                    {
                        Transform& parentTransform = transform3D->parent->GetComponent<Transform>();
                        const Vector3 parentWorldPosition = parentTransform.GetGlobalPosition();
                        const Vector3 parentWorldRotation = parentTransform.GetGlobalRotation();
                        const Vector3 parentWorldScale = parentTransform.GetGlobalScale();

                        const Vector3 parentSpacePosition = worldPosition - parentWorldPosition;
                        Matrix4 inverseParentRotation = Matrix4(1.0f);
                        inverseParentRotation = glm::rotate(inverseParentRotation, -parentWorldRotation.x, Vector3(1.0f, 0.0f, 0.0f));
                        inverseParentRotation = glm::rotate(inverseParentRotation, -parentWorldRotation.y, Vector3(0.0f, 1.0f, 0.0f));
                        inverseParentRotation = glm::rotate(inverseParentRotation, -parentWorldRotation.z, Vector3(0.0f, 0.0f, 1.0f));
                        const Vector4 localPosition4 = inverseParentRotation * Vector4(
                            parentSpacePosition.x,
                            parentSpacePosition.y,
                            parentSpacePosition.z,
                            0.0f);

                        transform3D->position.x = (parentWorldScale.x != 0.0f) ? (localPosition4.x / parentWorldScale.x) : localPosition4.x;
                        transform3D->position.y = (parentWorldScale.y != 0.0f) ? (localPosition4.y / parentWorldScale.y) : localPosition4.y;
                        transform3D->position.z = (parentWorldScale.z != 0.0f) ? (localPosition4.z / parentWorldScale.z) : localPosition4.z;

                        transform3D->rotation = worldRotation - parentWorldRotation;
                        transform3D->scale.x = (parentWorldScale.x != 0.0f) ? (worldScale.x / parentWorldScale.x) : worldScale.x;
                        transform3D->scale.y = (parentWorldScale.y != 0.0f) ? (worldScale.y / parentWorldScale.y) : worldScale.y;
                        transform3D->scale.z = (parentWorldScale.z != 0.0f) ? (worldScale.z / parentWorldScale.z) : worldScale.z;
                    }
                    else
                    {
                        transform3D->position = worldPosition;
                        transform3D->rotation = worldRotation;
                        transform3D->scale = worldScale;
                    }
                }
                else
                {
                    transform3D->position = worldPosition;
                    transform3D->rotation = worldRotation;
                    transform3D->scale = worldScale;
                }
            }
            else
            {
                ResetVertexSnapDrag();
            }

            return;
        }

        ResetVertexSnapDrag();

        RectTransform *rtc = (selected != nullptr && selected->HasComponent<RectTransform>() ? &selected->GetComponent<RectTransform>() : nullptr);
        if (!rtc)
            return;

        const bool useEditorSceneCamera2D =
            m_mode != EditorMode::HIDDEN &&
            m_sceneCameraMode == SceneCameraMode::SCENE_CAMERA_2D;

        if (!useEditorSceneCamera2D)
            return;

        const float renderWidth = std::max(1.0f, (m_gameTextureWidth > 0)
            ? static_cast<float>(m_gameTextureWidth)
            : static_cast<float>((m_gameViewportWidth > 0) ? m_gameViewportWidth : m_window->GetWindowWidth()));
        const float renderHeight = std::max(1.0f, (m_gameTextureHeight > 0)
            ? static_cast<float>(m_gameTextureHeight)
            : static_cast<float>((m_gameViewportHeight > 0) ? m_gameViewportHeight : m_window->GetWindowHeight()));

        Matrix4 projection = glm::ortho(0.0f, renderWidth, 0.0f, renderHeight, 0.0f, 100.0f);

        Vector2 pos = rtc->GetPosition();
        Vector2 globalScale = rtc->GetScale();

        Matrix4 model = Matrix4(1.0f);
        model = glm::translate(model, Vector3(pos.x, pos.y, 0.0f));
        model = glm::rotate(model, rtc->rotation, Vector3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, Vector3(rtc->size.x * globalScale.x, rtc->size.y * globalScale.y, 1.0f));

        Matrix4 view = Matrix4(1.0f);
        view = glm::translate(view, Vector3(-m_editorCamera2DPosition.x + renderWidth * 0.5f,
                                            -m_editorCamera2DPosition.y + renderHeight * 0.5f, 0.0f));
        // Keep camera's 2D behavior, but avoid zero Z scale for gizmo rendering.
        view = glm::scale(view, Vector3(m_editorCamera2DScale, m_editorCamera2DScale, 1.0f));

        ImGuizmo::SetOrthographic(true);

        static ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
        if (!ImGui::GetIO().WantTextInput)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_W))
                operation = ImGuizmo::TRANSLATE;
            if (ImGui::IsKeyPressed(ImGuiKey_E))
                operation = ImGuizmo::ROTATE;
            if (ImGui::IsKeyPressed(ImGuiKey_R))
                operation = ImGuizmo::SCALE;
        }

        ImGuizmo::Manipulate(
            glm::value_ptr(view),
            glm::value_ptr(projection),
            operation,
            (ImGuizmo::MODE)m_guizmoMode,
            glm::value_ptr(model));

        if (ImGuizmo::IsUsing())
        {
            float t[3], r[3], s[3];
            ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(model), t, r, s);

            Vector2 newPos(t[0], t[1]);
            rtc->SetPosition(newPos);

            rtc->rotation = DEG2RAD * r[2];

            rtc->SetScale(Vector2(s[0] / rtc->size.x, s[1] / rtc->size.y));
        }
    }

    void Editor::EnsureGameRenderTarget(int _width, int _height)
    {
        if (_width <= 0 || _height <= 0)
            return;

        if (m_gameFramebuffer != 0 &&
            _width == m_gameTextureWidth &&
            _height == m_gameTextureHeight)
        {
            return;
        }

        DestroyGameRenderTarget();

        glGenFramebuffers(1, &m_gameFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, m_gameFramebuffer);

        glGenTextures(1, &m_gameColorTexture);
        glBindTexture(GL_TEXTURE_2D, m_gameColorTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, _width, _height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_gameColorTexture, 0);

        glGenTextures(1, &m_gameDepthRbo);
        glBindTexture(GL_TEXTURE_2D, m_gameDepthRbo);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, _width, _height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_gameDepthRbo, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            Debug::Log("Game framebuffer incomplete.");
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        m_gameTextureWidth = _width;
        m_gameTextureHeight = _height;
    }

    void Editor::EnsureGamePickingRenderTarget(int _width, int _height)
    {
        if (_width <= 0 || _height <= 0)
            return;

        if (m_gamePickingFramebuffer != 0 &&
            _width == m_gamePickingTextureWidth &&
            _height == m_gamePickingTextureHeight)
        {
            return;
        }

        DestroyGamePickingRenderTarget();

        glGenFramebuffers(1, &m_gamePickingFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, m_gamePickingFramebuffer);

        glGenTextures(1, &m_gamePickingColorTexture);
        glBindTexture(GL_TEXTURE_2D, m_gamePickingColorTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, _width, _height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_gamePickingColorTexture, 0);

        glGenRenderbuffers(1, &m_gamePickingDepthRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, m_gamePickingDepthRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, _width, _height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_gamePickingDepthRbo);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            Debug::Log("Game picking framebuffer incomplete.");

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        m_gamePickingTextureWidth = _width;
        m_gamePickingTextureHeight = _height;
    }

    void Editor::EnsurePlayRenderTarget(int _width, int _height)
    {
        if (_width <= 0 || _height <= 0)
            return;

        if (m_playFramebuffer != 0 &&
            _width == m_playTextureWidth &&
            _height == m_playTextureHeight)
        {
            return;
        }

        DestroyPlayRenderTarget();

        glGenFramebuffers(1, &m_playFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, m_playFramebuffer);

        glGenTextures(1, &m_playColorTexture);
        glBindTexture(GL_TEXTURE_2D, m_playColorTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, _width, _height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_playColorTexture, 0);

        glGenTextures(1, &m_playDepthRbo);
        glBindTexture(GL_TEXTURE_2D, m_playDepthRbo);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, _width, _height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_playDepthRbo, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            Debug::Log("Play framebuffer incomplete.");
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        m_playTextureWidth = _width;
        m_playTextureHeight = _height;
    }

    void Editor::DestroyGameRenderTarget()
    {
        if (m_gameDepthRbo != 0)
        {
            glDeleteTextures(1, &m_gameDepthRbo);
            m_gameDepthRbo = 0;
        }

        if (m_gameColorTexture != 0)
        {
            glDeleteTextures(1, &m_gameColorTexture);
            m_gameColorTexture = 0;
        }

        if (m_gameFramebuffer != 0)
        {
            glDeleteFramebuffers(1, &m_gameFramebuffer);
            m_gameFramebuffer = 0;
        }

        m_gameTextureWidth = 0;
        m_gameTextureHeight = 0;
    }

    void Editor::DestroyGamePickingRenderTarget()
    {
        if (m_gamePickingDepthRbo != 0)
        {
            glDeleteRenderbuffers(1, &m_gamePickingDepthRbo);
            m_gamePickingDepthRbo = 0;
        }

        if (m_gamePickingColorTexture != 0)
        {
            glDeleteTextures(1, &m_gamePickingColorTexture);
            m_gamePickingColorTexture = 0;
        }

        if (m_gamePickingFramebuffer != 0)
        {
            glDeleteFramebuffers(1, &m_gamePickingFramebuffer);
            m_gamePickingFramebuffer = 0;
        }

        m_gamePickingTextureWidth = 0;
        m_gamePickingTextureHeight = 0;
    }

    void Editor::DestroyPlayRenderTarget()
    {
        if (m_playDepthRbo != 0)
        {
            glDeleteTextures(1, &m_playDepthRbo);
            m_playDepthRbo = 0;
        }

        if (m_playColorTexture != 0)
        {
            glDeleteTextures(1, &m_playColorTexture);
            m_playColorTexture = 0;
        }

        if (m_playFramebuffer != 0)
        {
            glDeleteFramebuffers(1, &m_playFramebuffer);
            m_playFramebuffer = 0;
        }

        m_playTextureWidth = 0;
        m_playTextureHeight = 0;
    }

    static std::vector<Canis::Entity*>* GetHierarchyChildren(Canis::Entity *_entity);
    static Canis::Entity* GetHierarchyParent(Canis::Entity *_entity);
    static bool SetHierarchyParentAtIndexKeepingLocal(Canis::Entity *_child, Canis::Entity *_parent, std::size_t _index);
    static bool SetHierarchyParentKeepingLocal(Canis::Entity *_child, Canis::Entity *_parent);
    static Canis::Entity* InstantiatePrefabHierarchyRoot(Canis::Scene &_scene, const SceneAssetHandle &_prefabHandle, const std::string &_displayName);
    static bool InsertRootEntityOrder(std::vector<Canis::UUID> &_rootOrder, Canis::Entity *_entity, int _targetRootPos);

    void Editor::RequestHierarchyReveal(Canis::Entity *_entity)
    {
        m_hierarchyRevealTargetUUID = UUID(0);
        m_hierarchyRevealPath.clear();

        if (_entity == nullptr)
            return;

        m_hierarchyRevealTargetUUID = _entity->uuid;

        for (Canis::Entity *current = GetHierarchyParent(_entity); current != nullptr; current = GetHierarchyParent(current))
            m_hierarchyRevealPath.push_back(current->uuid);
    }

    void Editor::FocusEntity(Canis::Entity *_entity)
    {
        for (int i = 0; i < m_scene->GetEntities().size(); i++)
        {
            if (m_scene->GetEntities()[i] == _entity)
            {
                m_index = i;
                m_selectedAssetPath.clear();
                RequestHierarchyReveal(_entity);
                return;
            }
        }
    }

    void Editor::NotifyAnimationPropertyEdited(
        Canis::Entity &_entity,
        const std::string &_componentName,
        const std::string &_propertyName,
        AnimationValueType _type,
        AnimationInterpolation _interpolation,
        const AnimationValue &_value)
    {
        if (!m_animationRecordEnabled || m_scene == nullptr || m_app == nullptr)
            return;

        if (_componentName.empty() || _propertyName.empty() || _type == AnimationValueType::NONE)
            return;

        if (m_animationClipStatePath.empty())
            m_animationClipStatePath = ResolveRememberedAnimationClipPath();

        if (m_animationClipStatePath.empty())
            return;

        AnimationClipAsset *clip = AssetManager::GetAnimationClip(m_animationClipStatePath);
        if (clip == nullptr)
            return;

        Entity *root = (m_animationTargetUUID != UUID(0)) ? m_scene->GetEntityWithUUID(m_animationTargetUUID) : nullptr;
        if (root == nullptr)
        {
            if (m_index >= 0 && m_index < static_cast<int>(m_scene->GetEntities().size()))
                root = m_scene->GetEntities()[m_index];

            if (root != nullptr)
                m_animationTargetUUID = root->uuid;
        }

        if (root == nullptr)
            return;

        std::string relativePath = {};
        if (root != &_entity && !BuildAnimationRelativePath(*root, _entity, relativePath))
            return;

        AnimationTrack *track = FindOrCreateAnimationTrack(
            *clip,
            relativePath,
            _componentName,
            _propertyName,
            _type,
            _interpolation);
        if (track == nullptr)
            return;

        const float keyTime = std::max(m_animationTime, 0.0f);
        bool updatedExisting = false;
        for (AnimationKeyframe &key : track->keys)
        {
            if (std::fabs(key.time - keyTime) <= kAnimationEditorKeyTimeEpsilon)
            {
                key.value = _value;
                updatedExisting = true;
                break;
            }
        }

        if (!updatedExisting)
            track->keys.push_back(AnimationKeyframe{ keyTime, _value });

        SortAnimationTrackKeys(*track);
        RefreshAnimationClipLength(*clip);
        (void)clip->Save();
        RememberLastAnimationClipAssetPath(m_animationClipStatePath);
    }

    void Editor::RebuildPrefabInstance(Canis::Entity *_entity)
    {
        if (_entity == nullptr || !_entity->HasComponent<PrefabInstance>())
            return;

        if (std::find(m_queuedPrefabInstanceRebuilds.begin(), m_queuedPrefabInstanceRebuilds.end(), _entity->uuid) ==
            m_queuedPrefabInstanceRebuilds.end())
        {
            m_queuedPrefabInstanceRebuilds.push_back(_entity->uuid);
        }
    }

    void Editor::RebuildAllPrefabInstances()
    {
        m_rebuildAllPrefabInstancesRequested = true;
    }

    void Editor::ApplyPrefabInstanceOverrides(Canis::Entity *_entity)
    {
        if (m_scene == nullptr || _entity == nullptr || !_entity->HasComponent<PrefabInstance>())
            return;

        PrefabInstance &prefabInstance = _entity->GetComponent<PrefabInstance>();
        const std::string prefabPath = AssetManager::ResolvePath(prefabInstance.prefab);
        if (prefabPath.empty())
            return;

        MetaFileAsset *meta = AssetManager::GetMetaFile(prefabPath);
        if (meta == nullptr || meta->type != MetaFileAsset::FileType::SCENE)
            return;

        std::vector<Canis::Entity*> exportRoots = {};
        if (prefabInstance.firstEntity != nullptr && prefabInstance.firstEntity != _entity)
        {
            if (std::vector<Canis::Entity*> *children = GetHierarchyChildren(_entity))
            {
                for (Canis::Entity *child : *children)
                {
                    if (child != nullptr)
                        exportRoots.push_back(child);
                }
            }
        }

        if (exportRoots.empty())
            exportRoots.push_back(_entity);

        if (!ExportHierarchyRootsToPrefabAsset(*m_scene, exportRoots, prefabPath))
            return;

        prefabInstance.prefab = MakeSceneAssetHandleFromPath(prefabPath);
        AssignPrefabHandle(_entity, prefabInstance.prefab);
        m_selectedAssetPath = prefabPath;
        m_forceRefresh = true;
    }

    Canis::Entity* Editor::RebuildPrefabInstanceNow(Canis::Entity *_entity, bool _focusSelection)
    {
        if (m_scene == nullptr || _entity == nullptr || !_entity->HasComponent<PrefabInstance>())
            return nullptr;

        PrefabInstance &prefabInstance = _entity->GetComponent<PrefabInstance>();
        if (prefabInstance.prefab.Empty())
            return nullptr;

        struct SavedTransformState
        {
            bool valid = false;
            Vector3 position = Vector3(0.0f);
            Vector3 rotation = Vector3(0.0f);
            Vector3 scale = Vector3(1.0f);
        };

        struct SavedRectTransformState
        {
            bool valid = false;
            Vector2 position = Vector2(0.0f);
            Vector2 size = Vector2(0.0f);
            Vector2 scale = Vector2(1.0f);
            Vector2 anchorMin = Vector2(0.5f);
            Vector2 anchorMax = Vector2(0.5f);
            Vector2 pivot = Vector2(0.5f);
            Vector2 originOffset = Vector2(0.0f);
            float depth = 0.0f;
            float rotation = 0.0f;
            Vector2 rotationOriginOffset = Vector2(0.0f);
        };

        auto saveTransformState = [](Canis::Entity *_target) -> SavedTransformState
        {
            SavedTransformState state = {};
            if (_target == nullptr || !_target->HasComponent<Transform>())
                return state;

            const Transform &transform = _target->GetComponent<Transform>();
            state.valid = true;
            state.position = transform.position;
            state.rotation = transform.rotation;
            state.scale = transform.scale;
            return state;
        };

        auto saveRectTransformState = [](Canis::Entity *_target) -> SavedRectTransformState
        {
            SavedRectTransformState state = {};
            if (_target == nullptr || !_target->HasComponent<RectTransform>())
                return state;

            const RectTransform &transform = _target->GetComponent<RectTransform>();
            state.valid = true;
            state.position = transform.position;
            state.size = transform.size;
            state.scale = transform.scale;
            state.anchorMin = transform.anchorMin;
            state.anchorMax = transform.anchorMax;
            state.pivot = transform.pivot;
            state.originOffset = transform.originOffset;
            state.depth = transform.depth;
            state.rotation = transform.rotation;
            state.rotationOriginOffset = transform.rotationOriginOffset;
            return state;
        };

        auto applyTransformState = [](Canis::Entity *_target, const SavedTransformState &_state) -> void
        {
            if (_target == nullptr || !_state.valid || !_target->HasComponent<Transform>())
                return;

            Transform &transform = _target->GetComponent<Transform>();
            transform.position = _state.position;
            transform.rotation = _state.rotation;
            transform.scale = _state.scale;
        };

        auto applyRectTransformState = [](Canis::Entity *_target, const SavedRectTransformState &_state) -> void
        {
            if (_target == nullptr || !_state.valid || !_target->HasComponent<RectTransform>())
                return;

            RectTransform &transform = _target->GetComponent<RectTransform>();
            transform.position = _state.position;
            transform.size = _state.size;
            transform.scale = _state.scale;
            transform.anchorMin = _state.anchorMin;
            transform.anchorMax = _state.anchorMax;
            transform.pivot = _state.pivot;
            transform.originOffset = _state.originOffset;
            transform.depth = _state.depth;
            transform.rotation = _state.rotation;
            transform.rotationOriginOffset = _state.rotationOriginOffset;
        };

        Canis::Entity *parent = GetHierarchyParent(_entity);
        int childIndex = -1;
        if (parent != nullptr)
        {
            if (std::vector<Canis::Entity*> *siblings = GetHierarchyChildren(parent))
            {
                auto it = std::find(siblings->begin(), siblings->end(), _entity);
                if (it != siblings->end())
                    childIndex = static_cast<int>(std::distance(siblings->begin(), it));
            }
        }

        int rootIndex = -1;
        if (parent == nullptr)
        {
            auto it = std::find(m_hierarchyRootOrder.begin(), m_hierarchyRootOrder.end(), _entity->uuid);
            if (it != m_hierarchyRootOrder.end())
                rootIndex = static_cast<int>(std::distance(m_hierarchyRootOrder.begin(), it));
            else
                rootIndex = static_cast<int>(m_hierarchyRootOrder.size());
        }

        const std::string instanceName = _entity->name;
        const std::string instanceTag = _entity->tag;
        const bool instanceActive = _entity->active;
        const SavedTransformState rootTransformState = saveTransformState(_entity);
        const SavedRectTransformState rootRectState = saveRectTransformState(_entity);
        const SavedTransformState firstTransformState = saveTransformState(prefabInstance.firstEntity);
        const SavedRectTransformState firstRectState = saveRectTransformState(prefabInstance.firstEntity);
        const SceneAssetHandle prefabHandle = prefabInstance.prefab;

        std::vector<Canis::UUID> updatedRootOrder = m_hierarchyRootOrder;
        if (parent == nullptr)
        {
            if (auto it = std::find(updatedRootOrder.begin(), updatedRootOrder.end(), _entity->uuid); it != updatedRootOrder.end())
                updatedRootOrder.erase(it);
        }

        m_scene->Destroy(*_entity);

        Canis::Entity *newTopLevel = InstantiatePrefabHierarchyRoot(*m_scene, prefabHandle, instanceName);
        if (newTopLevel == nullptr)
            return nullptr;

        newTopLevel->name = instanceName;
        newTopLevel->tag = instanceTag;
        newTopLevel->active = instanceActive;

        if (parent != nullptr)
        {
            const bool parented =
                (childIndex >= 0)
                    ? SetHierarchyParentAtIndexKeepingLocal(newTopLevel, parent, static_cast<std::size_t>(childIndex))
                    : SetHierarchyParentKeepingLocal(newTopLevel, parent);

            if (!parented)
            {
                m_scene->Destroy(*newTopLevel);
                return nullptr;
            }
        }
        else
        {
            InsertRootEntityOrder(updatedRootOrder, newTopLevel, rootIndex);
            m_hierarchyRootOrder = updatedRootOrder;
        }

        applyTransformState(newTopLevel, rootTransformState);
        applyRectTransformState(newTopLevel, rootRectState);

        if (newTopLevel->HasComponent<PrefabInstance>())
        {
            PrefabInstance &newPrefabInstance = newTopLevel->GetComponent<PrefabInstance>();
            applyTransformState(newPrefabInstance.firstEntity, firstTransformState);
            applyRectTransformState(newPrefabInstance.firstEntity, firstRectState);
        }

        if (_focusSelection)
        {
            FocusEntity(newTopLevel);
            m_selectedAssetPath.clear();
        }
        m_forceRefresh = true;
        return newTopLevel;
    }

    void Editor::ProcessQueuedPrefabRebuilds()
    {
        if (m_scene == nullptr)
            return;

        if (!m_rebuildAllPrefabInstancesRequested && m_queuedPrefabInstanceRebuilds.empty())
            return;

        std::vector<Canis::UUID> targets = {};
        targets.reserve(m_queuedPrefabInstanceRebuilds.size() + m_scene->GetEntities().size());

        if (m_rebuildAllPrefabInstancesRequested)
        {
            for (Canis::Entity *entity : m_scene->GetEntities())
            {
                if (entity != nullptr && entity->HasComponent<PrefabInstance>())
                    targets.push_back(entity->uuid);
            }
        }

        targets.insert(targets.end(), m_queuedPrefabInstanceRebuilds.begin(), m_queuedPrefabInstanceRebuilds.end());

        m_queuedPrefabInstanceRebuilds.clear();
        m_rebuildAllPrefabInstancesRequested = false;

        Canis::UUID selectedUuid = Canis::UUID(0);
        if (m_index >= 0 && m_index < static_cast<int>(m_scene->GetEntities().size()) && m_scene->GetEntities()[m_index] != nullptr)
            selectedUuid = m_scene->GetEntities()[m_index]->uuid;

        std::unordered_set<Canis::UUID> seen = {};
        const bool multipleTargets = targets.size() > 1;

        for (Canis::UUID uuid : targets)
        {
            if (!seen.insert(uuid).second)
                continue;

            Canis::Entity *entity = m_scene->GetEntityWithUUID(uuid);
            if (entity == nullptr || !entity->HasComponent<PrefabInstance>())
                continue;

            const bool focusSelection = !multipleTargets || uuid == selectedUuid;
            Canis::Entity *rebuilt = RebuildPrefabInstanceNow(entity, focusSelection);
            if (rebuilt != nullptr && uuid == selectedUuid)
                selectedUuid = rebuilt->uuid;
        }
    }

    void Editor::FrameEntityInScene(Canis::Entity *_entity)
    {
        if (_entity == nullptr || !_entity->HasComponent<Canis::Transform>())
            return;

        const Canis::Transform &transform = _entity->GetComponent<Canis::Transform>();
        const Vector3 target = transform.GetGlobalPosition();
        const Vector3 globalScale = transform.GetGlobalScale();
        const float radius = std::max({
            std::abs(globalScale.x),
            std::abs(globalScale.y),
            std::abs(globalScale.z),
            0.75f});

        const float yaw = DEG2RAD * m_editorCamera3DYaw;
        const float pitch = DEG2RAD * m_editorCamera3DPitch;

        Vector3 forward = Vector3(
            std::cos(pitch) * std::cos(yaw),
            std::sin(pitch),
            std::cos(pitch) * std::sin(yaw));

        if (glm::length(forward) <= 0.0001f)
            forward = Vector3(0.0f, 0.0f, -1.0f);
        else
            forward = glm::normalize(forward);

        const float halfFovRadians = DEG2RAD * m_editorCamera3DFovDegrees * 0.5f;
        const float fovTangent = std::max(std::tan(halfFovRadians), 0.15f);
        const float distance = std::clamp((radius / fovTangent) + radius, 4.0f, 500.0f);

        m_sceneCameraMode = SceneCameraMode::SCENE_CAMERA_3D;
        m_editorCamera3DPosition = target - forward * distance;
    }

    void Editor::InputEntity(const std::string &_name, Canis::Entity *&_variable)
    {
        InputEntity(_name, nullptr, _variable);
    }

    void Editor::InputEntity(const std::string &_name, const char *_idSuffix, Canis::Entity *&_variable)
    {
        PushInspectorFieldID(_name.c_str(), _idSuffix);
        ImGui::Text("%s", _name.c_str());
        
        ImGui::SameLine();

        std::string label;
        Canis::Entity *entity = *&_variable;
        if (entity)
            label = "[entity] " + entity->name;
        else
            label = "[ missing entity ]";

        ImGui::Button(label.c_str(), ImVec2(150, 0));

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ENTITY_DRAG"))
            {
                const Canis::UUID dropped = *static_cast<const Canis::UUID *>(payload->Data);
                Canis::Entity *e = m_scene->GetEntityWithUUID(dropped);

                if (e)
                    *&_variable = e;
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            if (entity)
                FocusEntity(entity);
        }

        if (entity && ImGui::BeginPopupContextItem("ctx"))
        {
            if (ImGui::MenuItem("Clear"))
                *&_variable = nullptr;

            if (ImGui::MenuItem("Select in Hierarchy"))
                FocusEntity(entity);

            ImGui::EndPopup();
        }

        PopInspectorFieldID(_idSuffix);
    }

    void Editor::InputAnimationClip(const std::string& _name, Canis::AnimationClip2DID &_variable)
    {
        InputAnimationClip(_name, nullptr, _variable);
    }

    void Editor::InputAnimationClipAsset(const std::string &_name, Canis::AnimationClipAssetHandle &_variable)
    {
        InputAnimationClipAsset(_name, nullptr, _variable);
    }

    void Editor::InputAnimatorControllerAsset(const std::string &_name, Canis::AnimatorControllerAssetHandle &_variable)
    {
        InputAnimatorControllerAsset(_name, nullptr, _variable);
    }

    void Editor::InputAudioAsset(const std::string &_name, Canis::AudioAssetHandle &_variable)
    {
        InputAudioAsset(_name, nullptr, _variable);
    }

    void Editor::InputAudioAsset(const std::string &_name, const char *_idSuffix, Canis::AudioAssetHandle &_variable)
    {
        PushInspectorFieldID(_name.c_str(), _idSuffix);
        ImGui::Text("%s", _name.c_str());
        ImGui::SameLine();

        std::string resolvedPath = _variable.path;
        if (_variable.uuid != UUID(0))
        {
            const std::string pathFromUUID = AssetManager::GetPath(_variable.uuid);
            if (pathFromUUID != "Path was not found in AssetLibrary")
                resolvedPath = pathFromUUID;
        }

        if (!resolvedPath.empty())
            _variable.path = resolvedPath;

        std::string label = "[ none ]";
        if (!resolvedPath.empty())
        {
            if (MetaFileAsset *meta = AssetManager::GetMetaFile(resolvedPath))
                label = meta->name;
            else
                label = resolvedPath;
        }

        ImGui::Button(label.c_str(), ImVec2(170, 0));

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
            {
                const AssetDragData dropped = *static_cast<const AssetDragData *>(payload->Data);
                const std::string path = AssetManager::GetPath(dropped.uuid);

                if (MetaFileAsset *meta = AssetManager::GetMetaFile(path))
                {
                    if (meta->type == MetaFileAsset::FileType::AUDIO)
                    {
                        _variable.uuid = meta->uuid;
                        _variable.path = path;
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopupContextItem("audio_asset_ctx"))
        {
            if (ImGui::MenuItem("Clear"))
            {
                _variable.uuid = UUID(0);
                _variable.path.clear();
            }

            ImGui::EndPopup();
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("X##clear_audio_asset"))
        {
            _variable.uuid = UUID(0);
            _variable.path.clear();
        }

        PopInspectorFieldID(_idSuffix);
    }

    void Editor::InputAnimationClip(const std::string& _name, const char *_idSuffix, Canis::AnimationClip2DID &_variable)
    {
        PushInspectorFieldID(_name.c_str(), _idSuffix);
        ImGui::Text("%s", _name.c_str());

        ImGui::SameLine();

        const char* empty = "[ empty ]";

        if (auto *meta = AssetManager::GetMetaFile(AssetManager::GetPath(_variable)))
            ImGui::Button(meta->name.c_str(), ImVec2(150, 0));
        else
            ImGui::Button(empty, ImVec2(150, 0));

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
            {
                const AssetDragData dropped = *static_cast<const AssetDragData*>(payload->Data);
                std::string path = AssetManager::GetPath(dropped.uuid);
                SpriteAnimationAsset* asset = AssetManager::GetSpriteAnimation(path);

                if (asset)
                {
                    _variable = AssetManager::GetID(path);
                }
            }
            ImGui::EndDragDropTarget();
        }

        PopInspectorFieldID(_idSuffix);
    }

    void Editor::InputAnimationClipAsset(const std::string &_name, const char *_idSuffix, Canis::AnimationClipAssetHandle &_variable)
    {
        PushInspectorFieldID(_name.c_str(), _idSuffix);
        ImGui::Text("%s", _name.c_str());
        ImGui::SameLine();

        std::string resolvedPath = AssetManager::ResolvePath(_variable);
        if (!resolvedPath.empty())
            _variable.path = resolvedPath;

        std::string label = "[ none ]";
        if (!resolvedPath.empty())
        {
            if (MetaFileAsset *meta = AssetManager::GetMetaFile(resolvedPath))
                label = meta->name;
            else
                label = resolvedPath;
        }

        ImGui::Button(label.c_str(), ImVec2(170, 0));

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
            {
                const AssetDragData dropped = *static_cast<const AssetDragData *>(payload->Data);
                const std::string path = AssetManager::GetPath(dropped.uuid);
                if (MetaFileAsset *meta = AssetManager::GetMetaFile(path))
                {
                    if (meta->type == MetaFileAsset::FileType::ANIMATIONCLIP)
                    {
                        _variable.uuid = meta->uuid;
                        _variable.path = path;
                        RememberLastAnimationClipAssetPath(path);
                        m_animationClipStatePath = path;
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopupContextItem("animation_clip_asset_ctx"))
        {
            if (ImGui::MenuItem("Clear"))
            {
                _variable.uuid = UUID(0);
                _variable.path.clear();
            }

            if (!resolvedPath.empty() && ImGui::MenuItem("Open In Animation"))
            {
                m_animationClipStatePath = resolvedPath;
                RememberLastAnimationClipAssetPath(resolvedPath);
            }

            ImGui::EndPopup();
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("X##clear_animation_clip_asset"))
        {
            _variable.uuid = UUID(0);
            _variable.path.clear();
        }

        PopInspectorFieldID(_idSuffix);
    }

    void Editor::InputAnimatorControllerAsset(const std::string &_name, const char *_idSuffix, Canis::AnimatorControllerAssetHandle &_variable)
    {
        PushInspectorFieldID(_name.c_str(), _idSuffix);
        ImGui::Text("%s", _name.c_str());
        ImGui::SameLine();

        std::string resolvedPath = AssetManager::ResolvePath(_variable);
        if (!resolvedPath.empty())
            _variable.path = resolvedPath;

        std::string label = "[ none ]";
        if (!resolvedPath.empty())
        {
            if (MetaFileAsset *meta = AssetManager::GetMetaFile(resolvedPath))
                label = meta->name;
            else
                label = resolvedPath;
        }

        ImGui::Button(label.c_str(), ImVec2(170, 0));

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
            {
                const AssetDragData dropped = *static_cast<const AssetDragData *>(payload->Data);
                const std::string path = AssetManager::GetPath(dropped.uuid);
                if (MetaFileAsset *meta = AssetManager::GetMetaFile(path))
                {
                    if (meta->type == MetaFileAsset::FileType::ANIMATORCONTROLLER)
                    {
                        _variable.uuid = meta->uuid;
                        _variable.path = path;
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopupContextItem("animator_controller_asset_ctx"))
        {
            if (ImGui::MenuItem("Clear"))
            {
                _variable.uuid = UUID(0);
                _variable.path.clear();
            }

            ImGui::EndPopup();
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("X##clear_animator_controller_asset"))
        {
            _variable.uuid = UUID(0);
            _variable.path.clear();
        }

        PopInspectorFieldID(_idSuffix);
    }

    void Editor::InputSceneAsset(const std::string &_name, Canis::SceneAssetHandle &_variable)
    {
        InputSceneAsset(_name, nullptr, _variable);
    }

    void Editor::InputSceneAsset(const std::string &_name, const char *_idSuffix, Canis::SceneAssetHandle &_variable)
    {
        PushInspectorFieldID(_name.c_str(), _idSuffix);
        ImGui::Text("%s", _name.c_str());
        ImGui::SameLine();

        std::string resolvedPath = _variable.path;
        if (_variable.uuid != UUID(0))
        {
            const std::string pathFromUUID = AssetManager::GetPath(_variable.uuid);
            if (pathFromUUID != "Path was not found in AssetLibrary")
                resolvedPath = pathFromUUID;
        }

        if (!resolvedPath.empty())
            _variable.path = resolvedPath;

        std::string label = "[ none ]";
        if (!resolvedPath.empty())
        {
            if (MetaFileAsset *meta = AssetManager::GetMetaFile(resolvedPath))
                label = meta->name;
            else
                label = resolvedPath;
        }

        ImGui::Button(label.c_str(), ImVec2(170, 0));

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
            {
                const AssetDragData dropped = *static_cast<const AssetDragData *>(payload->Data);
                const std::string path = AssetManager::GetPath(dropped.uuid);

                if (MetaFileAsset *meta = AssetManager::GetMetaFile(path))
                {
                    if (meta->type == MetaFileAsset::FileType::SCENE)
                    {
                        _variable.uuid = meta->uuid;
                        _variable.path = path;
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopupContextItem("scene_asset_ctx"))
        {
            if (ImGui::MenuItem("Clear"))
            {
                _variable.uuid = UUID(0);
                _variable.path.clear();
            }

            ImGui::EndPopup();
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("X##clear_scene_asset"))
        {
            _variable.uuid = UUID(0);
            _variable.path.clear();
        }

        PopInspectorFieldID(_idSuffix);
    }

    bool Editor::IsDescendantOf(Canis::Entity *_parent, Canis::Entity *_potentialChild)
    {
        if (!_parent || !_potentialChild)
            return false;

        std::vector<Canis::Entity*>* children = nullptr;
        if (auto *parentRT = (_parent != nullptr && _parent->HasComponent<RectTransform>() ? &_parent->GetComponent<RectTransform>() : nullptr))
            children = &parentRT->children;
        else if (auto *parentTransform = (_parent != nullptr && _parent->HasComponent<Transform>() ? &_parent->GetComponent<Transform>() : nullptr))
            children = &parentTransform->children;

        if (children == nullptr)
            return false;

        for (auto *child : *children)
        {
            if (!child)
                continue;
            if (child == _potentialChild)
                return true;
            if (IsDescendantOf(child, _potentialChild))
                return true;
        }

        return false;
    }

    static std::vector<Canis::Entity*>* GetHierarchyChildren(Canis::Entity *_entity)
    {
        if (_entity == nullptr)
            return nullptr;

        if (Canis::RectTransform* transform = (_entity != nullptr && _entity->HasComponent<RectTransform>() ? &_entity->GetComponent<RectTransform>() : nullptr))
            return &transform->children;

        if (Canis::Transform* transform = (_entity != nullptr && _entity->HasComponent<Transform>() ? &_entity->GetComponent<Transform>() : nullptr))
            return &transform->children;

        return nullptr;
    }

    static Canis::Entity* GetHierarchyParent(Canis::Entity *_entity)
    {
        if (_entity == nullptr)
            return nullptr;

        if (Canis::RectTransform* transform = (_entity != nullptr && _entity->HasComponent<RectTransform>() ? &_entity->GetComponent<RectTransform>() : nullptr))
            return transform->parent;

        if (Canis::Transform* transform = (_entity != nullptr && _entity->HasComponent<Transform>() ? &_entity->GetComponent<Transform>() : nullptr))
            return transform->parent;

        return nullptr;
    }

    static bool SetHierarchyParent(Canis::Entity *_child, Canis::Entity *_parent)
    {
        if (_child == nullptr)
            return false;

        if (Canis::RectTransform* childTransform = (_child != nullptr && _child->HasComponent<RectTransform>() ? &_child->GetComponent<RectTransform>() : nullptr))
        {
            if (_parent != nullptr && !_parent->HasComponent<RectTransform>())
                return false;

            childTransform->SetParent(_parent);
            return true;
        }

        if (Canis::Transform* childTransform = (_child != nullptr && _child->HasComponent<Transform>() ? &_child->GetComponent<Transform>() : nullptr))
        {
            if (_parent != nullptr && !_parent->HasComponent<Transform>())
                return false;

            childTransform->SetParent(_parent);
            return true;
        }

        return false;
    }

    static bool SetHierarchyParentAtIndex(Canis::Entity *_child, Canis::Entity *_parent, std::size_t _index)
    {
        if (_child == nullptr)
            return false;

        if (Canis::RectTransform* childTransform = (_child != nullptr && _child->HasComponent<RectTransform>() ? &_child->GetComponent<RectTransform>() : nullptr))
        {
            if (_parent != nullptr && !_parent->HasComponent<RectTransform>())
                return false;

            childTransform->SetParentAtIndex(_parent, _index);
            return true;
        }

        if (Canis::Transform* childTransform = (_child != nullptr && _child->HasComponent<Transform>() ? &_child->GetComponent<Transform>() : nullptr))
        {
            if (_parent != nullptr && !_parent->HasComponent<Transform>())
                return false;

            childTransform->SetParentAtIndex(_parent, _index);
            return true;
        }

        return false;
    }

    static bool UnparentHierarchyEntity(Canis::Entity *_entity)
    {
        if (_entity == nullptr)
            return false;

        if (Canis::RectTransform* transform = (_entity != nullptr && _entity->HasComponent<RectTransform>() ? &_entity->GetComponent<RectTransform>() : nullptr))
        {
            if (transform->parent == nullptr)
                return false;

            transform->SetParent(nullptr);
            return true;
        }

        if (Canis::Transform* transform = (_entity != nullptr && _entity->HasComponent<Transform>() ? &_entity->GetComponent<Transform>() : nullptr))
        {
            if (transform->parent == nullptr)
                return false;

            transform->SetParent(nullptr);
            return true;
        }

        return false;
    }

    static void GetHierarchyChildrenRecursive(Canis::Entity* _entity, std::vector<Canis::Entity*> &_entities)
    {
        std::vector<Canis::Entity*>* children = GetHierarchyChildren(_entity);
        if (children == nullptr)
            return;

        for (std::size_t i = 0; i < children->size(); ++i)
        {
            Canis::Entity* e = (*children)[i];
            if (!e)
                continue;

            _entities.push_back(e);

            GetHierarchyChildrenRecursive(e, _entities);
        }
    }

    enum class HierarchyEntityKind
    {
        None,
        Rect,
        Transform,
        Mixed
    };

    static std::string ResolveAssetDragPath(const AssetDragData &_dropped)
    {
        std::string path = AssetManager::GetPath(_dropped.uuid);
        if (path.rfind("Path was not found", 0) == 0)
            path = _dropped.path;

        return path;
    }

    static Canis::Entity* FindHierarchyEntityByUUID(const std::vector<Canis::Entity*> &_entities, Canis::UUID _uuid)
    {
        for (Canis::Entity* entity : _entities)
        {
            if (entity != nullptr && entity->uuid == _uuid)
                return entity;
        }

        return nullptr;
    }

    static HierarchyEntityKind GetHierarchyEntityKind(Canis::Entity *_entity)
    {
        if (_entity == nullptr)
            return HierarchyEntityKind::None;

        if (_entity->HasComponent<RectTransform>())
            return HierarchyEntityKind::Rect;

        if (_entity->HasComponent<Transform>())
            return HierarchyEntityKind::Transform;

        return HierarchyEntityKind::None;
    }

    static HierarchyEntityKind GetHierarchyEntityKind(const std::vector<Canis::Entity*> &_entities)
    {
        HierarchyEntityKind kind = HierarchyEntityKind::None;

        for (Canis::Entity* entity : _entities)
        {
            const HierarchyEntityKind entityKind = GetHierarchyEntityKind(entity);
            if (entityKind == HierarchyEntityKind::None)
                return HierarchyEntityKind::None;

            if (kind == HierarchyEntityKind::None)
            {
                kind = entityKind;
                continue;
            }

            if (kind != entityKind)
                return HierarchyEntityKind::Mixed;
        }

        return kind;
    }

    static bool SetHierarchyParentAtIndexKeepingLocal(Canis::Entity *_child, Canis::Entity *_parent, std::size_t _index)
    {
        if (_child == nullptr)
            return false;

        if (Canis::RectTransform* childTransform = (_child->HasComponent<RectTransform>() ? &_child->GetComponent<RectTransform>() : nullptr))
        {
            if (_parent != nullptr && !_parent->HasComponent<RectTransform>())
                return false;

            const Vector2 localPosition = childTransform->position;
            childTransform->SetParentAtIndex(_parent, _index);
            childTransform->position = localPosition;
            return true;
        }

        if (Canis::Transform* childTransform = (_child->HasComponent<Transform>() ? &_child->GetComponent<Transform>() : nullptr))
        {
            if (_parent != nullptr && !_parent->HasComponent<Transform>())
                return false;

            const Vector3 localPosition = childTransform->position;
            const Vector3 localRotation = childTransform->rotation;
            const Vector3 localScale = childTransform->scale;
            childTransform->SetParentAtIndex(_parent, _index);
            childTransform->position = localPosition;
            childTransform->rotation = localRotation;
            childTransform->scale = localScale;
            return true;
        }

        return false;
    }

    static bool SetHierarchyParentKeepingLocal(Canis::Entity *_child, Canis::Entity *_parent)
    {
        if (_parent == nullptr)
            return SetHierarchyParent(_child, nullptr);

        if (std::vector<Canis::Entity*>* children = GetHierarchyChildren(_parent))
            return SetHierarchyParentAtIndexKeepingLocal(_child, _parent, children->size());

        return false;
    }

    static Canis::Entity* CreatePrefabWrapperEntity(Canis::Scene &_scene, const std::string &_displayName, HierarchyEntityKind _kind)
    {
        if (_kind != HierarchyEntityKind::Rect && _kind != HierarchyEntityKind::Transform)
            return nullptr;

        Canis::Entity* wrapper = _scene.CreateEntity(MakeUniqueEntityName(_scene, _displayName));
        if (wrapper == nullptr)
            return nullptr;

        if (_kind == HierarchyEntityKind::Rect)
            wrapper->AddComponent<RectTransform>();
        else
            wrapper->AddComponent<Transform>();

        return wrapper;
    }

    static void TryAssignPrefabHandle(Canis::Entity *_entity, const SceneAssetHandle &_prefabHandle)
    {
        if (_entity == nullptr || !_entity->HasComponent<NetworkIdentity>())
            return;

        NetworkIdentity &identity = _entity->GetComponent<NetworkIdentity>();
        if (identity.prefab.Empty())
            identity.prefab = _prefabHandle;
    }

    static void AssignPrefabInstanceMetadata(Canis::Entity *_entity, const SceneAssetHandle &_prefabHandle, Canis::Entity *_firstEntity)
    {
        if (_entity == nullptr)
            return;

        PrefabInstance &prefabInstance = *_entity->AddComponent<PrefabInstance>();
        prefabInstance.prefab = _prefabHandle;
        prefabInstance.firstEntity = (_firstEntity != nullptr) ? _firstEntity : _entity;
    }

    static Canis::Entity* InstantiatePrefabHierarchyRoot(Canis::Scene &_scene, const SceneAssetHandle &_prefabHandle, const std::string &_displayName)
    {
        std::vector<Canis::Entity*> roots = _scene.Instantiate(_prefabHandle);
        if (roots.empty())
            return nullptr;

        const std::string uniqueDisplayName = MakeUniqueEntityName(_scene, _displayName);
        if (roots.size() == 1)
        {
            Canis::Entity* root = roots.front();
            if (root != nullptr)
            {
                root->name = uniqueDisplayName;
                AssignPrefabInstanceMetadata(root, _prefabHandle, root);
                TryAssignPrefabHandle(root, _prefabHandle);
            }

            return root;
        }

        const HierarchyEntityKind kind = GetHierarchyEntityKind(roots);
        if (kind != HierarchyEntityKind::Rect && kind != HierarchyEntityKind::Transform)
        {
            Debug::Warning(
                "Editor could not create a single hierarchy item for prefab '%s' because its roots do not share a supported hierarchy type.",
                _prefabHandle.path.c_str());

            for (Canis::Entity* root : roots)
            {
                if (root != nullptr)
                    _scene.Destroy(*root);
            }

            return nullptr;
        }

        Canis::Entity* wrapper = CreatePrefabWrapperEntity(_scene, uniqueDisplayName, kind);
        if (wrapper == nullptr)
        {
            for (Canis::Entity* root : roots)
            {
                if (root != nullptr)
                    _scene.Destroy(*root);
            }

            return nullptr;
        }

        for (std::size_t i = 0; i < roots.size(); ++i)
        {
            if (!SetHierarchyParentAtIndexKeepingLocal(roots[i], wrapper, i))
            {
                _scene.Destroy(*wrapper);

                for (Canis::Entity* root : roots)
                {
                    if (root != nullptr && _scene.GetEntity(root->id) == root)
                        _scene.Destroy(*root);
                }

                return nullptr;
            }
        }

        AssignPrefabInstanceMetadata(wrapper, _prefabHandle, roots.front());
        TryAssignPrefabHandle(wrapper, _prefabHandle);
        return wrapper;
    }

    static bool InsertRootEntityOrder(std::vector<Canis::UUID> &_rootOrder, Canis::Entity *_entity, int _targetRootPos)
    {
        if (_entity == nullptr)
            return false;

        if (auto it = std::find(_rootOrder.begin(), _rootOrder.end(), _entity->uuid); it != _rootOrder.end())
            _rootOrder.erase(it);

        _targetRootPos = std::clamp(_targetRootPos, 0, static_cast<int>(_rootOrder.size()));
        _rootOrder.insert(_rootOrder.begin() + _targetRootPos, _entity->uuid);
        return true;
    }

    static std::string SanitizePrefabAssetStem(const std::string &_name)
    {
        std::string sanitized = {};
        sanitized.reserve(_name.size());

        bool lastWasUnderscore = false;
        for (const char c : _name)
        {
            const unsigned char uc = static_cast<unsigned char>(c);
            if (std::isalnum(uc))
            {
                sanitized.push_back(static_cast<char>(std::tolower(uc)));
                lastWasUnderscore = false;
            }
            else if (!lastWasUnderscore)
            {
                sanitized.push_back('_');
                lastWasUnderscore = true;
            }
        }

        while (!sanitized.empty() && sanitized.front() == '_')
            sanitized.erase(sanitized.begin());
        while (!sanitized.empty() && sanitized.back() == '_')
            sanitized.pop_back();

        if (sanitized.empty())
            sanitized = "new_prefab";

        return sanitized;
    }

    static std::filesystem::path BuildUniquePrefabScenePath(const std::filesystem::path &_targetDirectory, const std::string &_entityName)
    {
        namespace fs = std::filesystem;

        const std::string stem = SanitizePrefabAssetStem(_entityName);
        fs::path candidate = _targetDirectory / (stem + ".scene");
        int index = 1;
        while (fs::exists(candidate) || fs::exists(candidate.string() + ".meta"))
        {
            candidate = _targetDirectory / (stem + "_" + std::to_string(index) + ".scene");
            ++index;
        }

        return candidate;
    }

    static void PrepareExportedPrefabRootNode(YAML::Node &_rootNode)
    {
        if (!_rootNode || !_rootNode.IsMap())
            return;

        _rootNode.remove(PrefabInstance::ScriptName);

        if (YAML::Node transformNode = _rootNode[Transform::ScriptName])
            transformNode["parent"] = static_cast<uint64_t>(0);

        if (YAML::Node rectTransformNode = _rootNode[RectTransform::ScriptName])
            rectTransformNode["parent"] = static_cast<uint64_t>(0);

        if (YAML::Node networkIdentityNode = _rootNode[NetworkIdentity::ScriptName])
            networkIdentityNode.remove("prefab");
    }

    static bool ExportHierarchyEntityToPrefabAsset(
        Canis::Scene &_scene,
        Canis::Entity &_rootEntity,
        const std::filesystem::path &_targetDirectory,
        std::string &_outPrefabPath)
    {
        namespace fs = std::filesystem;

        _outPrefabPath.clear();
        if (!fs::exists(_targetDirectory) || !fs::is_directory(_targetDirectory))
            return false;

        std::vector<Canis::Entity*> entities = { &_rootEntity };
        GetHierarchyChildrenRecursive(&_rootEntity, entities);

        YAML::Node entitiesNode(YAML::NodeType::Sequence);
        for (Canis::Entity *entity : entities)
        {
            if (entity == nullptr)
                continue;

            entitiesNode.push_back(_scene.EncodeEntity(*entity));
        }

        if (!entitiesNode.IsSequence() || entitiesNode.size() == 0)
            return false;

        YAML::Node rootNode = entitiesNode[0];
        PrepareExportedPrefabRootNode(rootNode);

        YAML::Node prefabSceneRoot(YAML::NodeType::Map);
        prefabSceneRoot["Environment"] = YAML::Node(YAML::NodeType::Map);
        prefabSceneRoot["Entities"] = entitiesNode;

        const fs::path prefabPath = BuildUniquePrefabScenePath(_targetDirectory, _rootEntity.name);
        YAML::Emitter out;
        out << prefabSceneRoot;

        std::ofstream file(prefabPath);
        if (!file.is_open())
            return false;

        file << out.c_str();
        file.close();
        if (!file.good())
            return false;

        _outPrefabPath = prefabPath.generic_string();
        return AssetManager::GetMetaFile(_outPrefabPath) != nullptr;
    }

    static bool ExportHierarchyRootsToPrefabAsset(
        Canis::Scene &_scene,
        const std::vector<Canis::Entity*> &_roots,
        const std::string &_prefabPath)
    {
        if (_roots.empty() || _prefabPath.empty())
            return false;

        YAML::Node entitiesNode(YAML::NodeType::Sequence);
        std::vector<std::size_t> rootNodeIndices = {};
        std::unordered_set<uint64_t> seenEntityIds = {};

        for (Canis::Entity *root : _roots)
        {
            if (root == nullptr)
                continue;

            std::vector<Canis::Entity*> entities = { root };
            GetHierarchyChildrenRecursive(root, entities);

            rootNodeIndices.push_back(entitiesNode.size());
            for (Canis::Entity *entity : entities)
            {
                if (entity == nullptr)
                    continue;

                const Canis::UUID liveUUID = _scene.GetLiveEntityUUID(entity);
                if ((uint64_t)liveUUID == 0 || !seenEntityIds.insert((uint64_t)liveUUID).second)
                    continue;

                entitiesNode.push_back(_scene.EncodeEntity(*entity));
            }
        }

        if (!entitiesNode.IsSequence() || entitiesNode.size() == 0)
            return false;

        for (const std::size_t rootNodeIndex : rootNodeIndices)
        {
            if (rootNodeIndex < entitiesNode.size())
            {
                YAML::Node rootNode = entitiesNode[rootNodeIndex];
                PrepareExportedPrefabRootNode(rootNode);
            }
        }

        YAML::Node prefabSceneRoot(YAML::NodeType::Map);
        prefabSceneRoot["Environment"] = YAML::Node(YAML::NodeType::Map);
        prefabSceneRoot["Entities"] = entitiesNode;

        YAML::Emitter out;
        out << prefabSceneRoot;

        std::ofstream file(_prefabPath);
        if (!file.is_open())
            return false;

        file << out.c_str();
        file.close();
        if (!file.good())
            return false;

        return AssetManager::GetMetaFile(_prefabPath) != nullptr;
    }

    static void AssignPrefabHandle(Canis::Entity *_entity, const SceneAssetHandle &_prefabHandle)
    {
        if (_entity == nullptr || !_entity->HasComponent<NetworkIdentity>())
            return;

        NetworkIdentity &identity = _entity->GetComponent<NetworkIdentity>();
        identity.prefab = _prefabHandle;
    }

    static std::string GetModelNodeDisplayName(const ModelAsset &_modelAsset, i32 _nodeIndex)
    {
        std::string nodeName = _modelAsset.GetNodeName(_nodeIndex);
        if (nodeName.empty())
            nodeName = "Node " + std::to_string(_nodeIndex);

        return nodeName;
    }

    static void ApplyModelNodeLocalTransform(Canis::Entity &_entity, const ModelAsset::Node3D &_node)
    {
        Transform &transform = _entity.GetComponent<Transform>();
        transform.position = _node.translation;
        transform.scale = _node.scale;
        transform.rotation = glm::eulerAngles(glm::quat(_node.rotation.w, _node.rotation.x, _node.rotation.y, _node.rotation.z));

        if (_node.hasMatrix)
        {
            Vector3 skew = Vector3(0.0f);
            Vector4 perspective = Vector4(0.0f);
            glm::quat orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            Vector3 translation = Vector3(0.0f);
            Vector3 scale = Vector3(1.0f);

            if (glm::decompose(_node.localMatrix, scale, orientation, translation, skew, perspective))
            {
                orientation = glm::normalize(orientation);
                transform.position = translation;
                transform.scale = scale;
                transform.rotation = glm::eulerAngles(orientation);
            }
        }
    }

    static void ConfigureModelNodeRenderer(Canis::Entity &_entity, i32 _modelId, i32 _nodeIndex)
    {
        Model &model = _entity.AddOrReplaceComponent<Model>();
        model.modelId = _modelId;
        model.nodeIndex = _nodeIndex;
        model.applyNodeTransform = false;
        model.color = Color(1.0f);

        Material &material = _entity.AddOrReplaceComponent<Material>();
        material.materialId = AssetManager::LoadMaterial("assets/defaults/materials/default.material");
    }

    static Canis::Entity* InstantiateModelNodeHierarchyRecursive(
        Canis::Scene &_scene,
        const ModelAsset &_modelAsset,
        i32 _modelId,
        i32 _nodeIndex,
        const std::string &_entityName)
    {
        const ModelAsset::Node3D *node = _modelAsset.GetNode(_nodeIndex);
        if (node == nullptr)
            return nullptr;

        Canis::Entity *entity = _scene.CreateEntity(_entityName);
        if (entity == nullptr)
            return nullptr;

        entity->AddComponent<Transform>();
        ApplyModelNodeLocalTransform(*entity, *node);

        if (_modelAsset.NodeHasPrimitives(_nodeIndex))
            ConfigureModelNodeRenderer(*entity, _modelId, _nodeIndex);

        for (std::size_t childIndex = 0; childIndex < node->children.size(); ++childIndex)
        {
            const i32 modelChildIndex = node->children[childIndex];
            Canis::Entity *childEntity = InstantiateModelNodeHierarchyRecursive(
                _scene,
                _modelAsset,
                _modelId,
                modelChildIndex,
                GetModelNodeDisplayName(_modelAsset, modelChildIndex));
            if (childEntity == nullptr)
                continue;

            if (!SetHierarchyParentAtIndexKeepingLocal(childEntity, entity, childIndex))
                _scene.Destroy(*childEntity);
        }

        return entity;
    }

    static Canis::Entity* InstantiateModelAssetHierarchyRoot(
        Canis::Scene &_scene,
        const std::string &_displayName,
        i32 _modelId,
        ModelAsset &_modelAsset)
    {
        const std::string uniqueDisplayName = MakeUniqueEntityName(_scene, _displayName);
        const std::vector<i32> &sceneRoots = _modelAsset.GetSceneRoots();

        if (sceneRoots.empty() || _modelAsset.GetNodeCount() <= 0)
        {
            Canis::Entity *entity = _scene.CreateEntity(uniqueDisplayName);
            if (entity == nullptr)
                return nullptr;

            entity->AddComponent<Transform>();
            ConfigureModelNodeRenderer(*entity, _modelId, -1);
            return entity;
        }

        if (sceneRoots.size() == 1)
            return InstantiateModelNodeHierarchyRecursive(_scene, _modelAsset, _modelId, sceneRoots.front(), uniqueDisplayName);

        Canis::Entity *wrapper = _scene.CreateEntity(uniqueDisplayName);
        if (wrapper == nullptr)
            return nullptr;

        wrapper->AddComponent<Transform>();
        for (std::size_t rootIndex = 0; rootIndex < sceneRoots.size(); ++rootIndex)
        {
            const i32 modelRootIndex = sceneRoots[rootIndex];
            Canis::Entity *rootEntity = InstantiateModelNodeHierarchyRecursive(
                _scene,
                _modelAsset,
                _modelId,
                modelRootIndex,
                GetModelNodeDisplayName(_modelAsset, modelRootIndex));
            if (rootEntity == nullptr)
                continue;

            if (!SetHierarchyParentAtIndexKeepingLocal(rootEntity, wrapper, rootIndex))
                _scene.Destroy(*rootEntity);
        }

        return wrapper;
    }

    static bool InstantiateSceneAssetIntoHierarchy(
        Canis::Scene &_scene,
        const AssetDragData &_dropped,
        Canis::Entity *_parent,
        int _childIndex,
        std::vector<Canis::UUID> *_rootOrder,
        int _targetRootPos,
        Canis::Entity* &_outTopLevel)
    {
        _outTopLevel = nullptr;

        const std::string droppedPath = ResolveAssetDragPath(_dropped);
        if (droppedPath.empty())
            return false;

        MetaFileAsset *meta = AssetManager::GetMetaFile(droppedPath);
        if (meta == nullptr)
            return false;

        const std::string displayName = meta->name.empty() ? GetFileName(droppedPath) : meta->name;
        Canis::Entity* topLevel = nullptr;

        if (meta->type == MetaFileAsset::FileType::SCENE)
        {
            const SceneAssetHandle prefabHandle = MakeSceneAssetHandleFromPath(droppedPath);
            topLevel = InstantiatePrefabHierarchyRoot(_scene, prefabHandle, displayName);
        }
        else if (meta->type == MetaFileAsset::FileType::MODEL)
        {
            const i32 modelId = AssetManager::LoadModel(droppedPath);
            ModelAsset *modelAsset = AssetManager::GetModel(modelId);
            if (modelAsset == nullptr)
                return false;

            topLevel = InstantiateModelAssetHierarchyRoot(_scene, displayName, modelId, *modelAsset);
        }
        else
        {
            return false;
        }

        if (topLevel == nullptr)
            return false;

        if (_parent != nullptr)
        {
            const bool parented = (_childIndex >= 0)
                ? SetHierarchyParentAtIndexKeepingLocal(topLevel, _parent, static_cast<std::size_t>(_childIndex))
                : SetHierarchyParentKeepingLocal(topLevel, _parent);

            if (!parented)
            {
                Debug::Warning(
                    "Editor could not parent asset '%s' under '%s' because the hierarchy types do not match.",
                    droppedPath.c_str(),
                    _parent->name.c_str());
                _scene.Destroy(*topLevel);
                return false;
            }
        }
        else if (_rootOrder != nullptr)
        {
            InsertRootEntityOrder(*_rootOrder, topLevel, _targetRootPos);
        }

        _outTopLevel = topLevel;
        return true;
    }

    void Editor::DrawHierarchyNode(Canis::Entity *_entity, std::vector<Canis::Entity *> &_entities, bool &_refresh)
    {
        if (!_entity)
            return;

        std::vector<Canis::Entity*> *children = GetHierarchyChildren(_entity);

        bool isSelected = (m_index >= 0 && m_index < (int)_entities.size() &&
                           _entities[m_index] == _entity);

        bool hasChildren = (children != nullptr && !children->empty());

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        if (!hasChildren)
            flags |= ImGuiTreeNodeFlags_Leaf;
        if (isSelected)
            flags |= ImGuiTreeNodeFlags_Selected;

        if (hasChildren &&
            std::find(m_hierarchyRevealPath.begin(), m_hierarchyRevealPath.end(), _entity->uuid) != m_hierarchyRevealPath.end())
        {
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        }

        std::string label = _entity->name + "##" + std::to_string(_entity->uuid);
        bool nodeOpen = ImGui::TreeNodeEx(label.c_str(), flags);

        if (m_hierarchyRevealTargetUUID == _entity->uuid)
        {
            ImGui::SetScrollHereY(0.5f);
            m_hierarchyRevealTargetUUID = UUID(0);
            m_hierarchyRevealPath.clear();
        }

        // select on click
        if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            for (int i = 0; i < (int)_entities.size(); ++i)
            {
                if (_entities[i] == _entity)
                {
                    m_index = i;
                    m_selectedAssetPath.clear();
                    _refresh = true;
                    break;
                }
            }
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            FocusEntity(_entity);
            FrameEntityInScene(_entity);
            _refresh = true;
        }

        // drag source
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
        {
            Canis::UUID uuid = _entity->uuid;
            ImGui::SetDragDropPayload("ENTITY_DRAG", &uuid, sizeof(Canis::UUID));
            ImGui::Text("Entity: %s", _entity->name.c_str());
            ImGui::EndDragDropSource();
        }

        // drop ON node = parent and append at end
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ENTITY_DRAG"))
            {
                Canis::UUID droppedUUID = *static_cast<const Canis::UUID *>(payload->Data);
                Canis::Entity *droppedEntity = FindHierarchyEntityByUUID(_entities, droppedUUID);

                if (droppedEntity && droppedEntity != _entity)
                {
                    if (!IsDescendantOf(droppedEntity, _entity) &&
                        SetHierarchyParent(droppedEntity, _entity))
                    {
                        _refresh = true;
                    }
                }
            }

            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
            {
                const AssetDragData dropped = *static_cast<const AssetDragData *>(payload->Data);
                Canis::Entity* spawnedEntity = nullptr;
                if (InstantiateSceneAssetIntoHierarchy(*m_scene, dropped, _entity, -1, nullptr, -1, spawnedEntity))
                {
                    FocusEntity(spawnedEntity);
                    m_selectedAssetPath.clear();
                    _refresh = true;
                }
            }
            ImGui::EndDragDropTarget();
        }

        // context menu
        bool removeRequested = false;
        if (ImGui::BeginPopupContextItem())
        {
            int idx = -1;
            for (int i = 0; i < (int)_entities.size(); ++i)
            {
                if (_entities[i] == _entity)
                {
                    idx = i;
                    break;
                }
            }

            (void)DrawHierarchyCreateMenu(*this, *m_app, *m_scene, _entity, _refresh);

            if (idx >= 0 && ImGui::MenuItem("Duplicate"))
            {
                Debug::Log("Duplicate");
                Canis::Entity *selected = _entities[idx];

                std::vector<Canis::Entity*> entities;
                entities.push_back(selected);

                // get all entities to duplicate
                GetHierarchyChildrenRecursive(selected, entities);

                // encode entities into sequence of nodes
                YAML::Node nodes;
                for (Canis::Entity* e : entities)
                {
                    nodes.push_back(m_scene->EncodeEntity(*e));
                }

                // option to tell it to generate new UUIDS
                m_scene->LoadEntityNodes(nodes, false);
            }

            if (idx >= 0 && ImGui::MenuItem("Remove"))
            {
                const SceneHistoryState beforeRemoveState = CaptureSceneHistoryState();
                const UUID removedUUID = _entity->uuid;
                m_scene->Destroy(idx);
                if (m_index == idx)
                    m_index = -1;
                m_hierarchyRootOrder.erase(
                    std::remove(m_hierarchyRootOrder.begin(), m_hierarchyRootOrder.end(), removedUUID),
                    m_hierarchyRootOrder.end());
                CommitSceneHistoryImmediateChange(beforeRemoveState);
                _refresh = true;
                removeRequested = true;
            }

            for (auto &item : m_app->GetInspectorItemRegistry())
            {
                if (ImGui::MenuItem((item.name + "##").c_str()))
                    item.Func(*m_app, *this, *_entity, m_app->GetScriptRegistry());
            }

            ImGui::EndPopup();
        }

        if (removeRequested)
        {
            if (nodeOpen)
                ImGui::TreePop();
            return;
        }

        // children + single per-gap drop slots
        if (nodeOpen)
        {
            if (children != nullptr)
            {
                for (std::size_t ci = 0; ci < children->size(); ++ci)
                {
                    Canis::Entity *child = (*children)[ci];
                    if (!child)
                        continue;

                    // drop BEFORE this child -> specific index
                    ImGui::PushID((void *)((uintptr_t)child ^ 0xBEEF));
                    {
                        ImVec2 slotSize(std::max(ImGui::GetContentRegionAvail().x, 1.0f), 1.0f);
                        ImGui::InvisibleButton("##drop_before", slotSize);

                        if (ImGui::BeginDragDropTarget())
                        {
                            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ENTITY_DRAG"))
                            {
                                Canis::UUID droppedUUID = *static_cast<const Canis::UUID *>(payload->Data);
                                Canis::Entity *droppedEntity = FindHierarchyEntityByUUID(_entities, droppedUUID);

                                if (droppedEntity && droppedEntity != _entity)
                                {
                                    if (!IsDescendantOf(droppedEntity, _entity) &&
                                        SetHierarchyParentAtIndex(droppedEntity, _entity, ci))
                                    {
                                        _refresh = true;
                                    }
                                }
                            }

                            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
                            {
                                const AssetDragData dropped = *static_cast<const AssetDragData *>(payload->Data);
                                Canis::Entity* spawnedEntity = nullptr;
                                if (InstantiateSceneAssetIntoHierarchy(*m_scene, dropped, _entity, static_cast<int>(ci), nullptr, -1, spawnedEntity))
                                {
                                    FocusEntity(spawnedEntity);
                                    m_selectedAssetPath.clear();
                                    _refresh = true;
                                }
                            }
                            ImGui::EndDragDropTarget();
                        }
                    }
                    ImGui::PopID();

                    // actual child row
                    DrawHierarchyNode(child, _entities, _refresh);
                    if (_refresh)
                        break;
                }
            }

            ImGui::TreePop();
        }
    }

    bool Editor::DrawHierarchyPanel()
    {
        ImGui::Begin("Hierarchy###Hierarchy", &m_showHierarchyPanel);
        bool refresh = false;

        std::vector<Canis::Entity *> &entities = m_scene->GetEntities();

        if (m_scene != nullptr && !m_scene->m_path.empty())
        {
            const std::string sceneName = Canis::GetFileName(m_scene->m_path);
            ImGui::TextDisabled("Scene: %s", sceneName.c_str());
            ImGui::Separator();
        }

        auto findEntityByUUID = [&](Canis::UUID _uuid) -> Canis::Entity*
        {
            for (Canis::Entity* entity : entities)
            {
                if (entity != nullptr && entity->uuid == _uuid)
                    return entity;
            }

            return nullptr;
        };

        // Build root list in stable editor-controlled order.
        std::vector<Canis::Entity*> rootsBySceneOrder = {};
        rootsBySceneOrder.reserve(entities.size());

        for (int i = 0; i < (int)entities.size(); ++i)
        {
            Canis::Entity *entity = entities[i];
            if (!entity)
                continue;

            if (GetHierarchyParent(entity) != nullptr)
                continue;

            rootsBySceneOrder.push_back(entity);
        }

        std::vector<Canis::UUID> rootOrderThisFrame = {};
        rootOrderThisFrame.reserve(rootsBySceneOrder.size());

        // Keep roots from previously saved order if they still exist and are roots.
        for (Canis::UUID orderedUUID : m_hierarchyRootOrder)
        {
            Canis::Entity* entity = findEntityByUUID(orderedUUID);
            if (entity == nullptr || GetHierarchyParent(entity) != nullptr)
                continue;

            if (std::find(rootOrderThisFrame.begin(), rootOrderThisFrame.end(), entity->uuid) == rootOrderThisFrame.end())
                rootOrderThisFrame.push_back(entity->uuid);
        }

        // Append new roots not yet tracked.
        for (Canis::Entity* entity : rootsBySceneOrder)
        {
            if (std::find(rootOrderThisFrame.begin(), rootOrderThisFrame.end(), entity->uuid) == rootOrderThisFrame.end())
                rootOrderThisFrame.push_back(entity->uuid);
        }

        m_hierarchyRootOrder = rootOrderThisFrame;

        std::vector<Canis::Entity *> rootEntities = {};
        rootEntities.reserve(rootOrderThisFrame.size());
        for (Canis::UUID rootUUID : rootOrderThisFrame)
        {
            if (Canis::Entity* entity = findEntityByUUID(rootUUID))
            {
                if (GetHierarchyParent(entity) == nullptr)
                    rootEntities.push_back(entity);
            }
        }

        auto moveRootToPos = [&](Canis::Entity *droppedEntity, int targetRootPos)
        {
            if (!droppedEntity)
                return;

            bool changed = false;

            if (GetHierarchyParent(droppedEntity) != nullptr)
            {
                if (UnparentHierarchyEntity(droppedEntity))
                    changed = true;
            }

            std::vector<Canis::UUID> updatedOrder = rootOrderThisFrame;

            if (auto it = std::find(updatedOrder.begin(), updatedOrder.end(), droppedEntity->uuid); it != updatedOrder.end())
                updatedOrder.erase(it);

            targetRootPos = std::clamp(targetRootPos, 0, static_cast<int>(updatedOrder.size()));
            updatedOrder.insert(updatedOrder.begin() + targetRootPos, droppedEntity->uuid);

            if (updatedOrder != rootOrderThisFrame)
            {
                rootOrderThisFrame = updatedOrder;
                m_hierarchyRootOrder = rootOrderThisFrame;
                refresh = true;
                changed = true;
            }

            if (changed)
                refresh = true;
        };

        // ---------- TOP ROOT DROP SLOT (move to front) ----------
        {
            ImVec2 slotSize(std::max(ImGui::GetContentRegionAvail().x, 1.0f), 1.0f);
            ImGui::InvisibleButton("##root_drop_before_first", slotSize);

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ENTITY_DRAG"))
                {
                    Canis::UUID droppedUUID = *static_cast<const Canis::UUID *>(payload->Data);
                    Canis::Entity *droppedEntity = FindHierarchyEntityByUUID(entities, droppedUUID);
                    moveRootToPos(droppedEntity, 0); // move to first root
                }

                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
                {
                    const AssetDragData dropped = *static_cast<const AssetDragData *>(payload->Data);
                    Canis::Entity* spawnedEntity = nullptr;
                    if (InstantiateSceneAssetIntoHierarchy(*m_scene, dropped, nullptr, -1, &rootOrderThisFrame, 0, spawnedEntity))
                    {
                        m_hierarchyRootOrder = rootOrderThisFrame;
                        FocusEntity(spawnedEntity);
                        m_selectedAssetPath.clear();
                        refresh = true;
                    }
                }
                ImGui::EndDragDropTarget();
            }
        }

        // ---------- ROOT ENTITIES + BETWEEN-SLOTS ----------
        for (int ri = 0; ri < (int)rootEntities.size(); ++ri)
        {
            Canis::Entity *entity = rootEntities[ri];
            if (!entity)
                continue;

            DrawHierarchyNode(entity, entities, refresh);
            if (refresh)
                break;

            // drop slot AFTER this root -> position ri+1
            ImGui::PushID((void *)((uintptr_t)entity ^ 0xABCDEF));
            ImVec2 slotSize(std::max(ImGui::GetContentRegionAvail().x, 1.0f), 1.0f);
            ImGui::InvisibleButton("##root_drop_after", slotSize);

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ENTITY_DRAG"))
                {
                    Canis::UUID droppedUUID = *static_cast<const Canis::UUID *>(payload->Data);
                    Canis::Entity *droppedEntity = FindHierarchyEntityByUUID(entities, droppedUUID);
                    moveRootToPos(droppedEntity, ri + 1);
                }

                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
                {
                    const AssetDragData dropped = *static_cast<const AssetDragData *>(payload->Data);
                    Canis::Entity* spawnedEntity = nullptr;
                    if (InstantiateSceneAssetIntoHierarchy(*m_scene, dropped, nullptr, -1, &rootOrderThisFrame, ri + 1, spawnedEntity))
                    {
                        m_hierarchyRootOrder = rootOrderThisFrame;
                        FocusEntity(spawnedEntity);
                        m_selectedAssetPath.clear();
                        refresh = true;
                    }
                }
                ImGui::EndDragDropTarget();
            }
            ImGui::PopID();
        }

        // --------- ROOT DROP ZONE (unparent children) ----------
        ImVec2 avail = ImGui::GetContentRegionAvail();
        if (avail.y < 24.0f)
            avail.y = 24.0f;

        avail.x = std::max(avail.x, 1.0f);
        avail.y = std::max(avail.y, 1.0f);
        ImGui::InvisibleButton("##hierarchy_root_drop_zone", avail);

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ENTITY_DRAG"))
            {
                Canis::UUID droppedUUID = *static_cast<const Canis::UUID *>(payload->Data);
                Canis::Entity *droppedEntity = FindHierarchyEntityByUUID(entities, droppedUUID);

                if (droppedEntity)
                {
                    moveRootToPos(droppedEntity, static_cast<int>(rootOrderThisFrame.size()));
                }
            }

            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
            {
                const AssetDragData dropped = *static_cast<const AssetDragData *>(payload->Data);
                Canis::Entity* spawnedEntity = nullptr;
                if (InstantiateSceneAssetIntoHierarchy(
                        *m_scene,
                        dropped,
                        nullptr,
                        -1,
                        &rootOrderThisFrame,
                        static_cast<int>(rootOrderThisFrame.size()),
                        spawnedEntity))
                {
                    m_hierarchyRootOrder = rootOrderThisFrame;
                    FocusEntity(spawnedEntity);
                    m_selectedAssetPath.clear();
                    refresh = true;
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopupContextItem("hierarchy_root_context"))
        {
            (void)DrawHierarchyCreateMenu(*this, *m_app, *m_scene, nullptr, refresh);
            ImGui::EndPopup();
        }
        // -----------------------------------------------------

        ImGui::End();
        return refresh;
    }

    void Editor::DrawInspectorPanel(bool _refresh)
    {
        ImGui::Begin("Inspector", &m_showInspectorPanel);

        if (!m_selectedAssetPath.empty())
        {
            if (DrawModelAssetInspector(m_selectedAssetPath))
            {
                ImGui::End();
                return;
            }

            if (DrawMaterialAssetInspector(m_selectedAssetPath))
            {
                ImGui::End();
                return;
            }

            if (DrawSkyboxAssetInspector(m_selectedAssetPath))
            {
                ImGui::End();
                return;
            }

            if (DrawPostProcessAssetInspector(m_selectedAssetPath))
            {
                ImGui::End();
                return;
            }

            if (DrawAnimationClipAssetInspector(m_selectedAssetPath))
            {
                ImGui::End();
                return;
            }

            if (DrawAnimatorControllerAssetInspector(m_selectedAssetPath))
            {
                ImGui::End();
                return;
            }

            if (DrawShaderGraphAssetInspector(m_selectedAssetPath))
            {
                ImGui::End();
                return;
            }
        }

        std::vector<Entity *> &entities = m_scene->GetEntities();

        if (entities.empty())
        {
            m_index = -1;
            ImGui::End();
            return;
        }

        if (m_index < 0 || m_index >= (int)entities.size())
            m_index = 0;

        if (entities[m_index] == nullptr)
        {
            m_index = -1;
            for (int i = 0; i < (int)entities.size(); ++i)
            {
                if (entities[i] != nullptr)
                {
                    m_index = i;
                    break;
                }
            }
        }

        if (m_index >= 0 && entities[m_index] != nullptr)
        {
            Entity &entity = *entities[m_index];

            ImGui::Text("active:");
            ImGui::SameLine();
            ImGui::Checkbox("##entity_active", &entity.active);
            ImGui::Text("name: ");
            ImGui::SameLine();
            ImGui::InputText("##name", &entity.name);
            ImGui::Text("tag:  ");
            ImGui::SameLine();
            ImGui::InputText("##tag", &entity.tag);

            for (ScriptConf &conf : m_app->GetScriptRegistry())
            {
                if (conf.Has(entity))
                {
                    bool open = ImGui::CollapsingHeader(conf.name.c_str());

                    if (ImGui::BeginPopupContextItem(std::string("Menu##" + conf.name).c_str()))
                    {
                        if (conf.kind == RegistryEntryKind::Script)
                        {
                            if (ImGui::MenuItem(std::string("Open##" + conf.name).c_str()))
                            {
                                if (!OpenScriptFromInspector(conf))
                                    Debug::Warning("Failed to locate script source for '%s'.", conf.name.c_str());
                            }
                        }

                        if (ImGui::MenuItem(std::string("Remove##" + conf.name).c_str()))
                        {
                            conf.Remove(entity);
                            open = false;
                        }

                        ImGui::EndPopup();
                    }

                    if (open)
                        conf.DrawInspector(*this, entity, conf);
                }
            }

            DrawAddComponentDropDown(_refresh);
        }

        ImGui::End();
    }

    bool Editor::DrawModelAssetInspector(const std::string &_modelPath)
    {
        MetaFileAsset *meta = AssetManager::GetMetaFile(_modelPath);
        if (meta == nullptr || meta->type != MetaFileAsset::FileType::MODEL)
            return false;

        const std::string modelPath = meta->path.empty() ? _modelPath : meta->path;

        ImGui::Text("Asset: %s", meta->name.c_str());
        ImGui::Text("Path: %s", modelPath.c_str());
        ImGui::Separator();

        const bool canExportMaterials = IsGltfModelAssetPath(modelPath);
        if (!canExportMaterials)
            ImGui::BeginDisabled();

        if (ImGui::Button("Export Materials/Textures...", ImVec2(240.0f, 0.0f)))
            RequestModelMaterialExport(modelPath);

        if (!canExportMaterials)
        {
            ImGui::EndDisabled();
            ImGui::TextDisabled("Material export currently supports .glb and .gltf models.");
        }

        return true;
    }

    bool Editor::DrawMaterialAssetInspector(const std::string &_materialPath)
    {
        MetaFileAsset *meta = AssetManager::GetMetaFile(_materialPath);
        if (meta == nullptr || meta->type != MetaFileAsset::FileType::MATERIAL)
            return false;

        const std::string materialPath = meta->path.empty() ? _materialPath : meta->path;

        ImGui::Text("Asset: %s", meta->name.c_str());
        ImGui::Text("Path: %s", materialPath.c_str());
        ImGui::Separator();

        if (!FileExists(materialPath.c_str()))
            return false;

        YAML::Node root;
        try
        {
            root = YAML::LoadFile(materialPath);
        }
        catch (const YAML::Exception &exception)
        {
            Debug::Warning("Failed to load material '%s': %s", materialPath.c_str(), exception.what());
            return false;
        }

        bool dirty = false;
        constexpr float kMaterialNumberFieldMaxWidth = 240.0f;
        auto setMaterialNumberFieldWidth = []() -> void
        {
            ImGui::SetNextItemWidth(kMaterialNumberFieldMaxWidth);
        };

        auto drawAssetField = [&](const char *_label, const char *_key, bool _shaderField) -> void
        {
            std::string refPath = ResolveAssetRefPath(root[_key]);
            std::string display = "None";
            if (!refPath.empty())
            {
                if (MetaFileAsset *assetMeta = AssetManager::GetMetaFile(refPath))
                    display = assetMeta->name;
                else
                    display = refPath;
            }

            ImGui::PushID(_key);

            ImGui::Text("%s", _label);
            ImGui::SameLine();
            const std::string buttonLabel = display + "##asset_ref";
            ImGui::Button(buttonLabel.c_str(), ImVec2(220, 0));

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
                {
                    const AssetDragData dropped = *static_cast<const AssetDragData *>(payload->Data);
                    std::string path = AssetManager::GetPath(dropped.uuid);

                    bool valid = false;
                    if (_shaderField)
                    {
                        if (MetaFileAsset *droppedMeta = AssetManager::GetMetaFile(path))
                        {
                            valid = droppedMeta->type == MetaFileAsset::FileType::VERTEX ||
                                    droppedMeta->type == MetaFileAsset::FileType::FRAGMENT;
                        }
                    }
                    else
                    {
                        TextureAsset *texture = AssetManager::GetTexture(path);
                        valid = texture != nullptr;
                    }

                    if (valid)
                    {
                        SetAssetRefUUID(root, _key, path);
                        dirty = true;
                    }
                }
                ImGui::EndDragDropTarget();
            }

            // Right-click on field to clear assigned asset reference.
            if (ImGui::BeginPopupContextItem("asset_ref_ctx"))
            {
                if (ImGui::MenuItem("Clear"))
                {
                    root.remove(_key);
                    dirty = true;
                }
                ImGui::EndPopup();
            }

            // Explicit clear button.
            ImGui::SameLine();
            if (ImGui::SmallButton("X##clear_asset_ref"))
            {
                root.remove(_key);
                dirty = true;
            }

            ImGui::PopID();
        };

        drawAssetField("shader", "shader", true);
        drawAssetField("albedo", "albedo", false);
        drawAssetField("specular", "specular", false);
        drawAssetField("roughness", "roughness", false);
        drawAssetField("metallic", "metallic", false);
        drawAssetField("emission", "emission", false);

        Color color = root["color"].as<Color>(Color(1.0f));
        if (ImGui::ColorEdit4("color", &color.r))
        {
            root["color"] = color;
            dirty = true;
        }

        float specularValue = root["specularValue"].as<float>(0.5f);
        setMaterialNumberFieldWidth();
        if (ImGui::DragFloat("specularValue", &specularValue, 0.01f, 0.0f, 1.0f))
        {
            root["specularValue"] = specularValue;
            dirty = true;
        }

        float roughnessValue = root["roughnessValue"].as<float>(0.5f);
        setMaterialNumberFieldWidth();
        if (ImGui::DragFloat("roughnessValue", &roughnessValue, 0.01f, 0.0f, 1.0f))
        {
            root["roughnessValue"] = roughnessValue;
            dirty = true;
        }

        float metallicValue = root["metallicValue"].as<float>(0.0f);
        setMaterialNumberFieldWidth();
        if (ImGui::DragFloat("metallicValue", &metallicValue, 0.01f, 0.0f, 1.0f))
        {
            root["metallicValue"] = metallicValue;
            dirty = true;
        }

        bool backFaceCulling = root["backFaceCulling"].as<bool>(false);
        if (ImGui::Checkbox("backFaceCulling", &backFaceCulling))
        {
            root["backFaceCulling"] = backFaceCulling;
            dirty = true;
        }

        bool frontFaceCulling = root["frontFaceCulling"].as<bool>(false);
        if (ImGui::Checkbox("frontFaceCulling", &frontFaceCulling))
        {
            root["frontFaceCulling"] = frontFaceCulling;
            dirty = true;
        }

        MaterialFields customUniforms = {};
        ReadMaterialUniformsFromNode(root, customUniforms);
        bool uniformsDirty = false;

        ImGui::Separator();
        ImGui::Text("custom uniforms");

        static std::string newUniformName = "";
        static int newUniformTypeIndex = 1;
        const char *uniformTypeOptions[] = {"int", "float", "Vector2", "Vector3", "Vector4", "Color", "Texture"};

        auto trimUniformName = [](const std::string &_value) -> std::string
        {
            const size_t first = _value.find_first_not_of(" \t\n\r");
            if (first == std::string::npos)
                return "";
            const size_t last = _value.find_last_not_of(" \t\n\r");
            return _value.substr(first, last - first + 1);
        };

        auto getTextureDisplayName = [](i32 _textureId) -> std::string
        {
            if (_textureId < 0)
                return "None";

            const std::string texturePath = AssetManager::GetPath(_textureId);
            if (texturePath.rfind("Path was not found", 0) == 0)
                return "[ missing ]";

            if (MetaFileAsset *textureMeta = AssetManager::GetMetaFile(texturePath))
                return textureMeta->name;

            return texturePath;
        };

        ImGui::SetNextItemWidth(170.0f);
        ImGui::InputTextWithHint("##new_uniform_name", "uniform name", &newUniformName);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        ImGui::Combo("##new_uniform_type", &newUniformTypeIndex, uniformTypeOptions, IM_ARRAYSIZE(uniformTypeOptions));
        ImGui::SameLine();
        if (ImGui::Button("Add Uniform"))
        {
            const std::string uniformName = trimUniformName(newUniformName);
            if (!uniformName.empty())
            {
                switch (newUniformTypeIndex)
                {
                    case 0: customUniforms.SetInt(uniformName, 0); break;
                    case 1: customUniforms.SetFloat(uniformName, 0.0f); break;
                    case 2: customUniforms.SetVec2(uniformName, Vector2(0.0f)); break;
                    case 3: customUniforms.SetVec3(uniformName, Vector3(0.0f)); break;
                    case 4: customUniforms.SetVec4(uniformName, Vector4(0.0f)); break;
                    case 5: customUniforms.SetColor(uniformName, Color(1.0f)); break;
                    case 6: customUniforms.SetTexture(uniformName, -1); break;
                    default: break;
                }

                uniformsDirty = true;
                newUniformName.clear();
            }
        }

        const auto intUniforms = customUniforms.GetIntUniforms();
        for (const MaterialFields::IntUniformData &uniform : intUniforms)
        {
            int value = uniform.value;
            bool removeUniform = false;

            ImGui::PushID(("int_" + uniform.name).c_str());
            setMaterialNumberFieldWidth();
            if (ImGui::DragInt("##value", &value, 1.0f))
            {
                customUniforms.SetInt(uniform.name, value);
                uniformsDirty = true;
            }
            ImGui::SameLine();
            ImGui::Text("%s (int)", uniform.name.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("X"))
                removeUniform = true;
            ImGui::PopID();

            if (removeUniform)
            {
                customUniforms.RemoveInt(uniform.name);
                uniformsDirty = true;
            }
        }

        const auto floatUniforms = customUniforms.GetFloatUniforms();
        for (const MaterialFields::FloatUniformData &uniform : floatUniforms)
        {
            float value = uniform.value;
            bool removeUniform = false;

            ImGui::PushID(("float_" + uniform.name).c_str());
            setMaterialNumberFieldWidth();
            if (ImGui::DragFloat("##value", &value, 0.01f))
            {
                customUniforms.SetFloat(uniform.name, value);
                uniformsDirty = true;
            }
            ImGui::SameLine();
            ImGui::Text("%s (float)", uniform.name.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("X"))
                removeUniform = true;
            ImGui::PopID();

            if (removeUniform)
            {
                customUniforms.RemoveFloat(uniform.name);
                uniformsDirty = true;
            }
        }

        const auto vec2Uniforms = customUniforms.GetVec2Uniforms();
        for (const MaterialFields::Vec2UniformData &uniform : vec2Uniforms)
        {
            Vector2 value = uniform.value;
            bool removeUniform = false;

            ImGui::PushID(("vec2_" + uniform.name).c_str());
            setMaterialNumberFieldWidth();
            if (ImGui::DragFloat2("##value", &value.x, 0.01f))
            {
                customUniforms.SetVec2(uniform.name, value);
                uniformsDirty = true;
            }
            ImGui::SameLine();
            ImGui::Text("%s (Vector2)", uniform.name.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("X"))
                removeUniform = true;
            ImGui::PopID();

            if (removeUniform)
            {
                customUniforms.RemoveVec2(uniform.name);
                uniformsDirty = true;
            }
        }

        const auto vec3Uniforms = customUniforms.GetVec3Uniforms();
        for (const MaterialFields::Vec3UniformData &uniform : vec3Uniforms)
        {
            Vector3 value = uniform.value;
            bool removeUniform = false;

            ImGui::PushID(("vec3_" + uniform.name).c_str());
            setMaterialNumberFieldWidth();
            if (ImGui::DragFloat3("##value", &value.x, 0.01f))
            {
                customUniforms.SetVec3(uniform.name, value);
                uniformsDirty = true;
            }
            ImGui::SameLine();
            ImGui::Text("%s (Vector3)", uniform.name.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("X"))
                removeUniform = true;
            ImGui::PopID();

            if (removeUniform)
            {
                customUniforms.RemoveVec3(uniform.name);
                uniformsDirty = true;
            }
        }

        const auto vec4Uniforms = customUniforms.GetVec4Uniforms();
        for (const MaterialFields::Vec4UniformData &uniform : vec4Uniforms)
        {
            Vector4 value = uniform.value;
            bool removeUniform = false;

            ImGui::PushID(("vec4_" + uniform.name).c_str());
            setMaterialNumberFieldWidth();
            if (ImGui::DragFloat4("##value", &value.x, 0.01f))
            {
                customUniforms.SetVec4(uniform.name, value);
                uniformsDirty = true;
            }
            ImGui::SameLine();
            ImGui::Text("%s (Vector4)", uniform.name.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("X"))
                removeUniform = true;
            ImGui::PopID();

            if (removeUniform)
            {
                customUniforms.RemoveVec4(uniform.name);
                uniformsDirty = true;
            }
        }

        const auto colorUniforms = customUniforms.GetColorUniforms();
        for (const MaterialFields::ColorUniformData &uniform : colorUniforms)
        {
            Color value = uniform.value;
            bool removeUniform = false;

            ImGui::PushID(("color_" + uniform.name).c_str());
            if (ImGui::ColorEdit4("##value", &value.r))
            {
                customUniforms.SetColor(uniform.name, value);
                uniformsDirty = true;
            }
            ImGui::SameLine();
            ImGui::Text("%s (Color)", uniform.name.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("X"))
                removeUniform = true;
            ImGui::PopID();

            if (removeUniform)
            {
                customUniforms.RemoveColor(uniform.name);
                uniformsDirty = true;
            }
        }

        const auto textureUniforms = customUniforms.GetTextureUniforms();
        for (const MaterialFields::TextureUniformData &uniform : textureUniforms)
        {
            i32 textureId = uniform.textureId;
            bool removeUniform = false;

            ImGui::PushID(("texture_" + uniform.name).c_str());

            const std::string buttonLabel = getTextureDisplayName(textureId) + "##texture_value";
            ImGui::Button(buttonLabel.c_str(), ImVec2(220, 0));

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
                {
                    const AssetDragData dropped = *static_cast<const AssetDragData *>(payload->Data);
                    std::string droppedPath = AssetManager::GetPath(dropped.uuid);
                    if (droppedPath.rfind("Path was not found", 0) == 0)
                        droppedPath = std::string(dropped.path);

                    if (MetaFileAsset *droppedMeta = AssetManager::GetMetaFile(droppedPath))
                    {
                        if (droppedMeta->type == MetaFileAsset::FileType::TEXTURE)
                            textureId = AssetManager::LoadTexture(droppedPath);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if (ImGui::BeginPopupContextItem("texture_uniform_ctx"))
            {
                if (ImGui::MenuItem("Clear"))
                    textureId = -1;
                ImGui::EndPopup();
            }

            ImGui::SameLine();
            ImGui::Text("%s (Texture)", uniform.name.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("X"))
                removeUniform = true;
            ImGui::PopID();

            if (textureId != uniform.textureId)
            {
                customUniforms.SetTexture(uniform.name, textureId);
                uniformsDirty = true;
            }

            if (removeUniform)
            {
                customUniforms.RemoveTexture(uniform.name);
                uniformsDirty = true;
            }
        }

        if (uniformsDirty)
        {
            WriteMaterialUniformsToNode(root, customUniforms);
            dirty = true;
        }

        if (dirty)
        {
            std::ofstream fout(materialPath);
            fout << root;
            fout.close();

            int id = AssetManager::GetID(materialPath);
            if (id >= 0)
                ApplyMaterialNodeToAsset(root, AssetManager::GetMaterial(id));
        }

        return true;
    }

    bool Editor::DrawSkyboxAssetInspector(const std::string &_skyboxPath)
    {
        MetaFileAsset *meta = AssetManager::GetMetaFile(_skyboxPath);
        if (meta == nullptr || meta->type != MetaFileAsset::FileType::SKYBOX)
            return false;

        ImGui::Text("Asset: %s", meta->name.c_str());

        YAML::Node root;
        if (FileExists(_skyboxPath.c_str()))
            root = YAML::LoadFile(_skyboxPath);
        if (!root || !root.IsMap())
            root = YAML::Node(YAML::NodeType::Map);

        bool changed = false;

        auto drawFaceAssetField = [&](const char *_label, const char *_key) -> void
        {
            std::string refPath = ResolveAssetRefPath(root[_key]);
            std::string buttonLabel = "[ none ]";
            if (!refPath.empty())
            {
                if (MetaFileAsset *assetMeta = AssetManager::GetMetaFile(refPath))
                    buttonLabel = assetMeta->name;
                else
                    buttonLabel = refPath;
            }

            ImGui::Text("%s", _label);
            ImGui::SameLine();

            ImGui::PushID(_key);
            ImGui::Button(buttonLabel.c_str(), ImVec2(170, 0));

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
                {
                    const AssetDragData dropped = *static_cast<const AssetDragData *>(payload->Data);
                    std::string path = AssetManager::GetPath(dropped.uuid);
                    if (MetaFileAsset *droppedMeta = AssetManager::GetMetaFile(path))
                    {
                        if (droppedMeta->type == MetaFileAsset::FileType::TEXTURE)
                        {
                            SetAssetRefUUID(root, _key, path);
                            changed = true;
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if (ImGui::BeginPopupContextItem("skybox_asset_ref_ctx"))
            {
                if (ImGui::MenuItem("Clear"))
                {
                    root.remove(_key);
                    changed = true;
                }
                ImGui::EndPopup();
            }
            ImGui::PopID();
        };

        drawFaceAssetField("right (+X)", "right");
        drawFaceAssetField("left (-X)", "left");
        drawFaceAssetField("top (+Y)", "top");
        drawFaceAssetField("bottom (-Y)", "bottom");
        drawFaceAssetField("front (+Z)", "front");
        drawFaceAssetField("back (-Z)", "back");

        if (changed)
        {
            std::ofstream fout(_skyboxPath);
            fout << root;
            fout.close();

            AssetManager::Free<SkyboxAsset>(_skyboxPath);
            AssetManager::LoadSkybox(_skyboxPath);
        }

        return true;
    }

    bool Editor::DrawPostProcessAssetInspector(const std::string &_postProcessPath)
    {
        MetaFileAsset *meta = AssetManager::GetMetaFile(_postProcessPath);
        if (meta == nullptr || meta->type != MetaFileAsset::FileType::POSTPROCESS)
            return false;

        ImGui::Text("Asset: %s", meta->name.c_str());
        ImGui::Text("Path: %s", meta->path.c_str());
        ImGui::Separator();

        YAML::Node root;
        if (FileExists(_postProcessPath.c_str()))
            root = YAML::LoadFile(_postProcessPath);
        if (!root || !root.IsMap())
            root = YAML::Node(YAML::NodeType::Map);

        YAML::Node passes = root["passes"];
        if (!passes || !passes.IsSequence())
            passes = YAML::Node(YAML::NodeType::Sequence);

        bool dirty = false;

        auto getShaderLabel = [&](const YAML::Node &_shaderNode) -> std::string
        {
            std::string shaderPath = ResolveAssetRefPath(_shaderNode);
            if (shaderPath.empty())
                return "[ none ]";

            if (MetaFileAsset *shaderMeta = AssetManager::GetMetaFile(shaderPath))
                return shaderMeta->name;

            return shaderPath;
        };

        auto assignShaderDrop = [&](YAML::Node &_passNode) -> void
        {
            if (!ImGui::BeginDragDropTarget())
                return;

            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
            {
                const AssetDragData dropped = *static_cast<const AssetDragData *>(payload->Data);
                std::string path = std::string(dropped.path);
                if (path.empty() || !FileExists(path.c_str()))
                    path = AssetManager::GetPath(dropped.uuid);

                bool valid = false;
                if (MetaFileAsset *droppedMeta = AssetManager::GetMetaFile(path))
                {
                    valid = droppedMeta->type == MetaFileAsset::FileType::VERTEX ||
                            droppedMeta->type == MetaFileAsset::FileType::FRAGMENT;
                }

                if (valid)
                {
                    SetAssetRefUUID(_passNode, "shader", path);
                    dirty = true;
                }
            }

            ImGui::EndDragDropTarget();
        };

        for (int i = 0; i < static_cast<int>(passes.size()); ++i)
        {
            YAML::Node passNode = passes[i];
            if (!passNode || !passNode.IsMap())
            {
                YAML::Node newPass(YAML::NodeType::Map);
                passes[i].reset(newPass);
                passNode = passes[i];
            }

            if (!passNode["name"])
                passNode["name"] = "Pass " + std::to_string(i + 1);
            if (!passNode["enabled"])
                passNode["enabled"] = true;

            ImGui::PushID(i);
            ImGui::Separator();

            std::string passName = passNode["name"].as<std::string>("Pass " + std::to_string(i + 1));
            if (ImGui::InputText("name", &passName))
            {
                passNode["name"] = passName;
                dirty = true;
            }

            bool enabled = passNode["enabled"].as<bool>(true);
            if (ImGui::Checkbox("enabled", &enabled))
            {
                passNode["enabled"] = enabled;
                dirty = true;
            }

            std::string shaderLabel = getShaderLabel(passNode["shader"]);
            ImGui::Text("shader");
            ImGui::SameLine();
            const std::string shaderPopupName = "postprocess_shader_picker##" + std::to_string(i);
            if (ImGui::Button(shaderLabel.c_str(), ImVec2(220, 0)))
            {
                ImGui::OpenPopup(shaderPopupName.c_str());
            }

            if (ImGui::BeginPopup(shaderPopupName.c_str()))
            {
                ImGui::TextUnformatted("Assign shader");
                ImGui::Separator();

                std::vector<std::string> shaderPaths = FindFilesInFolder("assets", "");
                bool hasShaderAssets = false;
                for (const std::string &path : shaderPaths)
                {
                    MetaFileAsset *shaderMeta = AssetManager::GetMetaFile(path);
                    if (shaderMeta == nullptr)
                        continue;

                    if (shaderMeta->type != MetaFileAsset::FileType::VERTEX &&
                        shaderMeta->type != MetaFileAsset::FileType::FRAGMENT)
                    {
                        continue;
                    }

                    hasShaderAssets = true;
                    const std::string label = shaderMeta->name + "##" + path;
                    if (ImGui::Selectable(label.c_str()))
                    {
                        SetAssetRefUUID(passNode, "shader", path);
                        dirty = true;
                        ImGui::CloseCurrentPopup();
                    }
                }

                if (!hasShaderAssets)
                    ImGui::TextUnformatted("No shader assets found.");

                if (ImGui::MenuItem("Clear Shader"))
                {
                    passNode.remove("shader");
                    dirty = true;
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
            assignShaderDrop(passNode);

            const std::string shaderPath = ToLowerCopy(NormalizeShaderAssetPath(ResolveAssetRefPath(passNode["shader"])));
            const bool showBloomControls = shaderPath.find("bloom") != std::string::npos;
            const bool showColorControls = shaderPath.find("color_correction") != std::string::npos;
            const bool showSsaoControls = shaderPath.find("ssao") != std::string::npos;

            auto drawPassFloat = [&](const char *_label, const char *_key, float _defaultValue, float _speed, float _min, float _max, const char *_format) -> void
            {
                YAML::Node settingsNode = passNode["settings"];
                const float currentValue = (settingsNode && settingsNode.IsMap())
                    ? settingsNode[_key].as<float>(_defaultValue)
                    : _defaultValue;

                float value = currentValue;
                if (ImGui::DragFloat(_label, &value, _speed, _min, _max, _format))
                {
                    if (!passNode["settings"] || !passNode["settings"].IsMap())
                        passNode["settings"] = YAML::Node(YAML::NodeType::Map);

                    passNode["settings"][_key] = value;
                    dirty = true;
                }
            };

            if (showBloomControls)
            {
                drawPassFloat("threshold", "bloomThreshold", 0.5f, 0.01f, 0.0f, 8.0f, "%.2f");
                drawPassFloat("intensity", "bloomIntensity", 0.85f, 0.01f, 0.0f, 8.0f, "%.2f");
            }

            if (showColorControls)
            {
                drawPassFloat("exposure", "exposure", 1.0f, 0.01f, 0.0f, 8.0f, "%.2f");
                drawPassFloat("contrast", "contrast", 1.05f, 0.01f, 0.0f, 4.0f, "%.2f");
                drawPassFloat("saturation", "saturation", 1.0f, 0.01f, 0.0f, 4.0f, "%.2f");
            }

            if (showSsaoControls)
            {
                drawPassFloat("radius", "ssaoRadius", 0.85f, 0.01f, 0.0f, 8.0f, "%.2f");
                drawPassFloat("bias", "ssaoBias", 0.025f, 0.001f, 0.0f, 1.0f, "%.4f");
                drawPassFloat("strength", "ssaoStrength", 1.25f, 0.01f, 0.0f, 8.0f, "%.2f");
            }

            if (ImGui::BeginPopupContextItem("postprocess_pass_ctx"))
            {
                if (ImGui::MenuItem("Clear Shader"))
                {
                    passNode.remove("shader");
                    dirty = true;
                }
                if (ImGui::MenuItem("Remove Pass"))
                {
                    passes.remove(i);
                    dirty = true;
                    ImGui::EndPopup();
                    ImGui::PopID();
                    break;
                }
                ImGui::EndPopup();
            }

            ImGui::PopID();
        }

        ImGui::Separator();
        if (ImGui::Button("Add Pass", ImVec2(-1.0f, 0.0f)))
        {
            YAML::Node pass(YAML::NodeType::Map);
            pass["name"] = "New Pass";
            pass["enabled"] = true;
            passes.push_back(pass);
            dirty = true;
        }

        if (dirty)
        {
            root["passes"] = passes;

            std::ofstream fout(_postProcessPath);
            fout << root;
            fout.close();

            AssetManager::Free<PostProcessAsset>(_postProcessPath);
            AssetManager::LoadPostProcess(_postProcessPath);
        }

        return true;
    }

    bool Editor::DrawAnimationClipAssetInspector(const std::string &_animationClipPath)
    {
        MetaFileAsset *meta = AssetManager::GetMetaFile(_animationClipPath);
        if (meta == nullptr || meta->type != MetaFileAsset::FileType::ANIMATIONCLIP)
            return false;

        const std::string clipPath = meta->path.empty() ? _animationClipPath : meta->path;
        AnimationClipAsset *clip = AssetManager::GetAnimationClip(clipPath);
        if (clip == nullptr)
            return false;

        m_animationClipStatePath = clipPath;
        RememberLastAnimationClipAssetPath(clipPath);

        ImGui::Text("Asset: %s", meta->name.c_str());
        ImGui::Text("Path: %s", clipPath.c_str());
        ImGui::Separator();
        ImGui::Text("Length: %.3fs", clip->length);
        ImGui::Text("Tracks: %d", static_cast<int>(clip->tracks.size()));
        ImGui::Text("Events: %d", static_cast<int>(clip->events.size()));

        if (ImGui::Button("Open In Animation Window"))
        {
            m_animationClipStatePath = clipPath;
            RememberLastAnimationClipAssetPath(clipPath);
        }

        ImGui::SameLine();
        if (ImGui::Button("Create Animator Controller"))
        {
            namespace fs = std::filesystem;

            const fs::path clipFsPath = fs::path(clipPath);
            const fs::path controllerPath = clipFsPath.parent_path() / (clipFsPath.stem().string() + ".animator");

            AnimatorControllerAsset controller = {};
            controller.entryState = "Default";
            AnimatorState state = {};
            state.name = "Default";
            state.clip = MakeAnimationClipAssetHandleFromPath(clipPath);
            controller.states.push_back(state);
            if (controller.Save(controllerPath.generic_string()))
            {
                (void)AssetManager::GetMetaFile(controllerPath.generic_string());
                m_selectedAssetPath = controllerPath.generic_string();
            }
        }

        return true;
    }

    bool Editor::DrawAnimatorControllerAssetInspector(const std::string &_animatorControllerPath)
    {
        MetaFileAsset *meta = AssetManager::GetMetaFile(_animatorControllerPath);
        if (meta == nullptr || meta->type != MetaFileAsset::FileType::ANIMATORCONTROLLER)
            return false;

        const std::string controllerPath = meta->path.empty() ? _animatorControllerPath : meta->path;
        AnimatorControllerAsset *controller = AssetManager::GetAnimatorController(controllerPath);
        if (controller == nullptr)
            return false;

        ImGui::Text("Asset: %s", meta->name.c_str());
        ImGui::Text("Path: %s", controllerPath.c_str());
        ImGui::Separator();

        if (ImGui::Button("Open In Animator Window"))
            m_animatorStatePath = controllerPath;

        bool dirty = false;
        dirty = EnsureAnimatorStateLayout(*controller) || dirty;

        char entryStateBuffer[128] = {};
        std::snprintf(entryStateBuffer, sizeof(entryStateBuffer), "%s", controller->entryState.c_str());
        if (ImGui::InputText("entryState", entryStateBuffer, sizeof(entryStateBuffer)))
        {
            controller->entryState = entryStateBuffer;
            dirty = true;
        }

        if (ImGui::CollapsingHeader("Parameters", ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (std::size_t parameterIndex = 0; parameterIndex < controller->parameters.size(); ++parameterIndex)
            {
                AnimatorParameterDefinition &parameter = controller->parameters[parameterIndex];
                ImGui::PushID(static_cast<int>(parameterIndex));
                if (ImGui::TreeNode(parameter.name.empty() ? "Parameter" : parameter.name.c_str()))
                {
                    char nameBuffer[128] = {};
                    std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", parameter.name.c_str());
                    if (ImGui::InputText("name", nameBuffer, sizeof(nameBuffer)))
                    {
                        const std::string previousName = parameter.name;
                        parameter.name = nameBuffer;
                        RenameAnimatorParameterReferences(*controller, previousName, parameter.name);
                        dirty = true;
                    }

                    int typeIndex = static_cast<int>(parameter.type);
                    const char *typeLabels[] = {"Float", "Int", "Bool", "Trigger"};
                    if (ImGui::Combo("type", &typeIndex, typeLabels, IM_ARRAYSIZE(typeLabels)))
                    {
                        parameter.type = static_cast<AnimatorParameterType>(std::clamp(typeIndex, 0, 3));
                        parameter.defaultValue = MakeDefaultAnimatorParameterValue(parameter.type);
                        ResetAnimatorParameterReferenceValues(*controller, parameter.name, parameter.type);
                        dirty = true;
                    }

                    if (parameter.type != AnimatorParameterType::TRIGGER)
                        dirty = DrawAnimationValueEditor("defaultValue", parameter.defaultValue) || dirty;

                    if (ImGui::SmallButton("Delete Parameter"))
                    {
                        RemoveAnimatorParameterReferences(*controller, parameter.name);
                        controller->parameters.erase(controller->parameters.begin() + static_cast<long>(parameterIndex));
                        dirty = true;
                        ImGui::TreePop();
                        ImGui::PopID();
                        break;
                    }

                    ImGui::TreePop();
                }
                ImGui::PopID();
            }

            if (ImGui::Button("Add Parameter"))
            {
                AnimatorParameterDefinition parameter = {};
                parameter.name = "Parameter";
                parameter.type = AnimatorParameterType::FLOAT;
                parameter.defaultValue = AnimationValue::Float(0.0f);
                controller->parameters.push_back(parameter);
                dirty = true;
            }
        }

        if (ImGui::CollapsingHeader("States", ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (std::size_t stateIndex = 0; stateIndex < controller->states.size(); ++stateIndex)
            {
                AnimatorState &state = controller->states[stateIndex];
                ImGui::PushID(static_cast<int>(stateIndex));
                if (ImGui::TreeNode(state.name.empty() ? "State" : state.name.c_str()))
                {
                    char nameBuffer[128] = {};
                    std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", state.name.c_str());
                    if (ImGui::InputText("name", nameBuffer, sizeof(nameBuffer)))
                    {
                        std::string renamedState = nameBuffer;
                        if (renamedState.empty())
                            renamedState = "State";
                        if (renamedState != state.name)
                        {
                            const std::string previousName = state.name;
                            renamedState = MakeUniqueAnimatorStateName(*controller, renamedState);
                            state.name = renamedState;
                            RenameAnimatorStateReferences(*controller, previousName, state.name);
                            dirty = true;
                        }
                    }

                    AnimationClipAssetHandle previousClip = state.clip;
                    InputAnimationClipAsset("clip", "animator_state_clip", state.clip);
                    if (AnimationClipAssetHandleChanged(previousClip, state.clip))
                        dirty = true;
                    if (ImGui::Checkbox("loop", &state.loop))
                        dirty = true;
                    if (ImGui::InputFloat("speed", &state.speed, 0.0f, 0.0f, "%.3f"))
                        dirty = true;

                    if (ImGui::CollapsingHeader("Transitions", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        for (std::size_t transitionIndex = 0; transitionIndex < state.transitions.size(); ++transitionIndex)
                        {
                            AnimatorTransition &transition = state.transitions[transitionIndex];
                            ImGui::PushID(static_cast<int>(transitionIndex));
                            if (ImGui::TreeNode(("To " + transition.toState).c_str()))
                            {
                                char toStateBuffer[128] = {};
                                std::snprintf(toStateBuffer, sizeof(toStateBuffer), "%s", transition.toState.c_str());
                                if (ImGui::InputText("toState", toStateBuffer, sizeof(toStateBuffer)))
                                {
                                    transition.toState = toStateBuffer;
                                    dirty = true;
                                }

                                if (ImGui::Checkbox("hasExitTime", &transition.hasExitTime))
                                    dirty = true;
                                if (transition.hasExitTime && ImGui::InputFloat("exitTimeNormalized", &transition.exitTimeNormalized, 0.0f, 0.0f, "%.3f"))
                                {
                                    transition.exitTimeNormalized = std::clamp(transition.exitTimeNormalized, 0.0f, 1.0f);
                                    dirty = true;
                                }

                                for (std::size_t conditionIndex = 0; conditionIndex < transition.conditions.size(); ++conditionIndex)
                                {
                                    AnimatorTransitionCondition &condition = transition.conditions[conditionIndex];
                                    ImGui::PushID(static_cast<int>(conditionIndex));
                                    if (ImGui::TreeNode(("Condition " + std::to_string(conditionIndex)).c_str()))
                                    {
                                        if (ImGui::BeginCombo("parameter", condition.parameter.empty() ? "[ none ]" : condition.parameter.c_str()))
                                        {
                                            for (const AnimatorParameterDefinition &parameter : controller->parameters)
                                            {
                                                const bool selected = parameter.name == condition.parameter;
                                                if (ImGui::Selectable(parameter.name.c_str(), selected))
                                                {
                                                    condition.parameter = parameter.name;
                                                    condition.value = parameter.defaultValue.type == AnimationValueType::NONE
                                                        ? MakeDefaultAnimatorParameterValue(parameter.type)
                                                        : parameter.defaultValue;
                                                    dirty = true;
                                                }
                                            }
                                            ImGui::EndCombo();
                                        }

                                        int modeIndex = static_cast<int>(condition.mode);
                                        const char *modeLabels[] = {"Greater", "Less", "Equal", "Not Equal", "If True", "If False", "Triggered"};
                                        if (ImGui::Combo("mode", &modeIndex, modeLabels, IM_ARRAYSIZE(modeLabels)))
                                        {
                                            condition.mode = static_cast<AnimatorConditionMode>(std::clamp(modeIndex, 0, 6));
                                            dirty = true;
                                        }

                                        bool allowValue = true;
                                        AnimatorParameterType parameterType = AnimatorParameterType::FLOAT;
                                        for (const AnimatorParameterDefinition &parameter : controller->parameters)
                                        {
                                            if (parameter.name == condition.parameter)
                                            {
                                                parameterType = parameter.type;
                                                break;
                                            }
                                        }

                                        if (condition.mode == AnimatorConditionMode::IF_TRUE ||
                                            condition.mode == AnimatorConditionMode::IF_FALSE ||
                                            condition.mode == AnimatorConditionMode::TRIGGERED)
                                        {
                                            allowValue = false;
                                        }

                                        if (allowValue)
                                        {
                                            if (condition.value.type == AnimationValueType::NONE)
                                                condition.value = MakeDefaultAnimatorParameterValue(parameterType);
                                            dirty = DrawAnimationValueEditor("value", condition.value) || dirty;
                                        }

                                        if (ImGui::SmallButton("Delete Condition"))
                                        {
                                            transition.conditions.erase(transition.conditions.begin() + static_cast<long>(conditionIndex));
                                            dirty = true;
                                            ImGui::TreePop();
                                            ImGui::PopID();
                                            break;
                                        }

                                        ImGui::TreePop();
                                    }
                                    ImGui::PopID();
                                }

                                if (ImGui::Button("Add Condition"))
                                {
                                    AnimatorTransitionCondition condition = {};
                                    if (!controller->parameters.empty())
                                    {
                                        condition.parameter = controller->parameters.front().name;
                                        condition.value = controller->parameters.front().defaultValue.type == AnimationValueType::NONE
                                            ? MakeDefaultAnimatorParameterValue(controller->parameters.front().type)
                                            : controller->parameters.front().defaultValue;
                                    }
                                    transition.conditions.push_back(condition);
                                    dirty = true;
                                }

                                if (ImGui::SmallButton("Delete Transition"))
                                {
                                    state.transitions.erase(state.transitions.begin() + static_cast<long>(transitionIndex));
                                    dirty = true;
                                    ImGui::TreePop();
                                    ImGui::PopID();
                                    break;
                                }

                                ImGui::TreePop();
                            }
                            ImGui::PopID();
                        }

                        if (ImGui::Button("Add Transition"))
                        {
                            AnimatorTransition transition = {};
                            if (!controller->states.empty())
                                transition.toState = controller->states.front().name;
                            state.transitions.push_back(transition);
                            dirty = true;
                        }
                    }

                    if (ImGui::SmallButton("Delete State"))
                    {
                        const std::string removedStateName = state.name;
                        RemoveAnimatorStateReferences(*controller, removedStateName);
                        controller->states.erase(controller->states.begin() + static_cast<long>(stateIndex));
                        if (controller->entryState.empty() && !controller->states.empty())
                            controller->entryState = controller->states.front().name;
                        dirty = true;
                        ImGui::TreePop();
                        ImGui::PopID();
                        break;
                    }

                    ImGui::TreePop();
                }
                ImGui::PopID();
            }

            if (ImGui::Button("Add State"))
            {
                AnimatorState state = {};
                state.name = MakeUniqueAnimatorStateName(*controller, "State");
                state.editorPosition = GetNextAnimatorStatePosition(*controller);
                controller->states.push_back(state);
                if (controller->entryState.empty())
                    controller->entryState = state.name;
                dirty = true;
            }
        }

        if (dirty)
            (void)controller->Save();

        return true;
    }

    void Editor::DrawAnimatorWindow()
    {
        namespace fs = std::filesystem;

        Entity *selectedEntity = nullptr;
        if (m_scene != nullptr && m_index >= 0 && m_index < static_cast<int>(m_scene->GetEntities().size()))
            selectedEntity = m_scene->GetEntities()[m_index];

        const std::string previousAnimatorPath = m_animatorStatePath;

        MetaFileAsset *selectedAssetMeta = nullptr;
        if (IsAnimatorControllerAssetPath(m_selectedAssetPath, &selectedAssetMeta))
        {
            m_animatorStatePath = selectedAssetMeta->path.empty() ? m_selectedAssetPath : selectedAssetMeta->path;
        }
        else if (selectedEntity != nullptr && selectedEntity->HasComponent<Animator>())
        {
            const std::string selectedAnimatorPath = AssetManager::ResolvePath(selectedEntity->GetComponent<Animator>().controller);
            if (!selectedAnimatorPath.empty())
                m_animatorStatePath = selectedAnimatorPath;
        }

        if (previousAnimatorPath != m_animatorStatePath)
        {
            m_animatorSelectedState = -1;
            m_animatorSelectedTransition = -1;
        }

        ImGui::Begin("Animator", &m_showAnimatorPanel);

        MetaFileAsset *meta = nullptr;
        if (!IsAnimatorControllerAssetPath(m_animatorStatePath, &meta))
        {
            ImGui::TextUnformatted("Select a .animator asset in Assets or select an entity with Canis::Animator.");
            ImGui::TextDisabled("The graph here is for state machines and transitions, while clips stay in the Animation window.");
            ImGui::End();
            return;
        }

        const std::string controllerPath = meta->path.empty() ? m_animatorStatePath : meta->path;
        if (m_animatorStatePath != controllerPath)
            m_animatorStatePath = controllerPath;

        AnimatorControllerAsset *controller = AssetManager::GetAnimatorController(controllerPath);
        if (controller == nullptr)
        {
            ImGui::TextDisabled("Unable to load %s", controllerPath.c_str());
            ImGui::End();
            return;
        }

        bool dirty = EnsureAnimatorStateLayout(*controller);

        Animator *boundAnimator = nullptr;
        if (selectedEntity != nullptr && selectedEntity->HasComponent<Animator>())
        {
            Animator &candidateAnimator = selectedEntity->GetComponent<Animator>();
            const std::string selectedAnimatorPath = AssetManager::ResolvePath(candidateAnimator.controller);
            if (selectedAnimatorPath == controllerPath)
                boundAnimator = &candidateAnimator;
        }

        if (m_animatorSelectedState < 0 || m_animatorSelectedState >= static_cast<int>(controller->states.size()))
        {
            if (boundAnimator != nullptr && !boundAnimator->currentState.empty())
                m_animatorSelectedState = FindAnimatorStateIndex(*controller, boundAnimator->currentState);

            if ((m_animatorSelectedState < 0 || m_animatorSelectedState >= static_cast<int>(controller->states.size())) &&
                !controller->entryState.empty())
            {
                m_animatorSelectedState = FindAnimatorStateIndex(*controller, controller->entryState);
            }

            if ((m_animatorSelectedState < 0 || m_animatorSelectedState >= static_cast<int>(controller->states.size())) &&
                !controller->states.empty())
            {
                m_animatorSelectedState = 0;
            }

            m_animatorSelectedTransition = -1;
        }

        std::string controllerLabel = meta->name;
        if (controllerLabel.empty())
            controllerLabel = fs::path(controllerPath).stem().string();

        ImGui::Text("Controller: %s", controllerLabel.c_str());
        if (boundAnimator != nullptr)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("| Bound to %s", selectedEntity->name.c_str());
        }

        ImGui::TextDisabled("%s", controllerPath.c_str());
        if (boundAnimator != nullptr)
        {
            ImGui::Text(
                "Runtime: %s @ %.3fs",
                boundAnimator->currentState.empty() ? "[ none ]" : boundAnimator->currentState.c_str(),
                boundAnimator->time);
        }

        if (selectedEntity != nullptr)
        {
            ImGui::SameLine();
            if (ImGui::SmallButton("Assign To Selected Entity"))
            {
                if (!selectedEntity->HasComponent<Animator>())
                    selectedEntity->AddComponent<Animator>();

                Animator &animator = selectedEntity->GetComponent<Animator>();
                animator.controller = MakeAnimatorControllerAssetHandleFromPath(controllerPath);
                animator.playing = true;
                boundAnimator = &animator;
            }
        }

        ImGui::SameLine();
        bool fitGraphRequested = ImGui::SmallButton("Fit Graph");
        ImGui::Separator();

        const ImGuiStyle &style = ImGui::GetStyle();
        const float sidebarWidth = std::max(360.0f, ImGui::GetFontSize() * 18.0f);
        ImVec2 availableSize = ImGui::GetContentRegionAvail();
        ImVec2 graphAreaSize(
            std::max(340.0f, availableSize.x - sidebarWidth - style.ItemSpacing.x),
            availableSize.y);

        AnimatorGraphViewState &viewState = g_animatorGraphViewStates[controllerPath];
        viewState.options.mDrawIONameOnHover = false;
        viewState.options.mDrawIONameInsideNode = true;
        viewState.options.mNodeSlotRadius = 7.0f;
        viewState.options.mLineThickness = 4.0f;
        GraphEditor::FitOnScreen fitRequest = fitGraphRequested ? GraphEditor::Fit_AllNodes : GraphEditor::Fit_None;

        ImGui::BeginChild("##animator_graph", graphAreaSize, true);

        const ImVec2 canvasScreenPos = ImGui::GetCursorScreenPos();
        const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        const ImRect canvasRect(canvasScreenPos, ImVec2(canvasScreenPos.x + canvasSize.x, canvasScreenPos.y + canvasSize.y));
        const int stateSelectionBeforeGraph = m_animatorSelectedState;

        AnimatorGraphDelegate delegate(
            *controller,
            m_animatorSelectedState,
            dirty,
            (boundAnimator != nullptr) ? boundAnimator->currentState : std::string{});
        GraphEditor::Show(delegate, viewState.options, viewState.viewState, true, &fitRequest);
        if (m_animatorSelectedState != stateSelectionBeforeGraph)
            m_animatorSelectedTransition = -1;

        if (ImGui::BeginDragDropTargetCustom(canvasRect, ImGui::GetID("AnimatorGraphDropTarget")))
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
            {
                const AssetDragData dropped = *static_cast<const AssetDragData *>(payload->Data);
                const std::string droppedPath = AssetManager::GetPath(dropped.uuid);
                MetaFileAsset *droppedMeta = AssetManager::GetMetaFile(droppedPath);
                if (droppedMeta != nullptr && droppedMeta->type == MetaFileAsset::FileType::ANIMATIONCLIP)
                {
                    AnimatorState state = {};
                    const std::string baseName = droppedMeta->name.empty() ? fs::path(droppedPath).stem().string() : droppedMeta->name;
                    state.name = MakeUniqueAnimatorStateName(*controller, baseName);
                    state.clip = MakeAnimationClipAssetHandleFromPath(droppedPath);
                    state.editorPosition = ScreenToAnimatorGraphPosition(ImGui::GetMousePos(), canvasScreenPos, viewState.viewState);
                    controller->states.push_back(state);
                    if (controller->entryState.empty())
                        controller->entryState = state.name;
                    m_animatorSelectedState = static_cast<int>(controller->states.size()) - 1;
                    m_animatorSelectedTransition = -1;
                    dirty = true;
                }
            }
            ImGui::EndDragDropTarget();
        }

        bool openContextMenu = false;
        ImVec2 popupScreenPos = viewState.contextMousePos;
        ImVec2 delegatePopupScreenPos = ImVec2(0.0f, 0.0f);
        if (delegate.ConsumeContextMenuRequest(delegatePopupScreenPos))
        {
            popupScreenPos = delegatePopupScreenPos;
            openContextMenu = true;
        }

        if (!openContextMenu && canvasRect.Contains(ImGui::GetMousePos()) && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
        {
            popupScreenPos = ImGui::GetMousePos();
            openContextMenu = true;
        }

        if (openContextMenu)
        {
            viewState.contextMousePos = popupScreenPos;
            ImGui::SetNextWindowPos(viewState.contextMousePos, ImGuiCond_Appearing);
            ImGui::OpenPopup("AnimatorContextMenu");
        }

        if (ImGui::BeginPopup("AnimatorContextMenu"))
        {
            const GraphEditor::NodeIndex contextNodeIndex = delegate.GetContextNodeIndex();
            const int contextStateIndex =
                (contextNodeIndex != kInvalidAnimatorGraphNodeIndex && contextNodeIndex < controller->states.size())
                ? static_cast<int>(contextNodeIndex)
                : -1;
            const Vector2 popupGraphPos = ScreenToAnimatorGraphPosition(
                viewState.contextMousePos,
                canvasScreenPos,
                viewState.viewState);

            if (contextStateIndex >= 0)
            {
                AnimatorState &state = controller->states[static_cast<std::size_t>(contextStateIndex)];

                if (ImGui::MenuItem("Set As Entry"))
                {
                    controller->entryState = state.name;
                    dirty = true;
                }

                if (ImGui::MenuItem("Duplicate State"))
                {
                    AnimatorState duplicateState = state;
                    duplicateState.name = MakeUniqueAnimatorStateName(*controller, state.name);
                    duplicateState.editorPosition = state.editorPosition + Vector2(42.0f, 36.0f);
                    controller->states.push_back(duplicateState);
                    m_animatorSelectedState = static_cast<int>(controller->states.size()) - 1;
                    m_animatorSelectedTransition = -1;
                    dirty = true;
                }

                if (ImGui::MenuItem("Delete State"))
                {
                    const std::string removedStateName = state.name;
                    RemoveAnimatorStateReferences(*controller, removedStateName);
                    controller->states.erase(controller->states.begin() + contextStateIndex);
                    if (controller->entryState.empty() && !controller->states.empty())
                        controller->entryState = controller->states.front().name;
                    if (m_animatorSelectedState >= static_cast<int>(controller->states.size()))
                        m_animatorSelectedState = static_cast<int>(controller->states.size()) - 1;
                    m_animatorSelectedTransition = -1;
                    dirty = true;
                }
            }
            else
            {
                if (ImGui::MenuItem("Add State"))
                {
                    AnimatorState state = {};
                    state.name = MakeUniqueAnimatorStateName(*controller, "State");
                    state.editorPosition = popupGraphPos;
                    controller->states.push_back(state);
                    if (controller->entryState.empty())
                        controller->entryState = state.name;
                    m_animatorSelectedState = static_cast<int>(controller->states.size()) - 1;
                    m_animatorSelectedTransition = -1;
                    dirty = true;
                }

                MetaFileAsset *selectedClipMeta = nullptr;
                const bool selectedAnimationClip = (AssetManager::GetMetaFile(m_selectedAssetPath) != nullptr) &&
                    ((selectedClipMeta = AssetManager::GetMetaFile(m_selectedAssetPath))->type == MetaFileAsset::FileType::ANIMATIONCLIP);
                if (selectedAnimationClip && ImGui::MenuItem("Add State From Selected Clip"))
                {
                    AnimatorState state = {};
                    const std::string baseName = selectedClipMeta->name.empty()
                        ? fs::path(m_selectedAssetPath).stem().string()
                        : selectedClipMeta->name;
                    state.name = MakeUniqueAnimatorStateName(*controller, baseName);
                    state.clip = MakeAnimationClipAssetHandleFromPath(selectedClipMeta->path.empty() ? m_selectedAssetPath : selectedClipMeta->path);
                    state.editorPosition = popupGraphPos;
                    controller->states.push_back(state);
                    if (controller->entryState.empty())
                        controller->entryState = state.name;
                    m_animatorSelectedState = static_cast<int>(controller->states.size()) - 1;
                    m_animatorSelectedTransition = -1;
                    dirty = true;
                }
            }

            ImGui::EndPopup();
        }

        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("##animator_sidebar", ImVec2(0.0f, 0.0f), true);

        if (ImGui::CollapsingHeader("Parameters", ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (std::size_t parameterIndex = 0; parameterIndex < controller->parameters.size(); ++parameterIndex)
            {
                AnimatorParameterDefinition &parameter = controller->parameters[parameterIndex];
                ImGui::PushID(static_cast<int>(parameterIndex));
                if (ImGui::TreeNode(parameter.name.empty() ? "Parameter" : parameter.name.c_str()))
                {
                    char nameBuffer[128] = {};
                    std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", parameter.name.c_str());
                    if (ImGui::InputText("name", nameBuffer, sizeof(nameBuffer)))
                    {
                        const std::string previousName = parameter.name;
                        parameter.name = nameBuffer;
                        RenameAnimatorParameterReferences(*controller, previousName, parameter.name);
                        dirty = true;
                    }

                    int typeIndex = static_cast<int>(parameter.type);
                    const char *typeLabels[] = {"Float", "Int", "Bool", "Trigger"};
                    if (ImGui::Combo("type", &typeIndex, typeLabels, IM_ARRAYSIZE(typeLabels)))
                    {
                        parameter.type = static_cast<AnimatorParameterType>(std::clamp(typeIndex, 0, 3));
                        parameter.defaultValue = MakeDefaultAnimatorParameterValue(parameter.type);
                        ResetAnimatorParameterReferenceValues(*controller, parameter.name, parameter.type);
                        dirty = true;
                    }

                    if (parameter.type != AnimatorParameterType::TRIGGER)
                        dirty = DrawAnimationValueEditor("defaultValue", parameter.defaultValue) || dirty;

                    if (ImGui::SmallButton("Delete Parameter"))
                    {
                        RemoveAnimatorParameterReferences(*controller, parameter.name);
                        controller->parameters.erase(controller->parameters.begin() + static_cast<long>(parameterIndex));
                        dirty = true;
                        ImGui::TreePop();
                        ImGui::PopID();
                        break;
                    }

                    ImGui::TreePop();
                }
                ImGui::PopID();
            }

            if (ImGui::Button("Add Parameter"))
            {
                AnimatorParameterDefinition parameter = {};
                parameter.name = "Parameter";
                parameter.type = AnimatorParameterType::FLOAT;
                parameter.defaultValue = AnimationValue::Float(0.0f);
                controller->parameters.push_back(parameter);
                dirty = true;
            }
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Selected State");
        if (m_animatorSelectedState < 0 || m_animatorSelectedState >= static_cast<int>(controller->states.size()))
        {
            ImGui::TextDisabled("Click a node in the graph to edit it.");
        }
        else
        {
            AnimatorState &state = controller->states[static_cast<std::size_t>(m_animatorSelectedState)];

            char stateNameBuffer[128] = {};
            std::snprintf(stateNameBuffer, sizeof(stateNameBuffer), "%s", state.name.c_str());
            if (ImGui::InputText("State Name", stateNameBuffer, sizeof(stateNameBuffer)))
            {
                std::string renamedState = stateNameBuffer;
                if (renamedState.empty())
                    renamedState = "State";
                if (renamedState != state.name)
                {
                    const std::string previousName = state.name;
                    renamedState = MakeUniqueAnimatorStateName(*controller, renamedState);
                    state.name = renamedState;
                    RenameAnimatorStateReferences(*controller, previousName, state.name);
                    dirty = true;
                }
            }

            AnimationClipAssetHandle previousClip = state.clip;
            InputAnimationClipAsset("Clip", "animator_window_state_clip", state.clip);
            if (AnimationClipAssetHandleChanged(previousClip, state.clip))
                dirty = true;

            const std::string stateClipPath = AssetManager::ResolvePath(state.clip);
            if (!stateClipPath.empty())
            {
                if (ImGui::SmallButton("Open Clip In Animation Window"))
                {
                    m_animationClipStatePath = stateClipPath;
                    RememberLastAnimationClipAssetPath(stateClipPath);
                }
            }

            if (ImGui::Checkbox("Loop", &state.loop))
                dirty = true;
            if (ImGui::InputFloat("Speed", &state.speed, 0.0f, 0.0f, "%.3f"))
                dirty = true;

            bool isEntryState = controller->entryState == state.name;
            if (ImGui::Checkbox("Entry State", &isEntryState))
            {
                if (isEntryState)
                    controller->entryState = state.name;
                else if (controller->entryState == state.name)
                    controller->entryState.clear();
                dirty = true;
            }

            ImGui::Text("Graph Position: %.1f, %.1f", state.editorPosition.x, state.editorPosition.y);

            ImGui::Separator();
            ImGui::TextUnformatted("Transitions");
            for (std::size_t transitionIndex = 0; transitionIndex < state.transitions.size(); ++transitionIndex)
            {
                const AnimatorTransition &transition = state.transitions[transitionIndex];
                std::string label = std::to_string(transitionIndex) + ": " +
                    (transition.toState.empty() ? std::string("[ none ]") : transition.toState);
                if (ImGui::Selectable(label.c_str(), m_animatorSelectedTransition == static_cast<int>(transitionIndex)))
                    m_animatorSelectedTransition = static_cast<int>(transitionIndex);
            }

            if (m_animatorSelectedTransition >= static_cast<int>(state.transitions.size()))
                m_animatorSelectedTransition = static_cast<int>(state.transitions.size()) - 1;

            if (ImGui::Button("Add Transition"))
            {
                AnimatorTransition transition = {};
                for (const AnimatorState &candidateState : controller->states)
                {
                    if (candidateState.name != state.name)
                    {
                        transition.toState = candidateState.name;
                        break;
                    }
                }
                state.transitions.push_back(transition);
                m_animatorSelectedTransition = static_cast<int>(state.transitions.size()) - 1;
                dirty = true;
            }

            if (m_animatorSelectedTransition >= 0 && m_animatorSelectedTransition < static_cast<int>(state.transitions.size()))
            {
                AnimatorTransition &transition = state.transitions[static_cast<std::size_t>(m_animatorSelectedTransition)];
                ImGui::Separator();

                if (ImGui::BeginCombo("To State", transition.toState.empty() ? "[ none ]" : transition.toState.c_str()))
                {
                    for (const AnimatorState &candidateState : controller->states)
                    {
                        if (candidateState.name == state.name)
                            continue;

                        const bool selected = candidateState.name == transition.toState;
                        if (ImGui::Selectable(candidateState.name.c_str(), selected))
                        {
                            transition.toState = candidateState.name;
                            dirty = true;
                        }
                    }
                    ImGui::EndCombo();
                }

                if (ImGui::Checkbox("Has Exit Time", &transition.hasExitTime))
                    dirty = true;
                if (transition.hasExitTime && ImGui::InputFloat("Exit Time Normalized", &transition.exitTimeNormalized, 0.0f, 0.0f, "%.3f"))
                {
                    transition.exitTimeNormalized = std::clamp(transition.exitTimeNormalized, 0.0f, 1.0f);
                    dirty = true;
                }

                ImGui::TextUnformatted("Conditions");
                for (std::size_t conditionIndex = 0; conditionIndex < transition.conditions.size(); ++conditionIndex)
                {
                    AnimatorTransitionCondition &condition = transition.conditions[conditionIndex];
                    ImGui::PushID(static_cast<int>(conditionIndex));
                    if (ImGui::TreeNode(("Condition " + std::to_string(conditionIndex)).c_str()))
                    {
                        if (ImGui::BeginCombo("parameter", condition.parameter.empty() ? "[ none ]" : condition.parameter.c_str()))
                        {
                            for (const AnimatorParameterDefinition &parameter : controller->parameters)
                            {
                                const bool selected = parameter.name == condition.parameter;
                                if (ImGui::Selectable(parameter.name.c_str(), selected))
                                {
                                    condition.parameter = parameter.name;
                                    condition.value = parameter.defaultValue.type == AnimationValueType::NONE
                                        ? MakeDefaultAnimatorParameterValue(parameter.type)
                                        : parameter.defaultValue;
                                    dirty = true;
                                }
                            }
                            ImGui::EndCombo();
                        }

                        int modeIndex = static_cast<int>(condition.mode);
                        const char *modeLabels[] = {"Greater", "Less", "Equal", "Not Equal", "If True", "If False", "Triggered"};
                        if (ImGui::Combo("mode", &modeIndex, modeLabels, IM_ARRAYSIZE(modeLabels)))
                        {
                            condition.mode = static_cast<AnimatorConditionMode>(std::clamp(modeIndex, 0, 6));
                            dirty = true;
                        }

                        const bool allowValue =
                            condition.mode != AnimatorConditionMode::IF_TRUE &&
                            condition.mode != AnimatorConditionMode::IF_FALSE &&
                            condition.mode != AnimatorConditionMode::TRIGGERED;
                        if (allowValue)
                        {
                            const AnimatorParameterType parameterType = FindAnimatorParameterType(*controller, condition.parameter);
                            if (condition.value.type == AnimationValueType::NONE)
                                condition.value = MakeDefaultAnimatorParameterValue(parameterType);
                            dirty = DrawAnimationValueEditor("value", condition.value) || dirty;
                        }

                        if (ImGui::SmallButton("Delete Condition"))
                        {
                            transition.conditions.erase(transition.conditions.begin() + static_cast<long>(conditionIndex));
                            dirty = true;
                            ImGui::TreePop();
                            ImGui::PopID();
                            break;
                        }

                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }

                if (ImGui::Button("Add Condition"))
                {
                    AnimatorTransitionCondition condition = {};
                    if (!controller->parameters.empty())
                    {
                        condition.parameter = controller->parameters.front().name;
                        condition.value = controller->parameters.front().defaultValue.type == AnimationValueType::NONE
                            ? MakeDefaultAnimatorParameterValue(controller->parameters.front().type)
                            : controller->parameters.front().defaultValue;
                    }
                    transition.conditions.push_back(condition);
                    dirty = true;
                }

                if (ImGui::SmallButton("Delete Transition"))
                {
                    state.transitions.erase(state.transitions.begin() + m_animatorSelectedTransition);
                    if (m_animatorSelectedTransition >= static_cast<int>(state.transitions.size()))
                        m_animatorSelectedTransition = static_cast<int>(state.transitions.size()) - 1;
                    dirty = true;
                }
            }
        }

        ImGui::EndChild();
        ImGui::End();

        if (dirty)
            (void)controller->Save();
    }

    void Editor::DrawAnimationWindow(float _deltaTime)
    {
        namespace fs = std::filesystem;

        if (m_animationClipStatePath.empty())
            m_animationClipStatePath = ResolveRememberedAnimationClipPath();

        Entity *selectedEntity = nullptr;
        if (m_scene != nullptr && m_index >= 0 && m_index < static_cast<int>(m_scene->GetEntities().size()))
            selectedEntity = m_scene->GetEntities()[m_index];

        Entity *rootEntity = nullptr;
        if (m_scene != nullptr && m_animationTargetUUID != UUID(0))
            rootEntity = m_scene->GetEntityWithUUID(m_animationTargetUUID);

        if (rootEntity == nullptr && selectedEntity != nullptr && m_animationTargetUUID == UUID(0))
            rootEntity = selectedEntity;

        if (m_animationClipStatePath.empty() && rootEntity != nullptr && rootEntity->HasComponent<AnimationPlayer>())
        {
            const std::string playerClipPath = AssetManager::ResolvePath(rootEntity->GetComponent<AnimationPlayer>().clip);
            if (!playerClipPath.empty())
            {
                m_animationClipStatePath = playerClipPath;
                RememberLastAnimationClipAssetPath(playerClipPath);
            }
        }

        if (m_animationClipStatePath.empty() && rootEntity != nullptr && rootEntity->HasComponent<Animator>())
        {
            Animator &animator = rootEntity->GetComponent<Animator>();
            const std::string controllerPath = AssetManager::ResolvePath(animator.controller);
            if (!controllerPath.empty())
            {
                if (AnimatorControllerAsset *controller = AssetManager::GetAnimatorController(controllerPath))
                {
                    const std::string desiredState = animator.currentState.empty()
                        ? (controller->entryState.empty() ? (controller->states.empty() ? std::string{} : controller->states.front().name) : controller->entryState)
                        : animator.currentState;
                    for (const AnimatorState &state : controller->states)
                    {
                        if (state.name != desiredState)
                            continue;

                        const std::string stateClipPath = AssetManager::ResolvePath(state.clip);
                        if (!stateClipPath.empty())
                        {
                            m_animationClipStatePath = stateClipPath;
                            RememberLastAnimationClipAssetPath(stateClipPath);
                        }
                        break;
                    }
                }
            }
        }

        AnimationClipAsset *clip = nullptr;
        MetaFileAsset *clipMeta = nullptr;
        if (!m_animationClipStatePath.empty())
        {
            clipMeta = AssetManager::GetMetaFile(m_animationClipStatePath);
            if (clipMeta != nullptr && clipMeta->type == MetaFileAsset::FileType::ANIMATIONCLIP)
                clip = AssetManager::GetAnimationClip(clipMeta->path.empty() ? m_animationClipStatePath : clipMeta->path);
        }

        auto restorePreviewState = [&]() -> void
        {
            if (m_scene == nullptr || m_app == nullptr || m_animationRestoreBindings.empty())
            {
                m_animationRestoreBindings.clear();
                m_animationPreviewTargetUUID = UUID(0);
                m_animationPreviewClipPath.clear();
                return;
            }

            Entity *restoreRoot = (m_animationPreviewTargetUUID != UUID(0)) ? m_scene->GetEntityWithUUID(m_animationPreviewTargetUUID) : nullptr;
            if (restoreRoot != nullptr)
            {
                for (const AnimationRestoreBinding &binding : m_animationRestoreBindings)
                {
                    AnimationTrack bindingTrack = {};
                    bindingTrack.path = binding.path;
                    bindingTrack.component = binding.component;
                    bindingTrack.property = binding.property;
                    bindingTrack.type = binding.value.type;

                    AnimationBindingTarget target = {};
                    if (!ResolveAnimationTrackTarget(*m_app, *restoreRoot, bindingTrack, target))
                        continue;

                    auto setterIt = target.conf->registry.animationSetters.find(binding.property);
                    if (setterIt == target.conf->registry.animationSetters.end())
                        continue;

                    setterIt->second(target.component, binding.value);
                }
            }

            m_animationRestoreBindings.clear();
            m_animationPreviewTargetUUID = UUID(0);
            m_animationPreviewClipPath.clear();
        };

        auto capturePreviewState = [&](AnimationClipAsset &_clip, Entity &_root) -> void
        {
            restorePreviewState();

            std::unordered_set<std::string> seenBindings = {};
            for (const AnimationTrack &track : _clip.tracks)
            {
                const std::string bindingKey = track.path + "\n" + track.component + "\n" + track.property;
                if (!seenBindings.insert(bindingKey).second)
                    continue;

                AnimationValue capturedValue = {};
                if (!CaptureAnimationTrackValue(*m_app, _root, track, capturedValue))
                    continue;

                AnimationRestoreBinding binding = {};
                binding.path = track.path;
                binding.component = track.component;
                binding.property = track.property;
                binding.value = capturedValue;
                m_animationRestoreBindings.push_back(binding);
            }

            m_animationPreviewTargetUUID = _root.uuid;
            m_animationPreviewClipPath = m_animationClipStatePath;
        };

        auto createNewClip = [&](const std::string &_nameHint) -> void
        {
            std::string baseName = _nameHint.empty() ? "new_animation" : _nameHint;
            for (char &c : baseName)
            {
                if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_'))
                    c = '_';
            }
            if (baseName.empty())
                baseName = "new_animation";

            std::error_code ec;
            fs::create_directories("assets/animations", ec);

            fs::path targetPath = fs::path("assets/animations") / (baseName + ".animclip");
            int suffix = 1;
            while (fs::exists(targetPath, ec))
            {
                targetPath = fs::path("assets/animations") / (baseName + "_" + std::to_string(suffix) + ".animclip");
                ++suffix;
            }

            AnimationClipAsset newClip = {};
            newClip.length = 1.0f;
            if (!newClip.Save(targetPath.generic_string()))
                return;

            (void)AssetManager::GetMetaFile(targetPath.generic_string());
            m_selectedAssetPath = targetPath.generic_string();
            m_animationClipStatePath = targetPath.generic_string();
            m_animationSelectedTrack = -1;
            m_animationSelectedEvent = -1;
            m_animationFirstFrame = 0;
            m_animationTime = 0.0f;
            RememberLastAnimationClipAssetPath(m_animationClipStatePath);
        };

        ImGui::Begin("Animation", &m_showAnimationPanel);

        std::string clipLabel = "[ none ]";
        if (clipMeta != nullptr)
            clipLabel = clipMeta->name;
        else if (!m_animationClipStatePath.empty())
            clipLabel = m_animationClipStatePath;

        ImGui::TextUnformatted("Clip");
        ImGui::SameLine();
        ImGui::Button(clipLabel.c_str(), ImVec2(180, 0));
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
            {
                const AssetDragData dropped = *static_cast<const AssetDragData *>(payload->Data);
                const std::string path = AssetManager::GetPath(dropped.uuid);
                if (MetaFileAsset *meta = AssetManager::GetMetaFile(path))
                {
                    if (meta->type == MetaFileAsset::FileType::ANIMATIONCLIP)
                    {
                        m_animationClipStatePath = path;
                        RememberLastAnimationClipAssetPath(path);
                        m_animationSelectedTrack = -1;
                        m_animationSelectedEvent = -1;
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("New##animation_clip"))
            createNewClip((selectedEntity != nullptr) ? selectedEntity->name : std::string("new_animation"));

        if (!m_animationClipStatePath.empty())
        {
            ImGui::SameLine();
            if (ImGui::SmallButton("Clear##animation_clip"))
            {
                ClearRememberedAnimationClipAssetPathIfMatches(m_animationClipStatePath);
                m_animationClipStatePath.clear();
                m_animationSelectedTrack = -1;
                m_animationSelectedEvent = -1;
            }
        }

        const char *rootName = (rootEntity != nullptr) ? rootEntity->name.c_str() : "[ none ]";
        ImGui::Text("Root: %s", rootName);
        if (selectedEntity != nullptr)
        {
            ImGui::SameLine();
            if (ImGui::SmallButton("Use Selected As Root"))
                m_animationTargetUUID = selectedEntity->uuid;
        }

        if (rootEntity != nullptr && clip != nullptr)
        {
            ImGui::SameLine();
            if (ImGui::SmallButton("Assign To AnimationPlayer"))
            {
                if (!rootEntity->HasComponent<AnimationPlayer>())
                    rootEntity->AddComponent<AnimationPlayer>();

                AnimationPlayer &player = rootEntity->GetComponent<AnimationPlayer>();
                player.clip = MakeAnimationClipAssetHandleFromPath(m_animationClipStatePath);
            }
        }

        ImGui::Separator();

        bool clipDirty = false;
        if (clip != nullptr)
        {
            clip->length = std::max(clip->length, 0.0f);
            if (m_animationPlaying)
                m_animationTime += _deltaTime;

            const float sampleTime = NormalizeAnimationSampleTime(m_animationTime, clip->length, true);
            if (clip->length > 0.0f)
                m_animationTime = sampleTime;
            else
                m_animationTime = std::max(m_animationTime, 0.0f);

            auto selectTrackByBinding = [&](const std::string &_path, const std::string &_component, const std::string &_property) -> void
            {
                m_animationSelectedTrack = -1;
                for (std::size_t i = 0; i < clip->tracks.size(); ++i)
                {
                    const AnimationTrack &track = clip->tracks[i];
                    if (track.path == _path && track.component == _component && track.property == _property)
                    {
                        m_animationSelectedTrack = static_cast<int>(i);
                        break;
                    }
                }
            };

            auto addPropertyTrack = [&](const AnimationPropertyCandidate &_candidate) -> void
            {
                if (rootEntity == nullptr)
                    return;

                AnimationTrack *track = FindOrCreateAnimationTrack(
                    *clip,
                    _candidate.path,
                    _candidate.component,
                    _candidate.property,
                    _candidate.type,
                    _candidate.interpolation);
                if (track == nullptr)
                    return;

                if (track->keys.empty())
                {
                    AnimationValue capturedValue = MakeDefaultAnimationValue(_candidate.type);
                    if (!CaptureAnimationTrackValue(*m_app, *rootEntity, *track, capturedValue))
                        capturedValue = MakeDefaultAnimationValue(_candidate.type);

                    track->keys.push_back(AnimationKeyframe{std::max(m_animationTime, 0.0f), capturedValue});
                    SortAnimationTrackKeys(*track);
                }

                RefreshAnimationClipLength(*clip);
                (void)clip->Save();
                clipDirty = true;
                selectTrackByBinding(_candidate.path, _candidate.component, _candidate.property);
            };

            ImGui::Checkbox("Preview", &m_animationPreviewEnabled);
            ImGui::SameLine();
            ImGui::Checkbox("Record", &m_animationRecordEnabled);
            ImGui::SameLine();
            if (ImGui::Button(m_animationPlaying ? "Pause" : "Play"))
                m_animationPlaying = !m_animationPlaying;

            ImGui::SameLine();
            if (ImGui::Button("Stop"))
            {
                m_animationPlaying = false;
                m_animationTime = 0.0f;
            }

            ImGui::SameLine();
            ImGui::SetNextItemWidth(110.0f);
            if (ImGui::InputFloat("Length", &clip->length, 0.0f, 0.0f, "%.3f"))
            {
                clip->length = std::max(clip->length, 0.0f);
                clipDirty = true;
            }

            if (rootEntity != nullptr)
            {
                ImGui::SameLine();
                if (ImGui::Button("Add Property"))
                    ImGui::OpenPopup("AnimationAddPropertyPopup");
            }

            ImGui::SameLine();
            if (ImGui::Button("Add Event At Current Time"))
            {
                AnimationEvent event = {};
                event.time = std::max(m_animationTime, 0.0f);
                event.name = "OnAnimationEvent";
                if (rootEntity != nullptr && selectedEntity != nullptr && selectedEntity != rootEntity)
                {
                    std::string relativePath = {};
                    if (BuildAnimationRelativePath(*rootEntity, *selectedEntity, relativePath))
                        event.path = relativePath;
                }

                clip->events.push_back(event);
                std::sort(clip->events.begin(), clip->events.end(), [](const AnimationEvent &_left, const AnimationEvent &_right)
                {
                    return _left.time < _right.time;
                });

                m_animationSelectedEvent = static_cast<int>(clip->events.size()) - 1;
                RefreshAnimationClipLength(*clip);
                clipDirty = true;
            }

            if (ImGui::BeginPopup("AnimationAddPropertyPopup"))
            {
                ImGui::SetNextItemWidth(320.0f);
                ImGui::InputTextWithHint("##animation_property_search", "Search property", &m_animationAddPropertySearch);
                ImGui::Separator();

                std::vector<AnimationPropertyCandidate> candidates = {};
                if (rootEntity != nullptr)
                    CollectAnimationPropertyCandidatesRecursive(*m_app, *rootEntity, *rootEntity, candidates);

                std::sort(candidates.begin(), candidates.end(), [](const AnimationPropertyCandidate &_left, const AnimationPropertyCandidate &_right)
                {
                    return _left.label < _right.label;
                });

                for (const AnimationPropertyCandidate &candidate : candidates)
                {
                    if (!m_animationAddPropertySearch.empty())
                    {
                        std::string haystack = candidate.label;
                        std::string needle = m_animationAddPropertySearch;
                        std::transform(haystack.begin(), haystack.end(), haystack.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                        std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                        if (haystack.find(needle) == std::string::npos)
                            continue;
                    }

                    const bool alreadyExists = FindAnimationTrack(*clip, candidate.path, candidate.component, candidate.property) != nullptr;
                    const std::string buttonLabel = alreadyExists ? (candidate.label + "  [select]") : candidate.label;
                    if (ImGui::Selectable(buttonLabel.c_str()))
                    {
                        addPropertyTrack(candidate);
                        ImGui::CloseCurrentPopup();
                    }
                }

                ImGui::EndPopup();
            }

            ImGui::Text("Time: %.3fs  Frame: %d", m_animationTime, AnimationFrameFromTime(m_animationTime));

            struct AnimationClipSequencer : ImSequencer::SequenceInterface
            {
                AnimationClipAsset &clip;
                std::vector<int> startFrames = {};
                std::vector<int> endFrames = {};
                std::vector<std::string> labels = {};
                int frameMax = 1;
                bool deletedTrack = false;

                explicit AnimationClipSequencer(AnimationClipAsset &_clip) : clip(_clip)
                {
                    Rebuild();
                }

                void Rebuild()
                {
                    startFrames.clear();
                    endFrames.clear();
                    labels.clear();
                    frameMax = std::max(1, AnimationFrameFromTime(std::max(clip.length, 0.0f)));

                    for (const AnimationTrack &track : clip.tracks)
                    {
                        int start = 0;
                        int end = 0;
                        if (!track.keys.empty())
                        {
                            start = AnimationFrameFromTime(track.keys.front().time);
                            end = AnimationFrameFromTime(track.keys.back().time);
                            frameMax = std::max(frameMax, end + 1);
                        }

                        startFrames.push_back(start);
                        endFrames.push_back(std::max(end, start));
                        labels.push_back(BuildAnimationTrackLabel(track));
                    }
                }

                int GetFrameMin() const override { return 0; }
                int GetFrameMax() const override { return frameMax; }
                int GetItemCount() const override { return static_cast<int>(clip.tracks.size()); }
                const char *GetItemLabel(int index) const override
                {
                    if (index < 0 || index >= static_cast<int>(labels.size()))
                        return "";
                    return labels[index].c_str();
                }

                void Get(int index, int **start, int **end, int *type, unsigned int *color) override
                {
                    if (start)
                        *start = &startFrames[index];
                    if (end)
                        *end = &endFrames[index];
                    if (type)
                        *type = 0;
                    if (color)
                        *color = 0xFF80C0FF;
                }

                void Del(int index) override
                {
                    if (index < 0 || index >= static_cast<int>(clip.tracks.size()))
                        return;

                    clip.tracks.erase(clip.tracks.begin() + index);
                    deletedTrack = true;
                    Rebuild();
                }

                void CustomDrawCompact(int index, ImDrawList *draw_list, const ImRect &rc, const ImRect &clippingRect) override
                {
                    if (index < 0 || index >= static_cast<int>(clip.tracks.size()))
                        return;

                    const AnimationTrack &track = clip.tracks[index];
                    const float frameSpan = static_cast<float>(std::max(frameMax, 1));

                    draw_list->PushClipRect(clippingRect.Min, clippingRect.Max, true);
                    for (const AnimationKeyframe &key : track.keys)
                    {
                        const float normalized = static_cast<float>(AnimationFrameFromTime(key.time)) / frameSpan;
                        const float x = ImLerp(rc.Min.x, rc.Max.x, normalized);
                        draw_list->AddLine(ImVec2(x, rc.Min.y + 4.0f), ImVec2(x, rc.Max.y - 4.0f), 0xFFFFFFFF, 2.0f);
                    }
                    draw_list->PopClipRect();
                }
            };

            int currentFrame = AnimationFrameFromTime(m_animationTime);
            AnimationClipSequencer sequencer(*clip);
            m_animationExpanded = m_animationExpanded || !clip->tracks.empty();
            const int sequenceFlags = ImSequencer::SEQUENCER_CHANGE_FRAME | ImSequencer::SEQUENCER_DEL;
            (void)ImSequencer::Sequencer(&sequencer, &currentFrame, &m_animationExpanded, &m_animationSelectedTrack, &m_animationFirstFrame, sequenceFlags);
            if (sequencer.deletedTrack)
            {
                if (m_animationSelectedTrack >= static_cast<int>(clip->tracks.size()))
                    m_animationSelectedTrack = static_cast<int>(clip->tracks.size()) - 1;
                clipDirty = true;
            }

            m_animationTime = AnimationTimeFromFrame(currentFrame);
            if (m_animationSelectedTrack >= 0 && m_animationSelectedTrack < static_cast<int>(clip->tracks.size()))
            {
                AnimationTrack &track = clip->tracks[m_animationSelectedTrack];
                ImGui::Separator();
                ImGui::Text("%s", BuildAnimationTrackLabel(track).c_str());

                int interpolationIndex = (track.interpolation == AnimationInterpolation::STEP) ? 1 : 0;
                const char *interpolationLabels[] = {"Linear", "Step"};
                if (ImGui::Combo("Interpolation", &interpolationIndex, interpolationLabels, IM_ARRAYSIZE(interpolationLabels)))
                {
                    track.interpolation = (interpolationIndex == 1) ? AnimationInterpolation::STEP : AnimationInterpolation::LINEAR;
                    clipDirty = true;
                }

                if (rootEntity != nullptr && ImGui::Button("Capture Key At Current Time"))
                {
                    AnimationValue capturedValue = {};
                    if (CaptureAnimationTrackValue(*m_app, *rootEntity, track, capturedValue))
                    {
                        bool updatedExisting = false;
                        for (AnimationKeyframe &key : track.keys)
                        {
                            if (std::fabs(key.time - m_animationTime) <= kAnimationEditorKeyTimeEpsilon)
                            {
                                key.value = capturedValue;
                                updatedExisting = true;
                                break;
                            }
                        }

                        if (!updatedExisting)
                            track.keys.push_back(AnimationKeyframe{m_animationTime, capturedValue});

                        SortAnimationTrackKeys(track);
                        clipDirty = true;
                    }
                }

                ImGui::SameLine();
                if (ImGui::Button("Add Empty Key"))
                {
                    AnimationValue value = track.keys.empty() ? MakeDefaultAnimationValue(track.type) : EvaluateAnimationTrack(track, std::max(m_animationTime, 0.0f));
                    track.keys.push_back(AnimationKeyframe{std::max(m_animationTime, 0.0f), value});
                    SortAnimationTrackKeys(track);
                    clipDirty = true;
                }

                for (std::size_t keyIndex = 0; keyIndex < track.keys.size(); ++keyIndex)
                {
                    AnimationKeyframe &key = track.keys[keyIndex];
                    ImGui::PushID(static_cast<int>(keyIndex));
                    bool keyDirty = false;

                    if (ImGui::InputFloat("time", &key.time, 0.0f, 0.0f, "%.3f"))
                    {
                        key.time = std::max(key.time, 0.0f);
                        keyDirty = true;
                    }

                    keyDirty = DrawAnimationValueEditor("value", key.value) || keyDirty;

                    if (ImGui::SmallButton("Set Current Time"))
                        m_animationTime = key.time;

                    ImGui::SameLine();
                    if (ImGui::SmallButton("Delete Key"))
                    {
                        track.keys.erase(track.keys.begin() + static_cast<long>(keyIndex));
                        clipDirty = true;
                        ImGui::PopID();
                        break;
                    }

                    if (keyDirty)
                    {
                        SortAnimationTrackKeys(track);
                        clipDirty = true;
                    }

                    ImGui::Separator();
                    ImGui::PopID();
                }

                if (!track.keys.empty())
                {
                    struct AnimationTrackCurveDelegate : ImCurveEdit::Delegate
                    {
                        AnimationTrack &track;
                        float clipLength = 1.0f;
                        ImVec2 min = ImVec2(0.0f, -1.0f);
                        ImVec2 max = ImVec2(1.0f, 1.0f);
                        std::vector<std::vector<ImVec2>> points = {};

                        AnimationTrackCurveDelegate(AnimationTrack &_track, float _clipLength)
                            : track(_track), clipLength(std::max(_clipLength, 1.0f))
                        {
                            Rebuild();
                        }

                        void Rebuild()
                        {
                            const int componentCount = std::max(1, GetAnimationValueComponentCount(track.type));
                            points.assign(static_cast<std::size_t>(componentCount), {});

                            float minValue = std::numeric_limits<float>::max();
                            float maxValue = std::numeric_limits<float>::lowest();
                            for (const AnimationKeyframe &key : track.keys)
                            {
                                for (int componentIndex = 0; componentIndex < componentCount; ++componentIndex)
                                {
                                    const float value = GetAnimationValueComponent(key.value, componentIndex);
                                    points[static_cast<std::size_t>(componentIndex)].push_back(ImVec2(key.time, value));
                                    minValue = std::min(minValue, value);
                                    maxValue = std::max(maxValue, value);
                                }
                            }

                            if (track.type == AnimationValueType::BOOL)
                            {
                                minValue = -0.1f;
                                maxValue = 1.1f;
                            }
                            else if (track.type == AnimationValueType::INT && minValue <= maxValue)
                            {
                                minValue -= 1.0f;
                                maxValue += 1.0f;
                            }
                            else if (!(minValue <= maxValue))
                            {
                                minValue = -1.0f;
                                maxValue = 1.0f;
                            }
                            else if (std::fabs(maxValue - minValue) < 0.01f)
                            {
                                minValue -= 1.0f;
                                maxValue += 1.0f;
                            }

                            min = ImVec2(0.0f, minValue);
                            max = ImVec2(std::max(clipLength, 0.1f), maxValue);
                        }

                        size_t GetCurveCount() override
                        {
                            return static_cast<size_t>(std::max(1, GetAnimationValueComponentCount(track.type)));
                        }

                        ImCurveEdit::CurveType GetCurveType(size_t) const override
                        {
                            return AnimationValueIsDiscrete(track.type) ? ImCurveEdit::CurveDiscrete : ImCurveEdit::CurveLinear;
                        }

                        ImVec2& GetMin() override { return min; }
                        ImVec2& GetMax() override { return max; }

                        size_t GetPointCount(size_t) override
                        {
                            return track.keys.size();
                        }

                        uint32_t GetCurveColor(size_t curveIndex) override
                        {
                            return GetAnimationCurveColor(static_cast<int>(curveIndex));
                        }

                        ImVec2* GetPoints(size_t curveIndex) override
                        {
                            Rebuild();
                            return points[curveIndex].data();
                        }

                        int EditPoint(size_t curveIndex, int pointIndex, ImVec2 value) override
                        {
                            if (pointIndex < 0 || pointIndex >= static_cast<int>(track.keys.size()))
                                return pointIndex;

                            AnimationKeyframe editedKey = track.keys[pointIndex];
                            editedKey.time = std::max(value.x, 0.0f);
                            SetAnimationValueComponent(editedKey.value, static_cast<int>(curveIndex), value.y);
                            track.keys[pointIndex] = editedKey;
                            SortAnimationTrackKeys(track);
                            Rebuild();

                            int bestIndex = 0;
                            float bestDistance = std::numeric_limits<float>::max();
                            for (std::size_t i = 0; i < track.keys.size(); ++i)
                            {
                                const float dx = std::fabs(track.keys[i].time - editedKey.time);
                                const float dy = std::fabs(GetAnimationValueComponent(track.keys[i].value, static_cast<int>(curveIndex)) - GetAnimationValueComponent(editedKey.value, static_cast<int>(curveIndex)));
                                const float distance = dx + dy;
                                if (distance < bestDistance)
                                {
                                    bestDistance = distance;
                                    bestIndex = static_cast<int>(i);
                                }
                            }

                            return bestIndex;
                        }

                        void AddPoint(size_t curveIndex, ImVec2 value) override
                        {
                            AnimationKeyframe key = {};
                            key.time = std::max(value.x, 0.0f);
                            key.value = track.keys.empty() ? MakeDefaultAnimationValue(track.type) : EvaluateAnimationTrack(track, key.time);
                            SetAnimationValueComponent(key.value, static_cast<int>(curveIndex), value.y);
                            track.keys.push_back(key);
                            SortAnimationTrackKeys(track);
                            Rebuild();
                        }
                    };

                    ImGui::TextUnformatted("Curves");
                    for (int componentIndex = 0; componentIndex < std::max(1, GetAnimationValueComponentCount(track.type)); ++componentIndex)
                    {
                        ImGui::SameLine(componentIndex == 0 ? 0.0f : 0.0f);
                        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(GetAnimationCurveColor(componentIndex)), "%s", GetAnimationValueComponentName(track.type, componentIndex));
                    }

                    static ImVector<ImCurveEdit::EditPoint> selectedCurvePoints;
                    AnimationTrackCurveDelegate curveDelegate(track, clip->length);
                    if (ImCurveEdit::Edit(curveDelegate, ImVec2(-1.0f, 220.0f), ImGui::GetID("AnimationCurves"), nullptr, &selectedCurvePoints))
                        clipDirty = true;
                }
            }

            ImGui::Separator();
            if (ImGui::CollapsingHeader("Events", ImGuiTreeNodeFlags_DefaultOpen))
            {
                for (std::size_t eventIndex = 0; eventIndex < clip->events.size(); ++eventIndex)
                {
                    const AnimationEvent &event = clip->events[eventIndex];
                    std::string label = std::to_string(eventIndex) + ": " + event.name + " @ " + std::to_string(event.time);
                    if (ImGui::Selectable(label.c_str(), m_animationSelectedEvent == static_cast<int>(eventIndex)))
                        m_animationSelectedEvent = static_cast<int>(eventIndex);
                }

                if (m_animationSelectedEvent >= static_cast<int>(clip->events.size()))
                    m_animationSelectedEvent = static_cast<int>(clip->events.size()) - 1;

                if (m_animationSelectedEvent >= 0 && m_animationSelectedEvent < static_cast<int>(clip->events.size()))
                {
                    AnimationEvent &event = clip->events[m_animationSelectedEvent];
                    ImGui::Separator();
                    if (ImGui::InputFloat("event time", &event.time, 0.0f, 0.0f, "%.3f"))
                    {
                        event.time = std::max(event.time, 0.0f);
                        std::sort(clip->events.begin(), clip->events.end(), [](const AnimationEvent &_left, const AnimationEvent &_right)
                        {
                            return _left.time < _right.time;
                        });
                        clipDirty = true;
                    }

                    char eventPathBuffer[256] = {};
                    std::snprintf(eventPathBuffer, sizeof(eventPathBuffer), "%s", event.path.c_str());
                    if (ImGui::InputText("target path", eventPathBuffer, sizeof(eventPathBuffer)))
                    {
                        event.path = eventPathBuffer;
                        clipDirty = true;
                    }

                    if (rootEntity != nullptr && selectedEntity != nullptr)
                    {
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Use Selected Entity"))
                        {
                            if (selectedEntity == rootEntity)
                            {
                                event.path.clear();
                                clipDirty = true;
                            }
                            else
                            {
                                std::string relativePath = {};
                                if (BuildAnimationRelativePath(*rootEntity, *selectedEntity, relativePath))
                                {
                                    event.path = relativePath;
                                    clipDirty = true;
                                }
                            }
                        }
                    }

                    char eventScriptBuffer[256] = {};
                    std::snprintf(eventScriptBuffer, sizeof(eventScriptBuffer), "%s", event.script.c_str());
                    if (ImGui::InputText("script", eventScriptBuffer, sizeof(eventScriptBuffer)))
                    {
                        event.script = eventScriptBuffer;
                        clipDirty = true;
                    }

                    char eventNameBuffer[256] = {};
                    std::snprintf(eventNameBuffer, sizeof(eventNameBuffer), "%s", event.name.c_str());
                    if (ImGui::InputText("event name", eventNameBuffer, sizeof(eventNameBuffer)))
                    {
                        event.name = eventNameBuffer;
                        clipDirty = true;
                    }

                    char stringPayloadBuffer[256] = {};
                    std::snprintf(stringPayloadBuffer, sizeof(stringPayloadBuffer), "%s", event.stringPayload.c_str());
                    if (ImGui::InputText("string payload", stringPayloadBuffer, sizeof(stringPayloadBuffer)))
                    {
                        event.stringPayload = stringPayloadBuffer;
                        clipDirty = true;
                    }

                    clipDirty = ImGui::InputFloat("float payload", &event.floatPayload, 0.0f, 0.0f, "%.3f") || clipDirty;
                    clipDirty = ImGui::InputInt("int payload", &event.intPayload) || clipDirty;

                    if (ImGui::SmallButton("Delete Event"))
                    {
                        clip->events.erase(clip->events.begin() + m_animationSelectedEvent);
                        if (m_animationSelectedEvent >= static_cast<int>(clip->events.size()))
                            m_animationSelectedEvent = static_cast<int>(clip->events.size()) - 1;
                        clipDirty = true;
                    }
                }
            }

            if (clipDirty)
            {
                RefreshAnimationClipLength(*clip);
                (void)clip->Save();
            }
        }
        else
        {
            ImGui::TextDisabled("Drop an .animclip here or create a new one.");
        }

        ImGui::End();

        if (clip != nullptr && m_animationTargetUUID == UUID(0) && rootEntity != nullptr)
            m_animationTargetUUID = rootEntity->uuid;

        const bool previewActive =
            (m_mode == EditorMode::EDIT) &&
            (clip != nullptr) &&
            (rootEntity != nullptr) &&
            (m_animationPreviewEnabled || m_animationRecordEnabled);

        const bool previewTargetChanged =
            m_animationPreviewTargetUUID != ((rootEntity != nullptr) ? rootEntity->uuid : UUID(0)) ||
            m_animationPreviewClipPath != m_animationClipStatePath;

        if (!previewActive)
        {
            restorePreviewState();
            return;
        }

        if (previewTargetChanged || m_animationRestoreBindings.empty())
            capturePreviewState(*clip, *rootEntity);

        const float sampleTime = NormalizeAnimationSampleTime(m_animationTime, clip->length, true);
        (void)ApplyAnimationClip(*m_app, *rootEntity, *clip, sampleTime);
    }

    void Editor::DrawAddComponentDropDown(bool _refresh)
    {
        if (m_index < 0 || m_index >= (int)m_scene->GetEntities().size())
            return;

        Entity *selectedEntity = m_scene->GetEntities()[m_index];
        if (selectedEntity == nullptr)
            return;

        Entity &entity = *selectedEntity;

        if (_refresh)
        {
            m_addComponentSelection = 0;
            m_addComponentSearch.clear();
            m_focusAddComponentSearch = false;
        }

        std::vector<AddComponentEntry> componentEntries = BuildAddComponentEntries(*m_app, entity);
        const bool hasAvailableComponents = !componentEntries.empty();

        if (!hasAvailableComponents)
        {
            ImGui::BeginDisabled();
            ImGui::Button("Add Component", ImVec2(-1.0f, 0.0f));
            ImGui::EndDisabled();
            return;
        }

        if (ImGui::Button("Add Component", ImVec2(-1.0f, 0.0f)))
        {
            m_addComponentSelection = 0;
            m_addComponentSearch.clear();
            m_focusAddComponentSearch = true;
            ImGui::OpenPopup("Add Component");
        }

        ImGuiViewport *viewport = ImGui::GetMainViewport();
        if (viewport != nullptr)
        {
            const ImVec2 popupCenter = ImVec2(
                viewport->Pos.x + (viewport->Size.x * 0.5f),
                viewport->Pos.y + (viewport->Size.y * 0.5f));
            ImGui::SetNextWindowPos(popupCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        }
        ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_Appearing);

        if (ImGui::BeginPopupModal("Add Component", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            auto addSelectedComponent = [&](const std::vector<int> &_filteredIndices) -> bool
            {
                if (_filteredIndices.empty())
                    return false;

                const std::string componentName = componentEntries[_filteredIndices[m_addComponentSelection]].componentName;
                for (ScriptConf &conf : m_app->GetScriptRegistry())
                {
                    if (conf.name == componentName)
                    {
                        conf.Add(entity);
                        ImGui::CloseCurrentPopup();
                        m_addComponentSelection = 0;
                        m_addComponentSearch.clear();
                        return true;
                    }
                }

                return false;
            };

            auto cancelAddComponent = [&]() -> void
            {
                m_addComponentSelection = 0;
                m_addComponentSearch.clear();
                ImGui::CloseCurrentPopup();
            };

            auto matchesSearch = [&](const AddComponentEntry &_entry) -> bool
            {
                if (m_addComponentSearch.empty())
                    return true;

                std::string displayName = _entry.displayName;
                std::string componentName = _entry.componentName;
                std::string search = m_addComponentSearch;

                std::transform(displayName.begin(), displayName.end(), displayName.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                std::transform(componentName.begin(), componentName.end(), componentName.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                std::transform(search.begin(), search.end(), search.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                return displayName.find(search) != std::string::npos || componentName.find(search) != std::string::npos;
            };

            if (m_focusAddComponentSearch)
            {
                ImGui::SetKeyboardFocusHere();
                m_focusAddComponentSearch = false;
            }

            ImGui::InputTextWithHint("##AddComponentSearch", "Search components...", &m_addComponentSearch);
            ImGui::Separator();

            std::vector<int> filteredIndices = {};
            filteredIndices.reserve(componentEntries.size());
            for (int i = 0; i < static_cast<int>(componentEntries.size()); ++i)
            {
                if (matchesSearch(componentEntries[i]))
                    filteredIndices.push_back(i);
            }

            if (filteredIndices.empty())
            {
                m_addComponentSelection = 0;
                ImGui::TextDisabled("No components match the current search.");
            }
            else
            {
                m_addComponentSelection = std::clamp(m_addComponentSelection, 0, static_cast<int>(filteredIndices.size()) - 1);

                ImGui::BeginChild("##AddComponentList", ImVec2(420.0f, 260.0f), true);
                for (int filteredIndex = 0; filteredIndex < static_cast<int>(filteredIndices.size()); ++filteredIndex)
                {
                    const int componentIndex = filteredIndices[filteredIndex];
                    const bool selected = (filteredIndex == m_addComponentSelection);
                    const AddComponentEntry &entry = componentEntries[componentIndex];

                    if (ImGui::Selectable(entry.displayName.c_str(), selected))
                        m_addComponentSelection = filteredIndex;

                    if (selected)
                        ImGui::SetItemDefaultFocus();

                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        addSelectedComponent(filteredIndices);
                }
                ImGui::EndChild();
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                cancelAddComponent();
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))
            {
                addSelectedComponent(filteredIndices);
            }

            const bool canAddComponent = !filteredIndices.empty();
            if (!canAddComponent)
                ImGui::BeginDisabled();

            if (ImGui::Button("Add", ImVec2(120.0f, 0.0f)))
                addSelectedComponent(filteredIndices);

            if (!canAddComponent)
                ImGui::EndDisabled();

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
                cancelAddComponent();

            ImGui::EndPopup();
        }
    }

    void Editor::DrawEnvironment()
    {
        ImGui::Begin("Environment", &m_showEnvironmentPanel);
        Color background = m_window->GetClearColor();
        ImGui::ColorEdit4("Background##", &background.r);

        if (background != m_window->GetClearColor())
            m_window->SetClearColor(background);

        Color ambientLight = m_scene->GetEnvironmentAmbientLight();
        if (ImGui::ColorEdit3("Ambient Light##", &ambientLight.r))
        {
            ambientLight.a = 1.0f;
            m_scene->SetEnvironmentAmbientLight(ambientLight);
        }
        float ambientLightIntensity = m_scene->GetEnvironmentAmbientLightIntensity();
        if (ImGui::SliderFloat("Ambient Intensity##", &ambientLightIntensity, 0.0f, 8.0f, "%.2f"))
            m_scene->SetEnvironmentAmbientLightIntensity(ambientLightIntensity);
        ImGui::TextDisabled("Scene-wide fill light for darker areas.");

        ImGui::Separator();
        bool showColliders = m_scene->GetShowColliders();
        if (ImGui::Checkbox("Show Colliders", &showColliders))
            m_scene->SetShowColliders(showColliders);
        ImGui::TextDisabled("Draw physics collider wireframes for this scene.");
        ImGui::Separator();

        ImGui::Text("Skybox");
        ImGui::SameLine();

        UUID skyboxUUID = m_scene->GetEnvironmentSkyboxUUID();
        std::string skyboxLabel = "[ none ]";
        if ((uint64_t)skyboxUUID != 0)
        {
            const std::string skyboxPath = AssetManager::GetPath(skyboxUUID);
            if (skyboxPath != "Path was not found in AssetLibrary")
            {
                if (MetaFileAsset *meta = AssetManager::GetMetaFile(skyboxPath))
                    skyboxLabel = meta->name;
                else
                    skyboxLabel = skyboxPath;
            }
        }

        ImGui::Button(skyboxLabel.c_str(), ImVec2(180, 0));
        if (ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
            m_scene->SetEnvironmentSkyboxUUID(UUID(0));

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
            {
                const AssetDragData dropped = *static_cast<const AssetDragData *>(payload->Data);
                std::string path = AssetManager::GetPath(dropped.uuid);
                if (MetaFileAsset *meta = AssetManager::GetMetaFile(path))
                {
                    if (meta->type == MetaFileAsset::FileType::SKYBOX)
                        m_scene->SetEnvironmentSkyboxUUID(meta->uuid);
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopupContextItem("skybox_env_ctx"))
        {
            if (ImGui::MenuItem("Clear"))
                m_scene->SetEnvironmentSkyboxUUID(UUID(0));
            ImGui::EndPopup();
        }

        ImGui::Text("Post Process");
        ImGui::SameLine();

        UUID postProcessUUID = m_scene->GetEnvironmentPostProcessUUID();
        std::string postProcessLabel = "[ none ]";
        if ((uint64_t)postProcessUUID != 0)
        {
            const std::string postProcessPath = AssetManager::GetPath(postProcessUUID);
            if (postProcessPath != "Path was not found in AssetLibrary")
            {
                if (MetaFileAsset *meta = AssetManager::GetMetaFile(postProcessPath))
                    postProcessLabel = meta->name;
                else
                    postProcessLabel = postProcessPath;
            }
        }

        ImGui::Button(postProcessLabel.c_str(), ImVec2(180, 0));
        if (ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
            m_scene->SetEnvironmentPostProcessUUID(UUID(0));

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
            {
                const AssetDragData dropped = *static_cast<const AssetDragData *>(payload->Data);
                std::string path = AssetManager::GetPath(dropped.uuid);
                if (MetaFileAsset *meta = AssetManager::GetMetaFile(path))
                {
                    if (meta->type == MetaFileAsset::FileType::POSTPROCESS)
                        m_scene->SetEnvironmentPostProcessUUID(meta->uuid);
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopupContextItem("postprocess_env_ctx"))
        {
            if (ImGui::MenuItem("Clear"))
                m_scene->SetEnvironmentPostProcessUUID(UUID(0));
            ImGui::EndPopup();
        }

        ImGui::End();
    }

    void Editor::CommitAssetRename()
    {
        using namespace std;
        namespace fs = std::filesystem;

        if (!m_isRenamingAsset || m_renamingPath.empty())
            return;

        string newName = m_renameBuffer;
        fs::path oldPath = m_renamingPath;
        const bool renamingDirectory = fs::is_directory(oldPath);
        const std::string extension = renamingDirectory ? "" : oldPath.extension().string();

        // nothing entered, cancel
        if (newName.empty())
        {
            m_isRenamingAsset = false;
            m_focusAssetRenameInput = false;
            return;
        }

        fs::path newPath = oldPath;
        newPath.replace_filename(extension.empty() ? newName : newName + extension);

        // handle no change
        if (newPath == oldPath)
        {
            m_isRenamingAsset = false;
            m_focusAssetRenameInput = false;
            return;
        }

        if (renamingDirectory)
        {
            if (fs::exists(newPath))
            {
                Debug::Warning("Rename target already exists: %s", newPath.string().c_str());
            }
            else
            {
                std::error_code ec;
                fs::rename(oldPath, newPath, ec);
                if (ec)
                {
                    Debug::Warning("Failed to rename folder '%s' to '%s': %s", oldPath.string().c_str(), newPath.string().c_str(), ec.message().c_str());
                }
                else
                {
                    const std::string oldPrefix = oldPath.generic_string();
                    const std::string newPrefix = newPath.generic_string();

                    auto remapTrackedPath = [&](std::string &_path)
                    {
                        if (!_path.empty())
                            _path = RemapPathPrefix(fs::path(_path).generic_string(), oldPrefix, newPrefix);
                    };

                    remapTrackedPath(m_selectedAssetPath);
                    remapTrackedPath(m_animationClipStatePath);
                    remapTrackedPath(m_shaderGraphStatePath);

                    auto &assetLibrary = AssetManager::GetAssetLibrary();
                    std::vector<std::pair<std::string, int>> remappedAssetPaths = {};
                    for (const auto &[pathKey, assetId] : assetLibrary.assetPath)
                    {
                        const std::string remappedPath = RemapPathPrefix(fs::path(pathKey).generic_string(), oldPrefix, newPrefix);
                        if (remappedPath != pathKey)
                            remappedAssetPaths.push_back({ pathKey, assetId });
                    }

                    for (const auto &[oldKey, assetId] : remappedAssetPaths)
                    {
                        const std::string newKey = RemapPathPrefix(fs::path(oldKey).generic_string(), oldPrefix, newPrefix);
                        assetLibrary.assetPath.erase(oldKey);
                        assetLibrary.assetPath[newKey] = assetId;

                        if (newKey.size() > 5 && newKey.ends_with(".meta") && assetLibrary.assets.contains(assetId))
                        {
                            if (MetaFileAsset *meta = static_cast<MetaFileAsset *>(assetLibrary.assets[assetId]))
                            {
                                const std::string assetPath = newKey.substr(0, newKey.size() - 5);
                                meta->path = assetPath;
                                meta->name = fs::path(assetPath).filename().string();
                                meta->Save();
                            }
                        }
                    }

                    for (auto &[uuid, pathValue] : assetLibrary.uuidAssetPath)
                        pathValue = RemapPathPrefix(fs::path(pathValue).generic_string(), oldPrefix, newPrefix);

                    m_assetPaths = FindFilesInFolder("assets", "");
                }
            }
        }
        else if (AssetManager::MoveAsset(oldPath.string(), newPath.string()))
        {
            if (m_selectedAssetPath == oldPath.string())
                m_selectedAssetPath = newPath.string();
            if (m_animationClipStatePath == oldPath.string())
                m_animationClipStatePath = newPath.string();
            if (m_shaderGraphStatePath == oldPath.string())
                m_shaderGraphStatePath = newPath.string();
        }

        m_isRenamingAsset = false;
        m_focusAssetRenameInput = false;
        m_renamingPath.clear();
    }

    void Editor::CommitScriptFolderRename(const std::filesystem::path &_includeRoot, const std::filesystem::path &_sourceRoot)
    {
        namespace fs = std::filesystem;

        if (!m_isRenamingScriptFolder || m_scriptFolderRenamingPath.empty())
            return;

        const std::string newFolderName = m_scriptFolderRenameBuffer;
        if (newFolderName.empty())
        {
            m_isRenamingScriptFolder = false;
            m_focusScriptFolderRenameInput = false;
            m_scriptFolderRenameError.clear();
            m_scriptFolderRenamingPath.clear();
            return;
        }

        const fs::path oldIncludeFolderPath = m_scriptFolderRenamingPath;
        fs::path newIncludeFolderPath = {};
        std::string error = "";
        if (!RenameScriptFolderPair(_includeRoot, _sourceRoot, oldIncludeFolderPath, newFolderName, newIncludeFolderPath, error))
        {
            m_scriptFolderRenameError = error.empty() ? "Failed to rename script folder." : error;
            m_focusScriptFolderRenameInput = true;
            return;
        }

        fs::path selectedRelativePath = {};
        if (!m_selectedScriptPath.empty() && TryMakeLexicalRelative(fs::path(m_selectedScriptPath), oldIncludeFolderPath, selectedRelativePath))
            m_selectedScriptPath = (selectedRelativePath == "." ? newIncludeFolderPath : newIncludeFolderPath / selectedRelativePath).string();

        fs::path createTargetRelativePath = {};
        if (!m_scriptCreateTargetDir.empty() && TryMakeLexicalRelative(fs::path(m_scriptCreateTargetDir), oldIncludeFolderPath, createTargetRelativePath))
            m_scriptCreateTargetDir = (createTargetRelativePath == "." ? newIncludeFolderPath : newIncludeFolderPath / createTargetRelativePath).string();

        m_isRenamingScriptFolder = false;
        m_focusScriptFolderRenameInput = false;
        m_scriptFolderRenameError.clear();
        m_scriptFolderRenamingPath.clear();
    }

    void Editor::DrawAssetCreateMenu(const std::filesystem::path &_folderPath)
    {
        namespace fs = std::filesystem;

        if (!ImGui::BeginMenu("Create"))
            return;

        auto writeSceneAsset = [&](const fs::path &_targetPath, YAML::Node &_root) -> bool
        {
            std::ofstream out(_targetPath.string());
            if (!out.is_open())
            {
                Debug::Warning("Failed to create scene asset: %s", _targetPath.string().c_str());
                return false;
            }

            out << _root;
            out.close();

            (void)AssetManager::GetMetaFile(_targetPath.string());
            m_selectedAssetPath = _targetPath.string();
            return true;
        };

        auto makeDefaultEnvironment = []() -> YAML::Node
        {
            YAML::Node environment(YAML::NodeType::Map);
            environment["ClearColor"] = Vector4(0.12f, 0.14f, 0.17f, 1.0f);
            environment["AmbientLight"] = Vector4(0.24f, 0.26f, 0.32f, 1.0f);
            environment["AmbientLightIntensity"] = 1.0f;
            environment["ShowColliders"] = false;
            return environment;
        };

        if (ImGui::MenuItem("Folder"))
        {
            fs::path targetPath = _folderPath / "New Folder";
            int index = 1;
            while (fs::exists(targetPath))
            {
                targetPath = _folderPath / ("New Folder " + std::to_string(index));
                ++index;
            }

            std::error_code ec;
            fs::create_directory(targetPath, ec);
            if (ec)
                Debug::Warning("Failed to create folder '%s': %s", targetPath.string().c_str(), ec.message().c_str());
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Scene"))
        {
            fs::path targetPath = BuildUniqueAssetPath(_folderPath, "new_scene", ".scene");
            YAML::Node root(YAML::NodeType::Map);
            root["Environment"] = makeDefaultEnvironment();
            root["Entities"] = YAML::Node(YAML::NodeType::Sequence);
            (void)writeSceneAsset(targetPath, root);
        }

        if (ImGui::MenuItem("3D Scene"))
        {
            fs::path targetPath = BuildUniqueAssetPath(_folderPath, "new_3d_scene", ".scene");

            uint64_t defaultMaterialUUID = 0;
            if (MetaFileAsset *materialMeta = AssetManager::GetMetaFile("assets/defaults/materials/default.material"))
                defaultMaterialUUID = static_cast<uint64_t>(materialMeta->uuid);

            uint64_t cubeModelUUID = 0;
            if (MetaFileAsset *cubeMeta = AssetManager::GetMetaFile("assets/defaults/models/cube.glb"))
                cubeModelUUID = static_cast<uint64_t>(cubeMeta->uuid);

            YAML::Node root(YAML::NodeType::Map);
            root["Environment"] = makeDefaultEnvironment();
            YAML::Node entities(YAML::NodeType::Sequence);

            YAML::Node camera(YAML::NodeType::Map);
            camera["Entity"] = static_cast<uint64_t>(UUID());
            camera["Name"] = "Camera";
            camera["Tag"] = "MainCamera";
            camera["Active"] = true;
            YAML::Node cameraTransform(YAML::NodeType::Map);
            cameraTransform["active"] = true;
            cameraTransform["position"] = Vector3(0.0f, 2.0f, 6.0f);
            cameraTransform["rotation"] = Vector3(-0.32f, 0.0f, 0.0f);
            cameraTransform["scale"] = Vector3(1.0f);
            cameraTransform["parent"] = static_cast<uint64_t>(0);
            cameraTransform["children"] = YAML::Node(YAML::NodeType::Sequence);
            camera["Canis::Transform"] = cameraTransform;
            YAML::Node cameraComponent(YAML::NodeType::Map);
            cameraComponent["primary"] = true;
            cameraComponent["fovDegrees"] = 60.0f;
            cameraComponent["nearClip"] = 0.1f;
            cameraComponent["farClip"] = 300.0f;
            camera["Canis::Camera"] = cameraComponent;
            entities.push_back(camera);

            YAML::Node light(YAML::NodeType::Map);
            light["Entity"] = static_cast<uint64_t>(UUID());
            light["Name"] = "Directional Light";
            light["Tag"] = "";
            light["Active"] = true;
            YAML::Node lightComponent(YAML::NodeType::Map);
            lightComponent["enabled"] = true;
            lightComponent["color"] = Vector4(1.0f);
            lightComponent["intensity"] = 1.0f;
            lightComponent["direction"] = Vector3(-0.4f, -1.0f, -0.25f);
            light["Canis::DirectionalLight"] = lightComponent;
            entities.push_back(light);

            YAML::Node cube(YAML::NodeType::Map);
            cube["Entity"] = static_cast<uint64_t>(UUID());
            cube["Name"] = "Cube";
            cube["Tag"] = "";
            cube["Active"] = true;
            YAML::Node cubeTransform(YAML::NodeType::Map);
            cubeTransform["active"] = true;
            cubeTransform["position"] = Vector3(0.0f);
            cubeTransform["rotation"] = Vector3(0.0f);
            cubeTransform["scale"] = Vector3(1.0f);
            cubeTransform["parent"] = static_cast<uint64_t>(0);
            cubeTransform["children"] = YAML::Node(YAML::NodeType::Sequence);
            cube["Canis::Transform"] = cubeTransform;
            YAML::Node cubeMaterial(YAML::NodeType::Map);
            cubeMaterial["color"] = Vector4(1.0f);
            YAML::Node materialAsset(YAML::NodeType::Map);
            materialAsset["uuid"] = defaultMaterialUUID;
            cubeMaterial["MaterialAsset"] = materialAsset;
            cube["Canis::Material"] = cubeMaterial;
            YAML::Node cubeModel(YAML::NodeType::Map);
            cubeModel["color"] = Vector4(1.0f);
            YAML::Node modelAsset(YAML::NodeType::Map);
            modelAsset["uuid"] = cubeModelUUID;
            cubeModel["ModelAsset"] = modelAsset;
            cube["Canis::Model"] = cubeModel;
            entities.push_back(cube);

            root["Entities"] = entities;
            (void)writeSceneAsset(targetPath, root);
        }

        if (ImGui::MenuItem("2D Scene"))
        {
            const fs::path templatePath = "assets/defaults/templates/scenes/2d_scene.scene";
            const fs::path targetPath = BuildUniqueAssetPath(_folderPath, "new_2d_scene", ".scene");

            std::error_code ec;
            fs::copy_file(templatePath, targetPath, ec);
            if (ec)
            {
                Debug::Warning("Failed to create 2D scene from template: %s", targetPath.string().c_str());
            }
            else
            {
                (void)AssetManager::GetMetaFile(targetPath.string());
                m_selectedAssetPath = targetPath.string();
            }
        }

        if (ImGui::MenuItem("Material"))
        {
            const fs::path templatePath = "assets/defaults/materials/default.material";
            const fs::path targetPath = BuildUniqueAssetPath(_folderPath, "new_material", ".material");

            std::error_code ec;
            fs::copy_file(templatePath, targetPath, ec);
            if (ec)
            {
                std::ofstream out(targetPath.string());
                out << "color: [1, 1, 1, 1]\n";
                out << "backFaceCulling: true\n";
                out.close();
            }

            (void)AssetManager::GetMetaFile(targetPath.string());
            m_selectedAssetPath = targetPath.string();
        }

        if (ImGui::MenuItem("Shader (Model3D Copy)"))
        {
            fs::path basePath = _folderPath / "new_shader";
            fs::path vertexPath = basePath;
            fs::path fragmentPath = basePath;
            vertexPath.replace_extension(".vs");
            fragmentPath.replace_extension(".fs");

            int index = 1;
            while (fs::exists(vertexPath) || fs::exists(fragmentPath) ||
                   fs::exists(vertexPath.string() + ".meta") || fs::exists(fragmentPath.string() + ".meta"))
            {
                basePath = _folderPath / ("new_shader_" + std::to_string(index));
                vertexPath = basePath;
                fragmentPath = basePath;
                vertexPath.replace_extension(".vs");
                fragmentPath.replace_extension(".fs");
                ++index;
            }

            auto copyTemplateShader = [](const fs::path &_targetPath, const std::vector<fs::path> &_sourceCandidates) -> bool
            {
                std::error_code ec;
                for (const fs::path &candidate : _sourceCandidates)
                {
                    ec.clear();
                    if (!fs::exists(candidate, ec))
                        continue;

                    ec.clear();
                    fs::copy_file(candidate, _targetPath, ec);
                    if (!ec)
                        return true;
                }

                return false;
            };

            const bool copiedVertex = copyTemplateShader(vertexPath, { "assets/shaders/model3d.vs", "project/assets/shaders/model3d.vs" });
            const bool copiedFragment = copyTemplateShader(fragmentPath, { "assets/shaders/model3d.fs", "project/assets/shaders/model3d.fs" });

            if (!copiedVertex)
                Debug::Warning("Failed to create shader vertex file from model3d template: %s", vertexPath.string().c_str());
            if (!copiedFragment)
                Debug::Warning("Failed to create shader fragment file from model3d template: %s", fragmentPath.string().c_str());

            if (copiedVertex)
                (void)AssetManager::GetMetaFile(vertexPath.string());
            if (copiedFragment)
                (void)AssetManager::GetMetaFile(fragmentPath.string());
        }

        if (ImGui::MenuItem("Shader Graph"))
        {
            fs::path targetPath = BuildUniqueAssetPath(_folderPath, "new_shader_graph", ".shadergraph");

            ShaderGraphDocument document = MakeDefaultShaderGraphDocument();
            if (!SaveShaderGraphDocument(targetPath.string(), document))
            {
                Debug::Warning("Failed to create shader graph asset: %s", targetPath.string().c_str());
            }
            else
            {
                std::string errorMessage = {};
                if (!GenerateShaderGraphAssets(targetPath.string(), document, &errorMessage) && !errorMessage.empty())
                    Debug::Warning("%s", errorMessage.c_str());

                (void)AssetManager::GetMetaFile(targetPath.string());
                (void)AssetManager::GetMetaFile(GetShaderGraphGeneratedVertexPath(targetPath.string()));
                (void)AssetManager::GetMetaFile(GetShaderGraphGeneratedFragmentPath(targetPath.string()));
                (void)AssetManager::GetMetaFile(GetShaderGraphGeneratedMaterialPath(targetPath.string()));

                m_selectedAssetPath = targetPath.string();
                RememberLastShaderGraphAssetPath(m_selectedAssetPath);
                m_shaderGraphStatePath.clear();
                m_shaderGraphSelectedNodeId = -1;
            }
        }

        if (ImGui::MenuItem("Animation Clip"))
        {
            fs::path targetPath = BuildUniqueAssetPath(_folderPath, "new_animation", ".animclip");

            AnimationClipAsset clip = {};
            clip.length = 1.0f;
            if (clip.Save(targetPath.string()))
            {
                (void)AssetManager::GetMetaFile(targetPath.string());
                m_selectedAssetPath = targetPath.string();
                m_animationClipStatePath = targetPath.string();
                RememberLastAnimationClipAssetPath(targetPath.string());
            }
        }

        if (ImGui::MenuItem("Animator Controller"))
        {
            fs::path targetPath = BuildUniqueAssetPath(_folderPath, "new_animator", ".animator");

            AnimatorControllerAsset controller = {};
            controller.entryState = "Default";
            controller.states.push_back(AnimatorState{});
            controller.states.back().name = "Default";
            if (controller.Save(targetPath.string()))
            {
                (void)AssetManager::GetMetaFile(targetPath.string());
                m_selectedAssetPath = targetPath.string();
            }
        }

        if (ImGui::MenuItem("Skybox"))
        {
            fs::path targetPath = BuildUniqueAssetPath(_folderPath, "new_skybox", ".skybox");

            YAML::Node skyboxRoot(YAML::NodeType::Map);
            auto makeFaceRef = []() -> YAML::Node
            {
                YAML::Node node(YAML::NodeType::Map);
                node["uuid"] = static_cast<uint64_t>(0);
                return node;
            };
            skyboxRoot["right"] = makeFaceRef();
            skyboxRoot["left"] = makeFaceRef();
            skyboxRoot["top"] = makeFaceRef();
            skyboxRoot["bottom"] = makeFaceRef();
            skyboxRoot["front"] = makeFaceRef();
            skyboxRoot["back"] = makeFaceRef();

            std::ofstream out(targetPath.string());
            out << skyboxRoot;
            out.close();

            (void)AssetManager::GetMetaFile(targetPath.string());
            m_selectedAssetPath = targetPath.string();
        }

        if (ImGui::MenuItem("PostProcess"))
        {
            const fs::path templatePath = "assets/defaults/postprocess/default.postprocess";
            fs::path targetPath = BuildUniqueAssetPath(_folderPath, "new_postprocess", ".postprocess");

            std::error_code ec;
            fs::copy_file(templatePath, targetPath, ec);
            if (ec)
            {
                YAML::Node root(YAML::NodeType::Map);
                YAML::Node passes(YAML::NodeType::Sequence);
                root["passes"] = passes;
                std::ofstream out(targetPath.string());
                out << root;
                out.close();
            }

            (void)AssetManager::GetMetaFile(targetPath.string());
            m_selectedAssetPath = targetPath.string();
        }

        ImGui::EndMenu();
    }

    void Editor::DrawDirectoryRecursive(const std::string &_dirPath)
    {
        namespace fs = std::filesystem;
        fs::path path = _dirPath;
        const std::string searchQuery = NormalizeAssetSearchQuery(m_assetSearch);
        const bool searchActive = !searchQuery.empty();

        std::vector<fs::directory_entry> entries = {};
        for (const auto &entry : fs::directory_iterator(path))
            entries.push_back(entry);

        for (const auto &entry : entries)
        {
            const std::string name = entry.path().filename().string();
            if (ShouldHideAssetBrowserPath(entry.path()))
                continue;

            if (entry.is_directory())
            {
                const std::string fullPath = entry.path().string();
                if (searchActive && !AssetDirectoryContainsSearchMatch(entry.path(), searchQuery))
                    continue;

                const bool isRenamingThis = m_isRenamingAsset && (m_renamingPath == fullPath);
                if (isRenamingThis)
                {
                    ImGui::PushID(fullPath.c_str());
                    ImGui::SetNextItemWidth(-1.0f);

                    ImGuiInputTextFlags flags =
                        ImGuiInputTextFlags_EnterReturnsTrue |
                        ImGuiInputTextFlags_CharsNoBlank |
                        ImGuiInputTextFlags_CallbackAlways;

                    if (m_focusAssetRenameInput)
                        ImGui::SetKeyboardFocusHere();

                    if (ImGui::InputText("##rename_asset_folder", m_renameBuffer, sizeof(m_renameBuffer), flags, PlaceRenameCursorAtEnd, &m_focusAssetRenameInput))
                        CommitAssetRename();

                    if (!ImGui::IsItemActive() &&
                        (ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1)))
                    {
                        m_isRenamingAsset = false;
                        m_focusAssetRenameInput = false;
                    }

                    ImGui::PopID();
                    continue;
                }

                ImGuiTreeNodeFlags nodeFlags =
                    ImGuiTreeNodeFlags_OpenOnArrow |
                    ImGuiTreeNodeFlags_SpanAvailWidth;
                if (searchActive)
                    nodeFlags |= ImGuiTreeNodeFlags_DefaultOpen;

                bool open = ImGui::TreeNodeEx(entry.path().string().c_str(), nodeFlags, "%s", name.c_str());

                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ENTITY_DRAG"))
                    {
                        if (m_scene != nullptr && m_mode == EditorMode::EDIT)
                        {
                            const Canis::UUID droppedUUID = *static_cast<const Canis::UUID *>(payload->Data);
                            if (Canis::Entity *droppedEntity = m_scene->GetEntityWithUUID(droppedUUID))
                            {
                                std::string prefabPath = {};
                                if (ExportHierarchyEntityToPrefabAsset(*m_scene, *droppedEntity, entry.path(), prefabPath))
                                {
                                    const SceneAssetHandle prefabHandle = MakeSceneAssetHandleFromPath(prefabPath);
                                    AssignPrefabInstanceMetadata(droppedEntity, prefabHandle, droppedEntity);
                                    AssignPrefabHandle(droppedEntity, prefabHandle);
                                    m_selectedAssetPath = prefabPath;
                                }
                            }
                        }
                    }

                    if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
                    {
                        const AssetDragData *data = static_cast<const AssetDragData *>(payload->Data);

                        fs::path src = data->path;

                        const std::string targetPath = (entry.path() / src.filename()).string();
                        if (AssetManager::MoveAsset(src.string(), targetPath))
                        {
                            if (m_selectedAssetPath == src.string())
                                m_selectedAssetPath = targetPath;
                            if (m_animationClipStatePath == src.string())
                                m_animationClipStatePath = targetPath;
                            if (m_shaderGraphStatePath == src.string())
                                m_shaderGraphStatePath = targetPath;
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                // right click
                if (ImGui::BeginPopupContextItem())
                {
                    DrawAssetCreateMenu(entry.path());
                    ImGui::Separator();

                    if (ImGui::MenuItem("Rename"))
                    {
                        m_isRenamingAsset = true;
                        m_renamingPath = fullPath;
                        m_focusAssetRenameInput = true;

                        std::strncpy(m_renameBuffer, name.c_str(), sizeof(m_renameBuffer));
                        m_renameBuffer[sizeof(m_renameBuffer) - 1] = '\0';
                    }

                    ImGui::EndPopup();
                }

                if (open)
                {
                    DrawDirectoryRecursive(entry.path().string());
                    ImGui::TreePop();
                }
            }
            else if (entry.is_regular_file())
            {
                const std::string fullPath = entry.path().string();
                if (searchActive && !AssetPathMatchesSearch(entry.path(), searchQuery))
                    continue;

                const bool isRenamingThis = m_isRenamingAsset && (m_renamingPath == fullPath);

                if (isRenamingThis)
                {
                    const std::string extension = entry.path().extension().string();

                    // rename input
                    ImGui::PushID(fullPath.c_str());
                    if (!extension.empty())
                    {
                        const float extensionWidth = ImGui::CalcTextSize(extension.c_str()).x + ImGui::GetStyle().ItemSpacing.x;
                        ImGui::SetNextItemWidth(-extensionWidth);
                    }
                    else
                    {
                        ImGui::SetNextItemWidth(-1.0f);
                    }

                    ImGuiInputTextFlags flags =
                        ImGuiInputTextFlags_EnterReturnsTrue |
                        ImGuiInputTextFlags_CharsNoBlank |
                        ImGuiInputTextFlags_CallbackAlways;

                    if (m_focusAssetRenameInput)
                        ImGui::SetKeyboardFocusHere();

                    if (ImGui::InputText("##rename", m_renameBuffer, sizeof(m_renameBuffer), flags, PlaceRenameCursorAtEnd, &m_focusAssetRenameInput))
                    {
                        CommitAssetRename();
                    }

                    if (!extension.empty())
                    {
                        ImGui::SameLine(0.0f, 0.0f);
                        ImGui::TextUnformatted(extension.c_str());
                    }

                    // click elsewhere or escape will cancel
                    if (!ImGui::IsItemActive() &&
                        (ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1)))
                    {
                        m_isRenamingAsset = false;
                        m_focusAssetRenameInput = false;
                    }

                    ImGui::PopID();
                }
                else
                {
                    bool deleteThisAsset = false;
                    bool duplicateThisAsset = false;
                    const bool selected = (m_selectedAssetPath == fullPath);
                    const bool clicked = ImGui::Selectable(name.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns);
                    if (clicked)
                    {
                        if (MetaFileAsset *meta = AssetManager::GetMetaFile(fullPath))
                        {
                            if (meta->type == MetaFileAsset::FileType::MODEL ||
                                meta->type == MetaFileAsset::FileType::MATERIAL ||
                                meta->type == MetaFileAsset::FileType::SKYBOX ||
                                meta->type == MetaFileAsset::FileType::POSTPROCESS ||
                                meta->type == MetaFileAsset::FileType::SHADERGRAPH ||
                                meta->type == MetaFileAsset::FileType::ANIMATORCONTROLLER ||
                                meta->type == MetaFileAsset::FileType::ANIMATIONCLIP)
                            {
                                m_selectedAssetPath = fullPath;
                                if (meta->type == MetaFileAsset::FileType::SHADERGRAPH)
                                    RememberLastShaderGraphAssetPath(m_selectedAssetPath);
                                if (meta->type == MetaFileAsset::FileType::ANIMATIONCLIP)
                                {
                                    m_animationClipStatePath = fullPath;
                                    RememberLastAnimationClipAssetPath(fullPath);
                                }
                            }
                        }
                    }

                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        if (MetaFileAsset *meta = AssetManager::GetMetaFile(fullPath))
                        {
                            if (meta->type == MetaFileAsset::FileType::MODEL ||
                                meta->type == MetaFileAsset::FileType::MATERIAL ||
                                meta->type == MetaFileAsset::FileType::SKYBOX ||
                                meta->type == MetaFileAsset::FileType::POSTPROCESS ||
                                meta->type == MetaFileAsset::FileType::SHADERGRAPH ||
                                meta->type == MetaFileAsset::FileType::ANIMATORCONTROLLER ||
                                meta->type == MetaFileAsset::FileType::ANIMATIONCLIP)
                            {
                                m_selectedAssetPath = fullPath;
                                if (meta->type == MetaFileAsset::FileType::SHADERGRAPH)
                                    RememberLastShaderGraphAssetPath(m_selectedAssetPath);
                                if (meta->type == MetaFileAsset::FileType::ANIMATIONCLIP)
                                {
                                    m_animationClipStatePath = fullPath;
                                    RememberLastAnimationClipAssetPath(fullPath);
                                }
                            }
                        }
                    }

                    // right click
                    if (ImGui::BeginPopupContextItem())
                    {
                        if (MetaFileAsset *meta = AssetManager::GetMetaFile(fullPath))
                        {
                            if (meta->type == MetaFileAsset::FileType::MODEL)
                            {
                                const bool canExportMaterials = IsGltfModelAssetPath(fullPath);
                                if (!canExportMaterials)
                                    ImGui::BeginDisabled();

                                if (ImGui::MenuItem("Export Materials/Textures..."))
                                    RequestModelMaterialExport(fullPath);

                                if (!canExportMaterials)
                                    ImGui::EndDisabled();

                                ImGui::Separator();
                            }
                        }

                        if (ImGui::MenuItem("Duplicate"))
                            duplicateThisAsset = true;

                        if (ImGui::MenuItem("Rename"))
                        {
                            m_isRenamingAsset = true;
                            m_renamingPath = fullPath;
                            m_focusAssetRenameInput = true;

                            const std::string stemName = entry.path().stem().string();
                            std::strncpy(m_renameBuffer, stemName.c_str(), sizeof(m_renameBuffer));
                            m_renameBuffer[sizeof(m_renameBuffer) - 1] = '\0';
                        }

                        if (ImGui::MenuItem("Delete"))
                            deleteThisAsset = true;

                        ImGui::EndPopup();
                    }

                    if (duplicateThisAsset)
                    {
                        const fs::path duplicatePath = BuildDuplicateAssetPath(entry.path());
                        std::error_code copyError;
                        fs::copy_file(entry.path(), duplicatePath, copyError);

                        if (copyError)
                        {
                            Debug::Warning("Failed to duplicate asset '%s' to '%s': %s", fullPath.c_str(), duplicatePath.string().c_str(), copyError.message().c_str());
                        }
                        else
                        {
                            const std::string sourceMetaPath = fullPath + ".meta";
                            const std::string duplicateMetaPath = duplicatePath.string() + ".meta";
                            if (fs::exists(sourceMetaPath))
                            {
                                std::error_code metaCopyError;
                                fs::copy_file(sourceMetaPath, duplicateMetaPath, metaCopyError);
                                if (metaCopyError)
                                    Debug::Warning("Failed to duplicate meta file '%s' to '%s': %s", sourceMetaPath.c_str(), duplicateMetaPath.c_str(), metaCopyError.message().c_str());
                                else
                                    RefreshDuplicatedMetaFile(duplicatePath.string());
                            }

                            if (MetaFileAsset *duplicatedMeta = AssetManager::GetMetaFile(duplicatePath.string()))
                            {
                                if (duplicatedMeta->type == MetaFileAsset::FileType::MODEL ||
                                    duplicatedMeta->type == MetaFileAsset::FileType::MATERIAL ||
                                    duplicatedMeta->type == MetaFileAsset::FileType::SKYBOX ||
                                    duplicatedMeta->type == MetaFileAsset::FileType::POSTPROCESS ||
                                    duplicatedMeta->type == MetaFileAsset::FileType::SHADERGRAPH ||
                                    duplicatedMeta->type == MetaFileAsset::FileType::ANIMATORCONTROLLER ||
                                    duplicatedMeta->type == MetaFileAsset::FileType::ANIMATIONCLIP)
                                {
                                    m_selectedAssetPath = duplicatePath.string();
                                    if (duplicatedMeta->type == MetaFileAsset::FileType::SHADERGRAPH)
                                        RememberLastShaderGraphAssetPath(m_selectedAssetPath);
                                    if (duplicatedMeta->type == MetaFileAsset::FileType::ANIMATIONCLIP)
                                    {
                                        m_animationClipStatePath = duplicatePath.string();
                                        RememberLastAnimationClipAssetPath(duplicatePath.string());
                                    }
                                }
                            }
                        }

                        continue;
                    }

                    if (deleteThisAsset)
                    {
                        if (AssetManager::DeleteAsset(fullPath))
                        {
                            ClearRememberedAnimationClipAssetPathIfMatches(fullPath);
                            ClearRememberedShaderGraphAssetPathIfMatches(fullPath);
                            if (m_selectedAssetPath == fullPath)
                                m_selectedAssetPath.clear();
                            if (m_animationClipStatePath == fullPath)
                                m_animationClipStatePath.clear();
                            if (m_shaderGraphStatePath == fullPath)
                            {
                                m_shaderGraphStatePath.clear();
                                m_shaderGraphSelectedNodeId = -1;
                            }

                            if (m_renamingPath == fullPath)
                            {
                                m_isRenamingAsset = false;
                                m_focusAssetRenameInput = false;
                                m_renamingPath.clear();
                            }
                        }

                        continue;
                    }

                    // drag source (for moving + using UUID elsewhere)
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
                    {
                        MetaFileAsset *meta = AssetManager::GetMetaFile(entry.path().string());
                        if (meta)
                        {
                            AssetDragData data{};
                            data.uuid = meta->uuid;

                            std::string full = entry.path().string();
                            std::snprintf(data.path, sizeof(data.path), "%s", full.c_str());

                            ImGui::SetDragDropPayload("ASSET_DRAG", &data, sizeof(data));
                            ImGui::Text("Asset: %s", meta->name.c_str());
                        }
                        ImGui::EndDragDropSource();
                    }

                    // double-click handling (open scene / shader)
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        MetaFileAsset *meta = AssetManager::GetMetaFile(entry.path().string());
                        if (!meta)
                            continue;

                        if (meta->type == MetaFileAsset::FileType::SCENE && m_mode == EditorMode::EDIT)
                        {
                            Canis::GetEditorConfig().lastEditorScene = MakeSceneAssetHandleFromPath(meta->path);
                            Canis::SaveEditorConfig();
                            m_scene->Unload();
                            m_scene->Load(meta->path);
                            ResetSceneHistory();
                        }
                        else if (meta->type == MetaFileAsset::FileType::MODEL ||
                                 meta->type == MetaFileAsset::FileType::MATERIAL ||
                                 meta->type == MetaFileAsset::FileType::SKYBOX ||
                                 meta->type == MetaFileAsset::FileType::POSTPROCESS ||
                                 meta->type == MetaFileAsset::FileType::SHADERGRAPH ||
                                 meta->type == MetaFileAsset::FileType::ANIMATORCONTROLLER ||
                                 meta->type == MetaFileAsset::FileType::ANIMATIONCLIP)
                        {
                            m_selectedAssetPath = fullPath;
                            if (meta->type == MetaFileAsset::FileType::SHADERGRAPH)
                                RememberLastShaderGraphAssetPath(m_selectedAssetPath);
                            if (meta->type == MetaFileAsset::FileType::ANIMATIONCLIP)
                            {
                                m_animationClipStatePath = fullPath;
                                RememberLastAnimationClipAssetPath(fullPath);
                            }
                        }
                        else if ((meta->type == MetaFileAsset::FileType::FRAGMENT ||
                                  meta->type == MetaFileAsset::FileType::VERTEX) &&
                                 m_mode == EditorMode::EDIT)
                        {
                            OpenInVSCode(std::string(SDL_GetBasePath()) + meta->path);
                        }
                    }
                }
            }
        }
    }

    void Editor::DrawScriptDirectoryRecursive(const std::filesystem::path &_includeRoot, const std::filesystem::path &_currentDir, const std::filesystem::path &_sourceRoot)
    {
        namespace fs = std::filesystem;

        std::vector<fs::directory_entry> entries = {};
        for (const auto &entry : fs::directory_iterator(_currentDir))
            entries.push_back(entry);

        for (const auto &entry : entries)
        {
            if (ShouldHideScriptBrowserEntry(entry.path()))
                continue;

            const std::string name = entry.path().filename().string();

            if (entry.is_directory())
            {
                const std::string fullPath = entry.path().string();
                const bool isRenamingThis = m_isRenamingScriptFolder && (m_scriptFolderRenamingPath == fullPath);
                if (isRenamingThis)
                {
                    ImGui::PushID(fullPath.c_str());
                    ImGui::SetNextItemWidth(-1.0f);

                    ImGuiInputTextFlags flags =
                        ImGuiInputTextFlags_EnterReturnsTrue |
                        ImGuiInputTextFlags_CharsNoBlank |
                        ImGuiInputTextFlags_CallbackAlways;

                    if (m_focusScriptFolderRenameInput)
                        ImGui::SetKeyboardFocusHere();

                    if (ImGui::InputText("##rename_script_folder", m_scriptFolderRenameBuffer, sizeof(m_scriptFolderRenameBuffer), flags, PlaceRenameCursorAtEnd, &m_focusScriptFolderRenameInput))
                        CommitScriptFolderRename(_includeRoot, _sourceRoot);

                    if (!m_scriptFolderRenameError.empty())
                        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", m_scriptFolderRenameError.c_str());

                    if (!ImGui::IsItemActive() &&
                        (ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1)))
                    {
                        m_isRenamingScriptFolder = false;
                        m_focusScriptFolderRenameInput = false;
                        m_scriptFolderRenameError.clear();
                        m_scriptFolderRenamingPath.clear();
                    }

                    ImGui::PopID();
                    continue;
                }

                const ImGuiTreeNodeFlags nodeFlags =
                    ImGuiTreeNodeFlags_OpenOnArrow |
                    ImGuiTreeNodeFlags_SpanAvailWidth;
                const bool open = ImGui::TreeNodeEx(entry.path().string().c_str(), nodeFlags, "%s", name.c_str());

                if (ImGui::BeginPopupContextItem())
                {
                    if (ImGui::MenuItem("Rename Folder"))
                    {
                        m_isRenamingScriptFolder = true;
                        m_scriptFolderRenamingPath = fullPath;
                        m_scriptFolderRenameError.clear();
                        m_focusScriptFolderRenameInput = true;

                        std::snprintf(m_scriptFolderRenameBuffer, sizeof(m_scriptFolderRenameBuffer), "%s", name.c_str());
                    }

                    if (ImGui::MenuItem("Create Folder"))
                    {
                        const fs::path includeFolderPath = MakeUniqueDirectoryPath(entry.path(), "NewFolder");
                        const fs::path sourceFolderPath = _sourceRoot / fs::relative(includeFolderPath, _includeRoot);

                        std::error_code includeEc;
                        fs::create_directories(includeFolderPath, includeEc);
                        if (includeEc)
                        {
                            Debug::Warning("Failed to create script folder '%s': %s", includeFolderPath.string().c_str(), includeEc.message().c_str());
                        }
                        else
                        {
                            std::error_code sourceEc;
                            fs::create_directories(sourceFolderPath, sourceEc);
                            if (sourceEc)
                                Debug::Warning("Failed to create matching source folder '%s': %s", sourceFolderPath.string().c_str(), sourceEc.message().c_str());
                        }
                    }

                    if (ImGui::MenuItem("Create Script"))
                    {
                        m_scriptCreateTargetDir = entry.path().string();
                        std::snprintf(m_scriptCreateNameBuffer, sizeof(m_scriptCreateNameBuffer), "%s", "NewScript");
                        m_scriptCreateError.clear();
                        m_focusScriptCreateNameInput = true;
                        m_openScriptCreatePopup = true;
                    }

                    ImGui::EndPopup();
                }

                if (open)
                {
                    DrawScriptDirectoryRecursive(_includeRoot, entry.path(), _sourceRoot);
                    ImGui::TreePop();
                }
            }
            else if (entry.is_regular_file())
            {
                const std::string fullPath = entry.path().string();
                const bool selected = (m_selectedScriptPath == fullPath);
                if (ImGui::Selectable(name.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns))
                    m_selectedScriptPath = fullPath;

                bool deleteScript = false;
                if (ImGui::BeginPopupContextItem())
                {
                    if (ImGui::MenuItem("Delete"))
                        deleteScript = true;

                    ImGui::EndPopup();
                }

                if (deleteScript)
                {
                    std::string deleteError = "";
                    if (DeleteScriptPair(_includeRoot, _sourceRoot, entry.path(), deleteError))
                    {
                        if (m_selectedScriptPath == fullPath)
                            m_selectedScriptPath.clear();
                    }
                    else
                    {
                        Debug::Warning("Failed to delete script '%s': %s", fullPath.c_str(), deleteError.c_str());
                    }

                    continue;
                }

                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    m_selectedScriptPath = fullPath;
                    OpenInVSCode(fullPath);

                    const fs::path pairedPath = GetPairedScriptPath(_includeRoot, _sourceRoot, entry.path());
                    if (!pairedPath.empty() && pairedPath != entry.path() && fs::exists(pairedPath))
                        OpenInVSCode(pairedPath.string());
                }
            }
        }
    }

    void Editor::DrawScriptsPanel()
    {
        namespace fs = std::filesystem;

        ImGui::Begin("Scripts", &m_showScriptsPanel);

        const fs::path gameCodeRoot = FindGameCodeRoot();
        if (gameCodeRoot.empty())
        {
            ImGui::TextDisabled("Unable to locate game/include and game/src.");
            ImGui::End();
            return;
        }

        const fs::path includeRoot = gameCodeRoot / "game" / "include";
        const fs::path sourceRoot = gameCodeRoot / "game" / "src";

        ImGui::Text("Path: %s", includeRoot.string().c_str());
        ImGui::Separator();

        if (ImGui::BeginPopupContextWindow("scripts_root_ctx", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::MenuItem("Create Folder"))
            {
                const fs::path includeFolderPath = MakeUniqueDirectoryPath(includeRoot, "NewFolder");
                const fs::path sourceFolderPath = sourceRoot / fs::relative(includeFolderPath, includeRoot);

                std::error_code includeEc;
                fs::create_directories(includeFolderPath, includeEc);
                if (includeEc)
                {
                    Debug::Warning("Failed to create script folder '%s': %s", includeFolderPath.string().c_str(), includeEc.message().c_str());
                }
                else
                {
                    std::error_code sourceEc;
                    fs::create_directories(sourceFolderPath, sourceEc);
                    if (sourceEc)
                        Debug::Warning("Failed to create matching source folder '%s': %s", sourceFolderPath.string().c_str(), sourceEc.message().c_str());
                }
            }

            if (ImGui::MenuItem("Create Script"))
            {
                m_scriptCreateTargetDir = includeRoot.string();
                std::snprintf(m_scriptCreateNameBuffer, sizeof(m_scriptCreateNameBuffer), "%s", "NewScript");
                m_scriptCreateError.clear();
                m_focusScriptCreateNameInput = true;
                m_openScriptCreatePopup = true;
            }

            ImGui::EndPopup();
        }

        DrawScriptDirectoryRecursive(includeRoot, includeRoot, sourceRoot);

        if (m_openScriptCreatePopup)
        {
            ImGui::OpenPopup("Create Script");
            m_openScriptCreatePopup = false;
        }

        if (ImGui::BeginPopupModal("Create Script", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted("Choose content type and enter a class name.");
            ImGui::Separator();

            ImGui::Combo("Script Type", &m_scriptCreateTypeSelection, kGameScriptTypeLabels, IM_ARRAYSIZE(kGameScriptTypeLabels));
            if (m_focusScriptCreateNameInput)
            {
                ImGui::SetKeyboardFocusHere();
                m_focusScriptCreateNameInput = false;
            }

            const bool submitByEnter = ImGui::InputTextWithHint(
                "Script Name",
                "Example: PlayerController",
                m_scriptCreateNameBuffer,
                sizeof(m_scriptCreateNameBuffer),
                ImGuiInputTextFlags_EnterReturnsTrue);

            if (!m_scriptCreateError.empty())
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", m_scriptCreateError.c_str());

            const bool clickedCreate = ImGui::Button("Create");
            const bool shouldCreate = clickedCreate || submitByEnter;
            if (shouldCreate)
            {
                const std::string scriptName = std::string(m_scriptCreateNameBuffer);
                if (!IsValidCppIdentifier(scriptName))
                {
                    m_scriptCreateError = "Script name must be a valid C++ identifier.";
                }
                else
                {
                    fs::path targetDir = includeRoot;
                    if (!m_scriptCreateTargetDir.empty())
                        targetDir = fs::path(m_scriptCreateTargetDir);

                    std::error_code dirEc;
                    if (!fs::is_directory(targetDir, dirEc))
                        targetDir = includeRoot;

                    std::error_code relativeEc;
                    const fs::path relativeDir = fs::relative(targetDir, includeRoot, relativeEc);
                    std::string scriptTarget = scriptName;
                    if (!relativeEc && !relativeDir.empty() && relativeDir != ".")
                        scriptTarget = (relativeDir / scriptName).generic_string();

                    std::string createdHeaderPath = "";
                    std::string createdSourcePath = "";
                    std::string error = "";
                    const GameScriptType selectedType = GetGameScriptTypeFromSelection(m_scriptCreateTypeSelection);
                    if (CreateGameScriptFiles(gameCodeRoot, scriptTarget, selectedType, {}, false, createdHeaderPath, createdSourcePath, error))
                    {
                        m_selectedScriptPath = createdHeaderPath;
                        m_scriptCreateError.clear();
                        m_scriptCreateTargetDir.clear();
                        ImGui::CloseCurrentPopup();
                    }
                    else
                    {
                        m_scriptCreateError = error.empty() ? "Failed to create script files." : error;
                    }
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                m_scriptCreateError.clear();
                m_scriptCreateTargetDir.clear();
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        ImGui::End();
    }

    void Editor::DrawAssetsPanel()
    {
        ImGui::Begin("Assets", &m_showAssetsPanel);

        ImGui::SetNextItemWidth(std::max(120.0f, ImGui::GetContentRegionAvail().x - 72.0f));
        ImGui::InputTextWithHint("##AssetSearch", "Search assets...", &m_assetSearch);
        ImGui::SameLine();
        const bool hasSearch = !NormalizeAssetSearchQuery(m_assetSearch).empty();
        if (!hasSearch)
            ImGui::BeginDisabled();
        if (ImGui::Button("Clear"))
            m_assetSearch.clear();
        if (!hasSearch)
            ImGui::EndDisabled();

        ImGui::Separator();

        DrawDirectoryRecursive("assets");

        if (ImGui::BeginPopupContextWindow("assets_root_ctx", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            DrawAssetCreateMenu("assets");
            ImGui::EndPopup();
        }

        ImGui::End();
    }

    void Editor::DrawSystemPanel()
    {
        ImGui::Begin("Systems", &m_showSystemsPanel);

        if (m_scene == nullptr)
        {
            ImGui::TextUnformatted("No active scene.");
            ImGui::End();
            return;
        }

        const std::vector<Scene::SystemTiming>& timings = m_scene->GetSystemTimings();
        if (timings.empty())
        {
            ImGui::TextUnformatted("No systems registered.");
            ImGui::End();
            return;
        }

        float totalUpdateMs = 0.0f;
        float totalRenderMs = 0.0f;

        if (ImGui::BeginTable("SystemTimingTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn("System");
            ImGui::TableSetupColumn("Update (ms)");
            ImGui::TableSetupColumn("Render (ms)");
            ImGui::TableSetupColumn("Total (ms)");
            ImGui::TableHeadersRow();

            for (const Scene::SystemTiming& timing : timings)
            {
                const float totalMs = timing.updateMs + timing.renderMs;
                totalUpdateMs += timing.updateMs;
                totalRenderMs += timing.renderMs;

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", timing.name.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.3f", timing.updateMs);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.3f", timing.renderMs);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%.3f", totalMs);
            }

            ImGui::EndTable();
        }

        ImGui::Separator();
        ImGui::Text("Update Total: %.3f ms", totalUpdateMs);
        ImGui::Text("Render Total: %.3f ms", totalRenderMs);
        ImGui::Text("Frame System Total: %.3f ms", totalUpdateMs + totalRenderMs);

        ImGui::End();
    }

    void Editor::DrawConsolePanel()
    {
        if (!ImGui::Begin("Canis Console", &m_showConsolePanel))
        {
            ImGui::End();
            return;
        }

        const std::vector<Canis::Debug::LogEntry> entries = Canis::Debug::GetEntries();
        const std::string lowerSearch = NormalizeAssetSearchQuery(m_consoleSearch);

        int visibleCount = 0;
        int logCount = 0;
        int warningCount = 0;
        int errorCount = 0;
        int fatalCount = 0;
        int matchedEntryCount = 0;
        std::vector<ConsoleDisplayEntry> displayEntries = {};
        for (const Canis::Debug::LogEntry &entry : entries)
        {
            switch (entry.level)
            {
                case Canis::Debug::LogLevel::Warning: ++warningCount; break;
                case Canis::Debug::LogLevel::Error: ++errorCount; break;
                case Canis::Debug::LogLevel::Fatal: ++fatalCount; break;
                case Canis::Debug::LogLevel::Log:
                default: ++logCount; break;
            }

            if (ConsoleLogLevelEnabled(entry.level, m_consoleShowLogs, m_consoleShowWarnings, m_consoleShowErrors, m_consoleShowFatal) &&
                ConsoleEntryMatchesSearch(entry, lowerSearch))
            {
                ++matchedEntryCount;

                if (m_consoleCollapseDuplicates)
                {
                    auto duplicate = std::find_if(displayEntries.begin(), displayEntries.end(), [&entry](const ConsoleDisplayEntry &_displayEntry)
                    {
                        return ConsoleEntriesAreDuplicates(_displayEntry.entry, entry);
                    });

                    if (duplicate != displayEntries.end())
                    {
                        ++duplicate->count;
                        continue;
                    }
                }

                displayEntries.push_back(ConsoleDisplayEntry{ .entry = entry, .count = 1 });
            }
        }
        visibleCount = static_cast<int>(displayEntries.size());

        ImGui::SetNextItemWidth(std::max(180.0f, ImGui::GetContentRegionAvail().x - 280.0f));
        ImGui::InputTextWithHint("##ConsoleSearch", "Search logs...", &m_consoleSearch);
        ImGui::SameLine();
        const bool hasSearch = !lowerSearch.empty();
        if (!hasSearch)
            ImGui::BeginDisabled();
        if (ImGui::Button("Clear Search"))
            m_consoleSearch.clear();
        if (!hasSearch)
            ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Clear Logs"))
        {
            Canis::Debug::ClearEntries();
            m_consoleLastEntryId = 0u;
        }

        ImGui::Checkbox(("Log (" + std::to_string(logCount) + ")").c_str(), &m_consoleShowLogs);
        ImGui::SameLine();
        ImGui::Checkbox(("Warning (" + std::to_string(warningCount) + ")").c_str(), &m_consoleShowWarnings);
        ImGui::SameLine();
        ImGui::Checkbox(("Error (" + std::to_string(errorCount) + ")").c_str(), &m_consoleShowErrors);
        ImGui::SameLine();
        ImGui::Checkbox(("Fatal (" + std::to_string(fatalCount) + ")").c_str(), &m_consoleShowFatal);
        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &m_consoleAutoScroll);
        ImGui::SameLine();
        ImGui::Checkbox("Collapse Duplicates", &m_consoleCollapseDuplicates);
        ImGui::SameLine();
        if (m_consoleCollapseDuplicates)
            ImGui::TextDisabled("%d rows / %d matching / %zu total", visibleCount, matchedEntryCount, entries.size());
        else
            ImGui::TextDisabled("%d / %zu", visibleCount, entries.size());

        ImGui::Separator();

        const uint64_t newestEntryId = entries.empty() ? 0u : entries.back().id;
        const bool hasNewEntries = newestEntryId != 0u && newestEntryId != m_consoleLastEntryId;

        ImGui::BeginChild("##ConsoleLogBody", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);
        if (ImGui::BeginTable(
            "ConsoleLogTable",
            5,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollX))
        {
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 84.0f);
            ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 64.0f);
            ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("File", ImGuiTableColumnFlags_WidthFixed, 220.0f);
            ImGui::TableSetupColumn("Line", ImGuiTableColumnFlags_WidthFixed, 64.0f);
            ImGui::TableHeadersRow();

            for (const ConsoleDisplayEntry &displayEntry : displayEntries)
            {
                const Canis::Debug::LogEntry &entry = displayEntry.entry;

                const std::string openTarget = MakeConsoleOpenTarget(entry);
                const std::string displayPath = MakeConsoleSourceDisplayPath(entry.file);
                const bool canOpenSource = !openTarget.empty();

                ImGui::PushID(static_cast<int>(entry.id));
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                ImGui::PushStyleColor(ImGuiCol_Text, ConsoleLogLevelColor(entry.level));
                const bool rowClicked = ImGui::Selectable(
                    Canis::Debug::LogLevelName(entry.level),
                    false,
                    ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap);
                ImGui::PopStyleColor();

                if (rowClicked && canOpenSource)
                    OpenInVSCode(openTarget);

                if (ImGui::IsItemHovered())
                {
                    if (canOpenSource)
                        ImGui::SetTooltip("%s\n%s", entry.message.c_str(), openTarget.c_str());
                    else
                    ImGui::SetTooltip("%s", entry.message.c_str());
                }

                ImGui::TableSetColumnIndex(1);
                if (displayEntry.count > 1)
                    ImGui::Text("x%d", displayEntry.count);
                else
                    ImGui::TextUnformatted("1");

                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(entry.message.c_str());

                ImGui::TableSetColumnIndex(3);
                if (!displayPath.empty())
                    ImGui::TextUnformatted(displayPath.c_str());
                else
                    ImGui::TextDisabled("-");

                ImGui::TableSetColumnIndex(4);
                if (entry.line > 0)
                    ImGui::Text("%d", entry.line);
                else
                    ImGui::TextDisabled("-");

                ImGui::PopID();
            }

            ImGui::EndTable();
        }

        if (m_consoleAutoScroll && hasNewEntries)
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndChild();
        m_consoleLastEntryId = newestEntryId;
        ImGui::End();
    }

    std::string Editor::ResolveRememberedShaderGraphPath() const
    {
        const ShaderGraphAssetHandle &remembered = Canis::GetEditorConfig().lastShaderGraph;
        if (remembered.Empty())
            return {};

        const std::string resolvedPath = AssetManager::ResolvePath(remembered);
        if (resolvedPath.empty())
            return {};

        if (MetaFileAsset *meta = AssetManager::GetMetaFile(resolvedPath))
        {
            if (meta->type == MetaFileAsset::FileType::SHADERGRAPH)
                return meta->path;
        }

        return {};
    }

    std::string Editor::ResolveRememberedAnimationClipPath() const
    {
        const AnimationClipAssetHandle &remembered = Canis::GetEditorConfig().lastAnimationClip;
        if (remembered.Empty())
            return {};

        const std::string resolvedPath = AssetManager::ResolvePath(remembered);
        if (!resolvedPath.empty())
        {
            if (MetaFileAsset *meta = AssetManager::GetMetaFile(resolvedPath))
            {
                if (meta->type == MetaFileAsset::FileType::ANIMATIONCLIP)
                    return meta->path;
            }
        }

        return {};
    }

    void Editor::RememberLastAnimationClipAssetPath(const std::string &_path)
    {
        const AnimationClipAssetHandle handle = MakeAnimationClipAssetHandleFromPath(_path);
        if (handle.Empty())
            return;

        AnimationClipAssetHandle &remembered = Canis::GetEditorConfig().lastAnimationClip;
        if (!AnimationClipAssetHandleChanged(remembered, handle))
            return;

        remembered = handle;
        Canis::SaveEditorConfig();
    }

    void Editor::ClearRememberedAnimationClipAssetPathIfMatches(const std::string &_path)
    {
        AnimationClipAssetHandle &remembered = Canis::GetEditorConfig().lastAnimationClip;
        if (remembered.Empty() || _path.empty())
            return;

        bool matches = false;
        if (MetaFileAsset *meta = AssetManager::GetMetaFile(_path))
        {
            matches = meta->type == MetaFileAsset::FileType::ANIMATIONCLIP &&
                remembered.uuid != UUID(0) &&
                meta->uuid == remembered.uuid;
        }

        if (!matches)
            matches = ResolveRememberedAnimationClipPath() == _path;

        if (!matches)
            return;

        remembered = {};
        Canis::SaveEditorConfig();
    }

    void Editor::RememberLastShaderGraphAssetPath(const std::string &_path)
    {
        const ShaderGraphAssetHandle handle = MakeShaderGraphAssetHandleFromPath(_path);
        if (handle.Empty())
            return;

        ShaderGraphAssetHandle &remembered = Canis::GetEditorConfig().lastShaderGraph;
        if (!ShaderGraphAssetHandleChanged(remembered, handle))
            return;

        remembered = handle;
        Canis::SaveEditorConfig();
    }

    void Editor::ClearRememberedShaderGraphAssetPathIfMatches(const std::string &_path)
    {
        ShaderGraphAssetHandle &remembered = Canis::GetEditorConfig().lastShaderGraph;
        if (remembered.Empty() || _path.empty())
            return;

        bool matches = false;
        if (MetaFileAsset *meta = AssetManager::GetMetaFile(_path))
        {
            matches = meta->type == MetaFileAsset::FileType::SHADERGRAPH &&
                remembered.uuid != UUID(0) &&
                meta->uuid == remembered.uuid;
        }

        if (!matches)
            matches = ResolveRememberedShaderGraphPath() == _path;

        if (!matches)
            return;

        remembered = {};
        Canis::SaveEditorConfig();
    }

    void Editor::ApplyEditorTheme(int _theme)
    {
        m_editorThemeSelection = NormalizeEditorThemeSelection(_theme);
        ApplyEditorThemeStyle(m_editorThemeSelection, m_editorUiScale);

        ImGuiStyle &style = ImGui::GetStyle();
        style.FontScaleDpi = m_editorUiScale;

        Canis::GetEditorConfig().theme = m_editorThemeSelection;
    }

    void Editor::RefreshEditorFontOptions()
    {
        namespace fs = std::filesystem;

        m_editorFontPaths.clear();
        m_editorFontLabels.clear();
        m_editorFontPaths.push_back("");
        m_editorFontLabels.push_back("Default");

        const fs::path runtimeBasePath = GetEditorRuntimeBasePath();
        const std::vector<fs::path> fontRoots =
        {
            runtimeBasePath / "assets" / "fonts",
            runtimeBasePath / "project" / "assets" / "fonts",
            fs::path("assets") / "fonts",
            fs::path("project") / "assets" / "fonts"
        };

        std::vector<std::string> discoveredFontPaths = {};
        std::unordered_set<std::string> seenFontPaths = {};
        std::error_code ec;

        for (const fs::path &fontRoot : fontRoots)
        {
            ec.clear();
            if (!fs::exists(fontRoot, ec) || !fs::is_directory(fontRoot, ec))
                continue;

            fs::recursive_directory_iterator it(fontRoot, fs::directory_options::skip_permission_denied, ec);
            fs::recursive_directory_iterator end = {};
            while (it != end)
            {
                if (ec)
                {
                    ec.clear();
                    it.increment(ec);
                    continue;
                }

                const fs::directory_entry entry = *it;
                std::error_code fileEc;
                if (entry.is_regular_file(fileEc) && !fileEc && HasSupportedFontExtension(entry.path()))
                {
                    std::string storedPath = entry.path().generic_string();

                    std::error_code relEc;
                    const fs::path relativeToRuntime = fs::relative(entry.path(), runtimeBasePath, relEc);
                    if (!relEc && !relativeToRuntime.empty())
                    {
                        const auto firstPart = relativeToRuntime.begin();
                        if (firstPart == relativeToRuntime.end() || firstPart->string() != "..")
                            storedPath = relativeToRuntime.generic_string();
                    }

                    if (seenFontPaths.insert(storedPath).second)
                        discoveredFontPaths.push_back(storedPath);
                }

                it.increment(ec);
            }
        }

        std::sort(discoveredFontPaths.begin(), discoveredFontPaths.end());
        for (const std::string &fontPath : discoveredFontPaths)
        {
            m_editorFontPaths.push_back(fontPath);

            std::string label = fontPath;
            constexpr const char* kAssetsPrefix = "assets/fonts/";
            constexpr const char* kProjectAssetsPrefix = "project/assets/fonts/";
            if (label.rfind(kAssetsPrefix, 0) == 0)
                label = label.substr(std::char_traits<char>::length(kAssetsPrefix));
            else if (label.rfind(kProjectAssetsPrefix, 0) == 0)
                label = label.substr(std::char_traits<char>::length(kProjectAssetsPrefix));

            m_editorFontLabels.push_back(label);
        }

        m_editorFontSelection = 0;
        const std::string configuredFontPath = Canis::GetEditorConfig().fontPath;
        for (size_t i = 1; i < m_editorFontPaths.size(); ++i)
        {
            if (m_editorFontPaths[i] == configuredFontPath)
            {
                m_editorFontSelection = static_cast<int>(i);
                break;
            }
        }
    }

    void Editor::ApplyEditorFont(const std::string &_fontPath)
    {
        ImGuiIO &io = ImGui::GetIO();
        io.Fonts->Clear();
        io.FontDefault = nullptr;

        const std::filesystem::path resolvedFontPath = ResolveEditorFontPath(_fontPath);
        m_editorFontScale = NormalizeEditorFontScale(m_editorFontScale);
        Canis::GetEditorConfig().fontScale = m_editorFontScale;

        const float editorFontSize = 18.0f * m_editorFontScale;
        ImFont *font = nullptr;
        ImFontConfig defaultFontConfig = {};
        defaultFontConfig.SizePixels = editorFontSize;

        if (!resolvedFontPath.empty())
            font = io.Fonts->AddFontFromFileTTF(resolvedFontPath.string().c_str(), editorFontSize);

        if (font == nullptr)
        {
            font = io.Fonts->AddFontDefault(&defaultFontConfig);
            Canis::GetEditorConfig().fontPath.clear();
            m_editorFontSelection = 0;
        }
        else
        {
            Canis::GetEditorConfig().fontPath = _fontPath;
        }

        io.FontDefault = font;

        // Keep runtime text size in sync with the newly selected/rebuilt font.
        ImGuiStyle &style = ImGui::GetStyle();
        style.FontSizeBase = (font != nullptr && font->LegacySize > 0.0f) ? font->LegacySize : editorFontSize;
    }

    void Editor::QueueEditorFontApply(const std::string &_fontPath, bool _saveConfig)
    {
        m_queuedEditorFontPath = _fontPath;
        m_editorFontApplyQueued = true;
        m_editorFontApplyShouldSaveConfig = m_editorFontApplyShouldSaveConfig || _saveConfig;
    }

    void Editor::DrawProjectSettings()
    {
        ImGui::Begin("ProjectSettings", &m_showProjectSettingsPanel);

        if (ImGui::Button("Save Project", ImVec2(-1.0f, 0.0f)))
        {
            Canis::SaveProjectConfig();
            Canis::SaveEditorConfig();
        }

        bool editorEnabled = Canis::GetProjectConfig().editor;
        ImGui::Text("editor mode");
        ImGui::SameLine();
        if (ImGui::Checkbox("##editorEnabled", &editorEnabled))
        {
            Canis::GetProjectConfig().editor = editorEnabled;
            Canis::SaveProjectConfig();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(restart required)");

        static const char* editorThemeLabels[] = { "Dark", "Light" };
        int editorThemeSelection = m_editorThemeSelection;
        ImGui::Text("editor theme");
        ImGui::SameLine();
        if (ImGui::Combo("##editorTheme", &editorThemeSelection, editorThemeLabels, IM_ARRAYSIZE(editorThemeLabels)))
        {
            ApplyEditorTheme(editorThemeSelection);
            Canis::SaveEditorConfig();
        }

        ImGui::Text("editor font");
        ImGui::SameLine();
        const char* fontPreview =
            (m_editorFontSelection >= 0 && m_editorFontSelection < static_cast<int>(m_editorFontLabels.size())) ?
            m_editorFontLabels[m_editorFontSelection].c_str() : "Default";
        if (ImGui::BeginCombo("##editorFont", fontPreview))
        {
            for (int i = 0; i < static_cast<int>(m_editorFontLabels.size()); ++i)
            {
                const bool selected = (i == m_editorFontSelection);
                if (ImGui::Selectable(m_editorFontLabels[i].c_str(), selected))
                {
                    m_editorFontSelection = i;
                    const std::string selectedPath =
                        (i >= 0 && i < static_cast<int>(m_editorFontPaths.size())) ? m_editorFontPaths[i] : std::string();
                    QueueEditorFontApply(selectedPath, true);
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("rescan##editorFont"))
            RefreshEditorFontOptions();

        float editorFontScale = m_editorFontScale;
        ImGui::Text("editor font scale");
        ImGui::SameLine();
        if (ImGui::SliderFloat("##editorFontScale", &editorFontScale, 0.5f, 2.5f, "%.2fx"))
            m_editorFontScale = NormalizeEditorFontScale(editorFontScale);

        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            const std::string selectedPath =
                (m_editorFontSelection >= 0 && m_editorFontSelection < static_cast<int>(m_editorFontPaths.size())) ?
                m_editorFontPaths[m_editorFontSelection] : std::string();
            QueueEditorFontApply(selectedPath, true);
        }

        SceneAssetHandle launchScene = Canis::GetProjectConfig().launchScene;
        InputSceneAsset("launch scene", "##launchScene", launchScene);
        if (SceneAssetHandleChanged(launchScene, Canis::GetProjectConfig().launchScene))
        {
            Canis::GetProjectConfig().launchScene = launchScene;
            Canis::SaveProjectConfig();
        }

        std::string &launchExecutablePath = Canis::GetProjectConfig().launchExecutablePath;
        ImGui::Text("launch executable");
        ImGui::SameLine();
        if (ImGui::InputText("##launchExecutablePath", &launchExecutablePath))
            Canis::SaveProjectConfig();

        std::string &launchWorkingDirectory = Canis::GetProjectConfig().launchWorkingDirectory;
        ImGui::Text("launch working dir");
        ImGui::SameLine();
        if (ImGui::InputText("##launchWorkingDirectory", &launchWorkingDirectory))
            Canis::SaveProjectConfig();

        std::string &launchArguments = Canis::GetProjectConfig().launchArguments;
        ImGui::Text("launch arguments");
        ImGui::SameLine();
        if (ImGui::InputText("##launchArguments", &launchArguments))
            Canis::SaveProjectConfig();

        ImGui::TextDisabled(
            "Launch runs a separate no-editor process. Empty executable defaults to %s",
            GetDefaultStandaloneExecutablePath().generic_string().c_str());

        int targetGameWidth = Canis::GetProjectConfig().targetGameWidth;
        int targetGameHeight = Canis::GetProjectConfig().targetGameHeight;
        ImGui::Text("target game width");
        ImGui::SameLine();
        if (ImGui::InputInt("##targetGameWidth", &targetGameWidth, 0))
        {
            Canis::GetProjectConfig().targetGameWidth = std::max(1, targetGameWidth);
            Canis::SaveProjectConfig();
        }

        ImGui::Text("target game height");
        ImGui::SameLine();
        if (ImGui::InputInt("##targetGameHeight", &targetGameHeight, 0))
        {
            Canis::GetProjectConfig().targetGameHeight = std::max(1, targetGameHeight);
            Canis::SaveProjectConfig();
        }

        // display sync mode
        static const char* syncLabels[] = { "VSync On", "Sync Off", "Adaptive Sync" };
        int syncIndex = 1;
        switch (Canis::GetProjectConfig().syncMode)
        {
            case PROJECT_SYNC_VSYNC: syncIndex = 0; break;
            case PROJECT_SYNC_ADAPTIVE: syncIndex = 2; break;
            case PROJECT_SYNC_OFF:
            default: syncIndex = 1; break;
        }

        ImGui::Text("display sync");
        ImGui::SameLine();
        if (ImGui::Combo("##displaySync", &syncIndex, syncLabels, IM_ARRAYSIZE(syncLabels)))
        {
            static const int indexToSyncMode[] = { PROJECT_SYNC_VSYNC, PROJECT_SYNC_OFF, PROJECT_SYNC_ADAPTIVE };
            Canis::GetProjectConfig().syncMode = indexToSyncMode[syncIndex];
            m_window->SetSync(static_cast<Window::Sync>(Canis::GetProjectConfig().syncMode));
            Canis::SaveProjectConfig();
        }

        bool mute = Canis::GetProjectConfig().mute;
        ImGui::Text("mute audio");
        ImGui::SameLine();
        if (ImGui::Checkbox("##muteAudio", &mute))
        {
            Canis::GetProjectConfig().mute = mute;
            if (mute)
                AudioManager::Mute();
            else
                AudioManager::UnMute();
            Canis::SaveProjectConfig();
        }

        float masterVolume = Canis::GetProjectConfig().volume;
        ImGui::Text("master volume");
        ImGui::SameLine();
        if (ImGui::SliderFloat("##masterVolume", &masterVolume, 0.0f, 1.0f, "%.2f"))
        {
            Canis::GetProjectConfig().volume = std::clamp(masterVolume, 0.0f, 1.0f);
            AudioManager::RefreshMixLevels();
            Canis::SaveProjectConfig();
        }

        float musicVolume = Canis::GetProjectConfig().musicVolume;
        ImGui::Text("music volume");
        ImGui::SameLine();
        if (ImGui::SliderFloat("##musicVolume", &musicVolume, 0.0f, 1.0f, "%.2f"))
        {
            Canis::GetProjectConfig().musicVolume = std::clamp(musicVolume, 0.0f, 1.0f);
            AudioManager::RefreshMixLevels();
            Canis::SaveProjectConfig();
        }

        float sfxVolume = Canis::GetProjectConfig().sfxVolume;
        ImGui::Text("sfx volume");
        ImGui::SameLine();
        if (ImGui::SliderFloat("##sfxVolume", &sfxVolume, 0.0f, 1.0f, "%.2f"))
        {
            Canis::GetProjectConfig().sfxVolume = std::clamp(sfxVolume, 0.0f, 1.0f);
            AudioManager::RefreshMixLevels();
            Canis::SaveProjectConfig();
        }

        // fps limit checkbox
        ImGui::Text("in-game fps limit");
        ImGui::SameLine();
        if (ImGui::Checkbox("##useFPSLimit", &Canis::GetProjectConfig().useFrameLimit) && m_mode == EditorMode::PLAY)
        {
            if (Canis::GetProjectConfig().useFrameLimit)
                Time::SetTargetFPS(Canis::GetProjectConfig().frameLimit + 0.0f);
            else
                Time::SetTargetFPS(100000.0f);
        }

        // fps limit input
        if (Canis::GetProjectConfig().useFrameLimit)
        {
            ImGui::Text("    fps limit");
            ImGui::SameLine();
            if (ImGui::InputInt("##frameLimit", &Canis::GetProjectConfig().frameLimit, 0) && m_mode == EditorMode::PLAY)
                Time::SetTargetFPS(Canis::GetProjectConfig().frameLimit + 0.0f);
        }

        // editor fps limit input
        ImGui::Text("editor fps");
        ImGui::SameLine();
        if (ImGui::InputInt("##editorframeLimit", &Canis::GetProjectConfig().frameLimitEditor, 0) && m_mode == EditorMode::EDIT)
            Time::SetTargetFPS(Canis::GetProjectConfig().frameLimitEditor + 0.0f);

        // application icon
        ImGui::Text("icon");
        ImGui::SameLine();
        ImGui::Button(
            AssetManager::GetMetaFile(AssetManager::GetPath(Canis::GetProjectConfig().iconUUID))->name.c_str(),
            ImVec2(150, 0));

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
            {
                const AssetDragData dropped = *static_cast<const AssetDragData *>(payload->Data);
                std::string path = AssetManager::GetPath(dropped.uuid);
                TextureAsset *asset = AssetManager::GetTexture(path);

                if (asset) // validate that this is a texture
                {
                    Canis::GetProjectConfig().iconUUID = dropped.uuid;
                    Canis::SaveProjectConfig();
                    m_window->SetWindowIcon(path);
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::End();
    }

    void Editor::FinalizeReloadBuildIfReady()
    {
        bool shouldFinalizeReload = false;
        bool buildSucceeded = false;
        int buildExitCode = -1;
        {
            std::scoped_lock lock(m_reloadBuildMutex);
            if (m_reloadBuildAwaitingFinalize && m_reloadBuildFinished)
            {
                shouldFinalizeReload = true;
                buildSucceeded = m_reloadBuildSucceeded;
                buildExitCode = m_reloadBuildExitCode;
                m_reloadBuildAwaitingFinalize = false;
            }
        }

        if (!shouldFinalizeReload)
            return;

        if (m_reloadBuildThread.joinable())
            m_reloadBuildThread.join();

        if (!buildSucceeded)
            Debug::Warning("GameCode build failed (exit code: %d). Attempting to reload existing game library.", buildExitCode);

        if (!buildSucceeded && !m_reloadBuildBackupPath.empty())
        {
            std::error_code ec;
            const std::string backupPath = m_reloadBuildBackupPath;
            const std::string backupSuffix = ".reload.bak";
            std::string livePath = backupPath;
            if (livePath.size() > backupSuffix.size() &&
                livePath.compare(livePath.size() - backupSuffix.size(), backupSuffix.size(), backupSuffix) == 0)
            {
                livePath.erase(livePath.size() - backupSuffix.size());
                std::filesystem::remove(livePath, ec);
                ec.clear();
                std::filesystem::rename(backupPath, livePath, ec);
                if (ec)
                    Debug::Warning("Failed to restore previous game shared library after build failure: %s", ec.message().c_str());
                else
                    m_reloadBuildBackupPath.clear();
            }
        }

        if (buildSucceeded && !m_reloadBuildBackupPath.empty())
        {
            std::error_code ec;
            std::filesystem::remove(m_reloadBuildBackupPath, ec);
            m_reloadBuildBackupPath.clear();
        }

        std::string loadError = "";
        if (!LoadGameCodeAfterReload(m_gameSharedLib, m_app, loadError))
            Debug::Warning("Failed to reload game code library after build: %s", loadError.c_str());

        m_scene->LoadSceneNode(g_lastPlaySceneNode);
        ResetSceneHistory();
    }

    void Editor::DrawReloadBuildPopup()
    {
        bool shouldOpenPopup = false;
        bool shouldDrawPopup = false;
        {
            std::scoped_lock lock(m_reloadBuildMutex);
            if (m_openReloadBuildPopup)
            {
                shouldOpenPopup = true;
                m_openReloadBuildPopup = false;
            }
            shouldDrawPopup = m_showReloadBuildPopup;
        }

        if (shouldOpenPopup)
            ImGui::OpenPopup("Reload Build Output");

        if (!shouldDrawPopup)
            return;

        std::string command = "";
        std::string output = "";
        bool inProgress = false;
        bool finished = false;
        bool succeeded = false;
        bool autoCloseOnSuccess = false;
        int exitCode = -1;
        {
            std::scoped_lock lock(m_reloadBuildMutex);
            command = m_reloadBuildCommand;
            output = m_reloadBuildOutput;
            inProgress = m_reloadBuildInProgress;
            finished = m_reloadBuildFinished;
            succeeded = m_reloadBuildSucceeded;
            autoCloseOnSuccess = m_reloadBuildAutoCloseOnSuccess;
            exitCode = m_reloadBuildExitCode;
        }

        const bool allowClose = finished && !inProgress;
        bool popupOpen = true;
        ImGui::SetNextWindowSize(ImVec2(900.0f, 520.0f), ImGuiCond_FirstUseEver);
        if (ImGui::BeginPopupModal("Reload Build Output", allowClose ? &popupOpen : nullptr))
        {
            if (!command.empty())
                ImGui::TextWrapped("Command: %s", command.c_str());

            if (inProgress)
                ImGui::TextUnformatted("Status: Building...");
            else if (finished && succeeded)
                ImGui::TextUnformatted("Status: Build succeeded.");
            else if (finished)
                ImGui::Text("Status: Build failed (exit code: %d).", exitCode);
            else
                ImGui::TextUnformatted("Status: Waiting...");

            ImGui::Separator();
            ImGui::BeginChild("##ReloadBuildLog", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing() - 4.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
            ImGui::TextUnformatted(output.c_str());
            if (inProgress)
                ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();

            if (ImGui::Checkbox("Auto close on success", &autoCloseOnSuccess))
            {
                {
                    std::scoped_lock lock(m_reloadBuildMutex);
                    m_reloadBuildAutoCloseOnSuccess = autoCloseOnSuccess;
                }
                Canis::GetEditorConfig().reloadBuildAutoCloseOnSuccess = autoCloseOnSuccess;
                Canis::SaveEditorConfig();
            }

            if (allowClose && succeeded && autoCloseOnSuccess)
            {
                popupOpen = false;
                ImGui::CloseCurrentPopup();
            }

            if (allowClose)
                ImGui::SameLine();
            if (allowClose && ImGui::Button("Close"))
            {
                popupOpen = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        if (allowClose && !popupOpen)
        {
            std::scoped_lock lock(m_reloadBuildMutex);
            m_showReloadBuildPopup = false;
        }
    }

    void Editor::DrawEditorWindowMenu()
    {
        if (ImGui::Button("Windows##EditorToolbar"))
            ImGui::OpenPopup("EditorWindowsPopup");

        if (!ImGui::BeginPopup("EditorWindowsPopup"))
            return;

        ImGui::MenuItem("Scene", nullptr, &m_showScenePanel);
        ImGui::MenuItem("Game", nullptr, &m_showGamePanel);
        ImGui::Separator();
        ImGui::MenuItem("Hierarchy", nullptr, &m_showHierarchyPanel);
        ImGui::MenuItem("Inspector", nullptr, &m_showInspectorPanel);
        ImGui::MenuItem("Environment", nullptr, &m_showEnvironmentPanel);
        ImGui::MenuItem("Systems", nullptr, &m_showSystemsPanel);
        ImGui::MenuItem("Assets", nullptr, &m_showAssetsPanel);
        ImGui::MenuItem("Scripts", nullptr, &m_showScriptsPanel);
        ImGui::MenuItem("Console", nullptr, &m_showConsolePanel);
        ImGui::Separator();
        ImGui::MenuItem("Animation", nullptr, &m_showAnimationPanel);
        ImGui::MenuItem("Animator", nullptr, &m_showAnimatorPanel);
        ImGui::MenuItem("ShaderGraph", nullptr, &m_showShaderGraphPanel);
        ImGui::MenuItem("Project Settings", nullptr, &m_showProjectSettingsPanel);

        ImGui::Separator();
        ImGui::MenuItem("Hot Reload Assets", nullptr, &m_hotReloadAssets);
        ImGui::Separator();
        if (ImGui::MenuItem("Show All"))
        {
            m_showScenePanel = true;
            m_showGamePanel = true;
            m_showHierarchyPanel = true;
            m_showInspectorPanel = true;
            m_showEnvironmentPanel = true;
            m_showSystemsPanel = true;
            m_showAssetsPanel = true;
            m_showScriptsPanel = true;
            m_showAnimationPanel = true;
            m_showAnimatorPanel = true;
            m_showShaderGraphPanel = true;
            m_showProjectSettingsPanel = true;
            m_showConsolePanel = true;
        }

        ImGui::EndPopup();
    }

    void Editor::DrawEditorPanel()
    {
        FinalizeReloadBuildIfReady();

        static float hotKeyCoolDown = 0.0f;
        const float HOTKEYRESET = 0.1f;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float toolbarHeight = GetEditorToolbarHeight();
        ImGuiWindowFlags toolbarFlags = ImGuiWindowFlags_NoTitleBar
            | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoDocking
            | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoNavFocus;
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, toolbarHeight));
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::Begin("##EditorToolbar", nullptr, toolbarFlags);
        ImGui::PopStyleVar(2);

        if (m_mode == EditorMode::EDIT)
        {
            (void)HandleSceneHistoryShortcuts(hotKeyCoolDown, HOTKEYRESET);

            const bool canUndo = CanUndoSceneEdit();
            if (!canUndo)
                ImGui::BeginDisabled();
            if (ImGui::Button("Undo##ScenePanel"))
            {
                hotKeyCoolDown = HOTKEYRESET;
                UndoSceneEdit();
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Ctrl+Z");
            if (!canUndo)
                ImGui::EndDisabled();

            ImGui::SameLine();
            const bool canRedo = CanRedoSceneEdit();
            if (!canRedo)
                ImGui::BeginDisabled();
            if (ImGui::Button("Redo##ScenePanel"))
            {
                hotKeyCoolDown = HOTKEYRESET;
                RedoSceneEdit();
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Ctrl+Y or Ctrl+Shift+Z");
            if (!canRedo)
                ImGui::EndDisabled();

            ImGui::SameLine();
            if (ImGui::Button("Save##ScenePanel") || (ImGui::IsKeyDown(ImGuiKey_S) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && hotKeyCoolDown < 0.0f))
            {
                hotKeyCoolDown = HOTKEYRESET;
                FlushSceneHistoryPendingChange();
                m_scene->Save();
            }            
            ImGui::SameLine();
            if (ImGui::Button("Play##ScenePanel") || (ImGui::IsKeyDown(ImGuiKey_P) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && hotKeyCoolDown < 0.0f))
            {
                hotKeyCoolDown = HOTKEYRESET;
                FlushSceneHistoryPendingChange();
                m_window->SetSync(static_cast<Window::Sync>(Canis::GetProjectConfig().syncMode));
                if (Canis::GetProjectConfig().useFrameLimit)
                    Time::SetTargetFPS(Canis::GetProjectConfig().frameLimit + 0.0f);
                else
                    Time::SetTargetFPS(100000.0f);
                // save copy of scene
                g_lastPlaySceneNode = m_scene->EncodeScene();
                g_lastPlayScenePath = m_scene->m_path;

                m_mode = EditorMode::PLAY;
            }
            ImGui::SameLine();
            if (ImGui::Button("Launch##ScenePanel"))
            {
                std::string launchMessage = "";
                if (LaunchStandaloneGameFromProjectConfig(launchMessage))
                    Debug::Log("%s", launchMessage.c_str());
                else
                    Debug::Error("%s", launchMessage.c_str());
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Launch a separate no-editor runtime using Project Settings.");
            ImGui::SameLine();
            if (ImGui::Button("Reload##ScenePanel") || (ImGui::IsKeyDown(ImGuiKey_R) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && hotKeyCoolDown < 0.0f))
            {
                hotKeyCoolDown = HOTKEYRESET;

                bool buildAlreadyRunning = false;
                {
                    std::scoped_lock lock(m_reloadBuildMutex);
                    buildAlreadyRunning = m_reloadBuildInProgress;
                }

                if (buildAlreadyRunning)
                {
                    std::scoped_lock lock(m_reloadBuildMutex);
                    m_showReloadBuildPopup = true;
                    m_openReloadBuildPopup = true;
                }
                else
                {
                FlushSceneHistoryPendingChange();
                const std::filesystem::path buildDir = std::filesystem::path("..") / "build";
                const GameCodeBuildConfigInfo buildConfigInfo = ReadGameCodeBuildConfigInfo(buildDir);
                if (!buildConfigInfo.singleConfigType.empty())
                {
                    Debug::Log(
                        "Reload build tree is single-config: '%s'. '--config Debug' may be ignored for this generator.",
                        buildConfigInfo.singleConfigType.c_str());
                }
                else if (!buildConfigInfo.multiConfigTypes.empty())
                {
                    Debug::Log(
                        "Reload build tree is multi-config: %s. Building Debug.",
                        buildConfigInfo.multiConfigTypes.c_str());
                }
                else
                {
                    Debug::Warning("Unable to determine build configuration from ../build/CMakeCache.txt.");
                }

                m_assetPaths = FindFilesInFolder("assets", "");
                ReloadEditorShaders();

                // Save scene state before unregistering scripts from game code.
                g_lastPlaySceneNode = m_scene->EncodeScene();

                // Unload scene data while old game code is still loaded.
                m_scene->Unload();

                std::string unloadError = "";
                if (!UnloadGameCodeForReload(m_gameSharedLib, m_app, unloadError, m_reloadBuildBackupPath))
                {
                    Debug::Warning("Reload canceled. Failed to unload current game code: %s", unloadError.c_str());
                    m_scene->LoadSceneNode(g_lastPlaySceneNode);
                }
                else
                {
                    if (m_reloadBuildThread.joinable())
                        m_reloadBuildThread.join();

                    {
                        std::scoped_lock lock(m_reloadBuildMutex);
                        m_reloadBuildCommand = "cmake --build \"" + buildDir.generic_string() + "\" --target GameCode --parallel --";
                        m_reloadBuildOutput = "[build] " + m_reloadBuildCommand + "\n";
                        m_reloadBuildInProgress = true;
                        m_reloadBuildFinished = false;
                        m_reloadBuildSucceeded = false;
                        m_reloadBuildAwaitingFinalize = true;
                        m_reloadBuildExitCode = -1;
                        m_showReloadBuildPopup = true;
                        m_openReloadBuildPopup = true;
                    }

                    m_reloadBuildThread = std::thread([this, buildDir]()
                    {
                        std::string buildCommand = "";
                        std::string buildError = "";
                        int buildExitCode = -1;

                        const bool buildSucceeded = BuildGameCodeForReload(
                            buildDir,
                            buildCommand,
                            buildError,
                            buildExitCode,
                            [this](const std::string &_line)
                            {
                                std::scoped_lock lock(m_reloadBuildMutex);
                                m_reloadBuildOutput += _line;
                            });

                        std::scoped_lock lock(m_reloadBuildMutex);
                        if (!buildCommand.empty())
                            m_reloadBuildCommand = buildCommand;

                        if (!buildError.empty())
                            m_reloadBuildOutput += "[build] " + buildError + "\n";

                        m_reloadBuildExitCode = buildExitCode;
                        m_reloadBuildSucceeded = buildSucceeded;
                        m_reloadBuildInProgress = false;
                        m_reloadBuildFinished = true;
                    });
                }
                }
            }
        }
        else
        {
            if (m_mode == EditorMode::PLAY)
            {
                if (ImGui::Button("Pause##ScenePanel") || (ImGui::IsKeyDown(ImGuiKey_P) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && hotKeyCoolDown < 0.0f))
                {
                    hotKeyCoolDown = HOTKEYRESET;
                    ReleasePlayMouseCapture();
                    m_mode = EditorMode::PAUSE;
                }
            }
            else if (m_mode == EditorMode::PAUSE)
            {
                if (ImGui::Button("Resume##ScenePanel") || (ImGui::IsKeyDown(ImGuiKey_P) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && hotKeyCoolDown < 0.0f))
                {
                    hotKeyCoolDown = HOTKEYRESET;
                    ReleasePlayMouseCapture();
                    m_mode = EditorMode::PLAY;
                }
            }

            ImGui::SameLine();

            if (ImGui::Button("Stop##ScenePanel") || (ImGui::IsKeyDown(ImGuiKey_Q) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && hotKeyCoolDown < 0.0f))
            {
                hotKeyCoolDown = HOTKEYRESET;
                StopPlayMode();
            }
        }

        hotKeyCoolDown -= Time::UnscaledDeltaTime();

        ImGui::SameLine();
        DrawEditorWindowMenu();

        ImGui::SameLine();
        ImGui::Text("FPS: %s", std::to_string(m_app->FPS()).c_str());

        ImGui::SameLine();
        if (m_guizmoMode == GuizmoMode::LOCAL)
        {
            if (ImGui::Button("Local##ScenePanel"))
            {
                m_guizmoMode = GuizmoMode::WORLD;
            }
        }
        else
        {
            if (ImGui::Button("World##ScenePanel"))
            {
                m_guizmoMode = GuizmoMode::LOCAL;
            }
        }

        ImGui::SameLine();
        if (ImGui::Button(m_vertexSnappingEnabled ? "Vertex Snap: On##ScenePanel" : "Vertex Snap: Off##ScenePanel"))
            m_vertexSnappingEnabled = !m_vertexSnappingEnabled;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Hold V while translating for temporary vertex snapping.");

        ImGui::SameLine();
        int sceneCameraMode = static_cast<int>(m_sceneCameraMode);
        const char* sceneCameraModeLabels[] = { "3D", "2D" };
        ImGui::SetNextItemWidth(70.0f);
        if (ImGui::Combo("##SceneCameraMode", &sceneCameraMode, sceneCameraModeLabels, IM_ARRAYSIZE(sceneCameraModeLabels)))
            m_sceneCameraMode = static_cast<SceneCameraMode>(sceneCameraMode);

        size_t entityCount = 0;

        for (Entity *e : m_scene->GetEntities())
            if (e != nullptr)
                entityCount++;

        ImGui::SameLine();
        ImGui::Text("Entity Count: %zu", entityCount);
        ImGui::SameLine();
        ImGui::Text("Update Time: %.3f ms", m_app->UpdateTimeMs());
        ImGui::SameLine();
        ImGui::Text("Draw Time: %.3f ms", m_app->RenderTimeMs());

        ImGui::End();
        DrawReloadBuildPopup();
    }

    void Editor::SelectSprite2D()
    {
        if (ImGuizmo::IsOver() || ImGuizmo::IsUsing())
            return;

        if (!m_gameViewHovered || m_gameViewportWidth <= 0 || m_gameViewportHeight <= 0 ||
            m_gameViewportDrawWidth <= 0.0f || m_gameViewportDrawHeight <= 0.0f)
            return;

        ImVec2 mousePos = ImGui::GetMousePos();
        float localX = mousePos.x - m_gameViewportPosX;
        float localY = mousePos.y - m_gameViewportPosY;
        float logicalWidth = static_cast<float>((m_gameTextureWidth > 0) ? m_gameTextureWidth : m_window->GetWindowWidth());
        float logicalHeight = static_cast<float>((m_gameTextureHeight > 0) ? m_gameTextureHeight : m_window->GetWindowHeight());
        float scaleX = logicalWidth / m_gameViewportDrawWidth;
        float scaleY = logicalHeight / m_gameViewportDrawHeight;
        localX *= scaleX;
        localY *= scaleY;
        Vector2 mouse(localX, logicalHeight - localY);

        mouse = mouse - (Vector2(logicalWidth, logicalHeight) / 2.0f);

        Vector2 camPos(0.0f);
        float camScale = 1.0f;
        const bool useEditorSceneCamera2D =
            m_mode != EditorMode::HIDDEN &&
            m_sceneCameraMode == SceneCameraMode::SCENE_CAMERA_2D;

        if (useEditorSceneCamera2D)
        {
            camPos = m_editorCamera2DPosition;
            camScale = m_editorCamera2DScale;
        }
        else
        {
            std::vector<Entity *> &entities = m_scene->GetEntities();
            for (Entity *entity : entities)
            {
                if (!entity)
                    continue;
                if (Camera2D *camera = (entity != nullptr && entity->HasComponent<Camera2D>() ? &entity->GetComponent<Camera2D>() : nullptr))
                {
                    camPos = camera->GetPosition();
                    camScale = camera->GetScale();
                    break;
                }
            }
        }

        if (camScale != 0.0f)
            mouse = (mouse / camScale) + camPos;

        m_selectionMouseWorld = mouse;

        if (!m_sceneViewClicked)
            return;

        bool mouseLock = false;

        for (int i = 0; i < m_scene->GetEntities().size(); i++)
        {
            if (m_scene->GetEntities()[i] == nullptr)
                continue;

            Entity* candidate = m_scene->GetEntities()[i];
            RectTransform *transform = (candidate != nullptr && candidate->HasComponent<RectTransform>())
                ? &candidate->GetComponent<RectTransform>()
                : nullptr;

            if (transform == nullptr)
                continue;

            const RectTransformRenderBounds bounds = GetRenderBounds(*candidate, *transform);
            float globalRotation = transform->GetRotation();
            Vector2 selectionMouse = mouse;

            if (globalRotation != 0.0f)
            {
                RotatePointAroundPivot(
                    selectionMouse,
                    bounds.rotationPivot,
                    globalRotation);
            }

            if (selectionMouse.x > bounds.min.x &&
                selectionMouse.x < bounds.min.x + bounds.size.x &&
                selectionMouse.y > bounds.min.y &&
                selectionMouse.y < bounds.min.y + bounds.size.y &&
                !mouseLock)
            {
                m_index = i;
            }
        }
    }

    void Editor::SelectModel3D()
    {
        if (ImGuizmo::IsOver() || ImGuizmo::IsUsing())
            return;

        if (m_scene == nullptr || m_window == nullptr)
            return;

        if (!m_gameViewHovered || m_gameViewportWidth <= 0 || m_gameViewportHeight <= 0 ||
            m_gameViewportDrawWidth <= 0.0f || m_gameViewportDrawHeight <= 0.0f)
        {
            return;
        }

        if (!m_sceneViewClicked)
            return;

        const int targetWidth = (m_gameTextureWidth > 0) ? m_gameTextureWidth : m_window->GetWindowWidth();
        const int targetHeight = (m_gameTextureHeight > 0) ? m_gameTextureHeight : m_window->GetWindowHeight();
        if (targetWidth <= 0 || targetHeight <= 0)
            return;

        ImVec2 mousePos = ImGui::GetMousePos();
        const float localX = mousePos.x - m_gameViewportPosX;
        const float localY = mousePos.y - m_gameViewportPosY;
        if (localX < 0.0f || localY < 0.0f || localX >= m_gameViewportDrawWidth || localY >= m_gameViewportDrawHeight)
            return;

        const float scaleX = static_cast<float>(targetWidth) / m_gameViewportDrawWidth;
        const float scaleY = static_cast<float>(targetHeight) / m_gameViewportDrawHeight;
        const int pixelX = std::clamp(static_cast<int>(localX * scaleX), 0, targetWidth - 1);
        const int pixelY = std::clamp(targetHeight - 1 - static_cast<int>(localY * scaleY), 0, targetHeight - 1);

        EnsureGamePickingRenderTarget(targetWidth, targetHeight);
        if (m_gamePickingFramebuffer == 0)
            return;

        Shader &pickingShader = GetModelPickingShader();
        entt::registry &registry = m_scene->GetRegistry();

        glBindFramebuffer(GL_FRAMEBUFFER, m_gamePickingFramebuffer);
        glViewport(0, 0, targetWidth, targetHeight);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glDisable(GL_DITHER);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        pickingShader.Use();
        pickingShader.SetMat4("P", m_scene->GetEditorCamera3DProjection());
        pickingShader.SetMat4("V", m_scene->GetEditorCamera3DView());

        auto modelView = registry.view<Transform, Model>();
        for (const entt::entity entityHandle : modelView)
        {
            Transform &transform = modelView.get<Transform>(entityHandle);
            Model &modelRenderer = modelView.get<Model>(entityHandle);
            Entity *entity = modelRenderer.entity;
            if (entity == nullptr)
                entity = transform.entity;

            if (entity == nullptr || !transform.IsActiveInHierarchy() || entity->id < 0 || modelRenderer.modelId < 0)
                continue;

            ModelAsset *model = AssetManager::GetModel(modelRenderer.modelId);
            if (model == nullptr)
                continue;

            const ModelAsset::Pose3D *pose = nullptr;
            if (ModelAnimation *animation = registry.try_get<ModelAnimation>(entityHandle))
            {
                if (animation->poseModelId == modelRenderer.modelId &&
                    animation->poseGeometryRevision == model->GetGeometryRevision())
                    pose = &animation->pose;
            }

            const uint32_t entityId = static_cast<uint32_t>(entity->id + 1);
            model->Draw(
                pickingShader,
                transform.GetModelMatrix(),
                pose,
                -1,
                EncodeEntityIdColor(entityId),
                nullptr,
                modelRenderer.nodeIndex,
                modelRenderer.applyNodeTransform);
        }

        pickingShader.UnUse();

        unsigned char pixel[4] = {0, 0, 0, 0};
        glReadPixels(pixelX, pixelY, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, m_window->GetWindowWidth(), m_window->GetWindowHeight());
        glEnable(GL_DITHER);
        glDisable(GL_DEPTH_TEST);

        const uint32_t entityId = DecodeEntityIdColor(pixel);
        if (entityId == 0)
            return;

        const int entityIndex = static_cast<int>(entityId) - 1;
        std::vector<Entity *> &entities = m_scene->GetEntities();
        if (entityIndex < 0 || entityIndex >= static_cast<int>(entities.size()) || entities[entityIndex] == nullptr)
            return;

        FocusEntity(entities[entityIndex]);
    }

    void Editor::DrawSelectionMouseDebug(Camera2D *_camera2D)
    {
        if (_camera2D == nullptr)
            return;

        Matrix4 projection = Matrix4(1.0f);
        if (m_scene != nullptr && m_scene->HasEditorCamera2DOverride())
        {
            projection = m_scene->GetEditorCamera2DMatrix();
        }
        else
        {
            _camera2D->UpdateMatrix();
            projection = _camera2D->GetCameraMatrix();
        }

        Canis::Shader &debugLineShader = GetDebugLineShader();

        const float halfSize = 7.0f;
        Vector2 vertices[4] = {
            Vector2(m_selectionMouseWorld.x - halfSize, m_selectionMouseWorld.y),
            Vector2(m_selectionMouseWorld.x + halfSize, m_selectionMouseWorld.y),
            Vector2(m_selectionMouseWorld.x, m_selectionMouseWorld.y - halfSize),
            Vector2(m_selectionMouseWorld.x, m_selectionMouseWorld.y + halfSize)
        };

        for (Vector2 &v : vertices)
            v = Vector2(projection * Vector4(v.x, v.y, 0.0f, 1.0f));

        GLuint VAO, VBO;
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        debugLineShader.Use();
        glBindVertexArray(VAO);
        glDrawArrays(GL_LINES, 0, 4);
        debugLineShader.UnUse();

        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
    }

    void Editor::DrawBoundingBox(Camera2D *_camera2D)
    {
        if (_camera2D == nullptr)
            return;

        Matrix4 projection = Matrix4(1.0f);
        if (m_scene != nullptr && m_scene->HasEditorCamera2DOverride())
        {
            projection = m_scene->GetEditorCamera2DMatrix();
        }
        else
        {
            _camera2D->UpdateMatrix();
            projection = _camera2D->GetCameraMatrix();
        }

        Canis::Shader &debugLineShader = GetDebugLineShader();
        Entity &debugRectTransformEntity = *m_scene->GetEntities()[m_index];
        RectTransform &rtc = debugRectTransformEntity.GetComponent<RectTransform>();
        const RectTransformRenderBounds bounds = GetRenderBounds(debugRectTransformEntity, rtc);
        Vector2 vertices[4];

        vertices[0] = {bounds.min.x, bounds.min.y};
        vertices[1] = {bounds.min.x + bounds.size.x, bounds.min.y};
        vertices[2] = {bounds.min.x + bounds.size.x, bounds.min.y + bounds.size.y};
        vertices[3] = {bounds.min.x, bounds.min.y + bounds.size.y};


        for (Vector2 &v : vertices)
            RotatePointAroundPivot(
                v,
                bounds.rotationPivot,
                -rtc.GetRotation()
            );

        for (Vector2 &v : vertices)
            v = Vector2(projection * Vector4(v.x, v.y, 0.0f, 1.0f));

        GLuint VAO, VBO;
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        debugLineShader.Use();

        glBindVertexArray(VAO);
        glDrawArrays(GL_LINE_LOOP, 0, 4);
        debugLineShader.UnUse();

        // clean up
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
    }
}
