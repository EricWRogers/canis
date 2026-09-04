#include "yaml-cpp/emittermanip.h"
#include <Canis/App.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_init.h>

#include <Canis/Canis.hpp>
#include <Canis/GameCodeObject.hpp>
#include <Canis/Time.hpp>
#include <Canis/Debug.hpp>
#include <Canis/OpenGL.hpp>
#include <Canis/Window.hpp>
#include <Canis/Editor.hpp>
#include <Canis/IOManager.hpp>
#include <Canis/InputManager.hpp>
#include <Canis/AudioManager.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/PostProcessPipeline.hpp>
#include <Canis/ConfigHelper.hpp>
#include <Canis/Network.hpp>
#include <Canis/VFX/Particles.hpp>
#include <Canis/Terrain.hpp>

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <limits>
#include <optional>
#include <sstream>
#include <unordered_map>

#include <stb_image_write.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

namespace Canis
{
    struct RuntimeTimelineEvent
    {
        double time = 0.0;
        YAML::Node data = {};
    };

    struct RuntimeCaptureRecord
    {
        double time = 0.0;
        std::string label = {};
        std::string path = {};
    };

    struct RuntimeInteractiveCommand
    {
        uint64_t frames = 1u;
        std::string captureLabel = {};
        bool stop = false;
        std::vector<RuntimeTimelineEvent> events = {};
    };

    struct RuntimeLaunchOptions
    {
        bool active = false;
        bool interactive = false;
        std::optional<bool> editorRuntimeOverride = std::nullopt;
        bool offscreen = false;
        bool captureFinal = true;
        std::string launchScene = {};
        std::string inputScript = {};
        std::filesystem::path captureDirectory = {};
        float forcedFPS = 0.0f;
        float fixedDelta = 0.0f;
        std::optional<unsigned int> randomSeed = std::nullopt;
        double stopAt = -1.0;
    };

    struct App::RuntimeContext
    {
        std::unique_ptr<Window> window;
        std::unique_ptr<Editor> editor;
        std::unique_ptr<InputManager> inputManager;
        GameCodeObject gameCodeObject = {};
        bool editorRuntimeEnabled = false;
        RenderTarget runtimeRenderTarget = {};
        RenderTarget runtimePostProcessTarget = {};
        RuntimeLaunchOptions launch = {};
        std::vector<RuntimeTimelineEvent> timeline = {};
        size_t nextTimelineEvent = 0u;
        double simulationTime = 0.0;
        uint64_t simulationFrame = 0u;
        bool stopAfterFrame = false;
        bool harnessCompleted = false;
        bool timeInitialized = false;
        bool gameCodeInitialized = false;
        std::string exitReason = "window-closed";
        std::vector<std::string> pendingCaptures = {};
        std::vector<RuntimeCaptureRecord> captures = {};
        std::vector<unsigned int> pulseKeys = {};
        std::vector<unsigned int> pulseMouseButtons = {};
        std::vector<unsigned int> pulseGamepadButtons = {};
        bool interactiveWaitingForCommand = false;
        uint64_t interactiveFramesRemaining = 0u;
        std::string interactiveCaptureLabel = "initial";
        size_t interactiveStep = 0u;
    };

    void App::FailRuntimeTest(const std::string &_message)
    {
        Debug::Error("Runtime test assertion failed: %s", _message.c_str());
        if (m_runtime == nullptr || !m_runtime->launch.active)
            return;
        m_exitCode = 6;
        m_runtime->exitReason = "assertion-failed";
        m_runtime->stopAfterFrame = true;
        m_runtime->pendingCaptures.push_back("assertion_failure");
    }

    namespace
    {
        namespace fs = std::filesystem;

        std::string ToLower(std::string _value)
        {
            std::transform(_value.begin(), _value.end(), _value.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });
            return _value;
        }

        std::string SanitizeFileName(std::string _value)
        {
            if (_value.empty())
                _value = "capture";
            for (char &character : _value)
            {
                if (!std::isalnum(static_cast<unsigned char>(character)) &&
                    character != '-' && character != '_')
                    character = '_';
            }
            return _value;
        }

        std::string EscapeJson(const std::string &_value)
        {
            std::string escaped = {};
            escaped.reserve(_value.size() + 8u);
            for (const char character : _value)
            {
                switch (character)
                {
                    case '\\': escaped += "\\\\"; break;
                    case '"': escaped += "\\\""; break;
                    case '\n': escaped += "\\n"; break;
                    case '\r': escaped += "\\r"; break;
                    case '\t': escaped += "\\t"; break;
                    default: escaped += character; break;
                }
            }
            return escaped;
        }

        bool ParsePositiveFloat(const std::string &_value, float &_outValue)
        {
            try
            {
                size_t parsed = 0u;
                const float value = std::stof(_value, &parsed);
                if (parsed != _value.size() || !std::isfinite(value) || value <= 0.0f)
                    return false;
                _outValue = value;
                return true;
            }
            catch (const std::exception&)
            {
                return false;
            }
        }

        bool ParseNonNegativeDouble(const std::string &_value, double &_outValue)
        {
            try
            {
                size_t parsed = 0u;
                const double value = std::stod(_value, &parsed);
                if (parsed != _value.size() || !std::isfinite(value) || value < 0.0)
                    return false;
                _outValue = value;
                return true;
            }
            catch (const std::exception&)
            {
                return false;
            }
        }

        bool ParseRuntimeLaunchOptions(
            const std::vector<std::string> &_arguments,
            const fs::path &_invocationDirectory,
            RuntimeLaunchOptions &_outOptions,
            std::string &_outError)
        {
            auto readValue = [&](size_t &_index, const std::string &_argument, const std::string &_name) -> std::optional<std::string>
            {
                const std::string prefix = _name + "=";
                if (_argument.rfind(prefix, 0u) == 0u)
                    return _argument.substr(prefix.size());
                if (_argument == _name && _index + 1u < _arguments.size())
                    return _arguments[++_index];
                return std::nullopt;
            };
            auto setEditorRuntimeOverride =
                [&](const bool _enabled, const std::string &_argument) -> bool
            {
                if (_outOptions.editorRuntimeOverride.has_value() &&
                    _outOptions.editorRuntimeOverride.value() != _enabled)
                {
                    _outError =
                        "Conflicting editor launch mode option: " + _argument;
                    return false;
                }
                _outOptions.editorRuntimeOverride = _enabled;
                return true;
            };
            auto parseSeed = [&](const std::string &_value) -> bool
            {
                try
                {
                    size_t parsed = 0u;
                    const unsigned long long value = std::stoull(_value, &parsed, 10);
                    if (parsed != _value.size() ||
                        value > std::numeric_limits<unsigned int>::max())
                        return false;
                    _outOptions.randomSeed = static_cast<unsigned int>(value);
                    return true;
                }
                catch (const std::exception&)
                {
                    return false;
                }
            };

            for (size_t index = 0u; index < _arguments.size(); ++index)
            {
                const std::string &argument = _arguments[index];
                if (argument == "--editor")
                {
                    if (!setEditorRuntimeOverride(true, argument))
                        return false;
                    continue;
                }
                if (argument == "--force-game-only" || argument == "--game-only")
                {
                    _outOptions.active = true;
                    if (!setEditorRuntimeOverride(false, argument))
                        return false;
                    continue;
                }
                if (argument == "--interactive-test")
                {
                    _outOptions.active = true;
                    _outOptions.interactive = true;
                    if (!setEditorRuntimeOverride(false, argument))
                        return false;
                    continue;
                }
                if (argument == "--offscreen")
                {
                    _outOptions.active = true;
                    _outOptions.offscreen = true;
                    continue;
                }
                if (argument == "--no-final-capture")
                {
                    _outOptions.active = true;
                    _outOptions.captureFinal = false;
                    continue;
                }
                if (argument == "--capture-final")
                {
                    _outOptions.active = true;
                    _outOptions.captureFinal = true;
                    continue;
                }

                if (auto value = readValue(index, argument, "--launch-scene"))
                {
                    _outOptions.active = true;
                    _outOptions.launchScene = *value;
                    continue;
                }
                if (auto value = readValue(index, argument, "--input-script"))
                {
                    _outOptions.active = true;
                    fs::path path(*value);
                    if (path.is_relative())
                        path = _invocationDirectory / path;
                    _outOptions.inputScript = path.lexically_normal().generic_string();
                    continue;
                }
                if (auto value = readValue(index, argument, "--capture-dir"))
                {
                    _outOptions.active = true;
                    fs::path path(*value);
                    if (path.is_relative())
                        path = _invocationDirectory / path;
                    _outOptions.captureDirectory = path.lexically_normal();
                    continue;
                }
                if (auto value = readValue(index, argument, "--force-fps"))
                {
                    _outOptions.active = true;
                    if (!ParsePositiveFloat(*value, _outOptions.forcedFPS))
                    {
                        _outError = "--force-fps requires a positive number.";
                        return false;
                    }
                    continue;
                }
                if (auto value = readValue(index, argument, "--fixed-delta"))
                {
                    _outOptions.active = true;
                    if (!ParsePositiveFloat(*value, _outOptions.fixedDelta))
                    {
                        _outError = "--fixed-delta requires a positive number of seconds.";
                        return false;
                    }
                    continue;
                }
                if (auto value = readValue(index, argument, "--seed"))
                {
                    _outOptions.active = true;
                    if (!parseSeed(*value))
                    {
                        _outError = "--seed requires an unsigned 32-bit integer.";
                        return false;
                    }
                    continue;
                }
                if (auto value = readValue(index, argument, "--stop-at"))
                {
                    _outOptions.active = true;
                    if (!ParseNonNegativeDouble(*value, _outOptions.stopAt))
                    {
                        _outError = "--stop-at requires a non-negative number of seconds.";
                        return false;
                    }
                    continue;
                }

                if (argument.rfind("--", 0u) == 0u && argument != "--help" && argument != "-h")
                {
                    _outError = "Unknown option: " + argument;
                    return false;
                }
            }

            if (_outOptions.forcedFPS > 0.0f && _outOptions.fixedDelta <= 0.0f)
                _outOptions.fixedDelta = 1.0f / _outOptions.forcedFPS;
            if (_outOptions.interactive && _outOptions.fixedDelta <= 0.0f)
                _outOptions.fixedDelta = 1.0f / 60.0f;
            if (_outOptions.interactive && !_outOptions.inputScript.empty())
            {
                _outError = "--interactive-test cannot be combined with --input-script.";
                return false;
            }

            if (_outOptions.active && _outOptions.captureDirectory.empty())
                _outOptions.captureDirectory = _invocationDirectory / "artifacts" / "runtime_test";
            return true;
        }

        unsigned int ResolveSyntheticKey(const std::string &_name)
        {
            const std::string normalized = ToLower(_name);
            static const std::unordered_map<std::string, SDL_Scancode> aliases = {
                {"lshift", SDL_SCANCODE_LSHIFT}, {"rshift", SDL_SCANCODE_RSHIFT},
                {"lctrl", SDL_SCANCODE_LCTRL}, {"rctrl", SDL_SCANCODE_RCTRL},
                {"lalt", SDL_SCANCODE_LALT}, {"ralt", SDL_SCANCODE_RALT},
                {"return", SDL_SCANCODE_RETURN}, {"enter", SDL_SCANCODE_RETURN},
                {"escape", SDL_SCANCODE_ESCAPE}, {"esc", SDL_SCANCODE_ESCAPE},
                {"space", SDL_SCANCODE_SPACE}, {"backspace", SDL_SCANCODE_BACKSPACE},
                {"up", SDL_SCANCODE_UP}, {"down", SDL_SCANCODE_DOWN},
                {"left", SDL_SCANCODE_LEFT}, {"right", SDL_SCANCODE_RIGHT},
            };
            const auto alias = aliases.find(normalized);
            if (alias != aliases.end())
                return static_cast<unsigned int>(alias->second);

            const SDL_Scancode scancode = SDL_GetScancodeFromName(_name.c_str());
            return static_cast<unsigned int>(scancode);
        }

        unsigned int ResolveSyntheticGamepadButton(const std::string &_name)
        {
            const std::string normalized = ToLower(_name);
            static const std::unordered_map<std::string, unsigned int> buttons = {
                {"a", ControllerButton::A}, {"b", ControllerButton::B},
                {"x", ControllerButton::X}, {"y", ControllerButton::Y},
                {"back", ControllerButton::BACK}, {"guide", ControllerButton::GUIDE},
                {"start", ControllerButton::START},
                {"leftstick", ControllerButton::LEFTSTICK},
                {"rightstick", ControllerButton::RIGHTSTICK},
                {"leftshoulder", ControllerButton::LEFTSHOULDER},
                {"rightshoulder", ControllerButton::RIGHTSHOULDER},
                {"dpad_up", ControllerButton::DPAD_UP},
                {"dpad_down", ControllerButton::DPAD_DOWN},
                {"dpad_left", ControllerButton::DPAD_LEFT},
                {"dpad_right", ControllerButton::DPAD_RIGHT},
            };
            const auto button = buttons.find(normalized);
            return button == buttons.end() ? 0u : button->second;
        }

        bool IsDownAction(const std::string &_action)
        {
            const std::string action = ToLower(_action);
            return action == "down" || action == "press" || action == "pressed" ||
                action == "true" || action == "1";
        }

        bool IsPulseAction(const std::string &_action)
        {
            const std::string action = ToLower(_action);
            return action == "press" || action == "pressed";
        }

        bool ReadVector2(const YAML::Node &_node, Vector2 &_outValue)
        {
            if (!_node || !_node.IsSequence() || _node.size() < 2u)
                return false;
            _outValue.x = _node[0].as<float>(0.0f);
            _outValue.y = _node[1].as<float>(0.0f);
            return true;
        }

        bool LoadRuntimeTimeline(
            const std::string &_path,
            std::vector<RuntimeTimelineEvent> &_outEvents,
            std::string &_outError)
        {
            if (_path.empty())
                return true;
            try
            {
                YAML::Node root = YAML::LoadFile(_path);
                const YAML::Node events = root["events"];
                if (!events || !events.IsSequence())
                {
                    _outError = "Input script must contain an events array: " + _path;
                    return false;
                }
                double previousTime = -1.0;
                for (const YAML::Node &event : events)
                {
                    if (!event.IsMap())
                    {
                        _outError = "Every input event must be an object.";
                        return false;
                    }
                    const double time = event["time"].as<double>(-1.0);
                    if (!std::isfinite(time) || time < 0.0)
                    {
                        _outError = "Every input event requires a non-negative time.";
                        return false;
                    }
                    if (time < previousTime)
                    {
                        _outError = "Input events must be ordered by ascending time.";
                        return false;
                    }
                    previousTime = time;
                    _outEvents.push_back({time, YAML::Clone(event)});
                }
                return true;
            }
            catch (const YAML::Exception &_exception)
            {
                _outError = "Failed to parse input script '" + _path + "': " + _exception.what();
                return false;
            }
        }

        bool ParseRuntimeInteractiveCommand(
            const std::string &_line,
            double _simulationTime,
            float _fixedDelta,
            RuntimeInteractiveCommand &_outCommand,
            std::string &_outError)
        {
            try
            {
                const YAML::Node command = YAML::Load(_line);
                if (!command || !command.IsMap())
                {
                    _outError = "Interactive commands must be JSON objects.";
                    return false;
                }

                _outCommand.stop = command["stop"].as<bool>(false);
                _outCommand.captureLabel = command["capture"].as<std::string>("");

                const double stepDelta = (_fixedDelta > 0.0f)
                    ? static_cast<double>(_fixedDelta)
                    : (1.0 / 60.0);
                bool explicitAdvance = false;
                if (command["frames"])
                {
                    const long long frames = command["frames"].as<long long>(0);
                    if (frames <= 0)
                    {
                        _outError = "Interactive command frames must be positive.";
                        return false;
                    }
                    _outCommand.frames = static_cast<uint64_t>(frames);
                    explicitAdvance = true;
                }
                else if (command["duration"])
                {
                    const double duration = command["duration"].as<double>(0.0);
                    if (!std::isfinite(duration) || duration <= 0.0)
                    {
                        _outError = "Interactive command duration must be positive.";
                        return false;
                    }
                    _outCommand.frames = std::max<uint64_t>(
                        1u,
                        static_cast<uint64_t>(std::ceil(duration / stepDelta)));
                    explicitAdvance = true;
                }

                double latestRelativeEventTime = 0.0;
                if (const YAML::Node events = command["events"]; events)
                {
                    if (!events.IsSequence())
                    {
                        _outError = "Interactive command events must be an array.";
                        return false;
                    }
                    double previousTime = -1.0;
                    for (const YAML::Node &event : events)
                    {
                        const double relativeTime = event["time"].as<double>(-1.0);
                        if (!event.IsMap() || !std::isfinite(relativeTime) ||
                            relativeTime < 0.0 || relativeTime < previousTime)
                        {
                            _outError = "Interactive events require ascending non-negative times.";
                            return false;
                        }
                        previousTime = relativeTime;
                        latestRelativeEventTime = relativeTime;
                        YAML::Node absoluteEvent = YAML::Clone(event);
                        absoluteEvent["time"] = _simulationTime + relativeTime;
                        _outCommand.events.push_back({
                            _simulationTime + relativeTime,
                            YAML::Clone(absoluteEvent)});
                    }
                }

                YAML::Node immediateEvent(YAML::NodeType::Map);
                immediateEvent["time"] = _simulationTime;
                bool hasImmediateInput = false;
                for (const char *field : {"keyboard", "mouse", "gamepad"})
                {
                    if (command[field])
                    {
                        immediateEvent[field] = YAML::Clone(command[field]);
                        hasImmediateInput = true;
                    }
                }
                if (hasImmediateInput)
                {
                    _outCommand.events.insert(
                        _outCommand.events.begin(),
                        {_simulationTime, YAML::Clone(immediateEvent)});
                }

                if (!explicitAdvance && latestRelativeEventTime > 0.0)
                {
                    _outCommand.frames = std::max<uint64_t>(
                        1u,
                        static_cast<uint64_t>(std::ceil(latestRelativeEventTime / stepDelta)) + 1u);
                }
                if (_outCommand.stop)
                    _outCommand.captureLabel =
                        _outCommand.captureLabel.empty() ? "final" : _outCommand.captureLabel;
                return true;
            }
            catch (const YAML::Exception &_exception)
            {
                _outError = std::string("Invalid interactive JSON command: ") + _exception.what();
                return false;
            }
        }

        bool WriteRuntimeFramebuffer(
            unsigned int _framebuffer,
            int _width,
            int _height,
            const fs::path &_outputPath)
        {
            if (_framebuffer == 0u || _width <= 0 || _height <= 0)
                return false;

            std::error_code error = {};
            fs::create_directories(_outputPath.parent_path(), error);
            if (error)
                return false;

            GLint previousReadFramebuffer = 0;
            GLint previousReadBuffer = GL_BACK;
            GLint previousPackAlignment = 4;
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFramebuffer);
            glGetIntegerv(GL_READ_BUFFER, &previousReadBuffer);
            glGetIntegerv(GL_PACK_ALIGNMENT, &previousPackAlignment);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, _framebuffer);
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);

            const size_t rowBytes = static_cast<size_t>(_width) * 4u;
            std::vector<unsigned char> pixels(rowBytes * static_cast<size_t>(_height));
            glReadPixels(0, 0, _width, _height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

            glPixelStorei(GL_PACK_ALIGNMENT, previousPackAlignment);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, previousReadFramebuffer);
            glReadBuffer(previousReadBuffer);

            std::vector<unsigned char> topDownPixels(pixels.size());
            for (int row = 0; row < _height; ++row)
            {
                const size_t sourceOffset = static_cast<size_t>(_height - row - 1) * rowBytes;
                const size_t destinationOffset = static_cast<size_t>(row) * rowBytes;
                std::copy_n(pixels.data() + sourceOffset, rowBytes, topDownPixels.data() + destinationOffset);
            }

            return stbi_write_png(
                _outputPath.generic_string().c_str(),
                _width,
                _height,
                4,
                topDownPixels.data(),
                static_cast<int>(rowBytes)) != 0;
        }

        void ApplyRuntimeTimelineEvent(
            const RuntimeTimelineEvent &_event,
            InputManager &_input,
            std::vector<unsigned int> &_pulseKeys,
            std::vector<unsigned int> &_pulseMouseButtons,
            std::vector<unsigned int> &_pulseGamepadButtons,
            std::vector<std::string> &_pendingCaptures,
            bool &_stopAfterFrame)
        {
            const YAML::Node &data = _event.data;

            if (const YAML::Node keyboard = data["keyboard"]; keyboard && keyboard.IsMap())
            {
                for (const auto &entry : keyboard)
                {
                    const std::string name = entry.first.as<std::string>("");
                    const std::string action = entry.second.as<std::string>("");
                    const unsigned int key = ResolveSyntheticKey(name);
                    if (key == static_cast<unsigned int>(SDL_SCANCODE_UNKNOWN))
                    {
                        Debug::Warning("Runtime input script ignored unknown key '%s'.", name.c_str());
                        continue;
                    }
                    _input.SetSyntheticKey(key, IsDownAction(action));
                    if (IsPulseAction(action))
                        _pulseKeys.push_back(key);
                }
            }

            if (const YAML::Node mouse = data["mouse"]; mouse && mouse.IsMap())
            {
                Vector2 value = {};
                if (ReadVector2(mouse["delta"], value))
                    _input.AddSyntheticMouseDelta(value);
                if (ReadVector2(mouse["position"], value))
                    _input.SetSyntheticMousePosition(value);
                if (mouse["wheel"])
                    _input.AddSyntheticMouseWheel(mouse["wheel"].as<int>(0));

                if (const YAML::Node buttons = mouse["buttons"]; buttons && buttons.IsMap())
                {
                    for (const auto &entry : buttons)
                    {
                        const std::string name = ToLower(entry.first.as<std::string>(""));
                        const std::string action = entry.second.as<std::string>("");
                        const unsigned int button = (name == "left") ? 1u : ((name == "right") ? 3u : 0u);
                        if (button == 0u)
                        {
                            Debug::Warning("Runtime input script ignored unknown mouse button '%s'.", name.c_str());
                            continue;
                        }
                        _input.SetSyntheticMouseButton(button, IsDownAction(action));
                        if (IsPulseAction(action))
                            _pulseMouseButtons.push_back(button);
                    }
                }
            }

            if (const YAML::Node gamepad = data["gamepad"]; gamepad && gamepad.IsMap())
            {
                Vector2 value = {};
                if (ReadVector2(gamepad["leftStick"], value))
                    _input.SetSyntheticGamepadLeftStick(value);
                if (ReadVector2(gamepad["rightStick"], value))
                    _input.SetSyntheticGamepadRightStick(value);

                float leftTrigger = gamepad["leftTrigger"].as<float>(0.0f);
                float rightTrigger = gamepad["rightTrigger"].as<float>(0.0f);
                if (gamepad["leftTrigger"] || gamepad["rightTrigger"])
                    _input.SetSyntheticGamepadTriggers(leftTrigger, rightTrigger);

                if (const YAML::Node buttons = gamepad["buttons"]; buttons && buttons.IsMap())
                {
                    for (const auto &entry : buttons)
                    {
                        const std::string name = entry.first.as<std::string>("");
                        const std::string action = entry.second.as<std::string>("");
                        const unsigned int button = ResolveSyntheticGamepadButton(name);
                        if (button == 0u)
                        {
                            Debug::Warning("Runtime input script ignored unknown gamepad button '%s'.", name.c_str());
                            continue;
                        }
                        _input.SetSyntheticGamepadButton(button, IsDownAction(action));
                        if (IsPulseAction(action))
                            _pulseGamepadButtons.push_back(button);
                    }
                }
            }

            if (const YAML::Node capture = data["capture"]; capture)
                _pendingCaptures.push_back(capture.as<std::string>("capture"));
            if (data["stop"].as<bool>(false))
                _stopAfterFrame = true;
        }

        bool WriteRuntimeManifest(
            const RuntimeLaunchOptions &_options,
            const std::string &_reason,
            double _simulationTime,
            uint64_t _simulationFrame,
            const std::vector<RuntimeCaptureRecord> &_captures,
            int _exitCode)
        {
            if (!_options.active)
                return true;

            std::error_code error = {};
            fs::create_directories(_options.captureDirectory, error);
            if (error)
                return false;

            const fs::path manifestPath = _options.captureDirectory / "manifest.json";
            std::ofstream output(manifestPath);
            if (!output.is_open())
                return false;

            output << "{\n"
                   << "  \"exitReason\": \"" << EscapeJson(_reason) << "\",\n"
                   << "  \"exitCode\": " << _exitCode << ",\n"
                   << "  \"simulationTime\": " << _simulationTime << ",\n"
                   << "  \"simulationFrame\": " << _simulationFrame << ",\n"
                   << "  \"fixedDelta\": " << _options.fixedDelta << ",\n"
                   << "  \"forcedFPS\": " << _options.forcedFPS << ",\n"
                   << "  \"randomSeed\": ";
            if (_options.randomSeed.has_value())
                output << _options.randomSeed.value();
            else
                output << "null";
            output << ",\n"
                   << "  \"scene\": \"" << EscapeJson(_options.launchScene) << "\",\n"
                   << "  \"frames\": [\n";
            for (size_t index = 0u; index < _captures.size(); ++index)
            {
                const RuntimeCaptureRecord &capture = _captures[index];
                output << "    {\"time\": " << capture.time
                       << ", \"label\": \"" << EscapeJson(capture.label)
                       << "\", \"path\": \"" << EscapeJson(capture.path) << "\"}";
                if (index + 1u < _captures.size())
                    output << ",";
                output << "\n";
            }
            output << "  ]\n}\n";
            output.close();

            std::cout << "CANIS_TEST_RESULT {\"manifest\":\""
                      << EscapeJson(fs::absolute(manifestPath).generic_string())
                      << "\",\"exitReason\":\"" << EscapeJson(_reason)
                      << "\",\"exitCode\":" << _exitCode << "}" << std::endl;
            return true;
        }

        const char *GetGameCodeSharedObjectPath()
        {
            static std::string resolvedPath;

#if defined(__EMSCRIPTEN__)
            return "";
#else
#if defined(_WIN32)
            constexpr const char *libraryName = "libGameCode.dll";
#elif defined(__APPLE__)
            constexpr const char *libraryName = "libGameCode.dylib";
#elif defined(__linux__)
            constexpr const char *libraryName = "libGameCode.so";
#else
            return "";
#endif

            std::vector<fs::path> candidates = {};
            if (const char *basePath = SDL_GetBasePath())
                candidates.emplace_back(fs::path(basePath) / libraryName);

            candidates.emplace_back(fs::current_path() / libraryName);

            for (const fs::path &candidatePath : candidates)
            {
                std::error_code ec;
                if (fs::exists(candidatePath, ec) && fs::is_regular_file(candidatePath, ec))
                {
                    resolvedPath = candidatePath.generic_string();
                    return resolvedPath.c_str();
                }
            }

            resolvedPath = std::string("./") + libraryName;
            return resolvedPath.c_str();
#endif
        }

        bool HasAssetsFolder(const fs::path &_path)
        {
            const fs::path assetsPath = _path / "assets";
            return fs::exists(assetsPath) && fs::is_directory(assetsPath);
        }

        fs::path WeaklyCanonicalPath(const fs::path &_path)
        {
            std::error_code ec;
            const fs::path canonicalPath = fs::weakly_canonical(_path, ec);
            return ec ? _path.lexically_normal() : canonicalPath;
        }

        std::optional<fs::path> NormalizeProjectPath(const fs::path &_path)
        {
            if (_path.empty())
                return std::nullopt;

            const fs::path normalizedPath = WeaklyCanonicalPath(_path);
            if (HasAssetsFolder(normalizedPath))
                return normalizedPath;

            const fs::path nestedProjectPath = WeaklyCanonicalPath(normalizedPath / "project");
            if (HasAssetsFolder(nestedProjectPath))
                return nestedProjectPath;

            return std::nullopt;
        }

        std::optional<fs::path> ResolveProjectFromEnvironment()
        {
            const char *projectPath = std::getenv("CANIS_PROJECT");
            if (projectPath == nullptr || projectPath[0] == '\0')
                return std::nullopt;

            return NormalizeProjectPath(projectPath);
        }

        bool TryParseEnvironmentBool(const char *_value, bool &_outValue)
        {
            if (_value == nullptr || _value[0] == '\0')
                return false;

            std::string normalized(_value);
            std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });

            if (normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on")
            {
                _outValue = true;
                return true;
            }

            if (normalized == "0" || normalized == "false" || normalized == "no" || normalized == "off")
            {
                _outValue = false;
                return true;
            }

            return false;
        }

        int EnvironmentDimension(const char *_name, int _fallback, int _minimum)
        {
            const char *value = std::getenv(_name);
            if (value == nullptr || value[0] == '\0')
                return _fallback;
            char *end = nullptr;
            const long parsed = std::strtol(value, &end, 10);
            if (end == value || *end != '\0' || parsed < _minimum || parsed > 16384)
            {
                Debug::Warning("Ignoring invalid %s='%s'.", _name, value);
                return _fallback;
            }
            return static_cast<int>(parsed);
        }

        Vector3 MaxVector3(const Vector3 &_left, const Vector3 &_right)
        {
            return Vector3(
                std::max(_left.x, _right.x),
                std::max(_left.y, _right.y),
                std::max(_left.z, _right.z));
        }

        bool TryGetAttachedModelLocalBounds(Entity &_entity, Vector3 &_outMin, Vector3 &_outMax)
        {
            if (!_entity.HasComponent<Model>())
                return false;

            const Model &modelRenderer = _entity.GetComponent<Model>();
            if (modelRenderer.modelId < 0)
                return false;

            ModelAsset *model = AssetManager::GetModel(modelRenderer.modelId);
            if (model == nullptr)
                return false;

            return model->GetLocalBounds(_outMin, _outMax, modelRenderer.nodeIndex, modelRenderer.applyNodeTransform);
        }

        bool DrawGenerateColliderButton(const char *_idSuffix)
        {
            const std::string label = BuildInspectorFieldLabel("Generate Collider", _idSuffix);
            return ImGui::Button(label.c_str());
        }

        void GenerateBoxColliderFromBounds(BoxCollider &_collider, const Vector3 &_min, const Vector3 &_max)
        {
            _collider.offset = (_min + _max) * 0.5f;
            _collider.size = MaxVector3(_max - _min, Vector3(0.01f));
        }

        void GenerateSphereColliderFromBounds(SphereCollider &_collider, const Vector3 &_min, const Vector3 &_max)
        {
            const Vector3 halfExtents = MaxVector3((_max - _min) * 0.5f, Vector3(0.01f));
            _collider.offset = (_min + _max) * 0.5f;
            _collider.radius = std::max(0.01f, static_cast<float>(std::sqrt(
                halfExtents.x * halfExtents.x +
                halfExtents.y * halfExtents.y +
                halfExtents.z * halfExtents.z)));
        }

        void GenerateCapsuleColliderFromBounds(CapsuleCollider &_collider, const Vector3 &_min, const Vector3 &_max)
        {
            const Vector3 halfExtents = MaxVector3((_max - _min) * 0.5f, Vector3(0.01f));
            _collider.offset = (_min + _max) * 0.5f;
            _collider.radius = std::max(0.01f, std::max(halfExtents.x, halfExtents.z));
            _collider.halfHeight = std::max(0.01f, halfExtents.y - _collider.radius);
        }

        void DrawModelColliderAssetField(const char *_id, std::string &_modelPath, i32 &_modelId)
        {
            std::string modelLabel = "[ drop a model ]";
            if (!_modelPath.empty())
            {
                if (MetaFileAsset *meta = AssetManager::GetMetaFile(_modelPath);
                    meta != nullptr && !meta->name.empty())
                {
                    modelLabel = meta->name;
                }
                else
                {
                    modelLabel = std::filesystem::path(_modelPath).stem().string();
                }
            }

            ImGui::PushID(_id);
            ImGui::TextUnformatted("model");
            ImGui::SameLine();
            ImGui::Button(modelLabel.c_str(), ImVec2(170.0f, 0.0f));
            if (ImGui::IsItemHovered() && !_modelPath.empty())
                ImGui::SetTooltip("%s", _modelPath.c_str());

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
                {
                    const AssetDragData dropped = *static_cast<const AssetDragData *>(payload->Data);
                    const std::string path = AssetManager::GetPath(dropped.uuid);
                    if (MetaFileAsset *meta = AssetManager::GetMetaFile(path);
                        meta != nullptr && meta->type == MetaFileAsset::FileType::MODEL)
                    {
                        _modelPath = meta->path.empty() ? path : meta->path;
                        _modelId = AssetManager::LoadModel(_modelPath);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if (!_modelPath.empty())
            {
                ImGui::SameLine();
                if (ImGui::SmallButton("X"))
                {
                    _modelPath.clear();
                    _modelId = -1;
                }
            }
            ImGui::PopID();
        }

        void DrawInspectorColorField(const char *_label, const char *_idSuffix, Color &_value)
        {
            DrawInspectorFieldLabel(_label);
            SetNextInspectorFieldItemWidth();
            const std::string widgetId = BuildInspectorFieldWidgetID(_label, _idSuffix);
            ImGui::ColorEdit4(widgetId.c_str(), &_value.r);
        }

        void ResolveProjectWorkingDirectory()
        {
            if (HasAssetsFolder(fs::current_path()))
                return;

            std::vector<fs::path> candidatePaths = {};
            if (const char *basePath = SDL_GetBasePath())
            {
                candidatePaths.emplace_back(basePath);
            }

            candidatePaths.push_back(fs::current_path() / "project");

            for (const fs::path &candidatePath : candidatePaths)
            {
                if (!HasAssetsFolder(candidatePath))
                    continue;

                fs::current_path(candidatePath);
                break;
            }
        }

        void LoadProjectAssetMetadata()
        {
            std::vector<std::string> paths = FindFilesInFolder("assets", "");
            for (const std::string &path : paths)
                (void)AssetManager::GetMetaFile(path);
        }

        bool IsLoadableSceneFile(const fs::path &_path)
        {
            std::error_code ec;
            if (_path.empty() || _path.extension() != ".scene")
                return false;

            if (!fs::exists(_path, ec) || !fs::is_regular_file(_path, ec))
                return false;

            try
            {
                YAML::Node root = YAML::LoadFile(_path.string());
                return root && root.IsMap();
            }
            catch (const YAML::Exception&)
            {
                return false;
            }
        }

        std::string ResolveExplicitScenePath(const std::string &_requestedPath)
        {
            const fs::path requestedPath(_requestedPath);
            if (IsLoadableSceneFile(requestedPath))
                return requestedPath.generic_string();
            if (!requestedPath.is_absolute() && requestedPath.parent_path().empty())
            {
                const fs::path scenesCandidate = fs::path("assets/scenes") / requestedPath;
                if (IsLoadableSceneFile(scenesCandidate))
                    return scenesCandidate.generic_string();
            }
            return "";
        }

        std::string FindFallbackScenePath()
        {
            std::vector<std::string> assetPaths = FindFilesInFolder("assets", "");
            std::sort(assetPaths.begin(), assetPaths.end());

            for (const std::string& assetPath : assetPaths)
            {
                const fs::path candidatePath(assetPath);
                if (!IsLoadableSceneFile(candidatePath))
                    continue;

                return candidatePath.generic_string();
            }

            return "";
        }

        YAML::Node CreateDefaultTransformNode(const Vector3& _position, const Vector3& _rotation, const Vector3& _scale)
        {
            YAML::Node transformNode(YAML::NodeType::Map);
            transformNode["active"] = true;
            transformNode["position"] = _position;
            transformNode["rotation"] = _rotation;
            transformNode["scale"] = _scale;
            transformNode["parent"] = 0;
            transformNode["children"] = YAML::Node(YAML::NodeType::Sequence);
            return transformNode;
        }

        YAML::Node CreateAssetReferenceNode(const std::string& _path)
        {
            YAML::Node assetNode(YAML::NodeType::Map);
            if (_path.empty())
                return assetNode;

            assetNode["path"] = _path;
            if (MetaFileAsset* meta = AssetManager::GetMetaFile(_path))
                assetNode["uuid"] = (uint64_t)meta->uuid;

            return assetNode;
        }

        std::string ResolveAssetReferencePath(const YAML::Node &_node)
        {
            if (!_node)
                return "";

            if (_node.IsMap())
            {
                if (auto uuidNode = _node["uuid"])
                {
                    const UUID uuid = uuidNode.as<uint64_t>(0);
                    if ((uint64_t)uuid != 0)
                    {
                        std::string path = AssetManager::GetPath(uuid);
                        if (path.rfind("Path was not found", 0) != 0)
                            return path;
                    }
                }

                if (auto pathNode = _node["path"])
                    return pathNode.as<std::string>("");

                return "";
            }

            if (_node.IsScalar())
            {
                const std::string rawValue = _node.as<std::string>("");
                if (rawValue.empty())
                    return "";

                const bool isNumeric = std::all_of(rawValue.begin(), rawValue.end(), [](unsigned char c)
                {
                    return std::isdigit(c) != 0;
                });
                if (isNumeric)
                {
                    const UUID uuid = static_cast<UUID>(std::stoull(rawValue));
                    std::string path = AssetManager::GetPath(uuid);
                    if (path.rfind("Path was not found", 0) != 0)
                        return path;
                }

                return rawValue;
            }

            return "";
        }

        std::string ToLowerCopy(std::string _value)
        {
            std::transform(_value.begin(), _value.end(), _value.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });
            return _value;
        }

        bool ApplyTypedMaterialOverride(MaterialFields &_fields, const std::string &_uniformName, const std::string &_type, const YAML::Node &_valueNode)
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
                    const std::string texturePath = ResolveAssetReferencePath(_valueNode);
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

        bool ApplyInferredMaterialOverride(MaterialFields &_fields, const std::string &_uniformName, const YAML::Node &_valueNode)
        {
            if (!_valueNode)
                return false;

            if (_valueNode.IsMap())
            {
                const std::string texturePath = ResolveAssetReferencePath(_valueNode);
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

        void EncodeMaterialOverrides(const MaterialFields &_fields, YAML::Node &_outNode)
        {
            YAML::Node uniformNode(YAML::NodeType::Map);

            for (const MaterialFields::IntUniformData &uniform : _fields.GetIntUniforms())
            {
                YAML::Node uniformValue(YAML::NodeType::Map);
                uniformValue["type"] = "int";
                uniformValue["value"] = uniform.value;
                uniformNode[uniform.name] = uniformValue;
            }

            for (const MaterialFields::FloatUniformData &uniform : _fields.GetFloatUniforms())
            {
                YAML::Node uniformValue(YAML::NodeType::Map);
                uniformValue["type"] = "float";
                uniformValue["value"] = uniform.value;
                uniformNode[uniform.name] = uniformValue;
            }

            for (const MaterialFields::Vec2UniformData &uniform : _fields.GetVec2Uniforms())
            {
                YAML::Node uniformValue(YAML::NodeType::Map);
                uniformValue["type"] = "vector2";
                uniformValue["value"] = uniform.value;
                uniformNode[uniform.name] = uniformValue;
            }

            for (const MaterialFields::Vec3UniformData &uniform : _fields.GetVec3Uniforms())
            {
                YAML::Node uniformValue(YAML::NodeType::Map);
                uniformValue["type"] = "vector3";
                uniformValue["value"] = uniform.value;
                uniformNode[uniform.name] = uniformValue;
            }

            for (const MaterialFields::Vec4UniformData &uniform : _fields.GetVec4Uniforms())
            {
                YAML::Node uniformValue(YAML::NodeType::Map);
                uniformValue["type"] = "vector4";
                uniformValue["value"] = uniform.value;
                uniformNode[uniform.name] = uniformValue;
            }

            for (const MaterialFields::ColorUniformData &uniform : _fields.GetColorUniforms())
            {
                YAML::Node uniformValue(YAML::NodeType::Map);
                uniformValue["type"] = "color";
                uniformValue["value"] = uniform.value;
                uniformNode[uniform.name] = uniformValue;
            }

            for (const MaterialFields::TextureUniformData &uniform : _fields.GetTextureUniforms())
            {
                YAML::Node uniformValue(YAML::NodeType::Map);
                uniformValue["type"] = "texture";
                if (uniform.textureId >= 0)
                {
                    const std::string texturePath = AssetManager::GetPath(uniform.textureId);
                    if (texturePath.rfind("Path was not found", 0) != 0)
                        uniformValue["value"] = CreateAssetReferenceNode(texturePath);
                    else
                        uniformValue["value"] = YAML::Node();
                }
                else
                {
                    uniformValue["value"] = YAML::Node();
                }
                uniformNode[uniform.name] = uniformValue;
            }

            if (uniformNode.size() > 0u)
                _outNode = uniformNode;
        }

        void DecodeMaterialOverrides(const YAML::Node &_node, MaterialFields &_fields)
        {
            _fields.Clear();
            if (!_node || !_node.IsMap())
                return;

            for (const auto &entry : _node)
            {
                const std::string uniformName = entry.first.as<std::string>("");
                if (uniformName.empty())
                    continue;

                const YAML::Node uniformNode = entry.second;
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
                        loaded = ApplyTypedMaterialOverride(_fields, uniformName, type, valueNode);

                    if (!loaded)
                        loaded = ApplyInferredMaterialOverride(_fields, uniformName, valueNode);
                }
                else
                {
                    loaded = ApplyInferredMaterialOverride(_fields, uniformName, uniformNode);
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

        void SaveLastEditorScenePath(const std::string& _path)
        {
            if (_path.empty())
                return;

            GetEditorConfig().lastEditorScene = MakeSceneAssetHandleFromPath(_path);
            SaveEditorConfig();
        }

        std::string GetConfiguredStartupScenePath(const bool _editorRuntimeEnabled)
        {
            if (_editorRuntimeEnabled)
            {
                const std::string editorScenePath = AssetManager::ResolvePath(GetEditorConfig().lastEditorScene);
                if (!editorScenePath.empty())
                    return editorScenePath;
            }

            const std::string launchScenePath = AssetManager::ResolvePath(GetProjectConfig().launchScene);
            if (!launchScenePath.empty())
                return launchScenePath;

            return "assets/scenes/sts/engine_splash.scene";
        }

        std::string GetDefaultSceneCreationPath(const std::string& _requestedPath)
        {
            const fs::path requestedPath(_requestedPath);
            const fs::path normalizedPath = requestedPath.lexically_normal();

            if (!normalizedPath.empty() && !normalizedPath.is_absolute() && normalizedPath.extension() == ".scene")
            {
                auto segment = normalizedPath.begin();
                if (segment != normalizedPath.end() && (*segment).string() == "assets")
                    return normalizedPath.generic_string();
            }

            return "assets/scenes/default.scene";
        }

        bool CreateDefaultSceneFile(const std::string& _scenePath, const Color& _clearColor)
        {
            if (_scenePath.empty())
                return false;

            const fs::path scenePath(_scenePath);
            std::error_code ec;
            if (scenePath.has_parent_path())
                fs::create_directories(scenePath.parent_path(), ec);

            if (ec)
                return false;

            const UUID cameraUUID = UUID();
            const UUID lightUUID = UUID();
            const UUID cubeUUID = UUID();

            YAML::Node environmentNode(YAML::NodeType::Map);
            environmentNode["ClearColor"] = _clearColor;
            environmentNode["AmbientLight"] = Color(0.24f, 0.26f, 0.32f, 1.0f);
            environmentNode["AmbientLightIntensity"] = 1.0f;
            environmentNode["PostProcessAsset"] = CreateAssetReferenceNode("assets/defaults/postprocess/default.postprocess");

            YAML::Node cameraNode(YAML::NodeType::Map);
            cameraNode["Entity"] = (uint64_t)cameraUUID;
            cameraNode["Name"] = "Camera";
            cameraNode["Tag"] = "MainCamera";
            cameraNode["Canis::Transform"] = CreateDefaultTransformNode(Vector3(0.0f, 6.0f, 14.0f), Vector3(-0.38f, 0.0f, 0.0f), Vector3(1.0f));
            YAML::Node cameraComponent(YAML::NodeType::Map);
            cameraComponent["primary"] = true;
            cameraComponent["fovDegrees"] = 60.0f;
            cameraComponent["nearClip"] = 0.1f;
            cameraComponent["farClip"] = 300.0f;
            cameraNode["Canis::Camera"] = cameraComponent;

            YAML::Node lightNode(YAML::NodeType::Map);
            lightNode["Entity"] = (uint64_t)lightUUID;
            lightNode["Name"] = "Directional Light";
            lightNode["Tag"] = "";
            YAML::Node lightComponent(YAML::NodeType::Map);
            lightComponent["enabled"] = true;
            lightComponent["color"] = Color(1.0f);
            lightComponent["intensity"] = 1.0f;
            lightComponent["direction"] = Vector3(-0.4f, -1.0f, -0.25f);
            lightNode["Canis::DirectionalLight"] = lightComponent;

            const std::string defaultMaterialPath = "assets/defaults/materials/default.material";
            const std::string defaultCubePath = "assets/defaults/models/cube.glb";

            YAML::Node cubeNode(YAML::NodeType::Map);
            cubeNode["Entity"] = (uint64_t)cubeUUID;
            cubeNode["Name"] = "Cube";
            cubeNode["Tag"] = "";
            cubeNode["Canis::Transform"] = CreateDefaultTransformNode(Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f), Vector3(1.0f));

            YAML::Node materialComponent(YAML::NodeType::Map);
            materialComponent["color"] = Color(1.0f);
            materialComponent["MaterialAsset"] = CreateAssetReferenceNode(defaultMaterialPath);
            cubeNode["Canis::Material"] = materialComponent;

            YAML::Node modelComponent(YAML::NodeType::Map);
            modelComponent["color"] = Color(1.0f);
            modelComponent["ModelAsset"] = CreateAssetReferenceNode(defaultCubePath);
            cubeNode["Canis::Model"] = modelComponent;

            YAML::Node entitiesNode(YAML::NodeType::Sequence);
            entitiesNode.push_back(cameraNode);
            entitiesNode.push_back(lightNode);
            entitiesNode.push_back(cubeNode);

            YAML::Node rootNode(YAML::NodeType::Map);
            rootNode["Environment"] = environmentNode;
            rootNode["Entities"] = entitiesNode;

            YAML::Emitter out;
            out << rootNode;

            std::ofstream file(scenePath);
            if (!file.is_open())
                return false;

            file << out.c_str();
            file.close();

            if (!file.good())
                return false;

            (void)AssetManager::GetMetaFile(scenePath.generic_string());
            return true;
        }

        PostProcessResult ApplyScenePostProcess(
            Scene& _scene,
            unsigned int _sourceFramebuffer,
            unsigned int _sourceColorTexture,
            unsigned int _sourceDepthTexture,
            int _width,
            int _height,
            RenderTarget *_outputTarget)
        {
            const PostProcessAsset *postProcess = nullptr;
            const UUID postProcessUUID = _scene.GetEnvironmentPostProcessUUID();
            if ((uint64_t)postProcessUUID != 0)
            {
                const std::string postProcessPath = AssetManager::GetPath(postProcessUUID);
                if (postProcessPath != "Path was not found in AssetLibrary")
                    postProcess = AssetManager::GetPostProcess(postProcessPath);
            }

            return ApplyPostProcessChain(
                postProcess,
                _sourceFramebuffer,
                _sourceColorTexture,
                _sourceDepthTexture,
                _width,
                _height,
                _scene.GetLastRenderProjection(),
                _outputTarget);
        }

        std::string ResolvePendingSceneLoadPath(const std::string& _requestedPath, const Color& _clearColor)
        {
            const fs::path requestedPath(_requestedPath);
            if (IsLoadableSceneFile(requestedPath))
                return requestedPath.generic_string();

            if (!requestedPath.is_absolute() && requestedPath.parent_path().empty())
            {
                const fs::path scenesCandidate = fs::path("assets/scenes") / requestedPath;
                if (IsLoadableSceneFile(scenesCandidate))
                    return scenesCandidate.generic_string();
            }

            Debug::Warning("Scene load requested for missing or invalid scene: %s", _requestedPath.c_str());

            const std::string fallbackScenePath = FindFallbackScenePath();
            if (!fallbackScenePath.empty())
            {
                Debug::Warning("Falling back to existing scene: %s", fallbackScenePath.c_str());
                return fallbackScenePath;
            }

            const std::string defaultScenePath = GetDefaultSceneCreationPath(_requestedPath);
            if (CreateDefaultSceneFile(defaultScenePath, _clearColor))
            {
                Debug::Warning("Created default scene: %s", defaultScenePath.c_str());
                return defaultScenePath;
            }

            Debug::Error("Failed to resolve or create a scene for requested path: %s", _requestedPath.c_str());
            return "";
        }

#if CANIS_EDITOR
        std::vector<ScriptConf*> GetConnectedUIScriptOptions(App& _app, Entity* _targetEntity)
        {
            std::vector<ScriptConf*> options = {};

            if (_targetEntity == nullptr)
                return options;

            for (ScriptConf& conf : _app.GetScriptRegistry())
            {
                if (conf.uiActions.empty() || conf.Has == nullptr || conf.Get == nullptr)
                    continue;

                if (!conf.Has(*_targetEntity))
                    continue;

                if (conf.Get(*_targetEntity) == nullptr)
                    continue;

                options.push_back(&conf);
            }

            std::sort(options.begin(), options.end(), [](const ScriptConf* _left, const ScriptConf* _right) -> bool
            {
                if (_left == nullptr || _right == nullptr)
                    return _left != nullptr;

                return _left->name < _right->name;
            });

            return options;
        }

        std::vector<std::string> GetConnectedUIActionOptions(ScriptConf* _scriptConf)
        {
            std::vector<std::string> options = {};

            if (_scriptConf == nullptr)
                return options;

            options.reserve(_scriptConf->uiActions.size());
            for (const auto& [actionName, actionInvoker] : _scriptConf->uiActions)
            {
                (void)actionInvoker;
                options.push_back(actionName);
            }

            std::sort(options.begin(), options.end());
            return options;
        }

        void DrawConnectedUIActionSelector(App& _app, Entity* _targetEntity, std::string& _targetScript, std::string& _actionName, const char* _idSuffix)
        {
            std::vector<ScriptConf*> scriptOptions = GetConnectedUIScriptOptions(_app, _targetEntity);

            ScriptConf* selectedScript = nullptr;
            for (ScriptConf* option : scriptOptions)
            {
                if (option != nullptr && option->name == _targetScript)
                {
                    selectedScript = option;
                    break;
                }
            }

            if (selectedScript == nullptr)
            {
                _targetScript.clear();
                _actionName.clear();
            }

            const std::string scriptLabel = BuildInspectorFieldLabel("targetScript", _idSuffix);
            const char* scriptPreview = _targetEntity == nullptr ? "<Select Target Entity>" :
                (_targetScript.empty() ? "<Select Script>" : _targetScript.c_str());

            ImGui::BeginDisabled(_targetEntity == nullptr);
            if (ImGui::BeginCombo(scriptLabel.c_str(), scriptPreview))
            {
                const bool noneSelected = _targetScript.empty();
                if (ImGui::Selectable("<None>", noneSelected))
                {
                    _targetScript.clear();
                    _actionName.clear();
                    selectedScript = nullptr;
                }

                if (noneSelected)
                    ImGui::SetItemDefaultFocus();

                for (ScriptConf* option : scriptOptions)
                {
                    if (option == nullptr)
                        continue;

                    const bool isSelected = (_targetScript == option->name);
                    if (ImGui::Selectable(option->name.c_str(), isSelected))
                    {
                        _targetScript = option->name;
                        selectedScript = option;
                        _actionName.clear();
                    }

                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }
            ImGui::EndDisabled();

            std::vector<std::string> actionOptions = GetConnectedUIActionOptions(selectedScript);
            const bool hasSelectedAction = std::find(actionOptions.begin(), actionOptions.end(), _actionName) != actionOptions.end();
            if (!hasSelectedAction)
                _actionName.clear();

            const std::string actionLabel = BuildInspectorFieldLabel("actionName", _idSuffix);
            const bool disableActions = selectedScript == nullptr;
            const char* actionPreview = disableActions ? "<Select Script First>" :
                (_actionName.empty() ? "<Select Action>" : _actionName.c_str());

            ImGui::BeginDisabled(disableActions);
            if (ImGui::BeginCombo(actionLabel.c_str(), actionPreview))
            {
                const bool noneSelected = _actionName.empty();
                if (ImGui::Selectable("<None>", noneSelected))
                    _actionName.clear();

                if (noneSelected)
                    ImGui::SetItemDefaultFocus();

                for (const std::string& actionName : actionOptions)
                {
                    const bool isSelected = (_actionName == actionName);
                    if (ImGui::Selectable(actionName.c_str(), isSelected))
                        _actionName = actionName;

                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }
            ImGui::EndDisabled();
        }
#endif
    }

    App::~App()
    {
        ShutdownRuntime();
    }

    void* App::GetGameCodeData()
    {
        return m_runtime != nullptr ? m_runtime->gameCodeObject.gameData : nullptr;
    }

    const void* App::GetGameCodeData() const
    {
        return m_runtime != nullptr ? m_runtime->gameCodeObject.gameData : nullptr;
    }

    void App::InitializeRuntime()
    {
        if (m_runtime != nullptr)
            return;

        Debug::Log("App Run");
        ResolveProjectWorkingDirectory();
        LoadProjectAssetMetadata();
        Canis::Init();

        m_runtime = new RuntimeContext();
        RuntimeContext &runtime = *m_runtime;

        std::string launchError = {};
        const fs::path invocationDirectory = m_invocationWorkingDirectory.empty()
            ? fs::current_path()
            : fs::path(m_invocationWorkingDirectory);
        if (!ParseRuntimeLaunchOptions(
                m_commandLineArguments,
                invocationDirectory,
                runtime.launch,
                launchError))
        {
            Debug::Error("%s", launchError.c_str());
            runtime.exitReason = "invalid-command-line";
            runtime.launch.active = false;
            m_exitCode = 2;
            return;
        }
        if (!LoadRuntimeTimeline(runtime.launch.inputScript, runtime.timeline, launchError))
        {
            Debug::Error("%s", launchError.c_str());
            runtime.exitReason = "input-script-error";
            m_exitCode = 3;
            return;
        }
        if (runtime.launch.randomSeed.has_value())
        {
            Canis::GetProjectConfig().overrideSeed = true;
            Canis::GetProjectConfig().seed = runtime.launch.randomSeed.value();
            Debug::Log(
                "Random seed set from command line: %u",
                runtime.launch.randomSeed.value());
        }

#if CANIS_EDITOR
        runtime.editorRuntimeEnabled = Canis::GetProjectConfig().editor;
        const char *editorModeSource = "project.canis";
        if (runtime.launch.editorRuntimeOverride.has_value())
        {
            runtime.editorRuntimeEnabled =
                runtime.launch.editorRuntimeOverride.value();
            editorModeSource = "command line";
        }
        else
        {
            bool editorRuntimeOverride = runtime.editorRuntimeEnabled;
            if (TryParseEnvironmentBool(
                    std::getenv("CANIS_EDITOR_RUNTIME"),
                    editorRuntimeOverride) ||
                TryParseEnvironmentBool(
                    std::getenv("CANIS_EDITOR"),
                    editorRuntimeOverride))
            {
                runtime.editorRuntimeEnabled = editorRuntimeOverride;
                editorModeSource = "environment";
            }
        }
        Debug::Log(
            "Editor runtime %s (%s).",
            runtime.editorRuntimeEnabled ? "enabled" : "disabled",
            editorModeSource);
#endif
        Canis::SetEditorRuntimeEnabled(runtime.editorRuntimeEnabled);

        const int configuredWidth = std::max(320, runtime.editorRuntimeEnabled ? GetProjectConfig().editorWindowWidth
                                                                                : GetProjectConfig().targetGameWidth);
        const int configuredHeight = std::max(240, runtime.editorRuntimeEnabled ? GetProjectConfig().editorWindowHeight
                                                                                 : GetProjectConfig().targetGameHeight);
        const int startupWidth = EnvironmentDimension("CANIS_WINDOW_WIDTH", configuredWidth, 320);
        const int startupHeight = EnvironmentDimension("CANIS_WINDOW_HEIGHT", configuredHeight, 240);
        if (startupWidth != configuredWidth || startupHeight != configuredHeight)
            Debug::Log("Runtime window override: %dx%d.", startupWidth, startupHeight);
        const std::string windowTitle = GetProjectConfig().gameName.empty()
            ? std::string("Canis Game") : GetProjectConfig().gameName;
        WindowOptions windowOptions = {};
        if (!runtime.editorRuntimeEnabled)
        {
            switch (GetProjectConfig().windowMode)
            {
                case PROJECT_WINDOW_BORDERLESS:
                    windowOptions.mode = NativeWindowMode::BORDERLESS;
                    break;
                case PROJECT_WINDOW_FULLSCREEN:
                    windowOptions.mode = NativeWindowMode::FULLSCREEN;
                    break;
                case PROJECT_WINDOW_WINDOWED:
                default:
                    windowOptions.mode = NativeWindowMode::WINDOWED;
                    break;
            }
            windowOptions.resizable = GetProjectConfig().windowResizable;
            windowOptions.startMaximized = GetProjectConfig().windowStartMaximized;
        }
        runtime.window = std::make_unique<Window>(
            windowTitle.c_str(), startupWidth, startupHeight, runtime.launch.offscreen, windowOptions);
        runtime.window->SetClearColor(Color(1.0f));
        runtime.window->SetSync(static_cast<Window::Sync>(GetProjectConfig().syncMode));
        AudioManager::Initialize();

        if (GetProjectConfig().iconUUID == UUID(0))
        {
            if (MetaFileAsset *iconMeta = AssetManager::GetMetaFile("assets/defaults/textures/engine_icon.png"))
            {
                GetProjectConfig().iconUUID = iconMeta->uuid;
                SaveProjectConfig();
            }
        }

        const std::string iconPath = AssetManager::GetPath(GetProjectConfig().iconUUID);
        if (iconPath != "Path was not found in AssetLibrary")
            runtime.window->SetWindowIcon(iconPath);

        runtime.editor = std::make_unique<Editor>();
        m_editor = runtime.editor.get();
#if CANIS_EDITOR
        if (runtime.editorRuntimeEnabled)
            runtime.editor->Init(runtime.window.get());
#endif

        RegisterDefaults(*runtime.editor);

        runtime.inputManager = std::make_unique<InputManager>();
        runtime.inputManager->SetGameInputWindowID(SDL_GetWindowID((SDL_Window*)runtime.window->GetSDLWindow()));

        if (Canis::GetProjectConfig().useFrameLimit)
            Time::Init(Canis::GetProjectConfig().frameLimit + 0.0f);
        else
            Time::Init(100000.0f);
        runtime.timeInitialized = true;

#if CANIS_EDITOR
        if (runtime.editorRuntimeEnabled)
            Time::SetTargetFPS(Canis::GetProjectConfig().frameLimitEditor + 0.0f);
#endif
        if (runtime.launch.forcedFPS > 0.0f)
            Time::SetTargetFPS(runtime.launch.forcedFPS);
        if (runtime.launch.fixedDelta > 0.0f)
            Time::SetFixedDelta(runtime.launch.fixedDelta);

        const char* startupSceneOverride = std::getenv("CANIS_START_SCENE");
        const std::string requestedStartupScenePath = !runtime.launch.launchScene.empty()
            ? runtime.launch.launchScene
            : ((startupSceneOverride != nullptr && startupSceneOverride[0] != '\0')
                ? std::string(startupSceneOverride)
                : GetConfiguredStartupScenePath(runtime.editorRuntimeEnabled));
        const std::string startupScenePath = runtime.launch.launchScene.empty()
            ? ResolvePendingSceneLoadPath(requestedStartupScenePath, runtime.window->GetClearColor())
            : ResolveExplicitScenePath(requestedStartupScenePath);
        if (startupScenePath.empty())
        {
            Debug::Error("Failed to resolve startup scene from '%s'.", requestedStartupScenePath.c_str());
            runtime.exitReason = "scene-load-error";
            m_exitCode = 3;
            return;
        }

        if (runtime.editorRuntimeEnabled)
        {
            SaveLastEditorScenePath(startupScenePath);
#if CANIS_EDITOR
            runtime.editor->LoadSceneCameraConfig();
#endif
        }

        scene.Init(this, runtime.window.get(), runtime.inputManager.get());
        m_network = std::make_unique<NetworkSession>(*this);

        runtime.gameCodeObject = GameCodeObjectInit(GetGameCodeSharedObjectPath());
        BeginGameCodeRegistration();
        GameCodeObjectInitFunction(&runtime.gameCodeObject, this);
        EndGameCodeRegistration();
        runtime.gameCodeInitialized = true;

        scene.Load(startupScenePath);
        runtime.launch.launchScene = startupScenePath;
        if (runtime.launch.interactive)
        {
            runtime.interactiveFramesRemaining = 1u;
            runtime.interactiveCaptureLabel = "initial";
        }

        if (runtime.launch.active)
        {
            Debug::Log(
                "Runtime test harness active: scene=%s fixedDelta=%.6f forceFPS=%.2f events=%zu offscreen=%s",
                startupScenePath.c_str(),
                runtime.launch.fixedDelta,
                runtime.launch.forcedFPS,
                runtime.timeline.size(),
                runtime.launch.offscreen ? "true" : "false");
        }
    }

    bool App::RunFrame()
    {
        if (m_runtime == nullptr)
            return false;

        RuntimeContext &runtime = *m_runtime;
        if (runtime.window == nullptr || runtime.inputManager == nullptr ||
            runtime.editor == nullptr || !runtime.gameCodeInitialized)
            return false;

        Window &window = *runtime.window;
        Editor &editor = *runtime.editor;
        InputManager &inputManager = *runtime.inputManager;
        GameCodeObject &gameCodeObject = runtime.gameCodeObject;

        if (runtime.launch.interactive && runtime.interactiveWaitingForCommand)
        {
            while (true)
            {
                std::string commandLine = {};
                if (!std::getline(std::cin, commandLine))
                {
                    runtime.exitReason = "interactive-stdin-closed";
                    return false;
                }
                if (commandLine.empty())
                    continue;

                RuntimeInteractiveCommand command = {};
                std::string commandError = {};
                if (!ParseRuntimeInteractiveCommand(
                        commandLine,
                        runtime.simulationTime,
                        runtime.launch.fixedDelta,
                        command,
                        commandError))
                {
                    std::cout << "CANIS_INTERACTIVE_ERROR {\"message\":\""
                              << EscapeJson(commandError) << "\"}" << std::endl;
                    continue;
                }

                runtime.interactiveWaitingForCommand = false;
                runtime.interactiveFramesRemaining = command.frames;
                runtime.interactiveCaptureLabel = command.captureLabel.empty()
                    ? "step_" + std::to_string(++runtime.interactiveStep)
                    : command.captureLabel;
                runtime.stopAfterFrame = command.stop;
                for (RuntimeTimelineEvent &event : command.events)
                    runtime.timeline.push_back(std::move(event));

                std::cout << "CANIS_INTERACTIVE_ACK {\"frames\":"
                          << runtime.interactiveFramesRemaining
                          << ",\"capture\":\""
                          << EscapeJson(runtime.interactiveCaptureLabel)
                          << "\",\"stop\":" << (command.stop ? "true" : "false")
                          << "}" << std::endl;
                break;
            }
        }

        inputManager.BeginSyntheticInputFrame();
        if (!inputManager.Update((void *)&window))
            return false;

        if (window.ShouldClose())
            return false;

        if (window.IsResized())
        {
            if (runtime.editorRuntimeEnabled)
            {
                GetProjectConfig().editorWindowWidth = window.GetWindowWidth();
                GetProjectConfig().editorWindowHeight = window.GetWindowHeight();
            }
            else if (GetProjectConfig().windowMode != PROJECT_WINDOW_FULLSCREEN)
            {
                GetProjectConfig().targetGameWidth = window.GetWindowWidth();
                GetProjectConfig().targetGameHeight = window.GetWindowHeight();
            }
        }

        if (inputManager.ConsumeResumeFrameResetRequest())
            Time::ResetFrameClock();

        f32 deltaTime = Time::StartFrame();

        for (const unsigned int key : runtime.pulseKeys)
            inputManager.SetSyntheticKey(key, false);
        runtime.pulseKeys.clear();
        for (const unsigned int button : runtime.pulseMouseButtons)
            inputManager.SetSyntheticMouseButton(button, false);
        runtime.pulseMouseButtons.clear();
        for (const unsigned int button : runtime.pulseGamepadButtons)
            inputManager.SetSyntheticGamepadButton(button, false);
        runtime.pulseGamepadButtons.clear();

        constexpr double timelineEpsilon = 1e-7;
        while (runtime.nextTimelineEvent < runtime.timeline.size() &&
               runtime.timeline[runtime.nextTimelineEvent].time <=
                   runtime.simulationTime + timelineEpsilon)
        {
            Debug::Log(
                "Applying runtime event %zu at %.3fs (simulation %.3fs).",
                runtime.nextTimelineEvent,
                runtime.timeline[runtime.nextTimelineEvent].time,
                runtime.simulationTime);
            ApplyRuntimeTimelineEvent(
                runtime.timeline[runtime.nextTimelineEvent],
                inputManager,
                runtime.pulseKeys,
                runtime.pulseMouseButtons,
                runtime.pulseGamepadButtons,
                runtime.pendingCaptures,
                runtime.stopAfterFrame);
            ++runtime.nextTimelineEvent;
        }
        if (runtime.launch.stopAt >= 0.0 &&
            runtime.simulationTime + timelineEpsilon >= runtime.launch.stopAt)
        {
            runtime.stopAfterFrame = true;
            runtime.exitReason = "stop-time-reached";
        }

        bool runGameTick = true;
#if CANIS_EDITOR
        if (runtime.editorRuntimeEnabled)
            runGameTick = (editor.m_mode == EditorMode::PLAY);
#endif

        if (runGameTick)
        {
            if (m_network != nullptr)
                m_network->Update(deltaTime);

            Uint64 sceneUpdateStart = SDL_GetTicksNS();
            scene.Update(deltaTime);
            m_sceneUpdateTimeMs = static_cast<float>(SDL_GetTicksNS() - sceneUpdateStart) / 1000000.0f;

            bool stopPlayModeRequested = false;
#if CANIS_EDITOR
            if (runtime.editorRuntimeEnabled)
                stopPlayModeRequested = editor.ConsumeStopPlayModeRequest();
#endif

            if (!stopPlayModeRequested)
            {
                Uint64 gameCodeUpdateStart = SDL_GetTicksNS();
                GameCodeObjectUpdateFunction(&gameCodeObject, this, deltaTime);
                m_gameCodeUpdateTimeMs = static_cast<float>(SDL_GetTicksNS() - gameCodeUpdateStart) / 1000000.0f;

#if CANIS_EDITOR
                if (runtime.editorRuntimeEnabled)
                    stopPlayModeRequested = editor.ConsumeStopPlayModeRequest();
#endif
            }
            else
            {
                m_gameCodeUpdateTimeMs = 0.0f;
            }

#if CANIS_EDITOR
            if (runtime.editorRuntimeEnabled && stopPlayModeRequested)
                editor.StopPlayMode();
#endif

            m_updateTimeMs = m_sceneUpdateTimeMs + m_gameCodeUpdateTimeMs;
        }
        else
        {
            scene.UpdateEditor();
            m_updateTimeMs = 0.0f;
            m_sceneUpdateTimeMs = 0.0f;
            m_gameCodeUpdateTimeMs = 0.0f;
        }

        ProcessPendingSceneLoad();

        if (!window.MakeContextCurrent())
        {
            Debug::Warning("Skipping render frame because the OpenGL context is unavailable after resume.");
            m_renderTimeMs = 0.0f;
            Time::ResetFrameClock();
            Time::EndFrame();
            return true;
        }

        if (!window.HasDrawableSurface())
        {
            m_renderTimeMs = 0.0f;
            Time::EndFrame();
            return true;
        }

        Uint64 renderStart = SDL_GetTicksNS();
        unsigned int captureFramebuffer = 0u;
        int captureWidth = 0;
        int captureHeight = 0;
        window.Clear();
#if CANIS_EDITOR
        if (runtime.editorRuntimeEnabled)
        {
            editor.Draw(&scene, &window, this, &gameCodeObject, deltaTime);
            inputManager.SetGameInputWindowID(editor.GetGameInputWindowID());
        }
        else
#endif
        {
            EnsureRenderTarget(runtime.runtimeRenderTarget, window.GetScreenWidth(), window.GetScreenHeight());

            glBindFramebuffer(GL_FRAMEBUFFER, runtime.runtimeRenderTarget.framebuffer);
            glViewport(0, 0, runtime.runtimeRenderTarget.width, runtime.runtimeRenderTarget.height);

            Color clear = window.GetClearColor();
            glClearColor(clear.r, clear.g, clear.b, clear.a);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            scene.Render(deltaTime);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            PostProcessResult postProcessResult = ApplyScenePostProcess(
                scene,
                runtime.runtimeRenderTarget.framebuffer,
                runtime.runtimeRenderTarget.colorTexture,
                runtime.runtimeRenderTarget.depthTexture,
                runtime.runtimeRenderTarget.width,
                runtime.runtimeRenderTarget.height,
                &runtime.runtimePostProcessTarget);
            captureFramebuffer = postProcessResult.framebuffer;
            captureWidth = runtime.runtimeRenderTarget.width;
            captureHeight = runtime.runtimeRenderTarget.height;

            BlitFramebuffer(
                postProcessResult.framebuffer,
                runtime.runtimeRenderTarget.width,
                runtime.runtimeRenderTarget.height,
                0,
                window.GetWindowWidth(),
                window.GetWindowHeight());

            inputManager.SetGameInputWindowID(SDL_GetWindowID((SDL_Window*)window.GetSDLWindow()));
        }

        if (runtime.launch.interactive &&
            runtime.interactiveFramesRemaining == 1u &&
            !runtime.interactiveCaptureLabel.empty())
        {
            runtime.pendingCaptures.push_back(runtime.interactiveCaptureLabel);
        }

        if (runtime.stopAfterFrame && runtime.launch.active &&
            runtime.launch.captureFinal && !runtime.launch.interactive)
        {
            if (std::find(runtime.pendingCaptures.begin(), runtime.pendingCaptures.end(), "final") ==
                runtime.pendingCaptures.end())
                runtime.pendingCaptures.push_back("final");
        }

        for (const std::string &requestedLabel : runtime.pendingCaptures)
        {
            const std::string label = requestedLabel.empty() ? "capture" : requestedLabel;
            std::ostringstream timestamp = {};
            timestamp << std::fixed << std::setprecision(3) << runtime.simulationTime;
            const std::string fileName =
                std::to_string(runtime.captures.size()) + "_" +
                SanitizeFileName(label) + "_" +
                SanitizeFileName(timestamp.str()) + ".png";
            const fs::path outputPath = runtime.launch.captureDirectory / fileName;
            if (!WriteRuntimeFramebuffer(
                    captureFramebuffer,
                    captureWidth,
                    captureHeight,
                    outputPath))
            {
                Debug::Error("Failed to capture runtime framebuffer to %s.", outputPath.generic_string().c_str());
                m_exitCode = 4;
                runtime.exitReason = "capture-error";
                runtime.stopAfterFrame = true;
                continue;
            }
            runtime.captures.push_back({
                runtime.simulationTime,
                label,
                fs::absolute(outputPath).generic_string()});
            Debug::Log(
                "Captured runtime framebuffer '%s' at %.3fs.",
                label.c_str(),
                runtime.simulationTime);
        }
        runtime.pendingCaptures.clear();

        window.SwapBuffer();
        m_renderTimeMs = static_cast<float>(SDL_GetTicksNS() - renderStart) / 1000000.0f;

        Time::EndFrame();
        ++runtime.simulationFrame;

        if (runtime.launch.interactive && runtime.interactiveFramesRemaining > 0u)
        {
            --runtime.interactiveFramesRemaining;
            if (runtime.interactiveFramesRemaining == 0u)
            {
                const RuntimeCaptureRecord *capture =
                    runtime.captures.empty() ? nullptr : &runtime.captures.back();
                std::cout << "CANIS_INTERACTIVE_FRAME {\"time\":"
                          << runtime.simulationTime
                          << ",\"frame\":" << runtime.simulationFrame
                          << ",\"label\":\""
                          << EscapeJson(capture != nullptr ? capture->label : "")
                          << "\",\"path\":\""
                          << EscapeJson(capture != nullptr ? capture->path : "")
                          << "\"}" << std::endl;
                if (!runtime.stopAfterFrame)
                    runtime.interactiveWaitingForCommand = true;
            }
        }

        if (runtime.stopAfterFrame)
        {
            if (runtime.exitReason == "window-closed")
                runtime.exitReason = "timeline-complete";
            if (!WriteRuntimeManifest(
                    runtime.launch,
                    runtime.exitReason,
                    runtime.simulationTime,
                    runtime.simulationFrame,
                    runtime.captures,
                    m_exitCode))
            {
                Debug::Error("Failed to write runtime test manifest.");
                m_exitCode = 5;
            }
            runtime.harnessCompleted = true;
            return false;
        }

        runtime.simulationTime += static_cast<double>(deltaTime);

        return true;
    }

    void App::ProcessPendingSceneLoad()
    {
        if (m_pendingScenePath.empty() || m_runtime == nullptr)
            return;

        RuntimeContext &runtime = *m_runtime;
        const std::string requestedScenePath = m_pendingScenePath;
        m_pendingScenePath.clear();

        const std::string nextScenePath = ResolvePendingSceneLoadPath(requestedScenePath, runtime.window->GetClearColor());
        if (nextScenePath.empty())
            return;

#if CANIS_EDITOR
        if (runtime.editorRuntimeEnabled && runtime.editor != nullptr && runtime.editor->m_mode == EditorMode::EDIT)
            SaveLastEditorScenePath(nextScenePath);
#endif

        scene.Unload();
        scene.Init(this, runtime.window.get(), runtime.inputManager.get());
        scene.Load(nextScenePath);
    }

    void App::ShutdownRuntime()
    {
        if (m_runtime == nullptr)
            return;

        RuntimeContext *runtime = m_runtime;
        m_runtime = nullptr;

        if (runtime->launch.active && !runtime->harnessCompleted)
        {
            if (!WriteRuntimeManifest(
                    runtime->launch,
                    runtime->exitReason,
                    runtime->simulationTime,
                    runtime->simulationFrame,
                    runtime->captures,
                    m_exitCode))
            {
                Debug::Error("Failed to write runtime test manifest.");
                if (m_exitCode == 0)
                    m_exitCode = 5;
            }
        }

        if (runtime->window != nullptr)
        {
            if (runtime->editorRuntimeEnabled)
            {
                GetProjectConfig().editorWindowWidth = runtime->window->GetWindowWidth();
                GetProjectConfig().editorWindowHeight = runtime->window->GetWindowHeight();
            }
            else if (GetProjectConfig().windowMode != PROJECT_WINDOW_FULLSCREEN)
            {
                GetProjectConfig().targetGameWidth = runtime->window->GetWindowWidth();
                GetProjectConfig().targetGameHeight = runtime->window->GetWindowHeight();
            }

            SaveProjectConfig();
        }

        if (runtime->window != nullptr)
            scene.Unload();
        if (runtime->timeInitialized)
            Time::Quit();
        if (runtime->gameCodeInitialized)
            GameCodeObjectShutdownFunction(&runtime->gameCodeObject, this);
        m_network.reset();

        // Destroy any remaining std::function state while the game shared object is still loaded.
        m_inspectorItemRegistry.clear();
        m_systemRegistry.clear();
        m_scriptRegistry.clear();

        if (runtime->window != nullptr)
            AudioManager::Shutdown();
        if (runtime->gameCodeInitialized)
            GameCodeObjectDestroy(&runtime->gameCodeObject);
        DestroyRenderTarget(runtime->runtimeRenderTarget);
        DestroyRenderTarget(runtime->runtimePostProcessTarget);
        m_editor = nullptr;
        delete runtime;
    }

#if defined(__EMSCRIPTEN__)
    void App::WebMainLoop(void *_appPtr)
    {
        App *app = static_cast<App *>(_appPtr);
        if (app == nullptr)
            return;

        if (!app->RunFrame())
        {
            app->ShutdownRuntime();
            emscripten_cancel_main_loop();
        }
    }
#endif

    int App::Run(int _argc, char **_argv)
    {
        m_exitCode = 0;
        m_commandLineArguments.clear();
        m_invocationWorkingDirectory = std::filesystem::current_path().generic_string();
        for (int index = 1; index < _argc; ++index)
        {
            if (_argv[index] != nullptr)
                m_commandLineArguments.emplace_back(_argv[index]);
        }

        if (std::find(m_commandLineArguments.begin(), m_commandLineArguments.end(), "--help") !=
                m_commandLineArguments.end() ||
            std::find(m_commandLineArguments.begin(), m_commandLineArguments.end(), "-h") !=
                m_commandLineArguments.end())
        {
            std::cout
                << "Canis runtime options:\n"
                << "  --editor                    Force the editor runtime on.\n"
                << "  --force-game-only           Bypass the editor and run the game directly.\n"
                << "  --game-only                 Alias for --force-game-only.\n"
                << "  --launch-scene PATH         Launch a scene path or a name under assets/scenes.\n"
                << "  --input-script PATH         Replay timestamped JSON/YAML input events.\n"
                << "  --interactive-test          Pause after each input batch and return a framebuffer.\n"
                << "  --force-fps NUMBER          Fix simulation delta to 1/FPS and pace rendering.\n"
                << "  --fixed-delta SECONDS       Use an exact simulation delta without changing pacing.\n"
                << "  --seed NUMBER               Seed runtime randomness with an unsigned 32-bit integer.\n"
                << "  --stop-at SECONDS           Stop after rendering at the requested simulation time.\n"
                << "  --capture-dir PATH          Write PNG captures and manifest.json here.\n"
                << "  --capture-final             Capture the final rendered frame (default).\n"
                << "  --no-final-capture          Disable automatic final-frame capture.\n"
                << "  --offscreen                 Create a hidden OpenGL window for automated runs.\n";
            return 0;
        }

#if CANIS_EDITOR && !defined(__EMSCRIPTEN__)
        std::optional<std::filesystem::path> environmentProject = ResolveProjectFromEnvironment();
        const char *environmentProjectPath = std::getenv("CANIS_PROJECT");
        if (!environmentProject.has_value() && environmentProjectPath != nullptr && environmentProjectPath[0] != '\0')
        {
            Debug::Error("CANIS_PROJECT is not a valid project folder: %s", environmentProjectPath);
            return 2;
        }

        if (environmentProject.has_value())
        {
            std::error_code ec;
            std::filesystem::current_path(*environmentProject, ec);
            if (ec)
            {
                Debug::Error("Failed to open project from CANIS_PROJECT: %s", environmentProject->generic_string().c_str());
                return 2;
            }
        }
#endif

        InitializeRuntime();

#if defined(__EMSCRIPTEN__)
        emscripten_set_main_loop_arg(&App::WebMainLoop, this, 0, true);
#else
        while (RunFrame())
        {
        }
        ShutdownRuntime();
#endif
        return m_exitCode;
    }

    void App::RegisterDefaults(Editor& _editor)
    {
        _editor.RegisterInspectorFieldDrawer<Canis::Entity*>([](Editor& _editor, const char* _label, const char* _idSuffix, Canis::Entity*& _value)
        {
            _editor.InputEntity(_label, _idSuffix, _value);
        });

        _editor.RegisterInspectorFieldDrawer<Canis::AudioAssetHandle>([](Editor& _editor, const char* _label, const char* _idSuffix, Canis::AudioAssetHandle& _value)
        {
            _editor.InputAudioAsset(_label, _idSuffix, _value);
        });

        _editor.RegisterInspectorFieldDrawer<Canis::SceneAssetHandle>([](Editor& _editor, const char* _label, const char* _idSuffix, Canis::SceneAssetHandle& _value)
        {
            _editor.InputSceneAsset(_label, _idSuffix, _value);
        });

        _editor.RegisterInspectorFieldDrawer<Canis::AnimationClipAssetHandle>([](Editor& _editor, const char* _label, const char* _idSuffix, Canis::AnimationClipAssetHandle& _value)
        {
            _editor.InputAnimationClipAsset(_label, _idSuffix, _value);
        });

        _editor.RegisterInspectorFieldDrawer<Canis::AnimatorControllerAssetHandle>([](Editor& _editor, const char* _label, const char* _idSuffix, Canis::AnimatorControllerAssetHandle& _value)
        {
            _editor.InputAnimatorControllerAsset(_label, _idSuffix, _value);
        });

        _editor.RegisterInspectorFieldDrawer<Canis::TerrainAssetHandle>([](Editor& _editor, const char* _label, const char* _idSuffix, Canis::TerrainAssetHandle& _value)
        {
            const std::string label = (_label != nullptr && _label[0] != '\0') ? _label : "Terrain Data";
            _editor.InputTerrainAsset(label == "terrain" ? "Terrain Data" : label, _idSuffix, _value);
        });

        ScriptConf prefabInstanceConf = {
            .name = "Canis::PrefabInstance",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void {
                _entity.AddComponent<PrefabInstance>();
            },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<PrefabInstance>(); },
            .Remove = [this](Entity& _entity) -> void { _entity.RemoveComponent<PrefabInstance>(); },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<PrefabInstance>() ? (void*)(&_entity.GetComponent<PrefabInstance>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                PrefabInstance* prefabInstance = _entity.HasComponent<PrefabInstance>() ? &_entity.GetComponent<PrefabInstance>() : nullptr;
                if (prefabInstance == nullptr)
                    return;

                YAML::Node comp;
                comp["prefab"] = prefabInstance->prefab;
                comp["firstEntity"] = _entity.scene.GetLiveEntityUUID(prefabInstance->firstEntity);
                _node["Canis::PrefabInstance"] = comp;
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                YAML::Node comp = _node["Canis::PrefabInstance"];
                if (!comp)
                    return;

                PrefabInstance &prefabInstance = *_entity.AddComponent<PrefabInstance>();
                prefabInstance.prefab = comp["prefab"].as<SceneAssetHandle>(prefabInstance.prefab);

                if (comp["firstEntity"].as<Canis::UUID>(0) != Canis::UUID(0))
                    _entity.scene.GetEntityAfterLoad(comp["firstEntity"].as<Canis::UUID>(0), prefabInstance.firstEntity);

                if (_callCreate)
                    prefabInstance.Create();
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                PrefabInstance* prefabInstance = _entity.HasComponent<PrefabInstance>() ? &_entity.GetComponent<PrefabInstance>() : nullptr;
                if (prefabInstance == nullptr)
                    return;

                DrawInspectorField(_editor, "prefab", _conf.name.c_str(), prefabInstance->prefab);
                DrawInspectorField(_editor, "firstEntity", _conf.name.c_str(), prefabInstance->firstEntity);

                if (ImGui::Button(BuildInspectorFieldLabel("Rebuild From Prefab", _conf.name.c_str()).c_str()))
                {
                    _editor.RebuildPrefabInstance(&_entity);
                    return;
                }

                if (ImGui::Button(BuildInspectorFieldLabel("Apply Overrides To Prefab", _conf.name.c_str()).c_str()))
                {
                    _editor.ApplyPrefabInstanceOverrides(&_entity);
                    return;
                }

                if (ImGui::Button(BuildInspectorFieldLabel("Rebuild All Prefabs In Scene", _conf.name.c_str()).c_str()))
                {
                    _editor.RebuildAllPrefabInstances();
                    return;
                }

                if (prefabInstance->firstEntity != nullptr)
                {
                    if (ImGui::Button(BuildInspectorFieldLabel("Select First Entity", _conf.name.c_str()).c_str()))
                        _editor.FocusEntity(prefabInstance->firstEntity);

                    if (prefabInstance->firstEntity->HasComponent<RectTransform>())
                    {
                        RectTransform &transform = prefabInstance->firstEntity->GetComponent<RectTransform>();
                        DrawInspectorField(_editor, "overridePosition", _conf.name.c_str(), transform.position);
                    }
                    else if (prefabInstance->firstEntity->HasComponent<Transform>())
                    {
                        Transform &transform = prefabInstance->firstEntity->GetComponent<Transform>();
                        DrawInspectorField(_editor, "overridePosition", _conf.name.c_str(), transform.position);
                    }
                    else
                    {
                        ImGui::Text("first entity has no transform");
                    }
                }
            },
        };

        RegisterScript(prefabInstanceConf);

        RegisterParticleEmitterComponent(*this);
        RegisterParticleEmitterSystem(*this);

        ScriptConf canvasConf = {
            .name = "Canis::Canvas",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void {
                if (!_entity.HasComponent<RectTransform>())
                    _entity.AddComponent<RectTransform>();
                Canvas& canvas = *_entity.AddComponent<Canvas>();
                canvas.scaleMode = CanvasScaleMode::SCALE_WITH_SCREEN_WIDTH;
                canvas.screenSize = Vector2(1280.0f, 800.0f);
            },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<Canvas>(); },
            .Remove = [this](Entity& _entity) -> void { _entity.RemoveComponent<Canvas>(); },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<Canvas>() ? (void*)(&_entity.GetComponent<Canvas>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (_entity.HasComponent<Canvas>())
                {
                    Canvas& canvas = _entity.GetComponent<Canvas>();

                    YAML::Node comp;
                    comp["active"] = canvas.active;
                    comp["renderMode"] = canvas.renderMode;
                    comp["scaleMode"] = canvas.scaleMode;
                    comp["screenSize"] = canvas.screenSize;
                    comp["receivesEvents"] = canvas.receivesEvents;
                    comp["interactionDistance"] = canvas.interactionDistance;
                    _node["Canis::Canvas"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                if (auto canvasNode = _node["Canis::Canvas"])
                {
                    auto &canvas = *_entity.AddComponent<Canvas>();
                    const Vector2 defaultScreenSize = Vector2(1280.0f, 800.0f);
                    canvas.active = canvasNode["active"].as<bool>(true);
                    canvas.renderMode = canvasNode["renderMode"].as<unsigned int>(CanvasRenderMode::SCREEN_SPACE_OVERLAY);
                    canvas.scaleMode = canvasNode["scaleMode"].as<unsigned int>(CanvasScaleMode::SCALE_WITH_SCREEN_WIDTH);
                    canvas.screenSize = canvasNode["screenSize"].as<Vector2>(defaultScreenSize);
                    canvas.receivesEvents = canvasNode["receivesEvents"].as<bool>(true);
                    canvas.interactionDistance = std::max(
                        0.0f, canvasNode["interactionDistance"].as<float>(3.0f));
                    canvas.screenSize.x = std::max(1.0f, canvas.screenSize.x);
                    canvas.screenSize.y = std::max(1.0f, canvas.screenSize.y);

                    if (_callCreate)
                        canvas.Create();
                }
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                Canvas* canvas = nullptr;
                if (_entity.HasComponent<Canvas>() && ((canvas = &_entity.GetComponent<Canvas>()), true))
                {
                    DrawInspectorField(_editor, "active", _conf.name.c_str(), canvas->active);

                    int renderMode = static_cast<int>(canvas->renderMode);
                    if (DrawInspectorComboField("renderMode", _conf.name.c_str(), &renderMode, CanvasRenderModeLabels, IM_ARRAYSIZE(CanvasRenderModeLabels)))
                        canvas->renderMode = static_cast<unsigned int>(renderMode);

                    if (canvas->renderMode == CanvasRenderMode::SCREEN_SPACE_OVERLAY)
                    {
                        int scaleMode = static_cast<int>(canvas->scaleMode);
                        if (DrawInspectorComboField("scaleMode", _conf.name.c_str(), &scaleMode, CanvasScaleModeLabels, IM_ARRAYSIZE(CanvasScaleModeLabels)))
                            canvas->scaleMode = static_cast<unsigned int>(scaleMode);

                        DrawInspectorField(_editor, "screenSize", _conf.name.c_str(), canvas->screenSize);
                        canvas->screenSize.x = std::max(1.0f, canvas->screenSize.x);
                        canvas->screenSize.y = std::max(1.0f, canvas->screenSize.y);
                    }
                    else if (canvas->renderMode == CanvasRenderMode::WORLD_SPACE)
                    {
                        DrawInspectorField(_editor, "receivesEvents", _conf.name.c_str(), canvas->receivesEvents);
                        DrawInspectorField(_editor, "interactionDistance", _conf.name.c_str(), canvas->interactionDistance);
                        canvas->interactionDistance = std::max(0.0f, canvas->interactionDistance);
                    }
                }
            },
        };

        RegisterScript(canvasConf);

        ScriptConf rectTransformConf = {
            .name = "Canis::RectTransform",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void {
                _entity.AddComponent<RectTransform>();
            },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<RectTransform>(); },
            .Remove = [this](Entity& _entity) -> void { _entity.RemoveComponent<RectTransform>(); },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<RectTransform>() ? (void*)(&_entity.GetComponent<RectTransform>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (_entity.HasComponent<RectTransform>())
                {
                    RectTransform& transform = _entity.GetComponent<RectTransform>();

                    YAML::Node comp;
                    comp["active"] = transform.active;
                    comp["position"] = transform.position;
                    comp["size"] = transform.size;
                    comp["scale"] = transform.scale;
                    comp["anchorMin"] = transform.anchorMin;
                    comp["anchorMax"] = transform.anchorMax;
                    comp["pivot"] = transform.pivot;
                    comp["originOffset"] = transform.originOffset;
                    comp["depth"] = transform.depth;
                    comp["rotation"] = transform.rotation;
                    comp["rotationOriginOffset"] = transform.rotationOriginOffset;
                    comp["parent"] = _entity.scene.GetLiveEntityUUID(transform.parent);
                    // children
                    YAML::Node children = YAML::Node(YAML::NodeType::Sequence);

                    for (Canis::Entity* c : transform.children)
                    {
                        const Canis::UUID childUUID = _entity.scene.GetLiveEntityUUID(c);
                        if (childUUID != Canis::UUID(0))
                            children.push_back(childUUID);
                    }

                    comp["children"] = children;

                    _node["Canis::RectTransform"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                if (auto rectTransform = _node["Canis::RectTransform"])
                {
                    auto &rt = *_entity.AddComponent<RectTransform>();
                    rt.active = rectTransform["active"].as<bool>(true);
                    rt.position = rectTransform["position"].as<Vector2>(rt.position);
                    rt.size = rectTransform["size"].as<Vector2>(rt.size);
                    rt.scale = rectTransform["scale"].as<Vector2>(rt.scale);
                    rt.anchorMin = rectTransform["anchorMin"].as<Vector2>(rt.anchorMin);
                    rt.anchorMax = rectTransform["anchorMax"].as<Vector2>(rt.anchorMax);
                    rt.pivot = rectTransform["pivot"].as<Vector2>(rt.pivot);
                    rt.originOffset = rectTransform["originOffset"].as<Vector2>(rt.originOffset);
                    rt.depth = rectTransform["depth"].as<float>(rt.depth);
                    rt.rotation = rectTransform["rotation"].as<float>(rt.rotation);
                    rt.rotationOriginOffset = rectTransform["rotationOriginOffset"].as<Vector2>(rt.rotationOriginOffset);

                    if (rectTransform["parent"].as<Canis::UUID>(0) != Canis::UUID(0))
                        _entity.scene.GetEntityAfterLoad(rectTransform["parent"].as<Canis::UUID>(0), rt.parent);
                    
                    if (auto children = rectTransform["children"]; children && children.IsSequence())
                    {
                        const std::size_t count = children.size();
                        rt.children.clear();
                        rt.children.resize(count);

                        std::size_t i = 0;
                        for (const auto &e : children)
                        {
                            auto uuid = e.as<Canis::UUID>(Canis::UUID(0));
                            _entity.scene.GetEntityAfterLoad(uuid, rt.children[i++]);
                        }
                    }
                    
                        //rt.scaleWithScreen = (ScaleWithScreen)rectTransform["scaleWithScreen"].as<int>(0);
                    if (_callCreate)
                        rt.Create();
                }
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                RectTransform* transform = nullptr;
                if (_entity.HasComponent<RectTransform>() && ((transform = &_entity.GetComponent<RectTransform>()), true))
                {
                    const bool beforeActive = transform->active;
                    const Vector2 beforePosition = transform->position;
                    const Vector2 beforeSize = transform->size;
                    const Vector2 beforeScale = transform->scale;
                    const Vector2 beforeAnchorMin = transform->anchorMin;
                    const Vector2 beforeAnchorMax = transform->anchorMax;
                    const Vector2 beforePivot = transform->pivot;
                    const Vector2 beforeOriginOffset = transform->originOffset;
                    const Vector2 beforeRotationOriginOffset = transform->rotationOriginOffset;
                    const float beforeDepth = transform->depth;
                    const float beforeRotation = transform->rotation;

                    DrawInspectorField(_editor, "active", _conf.name.c_str(), transform->active);
                    DrawInspectorField(_editor, "position", _conf.name.c_str(), transform->position);
                    DrawInspectorField(_editor, "size", _conf.name.c_str(), transform->size);
                    DrawInspectorField(_editor, "scale", _conf.name.c_str(), transform->scale);
                    int anchorPreset = transform->GetAnchorPreset();
                    int anchorPresetSelection = anchorPreset < 0 ? 0 : anchorPreset + 1;
                    static const char* anchorPresetLabels[] = {
                        "Custom",
                        "Top Left", "Top Center", "Top Right",
                        "Center Left", "Center", "Center Right",
                        "Bottom Left", "Bottom Center", "Bottom Right"
                    };
                    if (DrawInspectorComboField("anchorPreset", _conf.name.c_str(), &anchorPresetSelection, anchorPresetLabels, IM_ARRAYSIZE(anchorPresetLabels)) && anchorPresetSelection > 0)
                        transform->SetAnchorPreset(static_cast<RectAnchor>(anchorPresetSelection - 1));

                    DrawInspectorField(_editor, "anchorMin", _conf.name.c_str(), transform->anchorMin);
                    DrawInspectorField(_editor, "anchorMax", _conf.name.c_str(), transform->anchorMax);
                    DrawInspectorField(_editor, "pivot", _conf.name.c_str(), transform->pivot);
                    DrawInspectorField(_editor, "originOffset", _conf.name.c_str(), transform->originOffset);
                    DrawInspectorField(_editor, "rotationOriginOffset", _conf.name.c_str(), transform->rotationOriginOffset);
                    DrawInspectorField(_editor, "depth", _conf.name.c_str(), transform->depth);
                    // let user work with degrees
                    float degrees = RAD2DEG * transform->rotation;
                    DrawInspectorField(_editor, "rotation", _conf.name.c_str(), degrees);
                    transform->rotation = DEG2RAD * degrees;

                    auto clamp01 = [](float value) -> float
                    {
                        return std::clamp(value, 0.0f, 1.0f);
                    };

                    transform->anchorMin.x = clamp01(transform->anchorMin.x);
                    transform->anchorMin.y = clamp01(transform->anchorMin.y);
                    transform->anchorMax.x = std::clamp(transform->anchorMax.x, transform->anchorMin.x, 1.0f);
                    transform->anchorMax.y = std::clamp(transform->anchorMax.y, transform->anchorMin.y, 1.0f);
                    transform->pivot.x = clamp01(transform->pivot.x);
                    transform->pivot.y = clamp01(transform->pivot.y);

                    if (beforeActive != transform->active)
                        _editor.NotifyAnimationPropertyEdited(_entity, _conf.name, "active", AnimationValueType::BOOL, AnimationInterpolation::STEP, AnimationValue::Bool(transform->active));
                    if (beforePosition != transform->position)
                        _editor.NotifyAnimationPropertyEdited(_entity, _conf.name, "position", AnimationValueType::VEC2, AnimationInterpolation::LINEAR, AnimationValue::Vec2(transform->position));
                    if (beforeSize != transform->size)
                        _editor.NotifyAnimationPropertyEdited(_entity, _conf.name, "size", AnimationValueType::VEC2, AnimationInterpolation::LINEAR, AnimationValue::Vec2(transform->size));
                    if (beforeScale != transform->scale)
                        _editor.NotifyAnimationPropertyEdited(_entity, _conf.name, "scale", AnimationValueType::VEC2, AnimationInterpolation::LINEAR, AnimationValue::Vec2(transform->scale));
                    if (beforeAnchorMin != transform->anchorMin)
                        _editor.NotifyAnimationPropertyEdited(_entity, _conf.name, "anchorMin", AnimationValueType::VEC2, AnimationInterpolation::LINEAR, AnimationValue::Vec2(transform->anchorMin));
                    if (beforeAnchorMax != transform->anchorMax)
                        _editor.NotifyAnimationPropertyEdited(_entity, _conf.name, "anchorMax", AnimationValueType::VEC2, AnimationInterpolation::LINEAR, AnimationValue::Vec2(transform->anchorMax));
                    if (beforePivot != transform->pivot)
                        _editor.NotifyAnimationPropertyEdited(_entity, _conf.name, "pivot", AnimationValueType::VEC2, AnimationInterpolation::LINEAR, AnimationValue::Vec2(transform->pivot));
                    if (beforeOriginOffset != transform->originOffset)
                        _editor.NotifyAnimationPropertyEdited(_entity, _conf.name, "originOffset", AnimationValueType::VEC2, AnimationInterpolation::LINEAR, AnimationValue::Vec2(transform->originOffset));
                    if (beforeRotationOriginOffset != transform->rotationOriginOffset)
                        _editor.NotifyAnimationPropertyEdited(_entity, _conf.name, "rotationOriginOffset", AnimationValueType::VEC2, AnimationInterpolation::LINEAR, AnimationValue::Vec2(transform->rotationOriginOffset));
                    if (std::fabs(beforeDepth - transform->depth) > 0.00001f)
                        _editor.NotifyAnimationPropertyEdited(_entity, _conf.name, "depth", AnimationValueType::FLOAT, AnimationInterpolation::LINEAR, AnimationValue::Float(transform->depth));
                    if (std::fabs(beforeRotation - transform->rotation) > 0.00001f)
                        _editor.NotifyAnimationPropertyEdited(_entity, _conf.name, "rotation", AnimationValueType::FLOAT, AnimationInterpolation::LINEAR, AnimationValue::Float(transform->rotation));
                }
            },
        };

        REGISTER_PROPERTY(rectTransformConf, RectTransform, active);
        REGISTER_PROPERTY(rectTransformConf, RectTransform, position);
        REGISTER_PROPERTY(rectTransformConf, RectTransform, size);
        REGISTER_PROPERTY(rectTransformConf, RectTransform, scale);
        REGISTER_PROPERTY(rectTransformConf, RectTransform, anchorMin);
        REGISTER_PROPERTY(rectTransformConf, RectTransform, anchorMax);
        REGISTER_PROPERTY(rectTransformConf, RectTransform, pivot);
        REGISTER_PROPERTY(rectTransformConf, RectTransform, originOffset);
        REGISTER_PROPERTY(rectTransformConf, RectTransform, depth);
        REGISTER_PROPERTY(rectTransformConf, RectTransform, rotation);
        REGISTER_PROPERTY(rectTransformConf, RectTransform, rotationOriginOffset);

        RegisterScript(rectTransformConf);

        ScriptConf sprite2DConf = {
            .name = "Canis::Sprite2D",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void {
                // TODO: require a RectTransform component
                Sprite2D& sprite = *_entity.AddComponent<Sprite2D>();
                sprite.textureHandle = Canis::AssetManager::GetTextureHandle("assets/defaults/textures/square.png");
                //sprite->size.x = sprite->textureHandle.texture.width;
                //sprite->size.y = sprite->textureHandle.texture.height;
            },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<Sprite2D>(); },
            .Remove = [this](Entity& _entity) -> void { _entity.RemoveComponent<Sprite2D>(); },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<Sprite2D>() ? (void*)(&_entity.GetComponent<Sprite2D>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (_entity.HasComponent<Sprite2D>())
                {
                    Sprite2D& sprite = _entity.GetComponent<Sprite2D>();

                    YAML::Node comp;
                    comp["color"] = sprite.color;
                    comp["uv"] = sprite.uv;
                    comp["flipX"] = sprite.flipX;
                    comp["flipY"] = sprite.flipY;

                    YAML::Node textureAsset;
                    const std::string texturePath = AssetManager::GetPath(sprite.textureHandle.id);
                    if (texturePath != "Path was not found in AssetLibrary")
                    {
                        if (MetaFileAsset* meta = AssetManager::GetMetaFile(texturePath))
                            textureAsset["uuid"] = (uint64_t)meta->uuid;
                    }
                    
                    comp["TextureAsset"] = textureAsset;
                    _node["Canis::Sprite2D"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                if (auto comp = _node["Canis::Sprite2D"])
                {
                    auto &sprite = *_entity.AddComponent<Sprite2D>();
                    sprite.color = comp["color"].as<Vector4>();
                    sprite.uv = comp["uv"].as<Vector4>();
                    sprite.flipX = comp["flipX"].as<bool>(false);
                    sprite.flipY = comp["flipY"].as<bool>(false);
                    sprite.textureHandle = AssetManager::GetTextureHandle("assets/defaults/textures/square.png");
                    if (auto textureAsset = comp["TextureAsset"])
                    {
                        std::string path = "";

                        if (YAML::Node uuidNode = textureAsset["uuid"])
                        {
                            const UUID uuid = uuidNode.as<uint64_t>(0);
                            if ((uint64_t)uuid != 0)
                            {
                                path = AssetManager::GetPath(uuid);
                                if (path == "Path was not found in AssetLibrary")
                                    path.clear();
                            }
                        }

                        if (path.empty())
                            path = textureAsset["path"].as<std::string>("");

                        if (!path.empty())
                            sprite.textureHandle = AssetManager::GetTextureHandle(path);
                    }
                    //sprite.textureHandle = sprite2DComponent["TextureHandle"].as<TextureHandle>();//AssetManager::GetTextureHandle(sprite2DComponent["textureHandle"].as<std::string>());
                    if (_callCreate)
                        sprite.Create();
                }
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                Sprite2D* sprite = nullptr;
                if (_entity.HasComponent<Sprite2D>() && ((sprite = &_entity.GetComponent<Sprite2D>()), true))
                {
                    // textureHandle
                    DrawInspectorColorField("color", _conf.name.c_str(), sprite->color);
                    DrawInspectorField(_editor, "uv", _conf.name.c_str(), sprite->uv);

                    bool updateUV = false;
                    
                    const bool flipXBefore = sprite->flipX;
                    DrawInspectorField(_editor, "flipX", _conf.name.c_str(), sprite->flipX);
                    if (flipXBefore != sprite->flipX)
                        updateUV = true;
                    const bool flipYBefore = sprite->flipY;
                    DrawInspectorField(_editor, "flipY", _conf.name.c_str(), sprite->flipY);
                    if (flipYBefore != sprite->flipY)
                        updateUV = true;
                    
                    if (updateUV)
                    {
                        if (SpriteAnimation* animation = _entity.HasComponent<SpriteAnimation>() ? &_entity.GetComponent<SpriteAnimation>() : nullptr)
                        {
                            if (SpriteAnimationAsset* animationAsset = AssetManager::Get<SpriteAnimationAsset>(animation->id))
                            {
                                sprite->GetSpriteFromTextureAtlas(
                                    animationAsset->frames[animation->index].offsetX,
                                    animationAsset->frames[animation->index].offsetY,
                                    animationAsset->frames[animation->index].row,
                                    animationAsset->frames[animation->index].col,
                                    animationAsset->frames[animation->index].width,
                                    animationAsset->frames[animation->index].height);
                            }
                        }
                        else
                        {
                            sprite->GetSpriteFromTextureAtlas(0, 0, 0, 0, sprite->textureHandle.texture.width, sprite->textureHandle.texture.height);
                        }
                    }

                    ImGui::Text("texture");

                    ImGui::SameLine();

                    const std::string texturePath = AssetManager::GetPath(sprite->textureHandle.id);
                    std::string textureLabel = "[missing texture]";
                    if (texturePath != "Path was not found in AssetLibrary")
                    {
                        if (MetaFileAsset* meta = AssetManager::GetMetaFile(texturePath))
                            textureLabel = meta->name;
                        else
                            textureLabel = texturePath;
                    }

                    ImGui::Button(textureLabel.c_str(), ImVec2(150, 0));

                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
                        {
                            const AssetDragData dropped = *static_cast<const AssetDragData*>(payload->Data);
                            std::string path = AssetManager::GetPath(dropped.uuid);
                            TextureAsset* asset = AssetManager::GetTexture(path);

                            if (asset)
                            {
                                sprite->textureHandle = AssetManager::GetTextureHandle(path);
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                }
            },
        };

        REGISTER_PROPERTY(sprite2DConf, Sprite2D, uv);

        RegisterScript(sprite2DConf);

        ScriptConf textConf = {
            .name = "Canis::Text",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void {

                _entity.AddComponent<RectTransform>();

                Text& text = *_entity.AddComponent<Text>();
                text.assetId = AssetManager::LoadText("assets/fonts/Antonio-Bold.ttf", 32);
            },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<Text>(); },
            .Remove = [this](Entity& _entity) -> void { _entity.RemoveComponent<Text>(); },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<Text>() ? (void*)(&_entity.GetComponent<Text>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (_entity.HasComponent<Text>())
                {
                    Text& text = _entity.GetComponent<Text>();
                    YAML::Node comp;
                    comp["text"] = text.text;
                    comp["color"] = text.color;
                    comp["alignment"] = text.alignment;
                    comp["horizontalBoundary"] = text.horizontalBoundary;

                    if (text.assetId > -1)
                    {
                        if (TextAsset* textAsset = AssetManager::GetText(text.assetId))
                        {
                            if (MetaFileAsset* meta = AssetManager::GetMetaFile(textAsset->GetPath()))
                            {
                                YAML::Node fontAsset;
                                fontAsset["uuid"] = (uint64_t)meta->uuid;
                                fontAsset["fontSize"] = textAsset->GetFontSize();
                                comp["FontAsset"] = fontAsset;
                            }
                        }
                    }

                    _node["Canis::Text"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                if (auto comp = _node["Canis::Text"])
                {
                    Text& text = *_entity.AddComponent<Text>();
                    text.text = comp["text"].as<std::string>("");
                    text.color = comp["color"].as<Vector4>(Color(1.0f));
                    text.alignment = comp["alignment"].as<unsigned int>(Canis::TextAlignment::LEFT);
                    text.horizontalBoundary = comp["horizontalBoundary"].as<unsigned int>(Canis::TextBoundary::TB_OVERFLOW);
                    text._status = BIT::ONE;

                    if (auto fontAsset = comp["FontAsset"])
                    {
                        std::string path = "";

                        if (YAML::Node uuidNode = fontAsset["uuid"])
                        {
                            const UUID uuid = uuidNode.as<uint64_t>(0);
                            if ((uint64_t)uuid != 0)
                            {
                                path = AssetManager::GetPath(uuid);
                                if (path == "Path was not found in AssetLibrary")
                                    path.clear();
                            }
                        }

                        if (path.empty())
                            path = fontAsset["path"].as<std::string>("");

                        const unsigned int fontSize = fontAsset["fontSize"].as<unsigned int>(32u);
                        if (!path.empty())
                            text.assetId = AssetManager::LoadText(path, fontSize);
                    }

                    if (_callCreate)
                        text.Create();
                }
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                Text* text = nullptr;
                if (_entity.HasComponent<Text>() && ((text = &_entity.GetComponent<Text>()), true))
                {
                    static const char *alignmentLabels[] = {"Left", "Right", "Center"};
                    static const char *horizontalBoundaryLabels[] = {"Overflow", "Wrap"};

                    const std::string textBefore = text->text;
                    DrawInspectorField(_editor, "text", _conf.name.c_str(), text->text);
                    if (textBefore != text->text)
                    {
                        text->_status |= BIT::ONE;
                    }

                    DrawInspectorColorField("color", _conf.name.c_str(), text->color);

                    int alignment = (int)text->alignment;
                    if (DrawInspectorComboField("alignment", _conf.name.c_str(), &alignment, alignmentLabels, IM_ARRAYSIZE(alignmentLabels)))
                    {
                        text->alignment = (unsigned int)alignment;
                        text->_status |= BIT::ONE;
                    }

                    int boundary = (int)text->horizontalBoundary;
                    if (DrawInspectorComboField("horizontalBoundary", _conf.name.c_str(), &boundary, horizontalBoundaryLabels, IM_ARRAYSIZE(horizontalBoundaryLabels)))
                    {
                        text->horizontalBoundary = (unsigned int)boundary;
                        text->_status |= BIT::ONE;
                    }

                    if (text->assetId > -1)
                    {
                        ImGui::Text("font");
                        ImGui::SameLine();
                        TextAsset* textAsset = AssetManager::GetText(text->assetId);

                        if (textAsset != nullptr)
                        {
                            if (MetaFileAsset* meta = AssetManager::GetMetaFile(textAsset->GetPath()))
                            {
                                ImGui::Button(meta->name.c_str(), ImVec2(150, 0));
                            }
                            else
                            {
                                ImGui::Button("Missing Font", ImVec2(150, 0));
                            }
                        }

                        if (ImGui::BeginDragDropTarget())
                        {
                            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
                            {
                                const AssetDragData dropped = *static_cast<const AssetDragData*>(payload->Data);
                                std::string path = AssetManager::GetPath(dropped.uuid);
                                const unsigned int fontSize = (textAsset == nullptr) ? 32u : textAsset->GetFontSize();
                                text->assetId = AssetManager::LoadText(path, fontSize);
                                text->_status |= BIT::ONE;
                            }
                            ImGui::EndDragDropTarget();
                        }
                    }
                }
            },
        };

        RegisterScript(textConf);

        ScriptConf buttonConf = {
            .name = "Canis::UIButton",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void {
                if (!_entity.HasComponent<RectTransform>())
                    _entity.AddComponent<RectTransform>();
                _entity.AddComponent<UIButton>();
            },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<UIButton>(); },
            .Remove = [this](Entity& _entity) -> void { _entity.RemoveComponent<UIButton>(); },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<UIButton>() ? (void*)(&_entity.GetComponent<UIButton>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (UIButton* button = _entity.HasComponent<UIButton>() ? &_entity.GetComponent<UIButton>() : nullptr)
                {
                    YAML::Node comp;
                    comp["active"] = button->active;
                    comp["targetEntity"] = _entity.scene.GetLiveEntityUUID(button->targetEntity);
                    comp["targetScript"] = button->targetScript;
                    comp["actionName"] = button->actionName;
                    comp["hoverColor"] = button->hoverColor;
                    comp["pressedColor"] = button->pressedColor;
                    comp["hoverScale"] = button->hoverScale;
                    comp["pressedScale"] = button->pressedScale;
                    _node["Canis::UIButton"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                if (YAML::Node comp = _node["Canis::UIButton"])
                {
                    UIButton& button = *_entity.AddComponent<UIButton>();
                    button.active = comp["active"].as<bool>(true);
                    button.targetScript = comp["targetScript"].as<std::string>("");
                    button.actionName = comp["actionName"].as<std::string>("");
                    button.hoverColor = comp["hoverColor"].as<Vector4>(Color(1.0f));
                    button.pressedColor = comp["pressedColor"].as<Vector4>(Color(0.85f, 0.85f, 0.85f, 1.0f));
                    button.hoverScale = comp["hoverScale"].as<float>(1.03f);
                    button.pressedScale = comp["pressedScale"].as<float>(0.98f);

                    if (comp["targetEntity"].as<Canis::UUID>(0) != Canis::UUID(0))
                        _entity.scene.GetEntityAfterLoad(comp["targetEntity"].as<Canis::UUID>(0), button.targetEntity);

                    if (_callCreate)
                        button.Create();
                }
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                UIButton* button = _entity.HasComponent<UIButton>() ? &_entity.GetComponent<UIButton>() : nullptr;
                if (button == nullptr)
                    return;

                DrawInspectorField(_editor, "active", _conf.name.c_str(), button->active);
                DrawInspectorField(_editor, "targetEntity", _conf.name.c_str(), button->targetEntity);
#if CANIS_EDITOR
                DrawConnectedUIActionSelector(*this, button->targetEntity, button->targetScript, button->actionName, _conf.name.c_str());
#else
                DrawInspectorField(_editor, "targetScript", _conf.name.c_str(), button->targetScript);
                DrawInspectorField(_editor, "actionName", _conf.name.c_str(), button->actionName);
#endif
                DrawInspectorColorField("hoverColor", _conf.name.c_str(), button->hoverColor);
                DrawInspectorColorField("pressedColor", _conf.name.c_str(), button->pressedColor);
                
                DrawInspectorField(_editor, "hoverScale", _conf.name.c_str(), button->hoverScale);
                DrawInspectorField(_editor, "pressedScale", _conf.name.c_str(), button->pressedScale);
            },
        };

        RegisterScript(buttonConf);

        ScriptConf inputFieldConf = {
            .name = "Canis::UIInputField",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void {
                if (!_entity.HasComponent<RectTransform>())
                    _entity.AddComponent<RectTransform>();
                _entity.AddComponent<UIInputField>();
            },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<UIInputField>(); },
            .Remove = [this](Entity& _entity) -> void { _entity.RemoveComponent<UIInputField>(); },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<UIInputField>() ? (void*)(&_entity.GetComponent<UIInputField>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (UIInputField* inputField = _entity.HasComponent<UIInputField>() ? &_entity.GetComponent<UIInputField>() : nullptr)
                {
                    YAML::Node comp;
                    comp["active"] = inputField->active;
                    comp["displayEntity"] = _entity.scene.GetLiveEntityUUID(inputField->displayEntity);
                    comp["targetEntity"] = _entity.scene.GetLiveEntityUUID(inputField->targetEntity);
                    comp["targetScript"] = inputField->targetScript;
                    comp["targetProperty"] = inputField->targetProperty;
                    comp["text"] = inputField->text;
                    comp["placeholder"] = inputField->placeholder;
                    comp["allowedCharacters"] = inputField->allowedCharacters;
                    comp["maxLength"] = inputField->maxLength;
                    comp["hoverColor"] = inputField->hoverColor;
                    comp["focusedColor"] = inputField->focusedColor;
                    comp["textColor"] = inputField->textColor;
                    comp["placeholderColor"] = inputField->placeholderColor;
                    _node["Canis::UIInputField"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                if (YAML::Node comp = _node["Canis::UIInputField"])
                {
                    UIInputField& inputField = *_entity.AddComponent<UIInputField>();
                    inputField.active = comp["active"].as<bool>(true);
                    inputField.targetScript = comp["targetScript"].as<std::string>("");
                    inputField.targetProperty = comp["targetProperty"].as<std::string>("");
                    inputField.text = comp["text"].as<std::string>("");
                    inputField.placeholder = comp["placeholder"].as<std::string>("");
                    inputField.allowedCharacters = comp["allowedCharacters"].as<std::string>("");
                    inputField.maxLength = comp["maxLength"].as<int>(64);
                    inputField.hoverColor = comp["hoverColor"].as<Vector4>(Color(0.95f, 0.95f, 0.95f, 1.0f));
                    inputField.focusedColor = comp["focusedColor"].as<Vector4>(Color(1.0f));
                    inputField.textColor = comp["textColor"].as<Vector4>(Color(1.0f));
                    inputField.placeholderColor = comp["placeholderColor"].as<Vector4>(Color(0.7f, 0.7f, 0.7f, 1.0f));

                    if (comp["displayEntity"].as<Canis::UUID>(0) != Canis::UUID(0))
                        _entity.scene.GetEntityAfterLoad(comp["displayEntity"].as<Canis::UUID>(0), inputField.displayEntity);

                    if (comp["targetEntity"].as<Canis::UUID>(0) != Canis::UUID(0))
                        _entity.scene.GetEntityAfterLoad(comp["targetEntity"].as<Canis::UUID>(0), inputField.targetEntity);

                    if (_callCreate)
                        inputField.Create();
                }
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                UIInputField* inputField = _entity.HasComponent<UIInputField>() ? &_entity.GetComponent<UIInputField>() : nullptr;
                if (inputField == nullptr)
                    return;

                DrawInspectorField(_editor, "active", _conf.name.c_str(), inputField->active);
                DrawInspectorField(_editor, "displayEntity", _conf.name.c_str(), inputField->displayEntity);
                DrawInspectorField(_editor, "targetEntity", _conf.name.c_str(), inputField->targetEntity);
                DrawInspectorField(_editor, "targetScript", _conf.name.c_str(), inputField->targetScript);
                DrawInspectorField(_editor, "targetProperty", _conf.name.c_str(), inputField->targetProperty);
                DrawInspectorField(_editor, "text", _conf.name.c_str(), inputField->text);
                DrawInspectorField(_editor, "placeholder", _conf.name.c_str(), inputField->placeholder);
                DrawInspectorField(_editor, "allowedCharacters", _conf.name.c_str(), inputField->allowedCharacters);
                DrawInspectorField(_editor, "maxLength", _conf.name.c_str(), inputField->maxLength);
                DrawInspectorColorField("hoverColor", _conf.name.c_str(), inputField->hoverColor);
                DrawInspectorColorField("focusedColor", _conf.name.c_str(), inputField->focusedColor);
                DrawInspectorColorField("textColor", _conf.name.c_str(), inputField->textColor);
                DrawInspectorColorField("placeholderColor", _conf.name.c_str(), inputField->placeholderColor);
            },
        };

        RegisterScript(inputFieldConf);

        ScriptConf dragSourceConf = {
            .name = "Canis::UIDragSource",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void {
                if (!_entity.HasComponent<RectTransform>())
                    _entity.AddComponent<RectTransform>();
                _entity.AddComponent<UIDragSource>();
            },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<UIDragSource>(); },
            .Remove = [this](Entity& _entity) -> void { _entity.RemoveComponent<UIDragSource>(); },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<UIDragSource>() ? (void*)(&_entity.GetComponent<UIDragSource>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (UIDragSource* dragSource = _entity.HasComponent<UIDragSource>() ? &_entity.GetComponent<UIDragSource>() : nullptr)
                {
                    YAML::Node comp;
                    comp["active"] = dragSource->active;
                    comp["payloadType"] = dragSource->payloadType;
                    comp["payloadValue"] = dragSource->payloadValue;
                    _node["Canis::UIDragSource"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                if (YAML::Node comp = _node["Canis::UIDragSource"])
                {
                    UIDragSource& dragSource = *_entity.AddComponent<UIDragSource>();
                    dragSource.active = comp["active"].as<bool>(true);
                    dragSource.payloadType = comp["payloadType"].as<std::string>("");
                    dragSource.payloadValue = comp["payloadValue"].as<std::string>("");

                    if (_callCreate)
                        dragSource.Create();
                }
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                UIDragSource* dragSource = _entity.HasComponent<UIDragSource>() ? &_entity.GetComponent<UIDragSource>() : nullptr;
                if (dragSource == nullptr)
                    return;

                DrawInspectorField(_editor, "active", _conf.name.c_str(), dragSource->active);
                DrawInspectorField(_editor, "payloadType", _conf.name.c_str(), dragSource->payloadType);
                DrawInspectorField(_editor, "payloadValue", _conf.name.c_str(), dragSource->payloadValue);
            },
        };

        RegisterScript(dragSourceConf);

        ScriptConf dropTargetConf = {
            .name = "Canis::UIDropTarget",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void {
                if (!_entity.HasComponent<RectTransform>())
                    _entity.AddComponent<RectTransform>();
                _entity.AddComponent<UIDropTarget>();
            },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<UIDropTarget>(); },
            .Remove = [this](Entity& _entity) -> void { _entity.RemoveComponent<UIDropTarget>(); },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<UIDropTarget>() ? (void*)(&_entity.GetComponent<UIDropTarget>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (UIDropTarget* dropTarget = _entity.HasComponent<UIDropTarget>() ? &_entity.GetComponent<UIDropTarget>() : nullptr)
                {
                    YAML::Node comp;
                    comp["active"] = dropTarget->active;
                    comp["targetEntity"] = _entity.scene.GetLiveEntityUUID(dropTarget->targetEntity);
                    comp["targetScript"] = dropTarget->targetScript;
                    comp["actionName"] = dropTarget->actionName;
                    comp["acceptedPayloadType"] = dropTarget->acceptedPayloadType;
                    comp["baseColor"] = dropTarget->baseColor;
                    comp["hoverColor"] = dropTarget->hoverColor;
                    _node["Canis::UIDropTarget"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                if (YAML::Node comp = _node["Canis::UIDropTarget"])
                {
                    UIDropTarget& dropTarget = *_entity.AddComponent<UIDropTarget>();
                    dropTarget.active = comp["active"].as<bool>(true);
                    dropTarget.targetScript = comp["targetScript"].as<std::string>("");
                    dropTarget.actionName = comp["actionName"].as<std::string>("");
                    dropTarget.acceptedPayloadType = comp["acceptedPayloadType"].as<std::string>("");
                    dropTarget.baseColor = comp["baseColor"].as<Vector4>(Color(1.0f));
                    dropTarget.hoverColor = comp["hoverColor"].as<Vector4>(Color(1.0f));

                    if (comp["targetEntity"].as<Canis::UUID>(0) != Canis::UUID(0))
                        _entity.scene.GetEntityAfterLoad(comp["targetEntity"].as<Canis::UUID>(0), dropTarget.targetEntity);

                    if (_callCreate)
                        dropTarget.Create();
                }
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                UIDropTarget* dropTarget = _entity.HasComponent<UIDropTarget>() ? &_entity.GetComponent<UIDropTarget>() : nullptr;
                if (dropTarget == nullptr)
                    return;

                DrawInspectorField(_editor, "active", _conf.name.c_str(), dropTarget->active);
                DrawInspectorField(_editor, "targetEntity", _conf.name.c_str(), dropTarget->targetEntity);
#if CANIS_EDITOR
                DrawConnectedUIActionSelector(*this, dropTarget->targetEntity, dropTarget->targetScript, dropTarget->actionName, _conf.name.c_str());
#else
                DrawInspectorField(_editor, "targetScript", _conf.name.c_str(), dropTarget->targetScript);
                DrawInspectorField(_editor, "actionName", _conf.name.c_str(), dropTarget->actionName);
#endif
                DrawInspectorField(_editor, "acceptedPayloadType", _conf.name.c_str(), dropTarget->acceptedPayloadType);
                DrawInspectorColorField("baseColor", _conf.name.c_str(), dropTarget->baseColor);
                DrawInspectorColorField("hoverColor", _conf.name.c_str(), dropTarget->hoverColor);
            },
        };

        RegisterScript(dropTargetConf);

        ScriptConf camera2DConf = {
            .name = "Canis::Camera2D",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void {
                Camera2D& camera = *_entity.AddComponent<Camera2D>();
                camera.Create();
            },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<Camera2D>(); },
            .Remove = [this](Entity& _entity) -> void { _entity.RemoveComponent<Camera2D>(); },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<Camera2D>() ? (void*)(&_entity.GetComponent<Camera2D>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (_entity.HasComponent<Camera2D>())
                {
                    Camera2D& camera = _entity.GetComponent<Camera2D>();

                    YAML::Node comp;
                    comp["position"] = camera.GetPosition();
                    comp["scale"] = camera.GetScale();

                    _node["Canis::Camera2D"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                if (auto camera2DComponent = _node["Canis::Camera2D"])
                {
                    Camera2D& camera = *_entity.AddComponent<Camera2D>();
                    camera.SetPosition(camera2DComponent["position"].as<Vector2>(camera.GetPosition()));
                    camera.SetScale(camera2DComponent["scale"].as<float>(camera.GetScale()));
                    if (_callCreate)
                        camera.Create();
                }
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                Camera2D* camera = nullptr;
                if (_entity.HasComponent<Camera2D>() && ((camera = &_entity.GetComponent<Camera2D>()), true))
                {
                    Vector2 lastPosition = camera->GetPosition();
                    float lastScale = camera->GetScale();

                    DrawInspectorField(_editor, "position", _conf.name.c_str(), lastPosition);
                    DrawInspectorField(_editor, "scale", _conf.name.c_str(), lastScale);

                    if (lastPosition != camera->GetPosition())
                        camera->SetPosition(lastPosition);
                    
                    if (lastScale != camera->GetScale())
                        camera->SetScale(lastScale);
                }
            },
        };

        RegisterScript(camera2DConf);

        ScriptConf transformConf = {
            .name = "Canis::Transform",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void { _entity.AddComponent<Transform>(); },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<Transform>(); },
            .Remove = [this](Entity& _entity) -> void { _entity.RemoveComponent<Transform>(); },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<Transform>() ? (void*)(&_entity.GetComponent<Transform>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (_entity.HasComponent<Transform>())
                {
                    Transform& transform = _entity.GetComponent<Transform>();
                    YAML::Node comp;
                    comp["active"] = transform.active;
                    comp["position"] = transform.position;
                    comp["rotation"] = transform.rotation;
                    comp["scale"] = transform.scale;
                    comp["parent"] = _entity.scene.GetLiveEntityUUID(transform.parent);

                    YAML::Node children = YAML::Node(YAML::NodeType::Sequence);
                    for (Canis::Entity* child : transform.children)
                    {
                        const Canis::UUID childUUID = _entity.scene.GetLiveEntityUUID(child);
                        if (childUUID != Canis::UUID(0))
                            children.push_back(childUUID);
                    }
                    comp["children"] = children;

                    _node["Canis::Transform"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                YAML::Node comp = _node["Canis::Transform"];
                if (!comp)
                    comp = _node["Canis::Transform"];

                if (comp)
                {
                    auto &transform = *_entity.AddComponent<Transform>();
                    transform.active = comp["active"].as<bool>(true);
                    transform.position = comp["position"].as<Vector3>(Vector3(0.0f));
                    transform.rotation = comp["rotation"].as<Quaternion>(Quaternion(Vector3(0.0f)));
                    transform.scale = comp["scale"].as<Vector3>(Vector3(1.0f));

                    if (comp["parent"].as<Canis::UUID>(0) != Canis::UUID(0))
                        _entity.scene.GetEntityAfterLoad(comp["parent"].as<Canis::UUID>(0), transform.parent);

                    if (auto children = comp["children"]; children && children.IsSequence())
                    {
                        const std::size_t count = children.size();
                        transform.children.clear();
                        transform.children.resize(count);

                        std::size_t i = 0;
                        for (const auto &entry : children)
                        {
                            auto uuid = entry.as<Canis::UUID>(Canis::UUID(0));
                            _entity.scene.GetEntityAfterLoad(uuid, transform.children[i++]);
                        }
                    }

                    if (_callCreate)
                        transform.Create();
                }
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                Transform* transform = nullptr;
                if (_entity.HasComponent<Transform>() && ((transform = &_entity.GetComponent<Transform>()), true))
                {
                    const bool beforeActive = transform->active;
                    const Vector3 beforePosition = transform->position;
                    const Quaternion beforeRotation = transform->rotation;
                    const Vector3 beforeScale = transform->scale;

                    DrawInspectorField(_editor, "active", _conf.name.c_str(), transform->active);
                    DrawInspectorField(_editor, "position", _conf.name.c_str(), transform->position);

                    Vector3 degrees = glm::degrees(glm::eulerAngles(glm::normalize(transform->rotation)));
                    DrawInspectorField(_editor, "rotation", _conf.name.c_str(), degrees);
                    transform->rotation = glm::normalize(Quaternion(glm::radians(degrees)));

                    DrawInspectorField(_editor, "scale", _conf.name.c_str(), transform->scale);

                    if (transform->parent != nullptr)
                    {
                        ImGui::Text("parent: %s", transform->parent->name.c_str());
                        if (ImGui::Button("Unparent##Transform"))
                            transform->Unparent();
                    }
                    else
                    {
                        ImGui::Text("parent: [none]");
                    }

                    if (beforeActive != transform->active)
                        _editor.NotifyAnimationPropertyEdited(_entity, _conf.name, "active", AnimationValueType::BOOL, AnimationInterpolation::STEP, AnimationValue::Bool(transform->active));
                    if (beforePosition != transform->position)
                        _editor.NotifyAnimationPropertyEdited(_entity, _conf.name, "position", AnimationValueType::VEC3, AnimationInterpolation::LINEAR, AnimationValue::Vec3(transform->position));
                    if (1.0f - std::fabs(glm::dot(beforeRotation, transform->rotation)) > 0.00001f)
                        _editor.NotifyAnimationPropertyEdited(_entity, _conf.name, "rotation", AnimationValueType::VEC3, AnimationInterpolation::LINEAR, AnimationValue::Vec3(glm::eulerAngles(transform->rotation)));
                    if (beforeScale != transform->scale)
                        _editor.NotifyAnimationPropertyEdited(_entity, _conf.name, "scale", AnimationValueType::VEC3, AnimationInterpolation::LINEAR, AnimationValue::Vec3(transform->scale));
                }
            },
        };

        REGISTER_PROPERTY(transformConf, Transform, active);
        REGISTER_PROPERTY(transformConf, Transform, position);
        REGISTER_PROPERTY(transformConf, Transform, rotation);
        REGISTER_PROPERTY(transformConf, Transform, scale);

        RegisterScript(transformConf);

        ScriptConf networkIdentityConf = {
            .name = "Canis::NetworkIdentity",
            .Construct = nullptr,
            .Add = [this](Entity &_entity) -> void {
                _entity.AddComponent<NetworkIdentity>();
            },
            .Has = [this](Entity &_entity) -> bool { return _entity.HasComponent<NetworkIdentity>(); },
            .Remove = [this](Entity &_entity) -> void { _entity.RemoveComponent<NetworkIdentity>(); },
            .Get = [this](Entity &_entity) -> void* { return _entity.HasComponent<NetworkIdentity>() ? (void*)(&_entity.GetComponent<NetworkIdentity>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (NetworkIdentity *identity = _entity.HasComponent<NetworkIdentity>() ? &_entity.GetComponent<NetworkIdentity>() : nullptr)
                {
                    YAML::Node comp;
                    comp["netId"] = identity->netId;
                    comp["ownerClientId"] = identity->ownerClientId;
                    comp["serverOwned"] = identity->serverOwned;
                    comp["localOwned"] = identity->localOwned;
                    comp["replicateTransform"] = identity->replicateTransform;
                    comp["prefab"] = identity->prefab;
                    _node["Canis::NetworkIdentity"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                YAML::Node comp = _node["Canis::NetworkIdentity"];
                if (!comp)
                    return;

                NetworkIdentity &identity = *_entity.AddComponent<NetworkIdentity>();
                identity.netId = comp["netId"].as<NetworkObjectId>(0);
                identity.ownerClientId = comp["ownerClientId"].as<NetworkClientId>(0);
                identity.serverOwned = comp["serverOwned"].as<bool>(false);
                identity.localOwned = comp["localOwned"].as<bool>(false);
                identity.replicateTransform = comp["replicateTransform"].as<bool>(true);
                identity.prefab = comp["prefab"].as<SceneAssetHandle>(identity.prefab);

                if (_callCreate)
                    identity.Create();
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                NetworkIdentity *identity = nullptr;
                if (_entity.HasComponent<NetworkIdentity>() && ((identity = &_entity.GetComponent<NetworkIdentity>()), true))
                {
                    DrawInspectorField(_editor, "netId", _conf.name.c_str(), identity->netId);
                    DrawInspectorField(_editor, "ownerClientId", _conf.name.c_str(), identity->ownerClientId);
                    DrawInspectorField(_editor, "serverOwned", _conf.name.c_str(), identity->serverOwned);
                    DrawInspectorField(_editor, "localOwned", _conf.name.c_str(), identity->localOwned);
                    DrawInspectorField(_editor, "replicateTransform", _conf.name.c_str(), identity->replicateTransform);
                    _editor.InputSceneAsset("prefab", "NetworkIdentity", identity->prefab);
                }
            },
        };

        RegisterScript(networkIdentityConf);

        ScriptConf rigidbodyConf = {
            .name = "Canis::Rigidbody",
            .Construct = nullptr,
            .Add = [this](Entity &_entity) -> void {
                _entity.AddComponent<Transform>();

                if (!_entity.HasComponent<BoxCollider>()
                    && !_entity.HasComponent<SphereCollider>()
                    && !_entity.HasComponent<CapsuleCollider>()
                    && !_entity.HasComponent<MeshCollider>()
                    && !_entity.HasComponent<ConvexMeshCollider>())
                {
                    _entity.AddComponent<BoxCollider>();
                }

                _entity.AddComponent<Rigidbody>();
            },
            .Has = [this](Entity &_entity) -> bool { return _entity.HasComponent<Rigidbody>(); },
            .Remove = [this](Entity &_entity) -> void { _entity.RemoveComponent<Rigidbody>(); },
            .Get = [this](Entity &_entity) -> void* { return _entity.HasComponent<Rigidbody>() ? (void*)(&_entity.GetComponent<Rigidbody>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (Rigidbody *rigidbody = _entity.HasComponent<Rigidbody>() ? &_entity.GetComponent<Rigidbody>() : nullptr)
                {
                    YAML::Node comp;
                    comp["active"] = rigidbody->active;
                    comp["motionType"] = rigidbody->motionType;
                    comp["mass"] = rigidbody->mass;
                    comp["friction"] = rigidbody->friction;
                    comp["restitution"] = rigidbody->restitution;
                    comp["linearDamping"] = rigidbody->linearDamping;
                    comp["angularDamping"] = rigidbody->angularDamping;
                    comp["useGravity"] = rigidbody->useGravity;
                    comp["gravityFactor"] = rigidbody->gravityFactor;
                    comp["isSensor"] = rigidbody->isSensor;
                    comp["layer"] = rigidbody->layer;
                    comp["mask"] = rigidbody->mask;
                    comp["allowSleeping"] = rigidbody->allowSleeping;
                    comp["lockRotationX"] = rigidbody->lockRotationX;
                    comp["lockRotationY"] = rigidbody->lockRotationY;
                    comp["lockRotationZ"] = rigidbody->lockRotationZ;
                    comp["linearVelocity"] = rigidbody->linearVelocity;
                    comp["angularVelocity"] = rigidbody->angularVelocity;
                    _node["Canis::Rigidbody"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                YAML::Node comp = _node["Canis::Rigidbody"];
                if (!comp)
                    comp = _node["Canis::Rigidbody"];

                if (comp)
                {
                    auto &rigidbody = *_entity.AddComponent<Rigidbody>();
                    rigidbody.active = comp["active"].as<bool>(true);
                    rigidbody.motionType = comp["motionType"].as<int>(RigidbodyMotionType::DYNAMIC);
                    rigidbody.mass = comp["mass"].as<float>(1.0f);
                    rigidbody.friction = comp["friction"].as<float>(0.2f);
                    rigidbody.restitution = comp["restitution"].as<float>(0.0f);
                    rigidbody.linearDamping = comp["linearDamping"].as<float>(0.05f);
                    rigidbody.angularDamping = comp["angularDamping"].as<float>(0.05f);
                    rigidbody.useGravity = comp["useGravity"].as<bool>(true);
                    rigidbody.gravityFactor = comp["gravityFactor"].as<float>(1.0f);
                    rigidbody.isSensor = comp["isSensor"].as<bool>(false);
                    rigidbody.layer = comp["layer"].as<Mask>(
                        comp["collisionLayer"].as<Mask>(
                            comp["raycastMask"].as<Mask>(Rigidbody::DefaultLayer)));
                    rigidbody.mask = comp["mask"].as<Mask>(
                        comp["collisionMask"].as<Mask>(Rigidbody::DefaultMask));
                    rigidbody.allowSleeping = comp["allowSleeping"].as<bool>(true);
                    rigidbody.lockRotationX = comp["lockRotationX"].as<bool>(false);
                    rigidbody.lockRotationY = comp["lockRotationY"].as<bool>(false);
                    rigidbody.lockRotationZ = comp["lockRotationZ"].as<bool>(false);
                    rigidbody.linearVelocity = comp["linearVelocity"].as<Vector3>(Vector3(0.0f));
                    rigidbody.angularVelocity = comp["angularVelocity"].as<Vector3>(Vector3(0.0f));
                    if (_callCreate)
                        rigidbody.Create();
                }
            },
            .DrawInspector = [this](Editor &_editor, Entity &_entity, const ScriptConf &_conf) -> void {
                Rigidbody *rigidbody = _entity.HasComponent<Rigidbody>() ? &_entity.GetComponent<Rigidbody>() : nullptr;
                if (rigidbody == nullptr)
                    return;

                const char *motionTypeLabels[] = {"Static", "Kinematic", "Dynamic"};
                if (rigidbody->motionType < RigidbodyMotionType::STATIC
                    || rigidbody->motionType > RigidbodyMotionType::DYNAMIC)
                {
                    rigidbody->motionType = RigidbodyMotionType::DYNAMIC;
                }

                DrawInspectorField(_editor, "active", _conf.name.c_str(), rigidbody->active);
                DrawInspectorComboField("motionType", _conf.name.c_str(), &rigidbody->motionType, motionTypeLabels, IM_ARRAYSIZE(motionTypeLabels));
                DrawInspectorField(_editor, "mass", _conf.name.c_str(), rigidbody->mass);
                DrawInspectorField(_editor, "friction", _conf.name.c_str(), rigidbody->friction);
                DrawInspectorField(_editor, "restitution", _conf.name.c_str(), rigidbody->restitution);
                DrawInspectorField(_editor, "linearDamping", _conf.name.c_str(), rigidbody->linearDamping);
                DrawInspectorField(_editor, "angularDamping", _conf.name.c_str(), rigidbody->angularDamping);
                DrawInspectorField(_editor, "useGravity", _conf.name.c_str(), rigidbody->useGravity);
                DrawInspectorField(_editor, "gravityFactor", _conf.name.c_str(), rigidbody->gravityFactor);
                rigidbody->gravityFactor = std::max(0.0f, rigidbody->gravityFactor);
                DrawInspectorField(_editor, "isSensor", _conf.name.c_str(), rigidbody->isSensor);
                DrawInspectorField("layer", _conf.name.c_str(), rigidbody->layer);
                DrawInspectorField("mask", _conf.name.c_str(), rigidbody->mask);
                DrawInspectorField(_editor, "allowSleeping", _conf.name.c_str(), rigidbody->allowSleeping);
                DrawInspectorField(_editor, "lockRotationX", _conf.name.c_str(), rigidbody->lockRotationX);
                DrawInspectorField(_editor, "lockRotationY", _conf.name.c_str(), rigidbody->lockRotationY);
                DrawInspectorField(_editor, "lockRotationZ", _conf.name.c_str(), rigidbody->lockRotationZ);
                DrawInspectorField(_editor, "linearVelocity", _conf.name.c_str(), rigidbody->linearVelocity);
                DrawInspectorField(_editor, "angularVelocity", _conf.name.c_str(), rigidbody->angularVelocity);
            },
        };

        RegisterScript(rigidbodyConf);

        ScriptConf boxColliderConf = {
            .name = "Canis::BoxCollider",
            .Construct = nullptr,
            .Add = [this](Entity &_entity) -> void {
                _entity.AddComponent<Transform>();

                _entity.RemoveComponent<SphereCollider>();
                _entity.RemoveComponent<CapsuleCollider>();
                _entity.RemoveComponent<MeshCollider>();
                _entity.RemoveComponent<ConvexMeshCollider>();
                _entity.AddComponent<BoxCollider>();
            },
            .Has = [this](Entity &_entity) -> bool { return _entity.HasComponent<BoxCollider>(); },
            .Remove = [this](Entity &_entity) -> void { _entity.RemoveComponent<BoxCollider>(); },
            .Get = [this](Entity &_entity) -> void* { return _entity.HasComponent<BoxCollider>() ? (void*)(&_entity.GetComponent<BoxCollider>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (BoxCollider *boxCollider = _entity.HasComponent<BoxCollider>() ? &_entity.GetComponent<BoxCollider>() : nullptr)
                {
                    YAML::Node comp;
                    comp["active"] = boxCollider->active;
                    comp["offset"] = boxCollider->offset;
                    comp["size"] = boxCollider->size;
                    _node["Canis::BoxCollider"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                YAML::Node comp = _node["Canis::BoxCollider"];
                if (!comp)
                    comp = _node["Canis::BoxCollider"];

                if (comp)
                {
                    auto &boxCollider = *_entity.AddComponent<BoxCollider>();
                    boxCollider.active = comp["active"].as<bool>(true);
                    boxCollider.offset = comp["offset"].as<Vector3>(Vector3(0.0f));
                    boxCollider.size = comp["size"].as<Vector3>(Vector3(1.0f));
                    if (_callCreate)
                        boxCollider.Create();
                }
            },
            .DrawInspector = [this](Editor &_editor, Entity &_entity, const ScriptConf &_conf) -> void {
                BoxCollider *boxCollider = _entity.HasComponent<BoxCollider>() ? &_entity.GetComponent<BoxCollider>() : nullptr;
                if (boxCollider == nullptr)
                    return;

                DrawInspectorField(_editor, "active", _conf.name.c_str(), boxCollider->active);
                DrawInspectorField(_editor, "offset", _conf.name.c_str(), boxCollider->offset);
                DrawInspectorField(_editor, "size", _conf.name.c_str(), boxCollider->size);

                Vector3 boundsMin = Vector3(0.0f);
                Vector3 boundsMax = Vector3(0.0f);
                if (TryGetAttachedModelLocalBounds(_entity, boundsMin, boundsMax) && DrawGenerateColliderButton(_conf.name.c_str()))
                    GenerateBoxColliderFromBounds(*boxCollider, boundsMin, boundsMax);
            },
        };

        RegisterScript(boxColliderConf);

        ScriptConf sphereColliderConf = {
            .name = "Canis::SphereCollider",
            .Construct = nullptr,
            .Add = [this](Entity &_entity) -> void {
                if (!_entity.HasComponent<Transform>())
                    _entity.AddComponent<Transform>();

                _entity.RemoveComponent<BoxCollider>();
                _entity.RemoveComponent<CapsuleCollider>();
                _entity.RemoveComponent<MeshCollider>();
                _entity.RemoveComponent<ConvexMeshCollider>();
                _entity.AddComponent<SphereCollider>();
            },
            .Has = [this](Entity &_entity) -> bool { return _entity.HasComponent<SphereCollider>(); },
            .Remove = [this](Entity &_entity) -> void { _entity.RemoveComponent<SphereCollider>(); },
            .Get = [this](Entity &_entity) -> void* { return _entity.HasComponent<SphereCollider>() ? (void*)(&_entity.GetComponent<SphereCollider>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (SphereCollider *sphereCollider = _entity.HasComponent<SphereCollider>() ? &_entity.GetComponent<SphereCollider>() : nullptr)
                {
                    YAML::Node comp;
                    comp["active"] = sphereCollider->active;
                    comp["offset"] = sphereCollider->offset;
                    comp["radius"] = sphereCollider->radius;
                    _node["Canis::SphereCollider"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                YAML::Node comp = _node["Canis::SphereCollider"];
                if (!comp)
                    comp = _node["Canis::SphereCollider"];

                if (comp)
                {
                    auto &sphereCollider = *_entity.AddComponent<SphereCollider>();
                    sphereCollider.active = comp["active"].as<bool>(true);
                    sphereCollider.offset = comp["offset"].as<Vector3>(Vector3(0.0f));
                    sphereCollider.radius = comp["radius"].as<float>(0.5f);
                    if (_callCreate)
                        sphereCollider.Create();
                }
            },
            .DrawInspector = [this](Editor &_editor, Entity &_entity, const ScriptConf &_conf) -> void {
                SphereCollider *sphereCollider = _entity.HasComponent<SphereCollider>() ? &_entity.GetComponent<SphereCollider>() : nullptr;
                if (sphereCollider == nullptr)
                    return;

                DrawInspectorField(_editor, "active", _conf.name.c_str(), sphereCollider->active);
                DrawInspectorField(_editor, "offset", _conf.name.c_str(), sphereCollider->offset);
                DrawInspectorField(_editor, "radius", _conf.name.c_str(), sphereCollider->radius);

                Vector3 boundsMin = Vector3(0.0f);
                Vector3 boundsMax = Vector3(0.0f);
                if (TryGetAttachedModelLocalBounds(_entity, boundsMin, boundsMax) && DrawGenerateColliderButton(_conf.name.c_str()))
                    GenerateSphereColliderFromBounds(*sphereCollider, boundsMin, boundsMax);
            },
        };

        RegisterScript(sphereColliderConf);

        ScriptConf capsuleColliderConf = {
            .name = "Canis::CapsuleCollider",
            .Construct = nullptr,
            .Add = [this](Entity &_entity) -> void {
                if (!_entity.HasComponent<Transform>())
                    _entity.AddComponent<Transform>();

                _entity.RemoveComponent<BoxCollider>();
                _entity.RemoveComponent<SphereCollider>();
                _entity.RemoveComponent<MeshCollider>();
                _entity.RemoveComponent<ConvexMeshCollider>();
                _entity.AddComponent<CapsuleCollider>();
            },
            .Has = [this](Entity &_entity) -> bool { return _entity.HasComponent<CapsuleCollider>(); },
            .Remove = [this](Entity &_entity) -> void { _entity.RemoveComponent<CapsuleCollider>(); },
            .Get = [this](Entity &_entity) -> void* { return _entity.HasComponent<CapsuleCollider>() ? (void*)(&_entity.GetComponent<CapsuleCollider>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (CapsuleCollider *capsuleCollider = _entity.HasComponent<CapsuleCollider>() ? &_entity.GetComponent<CapsuleCollider>() : nullptr)
                {
                    YAML::Node comp;
                    comp["active"] = capsuleCollider->active;
                    comp["offset"] = capsuleCollider->offset;
                    comp["halfHeight"] = capsuleCollider->halfHeight;
                    comp["radius"] = capsuleCollider->radius;
                    _node["Canis::CapsuleCollider"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                YAML::Node comp = _node["Canis::CapsuleCollider"];
                if (!comp)
                    comp = _node["Canis::CapsuleCollider"];

                if (comp)
                {
                    auto &capsuleCollider = *_entity.AddComponent<CapsuleCollider>();
                    capsuleCollider.active = comp["active"].as<bool>(true);
                    capsuleCollider.offset = comp["offset"].as<Vector3>(Vector3(0.0f));
                    capsuleCollider.halfHeight = comp["halfHeight"].as<float>(0.5f);
                    capsuleCollider.radius = comp["radius"].as<float>(0.25f);
                    if (_callCreate)
                        capsuleCollider.Create();
                }
            },
            .DrawInspector = [this](Editor &_editor, Entity &_entity, const ScriptConf &_conf) -> void {
                CapsuleCollider *capsuleCollider = _entity.HasComponent<CapsuleCollider>() ? &_entity.GetComponent<CapsuleCollider>() : nullptr;
                if (capsuleCollider == nullptr)
                    return;

                DrawInspectorField(_editor, "active", _conf.name.c_str(), capsuleCollider->active);
                DrawInspectorField(_editor, "offset", _conf.name.c_str(), capsuleCollider->offset);
                DrawInspectorField(_editor, "halfHeight", _conf.name.c_str(), capsuleCollider->halfHeight);
                DrawInspectorField(_editor, "radius", _conf.name.c_str(), capsuleCollider->radius);

                Vector3 boundsMin = Vector3(0.0f);
                Vector3 boundsMax = Vector3(0.0f);
                if (TryGetAttachedModelLocalBounds(_entity, boundsMin, boundsMax) && DrawGenerateColliderButton(_conf.name.c_str()))
                    GenerateCapsuleColliderFromBounds(*capsuleCollider, boundsMin, boundsMax);
            },
        };

        RegisterScript(capsuleColliderConf);

        ScriptConf meshColliderConf = {
            .name = "Canis::MeshCollider",
            .Construct = nullptr,
            .Add = [this](Entity &_entity) -> void {
                if (!_entity.HasComponent<Transform>())
                    _entity.AddComponent<Transform>();

                Rigidbody *rigidbody = _entity.HasComponent<Rigidbody>()
                    ? &_entity.GetComponent<Rigidbody>()
                    : _entity.AddComponent<Rigidbody>();
                rigidbody->motionType = RigidbodyMotionType::STATIC;
                rigidbody->useGravity = false;

                _entity.RemoveComponent<BoxCollider>();
                _entity.RemoveComponent<SphereCollider>();
                _entity.RemoveComponent<CapsuleCollider>();
                _entity.RemoveComponent<ConvexMeshCollider>();
                _entity.AddComponent<MeshCollider>();
            },
            .Has = [this](Entity &_entity) -> bool { return _entity.HasComponent<MeshCollider>(); },
            .Remove = [this](Entity &_entity) -> void { _entity.RemoveComponent<MeshCollider>(); },
            .Get = [this](Entity &_entity) -> void* { return _entity.HasComponent<MeshCollider>() ? (void*)(&_entity.GetComponent<MeshCollider>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (MeshCollider *meshCollider = _entity.HasComponent<MeshCollider>() ? &_entity.GetComponent<MeshCollider>() : nullptr)
                {
                    YAML::Node comp;
                    comp["active"] = meshCollider->active;
                    comp["useAttachedModel"] = meshCollider->useAttachedModel;
                    if (!meshCollider->modelPath.empty())
                    {
                        if (MetaFileAsset* meta = AssetManager::GetMetaFile(meshCollider->modelPath))
                            comp["modelUUID"] = (uint64_t)meta->uuid;
                    }
                    _node["Canis::MeshCollider"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                YAML::Node comp = _node["Canis::MeshCollider"];
                if (!comp)
                    comp = _node["Canis::MeshCollider"];

                if (comp)
                {
                    if (!_entity.HasComponent<Rigidbody>())
                    {
                        Rigidbody &rigidbody = *_entity.AddComponent<Rigidbody>();
                        rigidbody.motionType = RigidbodyMotionType::STATIC;
                        rigidbody.useGravity = false;
                    }

                    auto &meshCollider = *_entity.AddComponent<MeshCollider>();
                    meshCollider.active = comp["active"].as<bool>(true);
                    meshCollider.useAttachedModel = comp["useAttachedModel"].as<bool>(true);
                    meshCollider.modelPath.clear();
                    if (YAML::Node modelUUIDNode = comp["modelUUID"])
                    {
                        const UUID uuid = modelUUIDNode.as<uint64_t>(0);
                        if ((uint64_t)uuid != 0)
                        {
                            const std::string path = AssetManager::GetPath(uuid);
                            if (path.rfind("Path was not found", 0) != 0)
                                meshCollider.modelPath = path;
                        }
                    }

                    if (meshCollider.modelPath.empty())
                        meshCollider.modelPath = comp["modelPath"].as<std::string>("");

                    meshCollider.modelId = -1;
                    if (!meshCollider.modelPath.empty())
                        meshCollider.modelId = AssetManager::LoadModel(meshCollider.modelPath);
                    if (_callCreate)
                        meshCollider.Create();
                }
            },
            .DrawInspector = [this](Editor &_editor, Entity &_entity, const ScriptConf &_conf) -> void {
                MeshCollider *meshCollider = _entity.HasComponent<MeshCollider>() ? &_entity.GetComponent<MeshCollider>() : nullptr;
                if (meshCollider == nullptr)
                    return;

                DrawInspectorField(_editor, "active", _conf.name.c_str(), meshCollider->active);
                DrawInspectorField(_editor, "useAttachedModel", _conf.name.c_str(), meshCollider->useAttachedModel);

                if (!meshCollider->useAttachedModel)
                    DrawModelColliderAssetField("MeshColliderModel", meshCollider->modelPath, meshCollider->modelId);
            },
        };

        RegisterScript(meshColliderConf);

        ScriptConf convexMeshColliderConf = {
            .name = "Canis::ConvexMeshCollider",
            .Construct = nullptr,
            .Add = [this](Entity &_entity) -> void {
                if (!_entity.HasComponent<Transform>())
                    _entity.AddComponent<Transform>();
                if (!_entity.HasComponent<Rigidbody>())
                    _entity.AddComponent<Rigidbody>();

                _entity.RemoveComponent<BoxCollider>();
                _entity.RemoveComponent<SphereCollider>();
                _entity.RemoveComponent<CapsuleCollider>();
                _entity.RemoveComponent<MeshCollider>();
                _entity.AddComponent<ConvexMeshCollider>();
            },
            .Has = [this](Entity &_entity) -> bool { return _entity.HasComponent<ConvexMeshCollider>(); },
            .Remove = [this](Entity &_entity) -> void { _entity.RemoveComponent<ConvexMeshCollider>(); },
            .Get = [this](Entity &_entity) -> void* { return _entity.HasComponent<ConvexMeshCollider>() ? (void*)(&_entity.GetComponent<ConvexMeshCollider>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (ConvexMeshCollider *collider = _entity.HasComponent<ConvexMeshCollider>() ? &_entity.GetComponent<ConvexMeshCollider>() : nullptr)
                {
                    YAML::Node comp;
                    comp["active"] = collider->active;
                    comp["useAttachedModel"] = collider->useAttachedModel;
                    comp["offset"] = collider->offset;
                    comp["scale"] = collider->scale;
                    comp["convexRadius"] = collider->convexRadius;
                    if (!collider->modelPath.empty())
                    {
                        if (MetaFileAsset *meta = AssetManager::GetMetaFile(collider->modelPath))
                            comp["modelUUID"] = static_cast<uint64_t>(meta->uuid);
                    }
                    _node["Canis::ConvexMeshCollider"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                if (YAML::Node comp = _node["Canis::ConvexMeshCollider"])
                {
                    if (!_entity.HasComponent<Rigidbody>())
                        _entity.AddComponent<Rigidbody>();

                    auto &collider = *_entity.AddComponent<ConvexMeshCollider>();
                    collider.active = comp["active"].as<bool>(true);
                    collider.useAttachedModel = comp["useAttachedModel"].as<bool>(true);
                    collider.offset = comp["offset"].as<Vector3>(Vector3(0.0f));
                    collider.scale = comp["scale"].as<Vector3>(Vector3(1.0f));
                    collider.convexRadius = std::max(0.0f, comp["convexRadius"].as<float>(0.05f));
                    collider.modelPath.clear();
                    if (YAML::Node modelUUIDNode = comp["modelUUID"])
                    {
                        const UUID uuid = modelUUIDNode.as<uint64_t>(0);
                        if (static_cast<uint64_t>(uuid) != 0)
                        {
                            const std::string path = AssetManager::GetPath(uuid);
                            if (path.rfind("Path was not found", 0) != 0)
                                collider.modelPath = path;
                        }
                    }
                    if (collider.modelPath.empty())
                        collider.modelPath = comp["modelPath"].as<std::string>("");

                    collider.modelId = collider.modelPath.empty() ? -1 : AssetManager::LoadModel(collider.modelPath);
                    if (_callCreate)
                        collider.Create();
                }
            },
            .DrawInspector = [this](Editor &_editor, Entity &_entity, const ScriptConf &_conf) -> void {
                ConvexMeshCollider *collider = _entity.HasComponent<ConvexMeshCollider>() ? &_entity.GetComponent<ConvexMeshCollider>() : nullptr;
                if (collider == nullptr)
                    return;

                DrawInspectorField(_editor, "active", _conf.name.c_str(), collider->active);
                DrawInspectorField(_editor, "useAttachedModel", _conf.name.c_str(), collider->useAttachedModel);
                DrawInspectorField(_editor, "offset", _conf.name.c_str(), collider->offset);
                DrawInspectorField(_editor, "scale", _conf.name.c_str(), collider->scale);
                DrawInspectorField(_editor, "convexRadius", _conf.name.c_str(), collider->convexRadius);
                collider->convexRadius = std::max(0.0f, collider->convexRadius);
                if (!collider->useAttachedModel)
                    DrawModelColliderAssetField("ConvexMeshColliderModel", collider->modelPath, collider->modelId);
            },
        };

        RegisterScript(convexMeshColliderConf);

        ScriptConf navMeshSurfaceConf = {
            .name = "Canis::NavMeshSurface",
            .Construct = nullptr,
            .Add = [this](Entity &_entity) -> void {
                if (!_entity.HasComponent<Transform>())
                    _entity.AddComponent<Transform>();
                _entity.AddComponent<NavMeshSurface>();
            },
            .Has = [this](Entity &_entity) -> bool { return _entity.HasComponent<NavMeshSurface>(); },
            .Remove = [this](Entity &_entity) -> void {
                _entity.scene.InvalidateNavMesh(_entity);
                _entity.RemoveComponent<NavMeshSurface>();
            },
            .Get = [this](Entity &_entity) -> void* {
                return _entity.HasComponent<NavMeshSurface>()
                    ? static_cast<void*>(&_entity.GetComponent<NavMeshSurface>())
                    : nullptr;
            },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (!_entity.HasComponent<NavMeshSurface>())
                    return;
                const NavMeshSurface& surface = _entity.GetComponent<NavMeshSurface>();
                YAML::Node comp;
                comp["active"] = surface.active;
                comp["buildOnReady"] = surface.buildOnReady;
                comp["showDebug"] = surface.showDebug;
                comp["meshCollidersOnly"] = surface.meshCollidersOnly;
                comp["size"] = surface.size;
                comp["cellSize"] = surface.cellSize;
                comp["agentRadius"] = surface.agentRadius;
                comp["agentHeight"] = surface.agentHeight;
                comp["maxFloorDelta"] = surface.maxFloorDelta;
                comp["collisionMask"] = surface.collisionMask;
                _node["Canis::NavMeshSurface"] = comp;
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                const YAML::Node comp = _node["Canis::NavMeshSurface"];
                if (!comp)
                    return;
                NavMeshSurface& surface = *_entity.AddComponent<NavMeshSurface>();
                surface.active = comp["active"].as<bool>(true);
                surface.buildOnReady = comp["buildOnReady"].as<bool>(true);
                surface.showDebug = comp["showDebug"].as<bool>(true);
                surface.meshCollidersOnly = comp["meshCollidersOnly"].as<bool>(true);
                surface.size = comp["size"].as<Vector3>(surface.size);
                surface.cellSize = std::max(0.1f, comp["cellSize"].as<float>(surface.cellSize));
                surface.agentRadius = std::max(0.0f, comp["agentRadius"].as<float>(surface.agentRadius));
                surface.agentHeight = std::max(0.1f, comp["agentHeight"].as<float>(surface.agentHeight));
                surface.maxFloorDelta = std::max(0.05f, comp["maxFloorDelta"].as<float>(surface.maxFloorDelta));
                surface.collisionMask = comp["collisionMask"].as<Mask>(Rigidbody::DefaultMask);
                if (_callCreate)
                    surface.Create();
            },
            .DrawInspector = [this](Editor &_editor, Entity &_entity, const ScriptConf &_conf) -> void {
                if (!_entity.HasComponent<NavMeshSurface>())
                    return;
                NavMeshSurface& surface = _entity.GetComponent<NavMeshSurface>();

                const bool previousActive = surface.active;
                const bool previousBuildOnReady = surface.buildOnReady;
                const bool previousMeshOnly = surface.meshCollidersOnly;
                const Vector3 previousSize = surface.size;
                const float previousCellSize = surface.cellSize;
                const float previousRadius = surface.agentRadius;
                const float previousHeight = surface.agentHeight;
                const float previousFloorDelta = surface.maxFloorDelta;
                const Mask previousMask = surface.collisionMask;

                DrawInspectorField(_editor, "active", _conf.name.c_str(), surface.active);
                DrawInspectorField(_editor, "buildOnReady", _conf.name.c_str(), surface.buildOnReady);
                DrawInspectorField(_editor, "showDebug", _conf.name.c_str(), surface.showDebug);
                DrawInspectorField(_editor, "meshCollidersOnly", _conf.name.c_str(), surface.meshCollidersOnly);
                DrawInspectorField(_editor, "size", _conf.name.c_str(), surface.size);
                DrawInspectorField(_editor, "cellSize", _conf.name.c_str(), surface.cellSize);
                DrawInspectorField(_editor, "agentRadius", _conf.name.c_str(), surface.agentRadius);
                DrawInspectorField(_editor, "agentHeight", _conf.name.c_str(), surface.agentHeight);
                DrawInspectorField(_editor, "maxFloorDelta", _conf.name.c_str(), surface.maxFloorDelta);
                DrawInspectorField("collisionMask", _conf.name.c_str(), surface.collisionMask);

                surface.size = glm::max(glm::abs(surface.size), Vector3(0.1f));
                surface.cellSize = std::max(0.1f, surface.cellSize);
                surface.agentRadius = std::max(0.0f, surface.agentRadius);
                surface.agentHeight = std::max(0.1f, surface.agentHeight);
                surface.maxFloorDelta = std::max(0.05f, surface.maxFloorDelta);

                if (previousActive != surface.active ||
                    previousBuildOnReady != surface.buildOnReady ||
                    previousMeshOnly != surface.meshCollidersOnly ||
                    previousSize != surface.size ||
                    previousCellSize != surface.cellSize ||
                    previousRadius != surface.agentRadius ||
                    previousHeight != surface.agentHeight ||
                    previousFloorDelta != surface.maxFloorDelta ||
                    previousMask != surface.collisionMask)
                {
                    surface.MarkDirty();
                    _entity.scene.InvalidateNavMesh(_entity);
                }

                ImGui::PushID("NavMeshSurfaceActions");
                if (ImGui::Button("Build Nav Mesh"))
                    (void)_entity.scene.BuildNavMesh(_entity);
                ImGui::SameLine();
                ImGui::TextDisabled(
                    "%zu points",
                    _entity.scene.GetNavMeshPointCount(_entity));
                ImGui::PopID();
            },
        };

        RegisterScript(navMeshSurfaceConf);

        ScriptConf cloudNavSurfaceConf = {
            .name = "Canis::CloudNavSurface",
            .Construct = nullptr,
            .Add = [this](Entity &_entity) -> void {
                if (!_entity.HasComponent<Transform>())
                    _entity.AddComponent<Transform>();
                _entity.AddComponent<CloudNavSurface>();
            },
            .Has = [this](Entity &_entity) -> bool { return _entity.HasComponent<CloudNavSurface>(); },
            .Remove = [this](Entity &_entity) -> void {
                _entity.scene.InvalidateCloudNav(_entity);
                _entity.RemoveComponent<CloudNavSurface>();
            },
            .Get = [this](Entity &_entity) -> void* {
                return _entity.HasComponent<CloudNavSurface>()
                    ? static_cast<void*>(&_entity.GetComponent<CloudNavSurface>())
                    : nullptr;
            },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (!_entity.HasComponent<CloudNavSurface>())
                    return;
                const CloudNavSurface& surface = _entity.GetComponent<CloudNavSurface>();
                YAML::Node comp;
                comp["active"] = surface.active;
                comp["buildOnReady"] = surface.buildOnReady;
                comp["showDebug"] = surface.showDebug;
                comp["size"] = surface.size;
                comp["nodeSpacing"] = surface.nodeSpacing;
                comp["agentRadius"] = surface.agentRadius;
                comp["collisionMask"] = surface.collisionMask;
                _node["Canis::CloudNavSurface"] = comp;
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                const YAML::Node comp = _node["Canis::CloudNavSurface"];
                if (!comp)
                    return;
                CloudNavSurface& surface = *_entity.AddComponent<CloudNavSurface>();
                surface.active = comp["active"].as<bool>(true);
                surface.buildOnReady = comp["buildOnReady"].as<bool>(true);
                surface.showDebug = comp["showDebug"].as<bool>(true);
                surface.size = comp["size"].as<Vector3>(surface.size);
                surface.nodeSpacing = std::max(0.25f, comp["nodeSpacing"].as<float>(surface.nodeSpacing));
                surface.agentRadius = std::max(0.0f, comp["agentRadius"].as<float>(surface.agentRadius));
                surface.collisionMask = comp["collisionMask"].as<Mask>(Rigidbody::DefaultMask);
                if (_callCreate)
                    surface.Create();
            },
            .DrawInspector = [this](Editor &_editor, Entity &_entity, const ScriptConf &_conf) -> void {
                if (!_entity.HasComponent<CloudNavSurface>())
                    return;
                CloudNavSurface& surface = _entity.GetComponent<CloudNavSurface>();

                const bool previousActive = surface.active;
                const bool previousBuild = surface.buildOnReady;
                const Vector3 previousSize = surface.size;
                const float previousSpacing = surface.nodeSpacing;
                const float previousRadius = surface.agentRadius;
                const Mask previousMask = surface.collisionMask;

                DrawInspectorField(_editor, "active", _conf.name.c_str(), surface.active);
                DrawInspectorField(_editor, "buildOnReady", _conf.name.c_str(), surface.buildOnReady);
                DrawInspectorField(_editor, "showDebug", _conf.name.c_str(), surface.showDebug);
                DrawInspectorField(_editor, "size", _conf.name.c_str(), surface.size);
                DrawInspectorField(_editor, "nodeSpacing", _conf.name.c_str(), surface.nodeSpacing);
                DrawInspectorField(_editor, "agentRadius", _conf.name.c_str(), surface.agentRadius);
                DrawInspectorField("collisionMask", _conf.name.c_str(), surface.collisionMask);

                surface.size = glm::max(glm::abs(surface.size), Vector3(0.25f));
                surface.nodeSpacing = std::max(0.25f, surface.nodeSpacing);
                surface.agentRadius = std::max(0.0f, surface.agentRadius);
                if (previousActive != surface.active || previousBuild != surface.buildOnReady ||
                    previousSize != surface.size || previousSpacing != surface.nodeSpacing ||
                    previousRadius != surface.agentRadius || previousMask != surface.collisionMask)
                {
                    surface.MarkDirty();
                    _entity.scene.InvalidateCloudNav(_entity);
                }

                ImGui::PushID("CloudNavSurfaceActions");
                if (ImGui::Button("Build Cloud Nav"))
                    (void)_entity.scene.BuildCloudNav(_entity);
                ImGui::SameLine();
                ImGui::TextDisabled("%zu points", _entity.scene.GetCloudNavPointCount(_entity));
                ImGui::PopID();
            },
        };

        RegisterScript(cloudNavSurfaceConf);

        ScriptConf terrainConf = {
            .name = "Canis::Terrain",
            .Construct = nullptr,
            .Add = [this](Entity &_entity) -> void {
                if (!_entity.HasComponent<Transform>())
                    _entity.AddComponent<Transform>();
                _entity.AddComponent<Terrain>();
            },
            .Has = [this](Entity &_entity) -> bool { return _entity.HasComponent<Terrain>(); },
            .Remove = [this](Entity &_entity) -> void {
                if (_entity.HasComponent<Terrain>())
                {
                    Terrain &terrain = _entity.GetComponent<Terrain>();
                    if (terrain.runtimeModelId >= 0)
                        AssetManager::FreeModel(terrain.runtimeModelId);
                }
                _entity.RemoveComponent<Terrain>();
            },
            .Get = [this](Entity &_entity) -> void* { return _entity.HasComponent<Terrain>() ? (void*)(&_entity.GetComponent<Terrain>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (Terrain *terrain = _entity.HasComponent<Terrain>() ? &_entity.GetComponent<Terrain>() : nullptr)
                {
                    YAML::Node comp;
                    UUID terrainUUID = terrain->terrain.uuid;
                    if (terrainUUID == UUID(0) && !terrain->terrain.path.empty())
                    {
                        if (MetaFileAsset *meta = AssetManager::GetMetaFile(terrain->terrain.path))
                            terrainUUID = meta->uuid;
                    }
                    if (terrainUUID != UUID(0))
                        comp["TerrainAsset"] = static_cast<uint64_t>(terrainUUID);
                    _node["Canis::Terrain"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                YAML::Node comp = _node["Canis::Terrain"];
                if (!comp)
                    return;

                auto &terrain = *_entity.AddComponent<Terrain>();
                terrain.terrain = comp["TerrainAsset"].as<TerrainAssetHandle>(TerrainAssetHandle{});
                if (_callCreate)
                    terrain.Create();
                RebuildTerrainEntity(_entity);
            },
            .DrawInspector = [this](Editor &_editor, Entity &_entity, const ScriptConf &_conf) -> void {
                Terrain *terrain = _entity.HasComponent<Terrain>() ? &_entity.GetComponent<Terrain>() : nullptr;
                if (terrain == nullptr)
                    return;

                if (_editor.InputTerrainAsset("Terrain Data", _conf.name.c_str(), terrain->terrain))
                    _editor.RebuildTerrainEntity(_entity);

                const std::string terrainPath = AssetManager::ResolvePath(terrain->terrain);
                TerrainAsset *asset = terrainPath.empty() ? nullptr : AssetManager::GetTerrain(terrainPath);
                if (asset == nullptr)
                {
                    if (_editor.m_terrainToolEnabled)
                    {
                        _editor.m_terrainToolEnabled = false;
                        _entity.scene.ClearDebugGizmoLines();
                    }
                    ImGui::TextDisabled("Assign terrain data to edit terrain.");
                    return;
                }

                ImGui::Separator();

                bool editTerrain = _editor.m_terrainToolEnabled;
                if (ImGui::Checkbox("Edit Terrain", &editTerrain))
                {
                    _editor.m_terrainToolEnabled = editTerrain;
                    if (!editTerrain)
                        _entity.scene.ClearDebugGizmoLines();
                }

                const char *tools[] = {"Raise", "Lower", "Smooth", "Flatten", "Paint"};
                ImGui::TextUnformatted("Tool");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(170.0f);
                (void)ImGui::Combo("##TerrainComponentTool", &asset->selectedTool, tools, IM_ARRAYSIZE(tools));

                ImGui::TextUnformatted("Radius");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(170.0f);
                (void)ImGui::DragFloat("##TerrainComponentRadius", &asset->brushRadius, 0.05f, 0.05f, 12.0f, "%.2f");

                ImGui::TextUnformatted("Strength");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(170.0f);
                (void)ImGui::DragFloat("##TerrainComponentStrength", &asset->brushStrength, 0.01f, 0.0f, 4.0f, "%.2f");

                if (asset->selectedTool == 3)
                {
                    ImGui::TextUnformatted("Flatten Height");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(170.0f);
                    (void)ImGui::DragFloat("##TerrainComponentFlattenHeight", &asset->flattenHeight, 0.05f, -64.0f, 64.0f, "%.2f");
                }
                else if (asset->selectedTool == 4)
                {
                    std::string layerLabels[TerrainAsset::MaxLayers] = {};
                    const char *layerItems[TerrainAsset::MaxLayers] = {};
                    for (int i = 0; i < TerrainAsset::MaxLayers; ++i)
                    {
                        std::string layerName = asset->layers[i].texturePath;
                        if (MetaFileAsset *meta = AssetManager::GetMetaFile(asset->layers[i].texturePath))
                            layerName = meta->name;
                        if (layerName.empty())
                            layerName = "[none]";

                        layerLabels[i] = std::to_string(i) + " - " + layerName;
                        layerItems[i] = layerLabels[i].c_str();
                    }

                    ImGui::TextUnformatted("Paint Layer");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(170.0f);
                    (void)ImGui::Combo("##TerrainComponentLayer", &asset->selectedLayer, layerItems, TerrainAsset::MaxLayers);

                    if (_editor.m_terrainToolEnabled)
                    {
                        if (_editor.m_terrainBrushDebugValid)
                        {
                            ImGui::TextDisabled(
                                "Weights %d,%d: %u %u %u %u",
                                _editor.m_terrainBrushDebugX,
                                _editor.m_terrainBrushDebugZ,
                                static_cast<unsigned int>(_editor.m_terrainBrushDebugWeights[0]),
                                static_cast<unsigned int>(_editor.m_terrainBrushDebugWeights[1]),
                                static_cast<unsigned int>(_editor.m_terrainBrushDebugWeights[2]),
                                static_cast<unsigned int>(_editor.m_terrainBrushDebugWeights[3]));
                        }
                        else
                        {
                            ImGui::TextDisabled("Weights --,--: -- -- -- --");
                        }
                    }
                }
            },
        };

        RegisterScript(terrainConf);

        ScriptConf cameraConf = {
            .name = "Canis::Camera",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void {
                if (!_entity.HasComponent<Transform>())
                    _entity.AddComponent<Transform>();

                _entity.AddComponent<Camera>();
            },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<Camera>(); },
            .Remove = [this](Entity& _entity) -> void { _entity.RemoveComponent<Camera>(); },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<Camera>() ? (void*)(&_entity.GetComponent<Camera>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (_entity.HasComponent<Camera>())
                {
                    Camera& camera = _entity.GetComponent<Camera>();
                    YAML::Node comp;
                    comp["primary"] = camera.primary;
                    comp["fovDegrees"] = camera.fovDegrees;
                    comp["nearClip"] = camera.nearClip;
                    comp["farClip"] = camera.farClip;
                    _node["Canis::Camera"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                YAML::Node comp = _node["Canis::Camera"];
                if (!comp)
                    comp = _node["Canis::Camera"];

                if (comp)
                {
                    auto &camera = *_entity.AddComponent<Camera>();
                    camera.primary = comp["primary"].as<bool>(true);
                    camera.fovDegrees = comp["fovDegrees"].as<float>(60.0f);
                    camera.nearClip = comp["nearClip"].as<float>(0.1f);
                    camera.farClip = comp["farClip"].as<float>(1000.0f);
                    if (_callCreate)
                        camera.Create();
                }
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                Camera* camera = nullptr;
                if (_entity.HasComponent<Camera>() && ((camera = &_entity.GetComponent<Camera>()), true))
                {
                    DrawInspectorField(_editor, "primary", _conf.name.c_str(), camera->primary);
                    DrawInspectorField(_editor, "fovDegrees", _conf.name.c_str(), camera->fovDegrees);
                    DrawInspectorField(_editor, "nearClip", _conf.name.c_str(), camera->nearClip);
                    DrawInspectorField(_editor, "farClip", _conf.name.c_str(), camera->farClip);
                }
            },
        };

        RegisterScript(cameraConf);

        ScriptConf directionalLightConf = {
            .name = "Canis::DirectionalLight",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void {
                _entity.AddComponent<DirectionalLight>();
            },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<DirectionalLight>(); },
            .Remove = [this](Entity& _entity) -> void { _entity.RemoveComponent<DirectionalLight>(); },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<DirectionalLight>() ? (void*)(&_entity.GetComponent<DirectionalLight>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (_entity.HasComponent<DirectionalLight>())
                {
                    DirectionalLight& light = _entity.GetComponent<DirectionalLight>();
                    YAML::Node comp;
                    comp["enabled"] = light.enabled;
                    comp["color"] = light.color;
                    comp["intensity"] = light.intensity;
                    comp["direction"] = light.direction;
                    _node["Canis::DirectionalLight"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                if (auto comp = _node["Canis::DirectionalLight"])
                {
                    auto &light = *_entity.AddComponent<DirectionalLight>();
                    light.enabled = comp["enabled"].as<bool>(true);
                    light.color = comp["color"].as<Vector4>(Color(1.0f));
                    light.intensity = comp["intensity"].as<float>(1.0f);
                    light.direction = comp["direction"].as<Vector3>(Vector3(-0.4f, -1.0f, -0.25f));
                    if (_callCreate)
                        light.Create();
                }
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                DirectionalLight* light = nullptr;
                if (_entity.HasComponent<DirectionalLight>() && ((light = &_entity.GetComponent<DirectionalLight>()), true))
                {
                    DrawInspectorField(_editor, "enabled", _conf.name.c_str(), light->enabled);
                    DrawInspectorColorField("color", _conf.name.c_str(), light->color);
                    DrawInspectorField(_editor, "intensity", _conf.name.c_str(), light->intensity);
                    DrawInspectorField(_editor, "direction", _conf.name.c_str(), light->direction);
                }
            },
        };

        RegisterScript(directionalLightConf);

        ScriptConf pointLightConf = {
            .name = "Canis::PointLight",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void {
                if (!_entity.HasComponent<Transform>())
                    _entity.AddComponent<Transform>();

                _entity.AddComponent<PointLight>();
            },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<PointLight>(); },
            .Remove = [this](Entity& _entity) -> void { _entity.RemoveComponent<PointLight>(); },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<PointLight>() ? (void*)(&_entity.GetComponent<PointLight>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (_entity.HasComponent<PointLight>())
                {
                    PointLight& light = _entity.GetComponent<PointLight>();
                    YAML::Node comp;
                    comp["enabled"] = light.enabled;
                    comp["color"] = light.color;
                    comp["intensity"] = light.intensity;
                    comp["range"] = light.range;
                    _node["Canis::PointLight"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                if (auto comp = _node["Canis::PointLight"])
                {
                    auto &light = *_entity.AddComponent<PointLight>();
                    light.enabled = comp["enabled"].as<bool>(true);
                    light.color = comp["color"].as<Vector4>(Color(1.0f));
                    light.intensity = comp["intensity"].as<float>(1.2f);
                    light.range = comp["range"].as<float>(12.0f);
                    if (_callCreate)
                        light.Create();
                }
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                PointLight* light = nullptr;
                if (_entity.HasComponent<PointLight>() && ((light = &_entity.GetComponent<PointLight>()), true))
                {
                    DrawInspectorField(_editor, "enabled", _conf.name.c_str(), light->enabled);
                    DrawInspectorColorField("color", _conf.name.c_str(), light->color);
                    DrawInspectorField(_editor, "intensity", _conf.name.c_str(), light->intensity);
                    DrawInspectorField(_editor, "range", _conf.name.c_str(), light->range);
                }
            },
        };

        RegisterScript(pointLightConf);

        ScriptConf materialConf = {
            .name = "Canis::Material",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void {
                if (!_entity.HasComponent<Model>())
                {
                    if (!_entity.HasComponent<Transform>())
                        _entity.AddComponent<Transform>();

                    Model* model = _entity.AddComponent<Model>();
                    model->modelId = AssetManager::LoadModel("assets/models/dq.gltf");
                }

                Material* material = _entity.AddComponent<Material>();
                material->materialId = AssetManager::LoadMaterial("assets/defaults/materials/default.material");
            },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<Material>(); },
            .Remove = [this](Entity& _entity) -> void { _entity.RemoveComponent<Material>(); },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<Material>() ? (void*)(&_entity.GetComponent<Material>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (_entity.HasComponent<Material>())
                {
                    Material& material = _entity.GetComponent<Material>();
                    YAML::Node comp;
                    comp["color"] = material.color;

                    if (material.materialId > -1)
                    {
                        const std::string materialPath = AssetManager::GetPath(material.materialId);
                        if (materialPath.rfind("Path was not found", 0) != 0)
                        {
                            if (MetaFileAsset* meta = AssetManager::GetMetaFile(materialPath))
                            {
                                YAML::Node materialAssetNode;
                                materialAssetNode["uuid"] = (uint64_t)meta->uuid;
                                comp["MaterialAsset"] = materialAssetNode;
                            }
                        }
                    }

                    YAML::Node slotAssets = YAML::Node(YAML::NodeType::Sequence);
                    for (i32 slotMaterialId : material.materialIds)
                    {
                        if (slotMaterialId < 0)
                        {
                            slotAssets.push_back(YAML::Node());
                            continue;
                        }

                        const std::string slotPath = AssetManager::GetPath(slotMaterialId);
                        if (slotPath.rfind("Path was not found", 0) == 0)
                        {
                            slotAssets.push_back(YAML::Node());
                            continue;
                        }

                        YAML::Node slotAssetNode;
                        if (MetaFileAsset* meta = AssetManager::GetMetaFile(slotPath))
                        {
                            slotAssetNode["uuid"] = (uint64_t)meta->uuid;
                            slotAssets.push_back(slotAssetNode);
                        }
                        else
                        {
                            slotAssets.push_back(YAML::Node());
                        }
                    }

                    if (!slotAssets.IsNull() && slotAssets.size() > 0)
                        comp["MaterialSlots"] = slotAssets;

                    YAML::Node uniformNode;
                    EncodeMaterialOverrides(material.materialFields, uniformNode);
                    if (uniformNode && uniformNode.IsMap() && uniformNode.size() > 0u)
                        comp["UniformOverrides"] = uniformNode;

                    _node["Canis::Material"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                if (auto comp = _node["Canis::Material"])
                {
                    auto &material = *_entity.AddComponent<Material>();
                    material.color = comp["color"].as<Vector4>(Color(1.0f));

                    std::string path = "";
                    if (auto materialAsset = comp["MaterialAsset"])
                    {
                        if (auto uuidNode = materialAsset["uuid"])
                        {
                            UUID uuid = uuidNode.as<uint64_t>(0);
                            path = AssetManager::GetPath(uuid);
                            if (path.rfind("Path was not found", 0) == 0)
                                path.clear();
                        }

                        if (path.empty())
                            path = materialAsset["path"].as<std::string>("");
                    }

                    if (!path.empty())
                        material.materialId = AssetManager::LoadMaterial(path);

                    material.materialIds.clear();
                    if (auto slotAssets = comp["MaterialSlots"]; slotAssets && slotAssets.IsSequence())
                    {
                        material.materialIds.resize(slotAssets.size(), -1);
                        for (size_t i = 0; i < slotAssets.size(); ++i)
                        {
                            const YAML::Node slotNode = slotAssets[i];
                            if (!slotNode || slotNode.IsNull())
                                continue;

                            std::string slotPath = "";
                            if (auto uuidNode = slotNode["uuid"])
                            {
                                UUID uuid = uuidNode.as<uint64_t>(0);
                                slotPath = AssetManager::GetPath(uuid);
                                if (slotPath.rfind("Path was not found", 0) == 0)
                                    slotPath.clear();
                            }

                            if (slotPath.empty())
                                slotPath = slotNode["path"].as<std::string>("");

                            if (!slotPath.empty())
                                material.materialIds[i] = AssetManager::LoadMaterial(slotPath);
                        }
                    }

                    material.materialFields.Clear();
                    if (auto uniformOverrides = comp["UniformOverrides"]; uniformOverrides && uniformOverrides.IsMap())
                        DecodeMaterialOverrides(uniformOverrides, material.materialFields);

                    if (_callCreate)
                        material.Create();
                }
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                Material* material = nullptr;
                if (_entity.HasComponent<Material>() && ((material = &_entity.GetComponent<Material>()), true))
                {
                    auto getMaterialLabel = [](i32 _materialId) -> std::string
                    {
                        if (_materialId < 0)
                            return "[ empty ]";

                        std::string path = AssetManager::GetPath(_materialId);
                        if (path.rfind("Path was not found", 0) == 0)
                            return "[ missing ]";

                        if (MetaFileAsset* meta = AssetManager::GetMetaFile(path))
                            return meta->name;

                        return path;
                    };

                    auto handleMaterialDrop = [](i32 &_materialId) -> void
                    {
                        if (!ImGui::BeginDragDropTarget())
                            return;

                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
                        {
                            const AssetDragData dropped = *static_cast<const AssetDragData*>(payload->Data);
                            std::string path = std::string(dropped.path);
                            if (path.empty() || !FileExists(path.c_str()))
                                path = AssetManager::GetPath(dropped.uuid);

                            if (MetaFileAsset* meta = AssetManager::GetMetaFile(path))
                            {
                                if (meta->type == MetaFileAsset::FileType::MATERIAL)
                                    _materialId = AssetManager::LoadMaterial(path);
                            }
                        }
                        ImGui::EndDragDropTarget();
                    };

                    auto getTextureLabel = [](i32 _textureId) -> std::string
                    {
                        if (_textureId < 0)
                            return "None";

                        std::string path = AssetManager::GetPath(_textureId);
                        if (path.rfind("Path was not found", 0) == 0)
                            return "[ missing ]";

                        if (MetaFileAsset *meta = AssetManager::GetMetaFile(path))
                            return meta->name;

                        return path;
                    };

                    DrawInspectorColorField("material color", _conf.name.c_str(), material->color);

                    std::string materialLabel = getMaterialLabel(material->materialId);

                    ImGui::Text("material");
                    ImGui::SameLine();
                    ImGui::Button(materialLabel.c_str(), ImVec2(150, 0));
                    handleMaterialDrop(material->materialId);

                    Model* model = _entity.HasComponent<Model>() ? &_entity.GetComponent<Model>() : nullptr;
                    ModelAsset* modelAsset = nullptr;
                    if (model != nullptr && model->modelId >= 0)
                        modelAsset = AssetManager::GetModel(model->modelId);

                    const i32 slotCount = (modelAsset != nullptr) ? modelAsset->GetMaterialSlotCount() : 0;
                    ImGui::Text("material slots: %d", slotCount);
                    if (slotCount > 0)
                    {
                        material->materialIds.resize(static_cast<size_t>(slotCount), -1);

                        for (i32 slotIndex = 0; slotIndex < slotCount; ++slotIndex)
                        {
                            const std::string slotName = modelAsset->GetMaterialSlotName(slotIndex);
                            const std::string slotLabel = slotName.empty()
                                ? ("slot " + std::to_string(slotIndex))
                                : ("slot " + std::to_string(slotIndex) + " (" + slotName + ")");
                            ImGui::Text("%s", slotLabel.c_str());
                            ImGui::SameLine();

                            std::string buttonLabel = getMaterialLabel(material->materialIds[static_cast<size_t>(slotIndex)]);
                            buttonLabel += "##material_slot_" + std::to_string(slotIndex);
                            ImGui::Button(buttonLabel.c_str(), ImVec2(180, 0));
                            handleMaterialDrop(material->materialIds[static_cast<size_t>(slotIndex)]);
                        }
                    }

                    MaterialAsset *baseMaterialAsset = nullptr;
                    if (material->materialId >= 0)
                        baseMaterialAsset = AssetManager::GetMaterial(material->materialId);

                    if (baseMaterialAsset != nullptr)
                    {
                        const bool hasUniforms =
                            !baseMaterialAsset->materialFields.GetIntUniforms().empty() ||
                            !baseMaterialAsset->materialFields.GetFloatUniforms().empty() ||
                            !baseMaterialAsset->materialFields.GetVec2Uniforms().empty() ||
                            !baseMaterialAsset->materialFields.GetVec3Uniforms().empty() ||
                            !baseMaterialAsset->materialFields.GetVec4Uniforms().empty() ||
                            !baseMaterialAsset->materialFields.GetColorUniforms().empty() ||
                            !baseMaterialAsset->materialFields.GetTextureUniforms().empty();

                        if (hasUniforms)
                        {
                            ImGui::Separator();
                            ImGui::Text("uniform overrides");

                            for (const MaterialFields::IntUniformData &baseUniform : baseMaterialAsset->materialFields.GetIntUniforms())
                            {
                                int overrideValue = 0;
                                bool hasOverride = material->materialFields.TryGetInt(baseUniform.name, overrideValue);
                                if (!hasOverride)
                                    overrideValue = baseUniform.value;

                                ImGui::PushID(("int_" + baseUniform.name).c_str());
                                const std::string label = baseUniform.name + " (int)";
                                DrawInspectorFieldLabel(label.c_str());
                                if (ImGui::Checkbox("##override", &hasOverride))
                                {
                                    if (hasOverride)
                                        material->materialFields.SetInt(baseUniform.name, overrideValue);
                                    else
                                        material->materialFields.RemoveInt(baseUniform.name);
                                }
                                ImGui::SameLine();

                                int editedValue = overrideValue;
                                if (!hasOverride)
                                    ImGui::BeginDisabled();
                                SetNextInspectorFieldItemWidth();
                                if (ImGui::DragInt("##value", &editedValue, 1.0f) && hasOverride)
                                    material->materialFields.SetInt(baseUniform.name, editedValue);
                                if (!hasOverride)
                                    ImGui::EndDisabled();
                                ImGui::PopID();
                            }

                            for (const MaterialFields::FloatUniformData &baseUniform : baseMaterialAsset->materialFields.GetFloatUniforms())
                            {
                                float overrideValue = 0.0f;
                                bool hasOverride = material->materialFields.TryGetFloat(baseUniform.name, overrideValue);
                                if (!hasOverride)
                                    overrideValue = baseUniform.value;

                                ImGui::PushID(("float_" + baseUniform.name).c_str());
                                const std::string label = baseUniform.name + " (float)";
                                DrawInspectorFieldLabel(label.c_str());
                                if (ImGui::Checkbox("##override", &hasOverride))
                                {
                                    if (hasOverride)
                                        material->materialFields.SetFloat(baseUniform.name, overrideValue);
                                    else
                                        material->materialFields.RemoveFloat(baseUniform.name);
                                }
                                ImGui::SameLine();

                                float editedValue = overrideValue;
                                if (!hasOverride)
                                    ImGui::BeginDisabled();
                                SetNextInspectorFieldItemWidth();
                                if (ImGui::DragFloat("##value", &editedValue, 0.01f) && hasOverride)
                                    material->materialFields.SetFloat(baseUniform.name, editedValue);
                                if (!hasOverride)
                                    ImGui::EndDisabled();
                                ImGui::PopID();
                            }

                            for (const MaterialFields::Vec2UniformData &baseUniform : baseMaterialAsset->materialFields.GetVec2Uniforms())
                            {
                                Vector2 overrideValue = Vector2(0.0f);
                                bool hasOverride = material->materialFields.TryGetVec2(baseUniform.name, overrideValue);
                                if (!hasOverride)
                                    overrideValue = baseUniform.value;

                                ImGui::PushID(("vec2_" + baseUniform.name).c_str());
                                const std::string label = baseUniform.name + " (Vector2)";
                                DrawInspectorFieldLabel(label.c_str());
                                if (ImGui::Checkbox("##override", &hasOverride))
                                {
                                    if (hasOverride)
                                        material->materialFields.SetVec2(baseUniform.name, overrideValue);
                                    else
                                        material->materialFields.RemoveVec2(baseUniform.name);
                                }
                                ImGui::SameLine();

                                Vector2 editedValue = overrideValue;
                                if (!hasOverride)
                                    ImGui::BeginDisabled();
                                SetNextInspectorFieldItemWidth();
                                if (ImGui::DragFloat2("##value", &editedValue.x, 0.01f) && hasOverride)
                                    material->materialFields.SetVec2(baseUniform.name, editedValue);
                                if (!hasOverride)
                                    ImGui::EndDisabled();
                                ImGui::PopID();
                            }

                            for (const MaterialFields::Vec3UniformData &baseUniform : baseMaterialAsset->materialFields.GetVec3Uniforms())
                            {
                                Vector3 overrideValue = Vector3(0.0f);
                                bool hasOverride = material->materialFields.TryGetVec3(baseUniform.name, overrideValue);
                                if (!hasOverride)
                                    overrideValue = baseUniform.value;

                                ImGui::PushID(("vec3_" + baseUniform.name).c_str());
                                const std::string label = baseUniform.name + " (Vector3)";
                                DrawInspectorFieldLabel(label.c_str());
                                if (ImGui::Checkbox("##override", &hasOverride))
                                {
                                    if (hasOverride)
                                        material->materialFields.SetVec3(baseUniform.name, overrideValue);
                                    else
                                        material->materialFields.RemoveVec3(baseUniform.name);
                                }
                                ImGui::SameLine();

                                Vector3 editedValue = overrideValue;
                                if (!hasOverride)
                                    ImGui::BeginDisabled();
                                SetNextInspectorFieldItemWidth();
                                if (ImGui::DragFloat3("##value", &editedValue.x, 0.01f) && hasOverride)
                                    material->materialFields.SetVec3(baseUniform.name, editedValue);
                                if (!hasOverride)
                                    ImGui::EndDisabled();
                                ImGui::PopID();
                            }

                            for (const MaterialFields::Vec4UniformData &baseUniform : baseMaterialAsset->materialFields.GetVec4Uniforms())
                            {
                                Vector4 overrideValue = Vector4(0.0f);
                                bool hasOverride = material->materialFields.TryGetVec4(baseUniform.name, overrideValue);
                                if (!hasOverride)
                                    overrideValue = baseUniform.value;

                                ImGui::PushID(("vec4_" + baseUniform.name).c_str());
                                const std::string label = baseUniform.name + " (Vector4)";
                                DrawInspectorFieldLabel(label.c_str());
                                if (ImGui::Checkbox("##override", &hasOverride))
                                {
                                    if (hasOverride)
                                        material->materialFields.SetVec4(baseUniform.name, overrideValue);
                                    else
                                        material->materialFields.RemoveVec4(baseUniform.name);
                                }
                                ImGui::SameLine();

                                Vector4 editedValue = overrideValue;
                                if (!hasOverride)
                                    ImGui::BeginDisabled();
                                SetNextInspectorFieldItemWidth();
                                if (ImGui::DragFloat4("##value", &editedValue.x, 0.01f) && hasOverride)
                                    material->materialFields.SetVec4(baseUniform.name, editedValue);
                                if (!hasOverride)
                                    ImGui::EndDisabled();
                                ImGui::PopID();
                            }

                            for (const MaterialFields::ColorUniformData &baseUniform : baseMaterialAsset->materialFields.GetColorUniforms())
                            {
                                Color overrideValue = Color(1.0f);
                                bool hasOverride = material->materialFields.TryGetColor(baseUniform.name, overrideValue);
                                if (!hasOverride)
                                    overrideValue = baseUniform.value;

                                ImGui::PushID(("color_" + baseUniform.name).c_str());
                                const std::string label = baseUniform.name + " (Color)";
                                DrawInspectorFieldLabel(label.c_str());
                                if (ImGui::Checkbox("##override", &hasOverride))
                                {
                                    if (hasOverride)
                                        material->materialFields.SetColor(baseUniform.name, overrideValue);
                                    else
                                        material->materialFields.RemoveColor(baseUniform.name);
                                }
                                ImGui::SameLine();

                                Color editedValue = overrideValue;
                                if (!hasOverride)
                                    ImGui::BeginDisabled();
                                SetNextInspectorFieldItemWidth();
                                if (ImGui::ColorEdit4("##value", &editedValue.r) && hasOverride)
                                    material->materialFields.SetColor(baseUniform.name, editedValue);
                                if (!hasOverride)
                                    ImGui::EndDisabled();
                                ImGui::PopID();
                            }

                            for (const MaterialFields::TextureUniformData &baseUniform : baseMaterialAsset->materialFields.GetTextureUniforms())
                            {
                                i32 overrideTextureId = -1;
                                bool hasOverride = material->materialFields.TryGetTexture(baseUniform.name, overrideTextureId);
                                if (!hasOverride)
                                    overrideTextureId = baseUniform.textureId;
                                const i32 originalOverrideTextureId = overrideTextureId;

                                ImGui::PushID(("texture_" + baseUniform.name).c_str());
                                const std::string label = baseUniform.name + " (Texture)";
                                DrawInspectorFieldLabel(label.c_str());
                                if (ImGui::Checkbox("##override", &hasOverride))
                                {
                                    if (hasOverride)
                                        material->materialFields.SetTexture(baseUniform.name, overrideTextureId);
                                    else
                                        material->materialFields.RemoveTexture(baseUniform.name);
                                }
                                ImGui::SameLine();

                                if (!hasOverride)
                                    ImGui::BeginDisabled();

                                const std::string textureButtonLabel = getTextureLabel(overrideTextureId) + "##texture_override";
                                SetNextInspectorFieldItemWidth();
                                ImGui::Button(textureButtonLabel.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0));
                                if (hasOverride && ImGui::BeginDragDropTarget())
                                {
                                    if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
                                    {
                                        const AssetDragData dropped = *static_cast<const AssetDragData *>(payload->Data);
                                        std::string path = std::string(dropped.path);
                                        if (path.empty() || !FileExists(path.c_str()))
                                            path = AssetManager::GetPath(dropped.uuid);

                                        if (MetaFileAsset *meta = AssetManager::GetMetaFile(path))
                                        {
                                            if (meta->type == MetaFileAsset::FileType::TEXTURE)
                                                overrideTextureId = AssetManager::LoadTexture(path);
                                        }
                                    }
                                    ImGui::EndDragDropTarget();
                                }

                                if (hasOverride && ImGui::BeginPopupContextItem("texture_override_ctx"))
                                {
                                    if (ImGui::MenuItem("Clear"))
                                        overrideTextureId = -1;
                                    ImGui::EndPopup();
                                }

                                if (!hasOverride)
                                    ImGui::EndDisabled();

                                ImGui::PopID();

                                if (hasOverride && overrideTextureId != originalOverrideTextureId)
                                    material->materialFields.SetTexture(baseUniform.name, overrideTextureId);
                            }
                        }
                    }
                }
            },
        };

        RegisterScript(materialConf);

        ScriptConf modelConf = {
            .name = "Canis::Model",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void {
                if (!_entity.HasComponent<Transform>())
                    _entity.AddComponent<Transform>();

                Model* model = _entity.AddComponent<Model>();
                model->modelId = AssetManager::LoadModel("assets/defaults/models/cube.glb");

                if (!_entity.HasComponent<Material>())
                {
                    Material* material = _entity.AddComponent<Material>();
                    material->materialId = AssetManager::LoadMaterial("assets/defaults/materials/default.material");
                }
            },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<Model>(); },
            .Remove = [this](Entity& _entity) -> void {
                _entity.RemoveComponent<ModelAnimation>();
                _entity.RemoveComponent<Model>();
            },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<Model>() ? (void*)(&_entity.GetComponent<Model>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (_entity.HasComponent<Model>())
                {
                    Model& model = _entity.GetComponent<Model>();
                    YAML::Node comp;
                    comp["color"] = model.color;
                    if (model.nodeIndex >= 0)
                        comp["nodeIndex"] = model.nodeIndex;
                    if (!model.applyNodeTransform)
                        comp["applyNodeTransform"] = model.applyNodeTransform;
                    if (model.staticModel)
                        comp["static"] = model.staticModel;
                    if (!model.castShadow)
                        comp["castShadow"] = model.castShadow;
                    if (model.modelId > -1)
                    {
                        if (ModelAsset* modelAsset = AssetManager::GetModel(model.modelId))
                        {
                            YAML::Node modelAssetNode;
                            if (MetaFileAsset* meta = AssetManager::GetMetaFile(modelAsset->GetPath()))
                                modelAssetNode["uuid"] = (uint64_t)meta->uuid;

                            comp["ModelAsset"] = modelAssetNode;
                        }
                    }

                    _node["Canis::Model"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                YAML::Node comp = _node["Canis::Model"];
                if (!comp)
                    comp = _node["Canis::Model"];

                if (comp)
                {
                    auto &model = *_entity.AddComponent<Model>();
                    model.color = comp["color"].as<Vector4>(Color(1.0f));
                    model.nodeIndex = comp["nodeIndex"].as<i32>(-1);
                    model.applyNodeTransform = comp["applyNodeTransform"].as<bool>(true);
                    model.staticModel = comp["static"].as<bool>(false);
                    model.castShadow = comp["castShadow"].as<bool>(true);
                    std::string path = "";
                    if (auto modelAsset = comp["ModelAsset"])
                    {
                        if (auto uuidNode = modelAsset["uuid"])
                        {
                            UUID uuid = uuidNode.as<uint64_t>(0);
                            path = AssetManager::GetPath(uuid);
                            if (path.rfind("Path was not found", 0) == 0)
                                path.clear();
                        }

                        if (path.empty())
                            path = modelAsset["path"].as<std::string>("");
                    }

                    if (!path.empty())
                        model.modelId = AssetManager::LoadModel(path);

                    // Backward compatibility: migrate legacy animation fields on Canis::Model.
                    //if (!_node["Canis::ModelAnimation"])
                    //{
                    //    const bool hasLegacyAnimation =
                    //        comp["playAnimation"].IsDefined() ||
                    //        comp["loop"].IsDefined() ||
                    //        comp["animationSpeed"].IsDefined() ||
                    //        comp["animationTime"].IsDefined() ||
                    //        comp["animationIndex"].IsDefined();
//
                    //    if (hasLegacyAnimation && !_entity.HasComponent<ModelAnimation>())
                    //    {
                    //        auto &animation = *_entity.AddComponent<ModelAnimation>();
                    //        animation.playAnimation = comp["playAnimation"].as<bool>(true);
                    //        animation.loop = comp["loop"].as<bool>(true);
                    //        animation.animationSpeed = comp["animationSpeed"].as<float>(1.0f);
                    //        animation.animationTime = comp["animationTime"].as<float>(0.0f);
                    //        animation.animationIndex = comp["animationIndex"].as<i32>(0);
//
                    //        if (_callCreate)
                    //            animation.Create();
                    //    }
                    //}

                    if (_callCreate)
                        model.Create();
                }
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                Model* model = nullptr;
                if (_entity.HasComponent<Model>() && ((model = &_entity.GetComponent<Model>()), true))
                {
                    DrawInspectorColorField("color", _conf.name.c_str(), model->color);
                    DrawInspectorField(_editor, "static", _conf.name.c_str(), model->staticModel);
                    DrawInspectorField(_editor, "castShadow", _conf.name.c_str(), model->castShadow);
                    std::string modelLabel = "[ empty ]";
                    ModelAsset* modelAsset = nullptr;
                    if (model->modelId > -1)
                    {
                        modelAsset = AssetManager::GetModel(model->modelId);
                        if (modelAsset != nullptr)
                        {
                            if (MetaFileAsset* meta = AssetManager::GetMetaFile(modelAsset->GetPath()))
                                modelLabel = meta->name;
                            else
                                modelLabel = modelAsset->GetPath();
                        }
                    }

                    ImGui::Text("model");
                    ImGui::SameLine();
                    ImGui::Button(modelLabel.c_str(), ImVec2(150, 0));

                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
                        {
                            const AssetDragData dropped = *static_cast<const AssetDragData*>(payload->Data);
                            std::string path = AssetManager::GetPath(dropped.uuid);
                            std::string extension = GetFileExtension(path);

                            if (extension == "gltf" || extension == "glb" || extension == "obj")
                            {
                                model->modelId = AssetManager::LoadModel(path);
                                model->nodeIndex = -1;
                                model->applyNodeTransform = true;
                                if (ModelAnimation* animation = _entity.HasComponent<ModelAnimation>() ? &_entity.GetComponent<ModelAnimation>() : nullptr)
                                {
                                    animation->animationTime = 0.0f;
                                    animation->animationIndex = 0;
                                    animation->poseModelId = -1;
                                    animation->poseGeometryRevision = 0u;
                                }
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    if (modelAsset != nullptr)
                    {
                        if (model->nodeIndex >= modelAsset->GetNodeCount())
                            model->nodeIndex = -1;

                        auto getNodeLabel = [&](i32 _nodeIndex) -> std::string
                        {
                            if (_nodeIndex < 0)
                                return "Whole Model";

                            std::string nodeName = modelAsset->GetNodeName(_nodeIndex);
                            if (nodeName.empty())
                                nodeName = "Node " + std::to_string(_nodeIndex);

                            return nodeName;
                        };

                        const std::string selectedNodeLabel = getNodeLabel(model->nodeIndex);
                        ImGui::Text("submodel");
                        ImGui::SameLine();
                        if (ImGui::BeginCombo("##modelNode", selectedNodeLabel.c_str()))
                        {
                            if (ImGui::Selectable("Whole Model", model->nodeIndex < 0))
                                model->nodeIndex = -1;

                            const i32 nodeCount = modelAsset->GetNodeCount();
                            for (i32 nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex)
                            {
                                const std::string nodeName = getNodeLabel(nodeIndex);
                                if (ImGui::Selectable(nodeName.c_str(), model->nodeIndex == nodeIndex))
                                    model->nodeIndex = nodeIndex;
                            }

                            ImGui::EndCombo();
                        }

                        if (model->nodeIndex >= 0)
                        {
                            ImGui::Text("apply node transform");
                            ImGui::SameLine();
                            ImGui::Checkbox("##modelApplyNodeTransform", &model->applyNodeTransform);
                        }
                    }

                }
            },
        };

        RegisterScript(modelConf);

        ScriptConf modelAnimationConf = {
            .name = "Canis::ModelAnimation",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void {
                if (!_entity.HasComponent<Model>())
                {
                    if (!_entity.HasComponent<Transform>())
                        _entity.AddComponent<Transform>();

                    Model* model = _entity.AddComponent<Model>();
                    model->modelId = AssetManager::LoadModel("assets/models/dq.gltf");
                }

                ModelAnimation* animation = _entity.AddComponent<ModelAnimation>();
                animation->animationIndex = 0;
                animation->animationTime = 0.0f;
            },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<ModelAnimation>(); },
            .Remove = [this](Entity& _entity) -> void { _entity.RemoveComponent<ModelAnimation>(); },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<ModelAnimation>() ? (void*)(&_entity.GetComponent<ModelAnimation>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (_entity.HasComponent<ModelAnimation>())
                {
                    ModelAnimation& animation = _entity.GetComponent<ModelAnimation>();
                    YAML::Node comp;
                    comp["playAnimation"] = animation.playAnimation;
                    comp["loop"] = animation.loop;
                    comp["animationSpeed"] = animation.animationSpeed;
                    comp["animationTime"] = animation.animationTime;
                    comp["animationIndex"] = animation.animationIndex;
                    comp["rootMotionMask"] = animation.rootMotionMask;
                    _node["Canis::ModelAnimation"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                YAML::Node comp = _node["Canis::ModelAnimation"];
                if (!comp)
                    comp = _node["Canis::ModelAnimation"];

                if (comp)
                {
                    auto &animation = *_entity.AddComponent<ModelAnimation>();
                    animation.playAnimation = comp["playAnimation"].as<bool>(true);
                    animation.loop = comp["loop"].as<bool>(true);
                    animation.animationSpeed = comp["animationSpeed"].as<float>(1.0f);
                    animation.animationTime = comp["animationTime"].as<float>(0.0f);
                    animation.animationIndex = comp["animationIndex"].as<i32>(0);
                    animation.rootMotionMask =
                        comp["rootMotionMask"].as<Vector3>(Vector3(0.0f));

                    if (_callCreate)
                        animation.Create();
                }
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                ModelAnimation* animation = nullptr;
                if (_entity.HasComponent<ModelAnimation>() && ((animation = &_entity.GetComponent<ModelAnimation>()), true))
                {
                    DrawInspectorField(_editor, "playAnimation", _conf.name.c_str(), animation->playAnimation);
                    DrawInspectorField(_editor, "loop", _conf.name.c_str(), animation->loop);
                    DrawInspectorField(_editor, "animationSpeed", _conf.name.c_str(), animation->animationSpeed);
                    DrawInspectorField(_editor, "animationTime", _conf.name.c_str(), animation->animationTime);
                    DrawInspectorField(_editor, "rootMotionMask", _conf.name.c_str(), animation->rootMotionMask);

                    ModelAsset* modelAsset = nullptr;
                    if (Model* model = _entity.HasComponent<Model>() ? &_entity.GetComponent<Model>() : nullptr)
                    {
                        if (model->modelId > -1)
                            modelAsset = AssetManager::GetModel(model->modelId);
                    }

                    if (modelAsset != nullptr)
                    {
                        const i32 animationCount = modelAsset->GetAnimationCount();
                        ImGui::Text("animations: %d", animationCount);

                        if (animationCount > 0)
                        {
                            animation->animationIndex = std::clamp(animation->animationIndex, 0, animationCount - 1);
                            DrawInspectorField(_editor, "animationIndex", _conf.name.c_str(), animation->animationIndex);
                            animation->animationIndex = std::clamp(animation->animationIndex, 0, animationCount - 1);

                            ImGui::Text("clip: %s", modelAsset->GetAnimationName(animation->animationIndex).c_str());
                        }
                    }
                    else
                    {
                        ImGui::Text("Model is required.");
                    }
                }
            },
        };

        RegisterScript(modelAnimationConf);

        ScriptConf animatorConf = {};
        animatorConf.name = "Canis::Animator";
        animatorConf.Construct = nullptr;
        animatorConf.Add = [this](Entity &_entity) -> void {
            _entity.AddComponent<Animator>();
        };
        animatorConf.Has = [this](Entity &_entity) -> bool { return _entity.HasComponent<Animator>(); };
        animatorConf.Remove = [this](Entity &_entity) -> void { _entity.RemoveComponent<Animator>(); };
        animatorConf.Get = [this](Entity &_entity) -> void* { return _entity.HasComponent<Animator>() ? static_cast<void*>(&_entity.GetComponent<Animator>()) : nullptr; };
        REGISTER_PROPERTY(animatorConf, Animator, controller);
        REGISTER_PROPERTY(animatorConf, Animator, playing);
        animatorConf.Encode = [](YAML::Node &_node, Entity &_entity) -> void {
            if (!_entity.HasComponent<Animator>())
                return;

            Animator &animator = _entity.GetComponent<Animator>();
            YAML::Node comp;
            comp["controller"] = animator.controller;
            comp["playing"] = animator.playing;
            _node["Canis::Animator"] = comp;
        };
        animatorConf.Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
            if (YAML::Node comp = _node["Canis::Animator"])
            {
                Animator &animator = *_entity.AddComponent<Animator>();
                animator.controller = comp["controller"].as<AnimatorControllerAssetHandle>(animator.controller);
                animator.playing = comp["playing"].as<bool>(animator.playing);
                if (_callCreate)
                    animator.Create();
            }
        };
        animatorConf.DrawInspector = [](Editor &_editor, Entity &_entity, const ScriptConf &_conf) -> void {
            if (!_entity.HasComponent<Animator>())
                return;

            Animator &animator = _entity.GetComponent<Animator>();
            DrawRegisteredProperties(_editor, _conf.registry, &animator, _conf.name);

            ImGui::Text("currentState: %s", animator.currentState.empty() ? "[ none ]" : animator.currentState.c_str());
            ImGui::Text("time: %.3f", animator.time);

            const std::string controllerPath = AssetManager::ResolvePath(animator.controller);
            AnimatorControllerAsset *controller = controllerPath.empty() ? nullptr : AssetManager::GetAnimatorController(controllerPath);
            if (controller == nullptr)
                return;

            for (const AnimatorParameterDefinition &definition : controller->parameters)
            {
                if (animator.GetParameter(definition.name) != nullptr)
                    continue;

                AnimatorParameterRuntime parameter = {};
                parameter.name = definition.name;
                switch (definition.type)
                {
                    case AnimatorParameterType::INT:
                        parameter.value = AnimationValue::Int(0);
                        break;
                    case AnimatorParameterType::BOOL:
                    case AnimatorParameterType::TRIGGER:
                        parameter.value = AnimationValue::Bool(false);
                        break;
                    case AnimatorParameterType::FLOAT:
                    default:
                        parameter.value = AnimationValue::Float(0.0f);
                        break;
                }

                if (definition.defaultValue.type != AnimationValueType::NONE)
                    parameter.value = definition.defaultValue;
                animator.parameters.push_back(parameter);
            }

            if (ImGui::CollapsingHeader("Parameters", ImGuiTreeNodeFlags_DefaultOpen))
            {
                for (const AnimatorParameterDefinition &definition : controller->parameters)
                {
                    AnimatorParameterRuntime *runtimeParameter = animator.GetParameter(definition.name);
                    if (runtimeParameter == nullptr)
                        continue;

                    ImGui::PushID(definition.name.c_str());
                    switch (definition.type)
                    {
                        case AnimatorParameterType::FLOAT:
                        {
                            float value = runtimeParameter->value.AsFloat();
                            DrawInspectorField(_editor, definition.name.c_str(), _conf.name.c_str(), value);
                            if (value != runtimeParameter->value.AsFloat())
                                runtimeParameter->value = AnimationValue::Float(value);
                            break;
                        }
                        case AnimatorParameterType::INT:
                        {
                            int value = runtimeParameter->value.AsInt();
                            DrawInspectorField(_editor, definition.name.c_str(), _conf.name.c_str(), value);
                            if (value != runtimeParameter->value.AsInt())
                                runtimeParameter->value = AnimationValue::Int(value);
                            break;
                        }
                        case AnimatorParameterType::BOOL:
                        {
                            bool value = runtimeParameter->value.AsBool();
                            DrawInspectorField(_editor, definition.name.c_str(), _conf.name.c_str(), value);
                            if (value != runtimeParameter->value.AsBool())
                                runtimeParameter->value = AnimationValue::Bool(value);
                            break;
                        }
                        case AnimatorParameterType::TRIGGER:
                        {
                            if (ImGui::Button(definition.name.c_str()))
                            {
                                runtimeParameter->triggerActive = true;
                                runtimeParameter->value = AnimationValue::Bool(true);
                            }
                            ImGui::SameLine();
                            ImGui::TextUnformatted(runtimeParameter->triggerActive ? "[ armed ]" : "[ idle ]");
                            break;
                        }
                    }
                    ImGui::PopID();
                }
            }
        };

        RegisterScript(animatorConf);

        ScriptConf animationPlayerConf = {};
        animationPlayerConf.name = "Canis::AnimationPlayer";
        animationPlayerConf.Construct = nullptr;
        animationPlayerConf.Add = [this](Entity &_entity) -> void {
            _entity.AddComponent<AnimationPlayer>();
        };
        animationPlayerConf.Has = [this](Entity &_entity) -> bool { return _entity.HasComponent<AnimationPlayer>(); };
        animationPlayerConf.Remove = [this](Entity &_entity) -> void { _entity.RemoveComponent<AnimationPlayer>(); };
        animationPlayerConf.Get = [this](Entity &_entity) -> void* { return _entity.HasComponent<AnimationPlayer>() ? static_cast<void*>(&_entity.GetComponent<AnimationPlayer>()) : nullptr; };
        REGISTER_PROPERTY(animationPlayerConf, AnimationPlayer, clip);
        REGISTER_PROPERTY(animationPlayerConf, AnimationPlayer, playing);
        REGISTER_PROPERTY(animationPlayerConf, AnimationPlayer, loop);
        REGISTER_PROPERTY(animationPlayerConf, AnimationPlayer, speed);
        REGISTER_PROPERTY(animationPlayerConf, AnimationPlayer, time);
        animationPlayerConf.Encode = [](YAML::Node &_node, Entity &_entity) -> void {
            if (!_entity.HasComponent<AnimationPlayer>())
                return;

            AnimationPlayer &player = _entity.GetComponent<AnimationPlayer>();
            YAML::Node comp;
            comp["clip"] = player.clip;
            comp["playing"] = player.playing;
            comp["loop"] = player.loop;
            comp["speed"] = player.speed;
            comp["time"] = player.time;
            _node["Canis::AnimationPlayer"] = comp;
        };
        animationPlayerConf.Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
            if (YAML::Node comp = _node["Canis::AnimationPlayer"])
            {
                AnimationPlayer &player = *_entity.AddComponent<AnimationPlayer>();
                player.clip = comp["clip"].as<AnimationClipAssetHandle>(player.clip);
                player.playing = comp["playing"].as<bool>(player.playing);
                player.loop = comp["loop"].as<bool>(player.loop);
                player.speed = comp["speed"].as<float>(player.speed);
                player.time = comp["time"].as<float>(player.time);
                if (_callCreate)
                    player.Create();
            }
        };
        animationPlayerConf.DrawInspector = [](Editor &_editor, Entity &_entity, const ScriptConf &_conf) -> void {
            if (AnimationPlayer *component = (_entity.HasComponent<AnimationPlayer>() ? &_entity.GetComponent<AnimationPlayer>() : nullptr))
                DrawRegisteredProperties(_editor, _conf.registry, component, _conf.name);
        };

        RegisterScript(animationPlayerConf);

        ScriptConf spriteAnimationConf = {
            .name = "Canis::SpriteAnimation",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void {
                if (!_entity.HasComponent<Sprite2D>())
                {
                    Sprite2D* sprite = _entity.AddComponent<Sprite2D>();
                    sprite->textureHandle = Canis::AssetManager::GetTextureHandle("assets/defaults/textures/square.png");
                }
                
                SpriteAnimation* anim = _entity.AddComponent<SpriteAnimation>();
            },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<SpriteAnimation>(); },
            .Remove = [this](Entity& _entity) -> void { _entity.RemoveComponent<SpriteAnimation>(); },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<SpriteAnimation>() ? (void*)(&_entity.GetComponent<SpriteAnimation>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (_entity.HasComponent<SpriteAnimation>())
                {
                    SpriteAnimation& animation = _entity.GetComponent<SpriteAnimation>();

                    YAML::Node comp;
                    comp["id"] = (uint64_t)AssetManager::GetMetaFile(AssetManager::GetPath(animation.id))->uuid;
                    comp["speed"] = animation.speed;

                    _node["Canis::SpriteAnimation"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                if (auto comp = _node["Canis::SpriteAnimation"])
                {
                    auto &animation = *_entity.AddComponent<SpriteAnimation>();
                    
                    Canis::UUID uuid = comp["id"].as<u64>();
                    AssetManager::GetSpriteAnimation(AssetManager::GetPath(uuid));
                    animation.id = AssetManager::GetID(uuid);
                    animation.speed = comp["speed"].as<f32>(1.0f);
                    
                    if (_callCreate)
                        animation.Create();
                }
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                SpriteAnimation* animation = nullptr;
                if (_entity.HasComponent<SpriteAnimation>() && ((animation = &_entity.GetComponent<SpriteAnimation>()), true))
                {
                    
                    _editor.InputAnimationClip("animation", animation->id);
                    DrawInspectorField(_editor, "speed", _conf.name.c_str(), animation->speed);
                }
            },
        };

        RegisterScript(spriteAnimationConf);

        // register inspector items
        InspectorItemRightClick inspectorCreateSquare = {
            .name = "Create Square",
            .Func = [](App& _app, Editor& _editor, Entity& _entity, std::vector<ScriptConf>& _scriptConfs) -> void {
                Canis::Entity *entityOne = _app.scene.CreateEntity("Square");
                RectTransform * transform = entityOne->AddComponent<RectTransform>();
                Canis::Sprite2D *sprite = entityOne->AddComponent<Sprite2D>();

                sprite->textureHandle = Canis::AssetManager::GetTextureHandle("assets/defaults/textures/square.png");
                transform->size = Vector2(64.0f);
            }
        };

        RegisterInspectorItem(inspectorCreateSquare);

        InspectorItemRightClick inspectorCreateCircle = {
            .name = "Create Circle",
            .Func = [](App& _app, Editor& _editor, Entity& _entity, std::vector<ScriptConf>& _scriptConfs) -> void {
                Canis::Entity *entityOne = _app.scene.CreateEntity("Circle");
                RectTransform * transform = entityOne->AddComponent<RectTransform>();
                Canis::Sprite2D *sprite = entityOne->AddComponent<Sprite2D>();

                sprite->textureHandle = Canis::AssetManager::GetTextureHandle("assets/defaults/textures/circle.png");
                transform->size = Vector2(64.0f);
            }
        };

        RegisterInspectorItem(inspectorCreateCircle);

        InspectorItemRightClick inspectorCreateDirectionalLight = {
            .name = "Create Directional Light",
            .Func = [](App& _app, Editor& _editor, Entity& _entity, std::vector<ScriptConf>& _scriptConfs) -> void {
                Canis::Entity *lightEntity = _app.scene.CreateEntity("Directional Light");
                lightEntity->AddComponent<DirectionalLight>();
            }
        };

        RegisterInspectorItem(inspectorCreateDirectionalLight);

        InspectorItemRightClick inspectorCreatePointLight = {
            .name = "Create Point Light",
            .Func = [](App& _app, Editor& _editor, Entity& _entity, std::vector<ScriptConf>& _scriptConfs) -> void {
                Canis::Entity *lightEntity = _app.scene.CreateEntity("Point Light");
                Transform *transform = lightEntity->AddComponent<Transform>();
                transform->position = Vector3(2.0f, 2.5f, 2.0f);
                lightEntity->AddComponent<PointLight>();
            }
        };

        RegisterInspectorItem(inspectorCreatePointLight);

        InspectorItemRightClick inspectorCreateCube = {
            .name = "Create Cube",
            .Func = [](App& _app, Editor& _editor, Entity& _entity, std::vector<ScriptConf>& _scriptConfs) -> void {
                Canis::Entity *cube = _app.scene.CreateEntity("Cube");
                
                Transform *transform = cube->AddComponent<Transform>();
                transform->position = Vector3(0.0f);
                
                Model* model = cube->AddComponent<Model>();
                model->modelId = AssetManager::LoadModel("assets/defaults/models/cube.glb");

                Material* material = cube->AddComponent<Material>();
                material->materialId = AssetManager::LoadMaterial("assets/defaults/materials/default.material");
            }
        };

        RegisterInspectorItem(inspectorCreateCube);
    }

    float App::FPS()
    {
        return Time::FPS();
    }

    float App::DeltaTime()
    {
        return Time::DeltaTime();
    }

    void App::SetTargetFPS(float _targetFPS)
    {
        Time::SetTargetFPS(_targetFPS);
    }

    ComponentConf* App::GetComponentConf(const std::string& _name)
    {
        for (ComponentConf& sc : m_scriptRegistry)
        {
            if (sc.name == _name)
                return &sc;
        }

        return nullptr;
    }

    ScriptConf* App::GetScriptConf(const std::string& _name)
    {
        return GetComponentConf(_name);
    }

    SystemConf* App::GetSystemConf(const std::string& _name)
    {
        for (SystemConf& conf : m_systemRegistry)
        {
            if (conf.name == _name)
                return &conf;
        }

        return nullptr;
    }

    void App::LoadScene(const std::string& _path)
    {
        if (_path.empty())
            return;

        m_pendingScenePath = _path;
    }

    void App::LoadScene(const SceneAssetHandle& _sceneAssetHandle)
    {
        LoadScene(AssetManager::ResolvePath(_sceneAssetHandle));
    }

    NetworkSession& App::GetNetwork()
    {
        if (m_network == nullptr)
            m_network = std::make_unique<NetworkSession>(*this);

        return *m_network;
    }

    const NetworkSession& App::GetNetwork() const
    {
        if (m_network == nullptr)
            m_network = std::make_unique<NetworkSession>(*const_cast<App*>(this));

        return *m_network;
    }

    bool App::DispatchUIAction(Entity& _targetEntity, const std::string& _scriptName, const std::string& _actionName, const UIActionContext& _context)
    {
        if (_actionName.empty())
            return false;

        auto invokeAction = [&](ScriptConf& _conf) -> bool
        {
            if (_conf.kind != RegistryEntryKind::Script)
                return false;

            auto actionIt = _conf.uiActions.find(_actionName);
            if (actionIt == _conf.uiActions.end() || _conf.Get == nullptr)
                return false;

            if (ScriptableEntity* script = static_cast<ScriptableEntity*>(_conf.Get(_targetEntity)))
            {
                actionIt->second(*script, _context);
                return true;
            }

            return false;
        };

        if (!_scriptName.empty())
        {
            if (ScriptConf* conf = GetScriptConf(_scriptName))
                return invokeAction(*conf);

            return false;
        }

        bool handled = false;
        for (ScriptConf& conf : m_scriptRegistry)
            handled = invokeAction(conf) || handled;

        return handled;
    }

    bool App::DispatchAnimationEvent(Entity& _targetEntity, const std::string& _scriptName, const std::string& _eventName, const AnimationEventContext& _context)
    {
        if (_eventName.empty())
            return false;

        auto invokeEvent = [&](ScriptConf& _conf) -> bool
        {
            if (_conf.kind != RegistryEntryKind::Script)
                return false;

            auto eventIt = _conf.animationEvents.find(_eventName);
            if (eventIt == _conf.animationEvents.end() || _conf.Get == nullptr)
                return false;

            if (ScriptableEntity* script = static_cast<ScriptableEntity*>(_conf.Get(_targetEntity)))
            {
                eventIt->second(*script, _context);
                return true;
            }

            return false;
        };

        if (!_scriptName.empty())
        {
            if (ScriptConf* conf = GetScriptConf(_scriptName))
                return invokeEvent(*conf);

            return false;
        }

        bool handled = false;
        for (ScriptConf& conf : m_scriptRegistry)
            handled = invokeEvent(conf) || handled;

        return handled;
    }

    bool App::AddRequiredComponent(Entity& _entity, const std::string& _name)
    {
        if (ComponentConf* sc = GetComponentConf(_name))
        {
            if (sc->Has && sc->Has(_entity) == false)
            {
                if (sc->Add)
                    sc->Add(_entity);
            }

            return true;
        }
        else
        {
            return false;
        }
    }

    bool App::AddRequiredScript(Entity& _entity, const std::string& _name)
    {
        return AddRequiredComponent(_entity, _name);
    }

    void App::RegisterComponent(ComponentConf &_conf)
    {
        for (ComponentConf &sc : m_scriptRegistry)
            if (_conf.name == sc.name)
                return;

        ComponentConf conf = _conf;
        conf.registeredFromGameCode = m_registeringGameCodeScripts;
        m_scriptRegistry.push_back(conf);
    }

    void App::RegisterScript(ScriptConf &_conf)
    {
        RegisterComponent(_conf);
    }

    void App::UnregisterComponent(ComponentConf &_conf)
    {
        for (int i = 0; i < m_scriptRegistry.size(); i++)
        {
            if (_conf.name == m_scriptRegistry[i].name)
            {
                ScriptConf& conf = m_scriptRegistry[i];

                for (Entity* entity : scene.GetEntities())
                {
                    if (entity == nullptr)
                        continue;

                    if (conf.Has != nullptr && !conf.Has(*entity))
                        continue;

                    if (conf.Remove != nullptr)
                        conf.Remove(*entity);
                }

                m_scriptRegistry.erase(m_scriptRegistry.begin() + i);
                i--;
            }
        }
    }

    void App::UnregisterScript(ScriptConf &_conf)
    {
        UnregisterComponent(_conf);
    }

    void App::RegisterSystem(SystemConf &_conf)
    {
        if (_conf.Construct == nullptr)
            return;

        for (SystemConf &conf : m_systemRegistry)
        {
            if (conf.name == _conf.name)
                return;
        }

        m_systemRegistry.push_back(_conf);
    }

    void App::UnregisterSystem(SystemConf &_conf)
    {
        for (int i = 0; i < m_systemRegistry.size(); ++i)
        {
            if (m_systemRegistry[i].name != _conf.name)
                continue;

            m_systemRegistry.erase(m_systemRegistry.begin() + i);
            --i;
        }
    }

    void App::RegisterInspectorItem(InspectorItemRightClick& _item)
    {
        for (InspectorItemRightClick &item : m_inspectorItemRegistry)
            if (item.name == _item.name)
                return;

        m_inspectorItemRegistry.push_back(_item);
    }

    void App::UnregisterInspectorItem(InspectorItemRightClick& _item)
    {
        for (int i = 0; i < m_inspectorItemRegistry.size(); i++)
        {
            if (_item.name == m_inspectorItemRegistry[i].name)
            {
                m_inspectorItemRegistry.erase(m_inspectorItemRegistry.begin() + i);
                i--;
            }
        }
    }

} // namespace Canis
