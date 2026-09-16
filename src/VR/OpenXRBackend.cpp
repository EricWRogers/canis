#include "OpenXRBackend.hpp"
#include <stdexcept>
#ifndef CANIS_OPENXR
#define CANIS_OPENXR 0
#endif
#if CANIS_OPENXR
#include <Canis/OpenGL.hpp>
#include <Canis/Window.hpp>
#include <Canis/Debug.hpp>
#include <SDL3/SDL.h>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#define XR_USE_PLATFORM_WIN32
#else
#include <X11/Xlib.h>
#include <GL/glx.h>
#define XR_USE_PLATFORM_XLIB
#endif
#define XR_USE_GRAPHICS_API_OPENGL
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <vector>
#include <thread>
#include <chrono>

namespace Canis::VR
{
    namespace
    {
        void Check(XrResult result, const char* operation)
        {
            if (XR_FAILED(result)) throw std::runtime_error(std::string(operation) + " failed (OpenXR " + std::to_string(result) + "). Check the active OpenXR runtime and connected headset.");
        }
        Pose ConvertPose(const XrPosef& p, bool valid, float floor)
        {
            if (!valid) return {};
            return {{p.position.x, p.position.y + floor, p.position.z},
                {p.orientation.w, p.orientation.x, p.orientation.y, p.orientation.z}, valid};
        }
        constexpr XrSpaceLocationFlags validPose = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
        struct Chain
        {
            XrSwapchain handle = XR_NULL_HANDLE;
            int width = 0, height = 0;
            GLuint framebuffer = 0, depth = 0;
            std::vector<XrSwapchainImageOpenGLKHR> images;
        };
        class NativeBackend final : public OpenXRBackend
        {
            XrInstance instance = XR_NULL_HANDLE;
            XrSystemId system = XR_NULL_SYSTEM_ID;
            XrSession session = XR_NULL_HANDLE;
            XrSpace space = XR_NULL_HANDLE;
            XrSpace headSpace = XR_NULL_HANDLE;
            XrActionSet actions = XR_NULL_HANDLE;
            bool gazeSupported = false;
            std::array<XrHandTrackerEXT, 2> handTrackers{};
            PFN_xrCreateHandTrackerEXT createHandTracker = nullptr;
            PFN_xrDestroyHandTrackerEXT destroyHandTracker = nullptr;
            PFN_xrLocateHandJointsEXT locateHandJoints = nullptr;
            std::array<bool, 2> handLocateWarned{};
            XrAction gazeAction = XR_NULL_HANDLE;
            XrSpace gazeSpace = XR_NULL_HANDLE;
            XrAction gripAction = XR_NULL_HANDLE, aimAction = XR_NULL_HANDLE;
            XrAction triggerAction = XR_NULL_HANDLE, squeezeAction = XR_NULL_HANDLE;
            XrAction stickAction = XR_NULL_HANDLE, selectAction = XR_NULL_HANDLE, hapticAction = XR_NULL_HANDLE;
            std::array<XrPath, 2> handPaths{};
            std::array<XrSpace, 2> grips{}, aims{};
            std::array<Chain, 2> chains{};
            std::array<XrView, 2> views{{{XR_TYPE_VIEW}, {XR_TYPE_VIEW}}};
            XrSessionState sessionState = XR_SESSION_STATE_UNKNOWN;
            XrEnvironmentBlendMode blend = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
            XrTime displayTime = 0;
            Config config;
            bool running = false, begun = false, stage = false;
            float floorOffset = 0;
            XrPath Path(const char* text)
            { XrPath path{}; Check(xrStringToPath(instance, text, &path), "xrStringToPath"); return path; }
            XrAction Action(const char* name, const char* label, XrActionType type)
            {
                XrActionCreateInfo info{XR_TYPE_ACTION_CREATE_INFO};
                std::strncpy(info.actionName, name, XR_MAX_ACTION_NAME_SIZE - 1);
                std::strncpy(info.localizedActionName, label, XR_MAX_LOCALIZED_ACTION_NAME_SIZE - 1);
                info.actionType = type;
                info.countSubactionPaths = 2;
                info.subactionPaths = handPaths.data();
                XrAction action{};
                Check(xrCreateAction(actions, &info, &action), "xrCreateAction");
                return action;
            }
            void InputSetup()
            {
                handPaths = {Path("/user/hand/left"), Path("/user/hand/right")};
                XrActionSetCreateInfo setInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
                std::strcpy(setInfo.actionSetName, "gameplay");
                std::strcpy(setInfo.localizedActionSetName, "Food truck gameplay");
                Check(xrCreateActionSet(instance, &setInfo, &actions), "xrCreateActionSet");
                gripAction = Action("grip_pose", "Grip pose", XR_ACTION_TYPE_POSE_INPUT);
                aimAction = Action("aim_pose", "Aim pose", XR_ACTION_TYPE_POSE_INPUT);
                triggerAction = Action("trigger", "Trigger", XR_ACTION_TYPE_FLOAT_INPUT);
                squeezeAction = Action("squeeze", "Grab", XR_ACTION_TYPE_FLOAT_INPUT);
                stickAction = Action("thumbstick", "Locomotion", XR_ACTION_TYPE_VECTOR2F_INPUT);
                selectAction = Action("select", "Select", XR_ACTION_TYPE_BOOLEAN_INPUT);
                hapticAction = Action("haptic", "Haptic feedback", XR_ACTION_TYPE_VIBRATION_OUTPUT);
                for (bool simple : {false, true})
                {
                    std::vector<XrActionSuggestedBinding> bindings;
                    for (unsigned i = 0; i < 2; ++i)
                    {
                        const std::string prefix = i == 0 ? "/user/hand/left" : "/user/hand/right";
                        auto add = [&](XrAction a, const char* suffix) { bindings.push_back({a, Path((prefix + suffix).c_str())}); };
                        add(gripAction, "/input/grip/pose"); add(aimAction, "/input/aim/pose");
                        add(hapticAction, "/output/haptic");
                        if (simple) add(selectAction, "/input/select/click");
                        else
                        {
                            add(triggerAction, "/input/trigger/value"); add(squeezeAction, "/input/squeeze/value");
                            add(stickAction, "/input/thumbstick");
                            add(selectAction, i == 0 ? "/input/x/click" : "/input/a/click");
                        }
                    }
                    XrInteractionProfileSuggestedBinding info{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
                    info.interactionProfile = Path(simple ? "/interaction_profiles/khr/simple_controller" : "/interaction_profiles/oculus/touch_controller");
                    info.countSuggestedBindings = static_cast<uint32_t>(bindings.size());
                    info.suggestedBindings = bindings.data();
                    const auto result = xrSuggestInteractionProfileBindings(instance, &info);
                    if (result != XR_ERROR_PATH_UNSUPPORTED) Check(result, "xrSuggestInteractionProfileBindings");
                }
                for (unsigned i = 0; i < 2; ++i)
                {
                    XrActionSpaceCreateInfo info{XR_TYPE_ACTION_SPACE_CREATE_INFO};
                    info.subactionPath = handPaths[i]; info.poseInActionSpace.orientation.w = 1;
                    info.action = gripAction;
                    Check(xrCreateActionSpace(session, &info, &grips[i]), "xrCreateActionSpace(grip)");
                    info.action = aimAction;
                    Check(xrCreateActionSpace(session, &info, &aims[i]), "xrCreateActionSpace(aim)");
                }
                if (gazeSupported)
                {
                    XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
                    std::strcpy(actionInfo.actionName, "eye_gaze"); std::strcpy(actionInfo.localizedActionName, "Eye gaze");
                    actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
                    Check(xrCreateAction(actions, &actionInfo, &gazeAction), "xrCreateAction(gaze)");
                    XrActionSuggestedBinding binding{gazeAction, Path("/user/eyes_ext/input/gaze_ext/pose")};
                    XrInteractionProfileSuggestedBinding suggestion{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
                    suggestion.interactionProfile = Path("/interaction_profiles/ext/eye_gaze_interaction");
                    suggestion.countSuggestedBindings = 1; suggestion.suggestedBindings = &binding;
                    Check(xrSuggestInteractionProfileBindings(instance, &suggestion), "xrSuggestInteractionProfileBindings(gaze)");
                    XrActionSpaceCreateInfo gazeInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
                    gazeInfo.action = gazeAction; gazeInfo.poseInActionSpace.orientation.w = 1;
                    Check(xrCreateActionSpace(session, &gazeInfo, &gazeSpace), "xrCreateActionSpace(gaze)");
                }
                XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
                attach.countActionSets = 1; attach.actionSets = &actions;
                Check(xrAttachSessionActionSets(session, &attach), "xrAttachSessionActionSets");
            }
            Pose Locate(XrSpace target)
            {
                XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};
                Check(xrLocateSpace(target, space, displayTime, &location), "xrLocateSpace");
                return ConvertPose(location.pose, (location.locationFlags & validPose) == validPose, floorOffset);
            }
            void HandTrackingSetup(Diagnostics& diagnostics)
            {
                XrSystemHandTrackingPropertiesEXT support{XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT};
                XrSystemProperties properties{XR_TYPE_SYSTEM_PROPERTIES}; properties.next = &support;
                const auto result = xrGetSystemProperties(instance, system, &properties);
                if (XR_FAILED(result) || !support.supportsHandTracking) return;
                if (XR_FAILED(xrGetInstanceProcAddr(instance, "xrCreateHandTrackerEXT", reinterpret_cast<PFN_xrVoidFunction*>(&createHandTracker))) ||
                    XR_FAILED(xrGetInstanceProcAddr(instance, "xrDestroyHandTrackerEXT", reinterpret_cast<PFN_xrVoidFunction*>(&destroyHandTracker))) ||
                    XR_FAILED(xrGetInstanceProcAddr(instance, "xrLocateHandJointsEXT", reinterpret_cast<PFN_xrVoidFunction*>(&locateHandJoints))) ||
                    !createHandTracker || !destroyHandTracker || !locateHandJoints) return;
                for (unsigned i = 0; i < 2; ++i)
                {
                    XrHandTrackerCreateInfoEXT info{XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT};
                    info.hand = i == 0 ? XR_HAND_LEFT_EXT : XR_HAND_RIGHT_EXT;
                    info.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;
                    const auto created = createHandTracker(session, &info, &handTrackers[i]);
                    if (XR_FAILED(created))
                    {
                        handTrackers[i] = XR_NULL_HANDLE;
                        Debug::Warning("OpenXR hand tracker %u unavailable (%d); using controller animation for that hand", i, created);
                    }
                }
                diagnostics.handTrackingSupported = handTrackers[0] || handTrackers[1];
                if (diagnostics.handTrackingSupported)
                    diagnostics.handTracking = "XR_EXT_hand_tracking available; individual joints used when active";
            }
            void ReadHandJoints(State& state)
            {
                static_assert(static_cast<unsigned>(HandJoint::Count) == XR_HAND_JOINT_COUNT_EXT);
                if (!state.focused || !state.shouldRender) return;
                for (unsigned i = 0; i < 2; ++i)
                {
                    if (!handTrackers[i]) continue;
                    std::array<XrHandJointLocationEXT, XR_HAND_JOINT_COUNT_EXT> joints{};
                    XrHandJointLocationsEXT locations{XR_TYPE_HAND_JOINT_LOCATIONS_EXT};
                    locations.jointCount = joints.size(); locations.jointLocations = joints.data();
                    XrHandJointsLocateInfoEXT info{XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT};
                    info.baseSpace = space; info.time = displayTime;
                    const auto result = locateHandJoints(handTrackers[i], &info, &locations);
                    if (XR_FAILED(result))
                    {
                        if (!handLocateWarned[i]) Debug::Warning("OpenXR hand joints %u unavailable (%d); using controller animation", i, result);
                        handLocateWarned[i] = true;
                        continue;
                    }
                    handLocateWarned[i] = false;
                    if (!locations.isActive) continue;
                    auto& skeleton = state.hands[i].skeleton;
                    bool valid = true;
                    for (unsigned j = 0; j < joints.size(); ++j)
                    {
                        const auto& source = joints[j];
                        auto& target = skeleton.joints[j];
                        target.pose = ConvertPose(source.pose, (source.locationFlags & validPose) == validPose, floorOffset);
                        const auto& p = target.pose.position; const auto& q = target.pose.orientation;
                        target.pose.valid = target.pose.valid && std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) &&
                            std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z) && std::isfinite(q.w) && glm::length(q) > 0.001f;
                        if (target.pose.valid) target.pose.orientation = glm::normalize(q);
                        constexpr auto tracked = XR_SPACE_LOCATION_POSITION_TRACKED_BIT | XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
                        target.tracked = target.pose.valid && (source.locationFlags & tracked) == tracked;
                        target.radius = target.pose.valid && std::isfinite(source.radius) ? std::max(0.0f, source.radius) : 0;
                        valid = valid && target.pose.valid;
                    }
                    skeleton.active = valid;
                    if (!valid) skeleton = {};
                }
            }
            void ReadInput(State& state)
            {
                state.hands = {};
                if (!state.focused) return;
                XrActiveActionSet active{actions, XR_NULL_PATH};
                XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO}; sync.countActiveActionSets = 1; sync.activeActionSets = &active;
                const auto result = xrSyncActions(session, &sync);
                if (result == XR_SESSION_NOT_FOCUSED) { state.focused = false; return; }
                Check(result, "xrSyncActions");
                if (gazeSupported)
                {
                    XrActionStateGetInfo info{XR_TYPE_ACTION_STATE_GET_INFO}; info.action = gazeAction;
                    XrActionStatePose gaze{XR_TYPE_ACTION_STATE_POSE};
                    Check(xrGetActionStatePose(session, &info, &gaze), "xrGetActionStatePose(gaze)");
                    if (gaze.isActive) state.gaze = Locate(gazeSpace);
                }
                for (unsigned i = 0; i < 2; ++i)
                {
                    auto& hand = state.hands[i];
                    XrActionStateGetInfo info{XR_TYPE_ACTION_STATE_GET_INFO}; info.subactionPath = handPaths[i];
                    info.action = gripAction;
                    XrActionStatePose pose{XR_TYPE_ACTION_STATE_POSE};
                    Check(xrGetActionStatePose(session, &info, &pose), "xrGetActionStatePose(grip)");
                    if (pose.isActive) hand.grip = Locate(grips[i]);
                    info.action = aimAction; pose = {XR_TYPE_ACTION_STATE_POSE};
                    Check(xrGetActionStatePose(session, &info, &pose), "xrGetActionStatePose(aim)");
                    if (pose.isActive) hand.aim = Locate(aims[i]);
                    hand.active = hand.grip.valid;
                    if (!hand.active) continue;
                    auto scalar = [&](XrAction a) {
                        info.action = a;
                        XrActionStateFloat v{XR_TYPE_ACTION_STATE_FLOAT};
                        Check(xrGetActionStateFloat(session, &info, &v), "xrGetActionStateFloat");
                        return v.isActive ? std::clamp(v.currentState, 0.0f, 1.0f) : 0.0f;
                    };
                    hand.trigger = scalar(triggerAction); hand.squeeze = scalar(squeezeAction);
                    info.action = stickAction;
                    XrActionStateVector2f stick{XR_TYPE_ACTION_STATE_VECTOR2F};
                    Check(xrGetActionStateVector2f(session, &info, &stick), "xrGetActionStateVector2f");
                    if (stick.isActive) hand.stick = {stick.currentState.x, stick.currentState.y};
                    info.action = selectAction;
                    XrActionStateBoolean select{XR_TYPE_ACTION_STATE_BOOLEAN};
                    Check(xrGetActionStateBoolean(session, &info, &select), "xrGetActionStateBoolean");
                    hand.select = select.isActive && select.currentState;
                }
            }
        public:
            ~NativeBackend() override
            {
                if (begun)
                {
                    XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO}; end.displayTime = displayTime; end.environmentBlendMode = blend;
                    xrEndFrame(session, &end);
                }
                for (auto& chain : chains)
                {
                    if (chain.framebuffer) glDeleteFramebuffers(1, &chain.framebuffer);
                    if (chain.depth) glDeleteRenderbuffers(1, &chain.depth);
                    if (chain.handle) xrDestroySwapchain(chain.handle);
                }
                for (auto s : grips) if (s) xrDestroySpace(s);
                for (auto s : aims) if (s) xrDestroySpace(s);
                if (gazeSpace) xrDestroySpace(gazeSpace);
                if (headSpace) xrDestroySpace(headSpace);
                if (space) xrDestroySpace(space);
                for (auto tracker : handTrackers) if (tracker && destroyHandTracker) destroyHandTracker(tracker);
                if (session) xrDestroySession(session);
                if (actions) xrDestroyActionSet(actions);
                if (instance) xrDestroyInstance(instance);
            }
            void Initialize(Window& window, const Config& cfg, Diagnostics& diagnostics) override
            {
                config = cfg;
                if (!window.MakeContextCurrent()) throw std::runtime_error("Cannot make the VR window GL context current");
                uint32_t count = 0;
                Check(xrEnumerateInstanceExtensionProperties(nullptr, 0, &count, nullptr), "xrEnumerateInstanceExtensionProperties");
                std::vector<XrExtensionProperties> extensions(count, {XR_TYPE_EXTENSION_PROPERTIES});
                Check(xrEnumerateInstanceExtensionProperties(nullptr, count, &count, extensions.data()), "xrEnumerateInstanceExtensionProperties");
                for (const auto& e : extensions) diagnostics.extensions.emplace_back(e.extensionName);
                if (std::find(diagnostics.extensions.begin(), diagnostics.extensions.end(), XR_KHR_OPENGL_ENABLE_EXTENSION_NAME) == diagnostics.extensions.end())
                    throw std::runtime_error("The active OpenXR runtime does not support desktop OpenGL (XR_KHR_opengl_enable). Select SteamVR or a compatible OpenGL runtime.");
                std::vector<const char*> enabled{XR_KHR_OPENGL_ENABLE_EXTENSION_NAME};
                const bool gazeExtension = config.foveation == FoveationMode::EyeTracked &&
                    std::find(diagnostics.extensions.begin(), diagnostics.extensions.end(), XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME) != diagnostics.extensions.end();
                if (gazeExtension) enabled.push_back(XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME);
                const bool handExtension = std::find(diagnostics.extensions.begin(), diagnostics.extensions.end(), XR_EXT_HAND_TRACKING_EXTENSION_NAME) != diagnostics.extensions.end();
                if (handExtension) enabled.push_back(XR_EXT_HAND_TRACKING_EXTENSION_NAME);
                XrInstanceCreateInfo instanceInfo{XR_TYPE_INSTANCE_CREATE_INFO};
                std::strcpy(instanceInfo.applicationInfo.applicationName, "Canis VR Foodtruck");
                std::strcpy(instanceInfo.applicationInfo.engineName, "Canis");
                instanceInfo.applicationInfo.applicationVersion = 1; instanceInfo.applicationInfo.engineVersion = 1;
                instanceInfo.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);
                instanceInfo.enabledExtensionCount = static_cast<uint32_t>(enabled.size()); instanceInfo.enabledExtensionNames = enabled.data();
                Check(xrCreateInstance(&instanceInfo, &instance), "xrCreateInstance");
                XrInstanceProperties properties{XR_TYPE_INSTANCE_PROPERTIES};
                Check(xrGetInstanceProperties(instance, &properties), "xrGetInstanceProperties");
                diagnostics.runtime = properties.runtimeName;
                XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO}; systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
                Check(xrGetSystem(instance, &systemInfo, &system), "xrGetSystem");
                if (gazeExtension)
                {
                    XrSystemEyeGazeInteractionPropertiesEXT gaze{XR_TYPE_SYSTEM_EYE_GAZE_INTERACTION_PROPERTIES_EXT};
                    XrSystemProperties properties{XR_TYPE_SYSTEM_PROPERTIES}; properties.next = &gaze;
                    Check(xrGetSystemProperties(instance, system, &properties), "xrGetSystemProperties(gaze)");
                    gazeSupported = gaze.supportsEyeGazeInteraction;
                    diagnostics.eyeGazeSupported = gazeSupported;
                }
                PFN_xrGetOpenGLGraphicsRequirementsKHR requirementsFn = nullptr;
                Check(xrGetInstanceProcAddr(instance, "xrGetOpenGLGraphicsRequirementsKHR", reinterpret_cast<PFN_xrVoidFunction*>(&requirementsFn)), "xrGetInstanceProcAddr");
                XrGraphicsRequirementsOpenGLKHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
                Check(requirementsFn(instance, system, &requirements), "xrGetOpenGLGraphicsRequirementsKHR");
                GLint major = 0, minor = 0;
                glGetIntegerv(GL_MAJOR_VERSION, &major); glGetIntegerv(GL_MINOR_VERSION, &minor);
                auto version = XR_MAKE_VERSION(major, minor, 0);
                if (version < requirements.minApiVersionSupported || version > requirements.maxApiVersionSupported)
                    throw std::runtime_error("Current OpenGL context version is outside the OpenXR runtime's supported range");
#ifdef _WIN32
                XrGraphicsBindingOpenGLWin32KHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR};
                // Borrow the exact DC/context pair owned by SDL; do not acquire
                // or release a second window DC around a live WGL context.
                binding.hDC = wglGetCurrentDC(); binding.hGLRC = wglGetCurrentContext();
                if (!binding.hDC || !binding.hGLRC) throw std::runtime_error("OpenXR requires an active WGL context");
#else
                (void)window;
                XrGraphicsBindingOpenGLXlibKHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR};
                binding.xDisplay = glXGetCurrentDisplay(); binding.glxContext = glXGetCurrentContext(); binding.glxDrawable = glXGetCurrentDrawable();
                if (!binding.xDisplay || !binding.glxContext) throw std::runtime_error("OpenXR OpenGL requires an active X11/GLX context; EGL/Wayland is not implemented");
                int configID = 0;
                if (glXQueryContext(binding.xDisplay, binding.glxContext, GLX_FBCONFIG_ID, &configID) != Success)
                    throw std::runtime_error("Cannot query GLX framebuffer configuration");
                int screen = 0;
                glXQueryContext(binding.xDisplay, binding.glxContext, GLX_SCREEN, &screen);
                const int attrs[] = {GLX_FBCONFIG_ID, configID, None};
                int configsCount = 0;
                auto configs = glXChooseFBConfig(binding.xDisplay, screen, attrs, &configsCount);
                if (!configs || !configsCount) { if (configs) XFree(configs); throw std::runtime_error("No matching GLX framebuffer configuration"); }
                binding.glxFBConfig = configs[0];
                auto visual = glXGetVisualFromFBConfig(binding.xDisplay, configs[0]);
                XFree(configs);
                if (!visual) throw std::runtime_error("No GLX visual");
                binding.visualid = visual->visualid; XFree(visual);
#endif
                XrSessionCreateInfo sessionInfo{XR_TYPE_SESSION_CREATE_INFO}; sessionInfo.systemId = system; sessionInfo.next = &binding;
                Check(xrCreateSession(instance, &sessionInfo, &session), "xrCreateSession");
                if (handExtension) HandTrackingSetup(diagnostics);
                Check(xrEnumerateReferenceSpaces(session, 0, &count, nullptr), "xrEnumerateReferenceSpaces");
                std::vector<XrReferenceSpaceType> spaces(count);
                Check(xrEnumerateReferenceSpaces(session, count, &count, spaces.data()), "xrEnumerateReferenceSpaces");
                stage = std::find(spaces.begin(), spaces.end(), XR_REFERENCE_SPACE_TYPE_STAGE) != spaces.end();
                floorOffset = stage ? 0.0f : config.player.localFloorHeight;
                XrReferenceSpaceCreateInfo spaceInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
                spaceInfo.referenceSpaceType = stage ? XR_REFERENCE_SPACE_TYPE_STAGE : XR_REFERENCE_SPACE_TYPE_LOCAL;
                spaceInfo.poseInReferenceSpace.orientation.w = 1;
                Check(xrCreateReferenceSpace(session, &spaceInfo, &space), "xrCreateReferenceSpace");
                spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
                Check(xrCreateReferenceSpace(session, &spaceInfo, &headSpace), "xrCreateReferenceSpace(view)");
                InputSetup();
                Check(xrEnumerateEnvironmentBlendModes(instance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &count, nullptr), "xrEnumerateEnvironmentBlendModes");
                std::vector<XrEnvironmentBlendMode> modes(count);
                Check(xrEnumerateEnvironmentBlendModes(instance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, count, &count, modes.data()), "xrEnumerateEnvironmentBlendModes");
                if (std::find(modes.begin(), modes.end(), blend) == modes.end()) throw std::runtime_error("This VR renderer requires an opaque environment blend mode");
                Check(xrEnumerateViewConfigurationViews(instance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &count, nullptr), "xrEnumerateViewConfigurationViews");
                if (count != 2) throw std::runtime_error("This VR renderer requires primary stereo with exactly two views");
                std::array<XrViewConfigurationView, 2> recommendations{{{XR_TYPE_VIEW_CONFIGURATION_VIEW}, {XR_TYPE_VIEW_CONFIGURATION_VIEW}}};
                Check(xrEnumerateViewConfigurationViews(instance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2, &count, recommendations.data()), "xrEnumerateViewConfigurationViews");
                Check(xrEnumerateSwapchainFormats(session, 0, &count, nullptr), "xrEnumerateSwapchainFormats");
                std::vector<int64_t> formats(count);
                Check(xrEnumerateSwapchainFormats(session, count, &count, formats.data()), "xrEnumerateSwapchainFormats");
                // Canis shaders emit display-encoded colors: store them in an sRGB
                // texture with framebuffer conversion OFF so the compositor decodes once.
                int64_t format = 0;
                for (int64_t candidate : {int64_t(GL_SRGB8_ALPHA8)})
                    if (!format && std::find(formats.begin(), formats.end(), candidate) != formats.end()) format = candidate;
                if (!format) throw std::runtime_error("OpenXR has no sRGB8 color swapchain format required by the Canis shader output");
                GLint oldFbo = 0, oldRbo = 0;
                glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFbo); glGetIntegerv(GL_RENDERBUFFER_BINDING, &oldRbo);
                for (unsigned i = 0; i < 2; ++i)
                {
                    auto& chain = chains[i];
                    chain.width = recommendations[i].recommendedImageRectWidth; chain.height = recommendations[i].recommendedImageRectHeight;
                    XrSwapchainCreateInfo info{XR_TYPE_SWAPCHAIN_CREATE_INFO};
                    info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT;
                    info.format = format; info.sampleCount = 1; info.width = chain.width; info.height = chain.height;
                    info.faceCount = 1; info.arraySize = 1; info.mipCount = 1;
                    Check(xrCreateSwapchain(session, &info, &chain.handle), "xrCreateSwapchain");
                    Check(xrEnumerateSwapchainImages(chain.handle, 0, &count, nullptr), "xrEnumerateSwapchainImages");
                    chain.images.resize(count, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR});
                    Check(xrEnumerateSwapchainImages(chain.handle, count, &count, reinterpret_cast<XrSwapchainImageBaseHeader*>(chain.images.data())), "xrEnumerateSwapchainImages");
                    glGenFramebuffers(1, &chain.framebuffer); glGenRenderbuffers(1, &chain.depth);
                    glBindRenderbuffer(GL_RENDERBUFFER, chain.depth);
                    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, chain.width, chain.height);
                }
                glBindFramebuffer(GL_FRAMEBUFFER, oldFbo); glBindRenderbuffer(GL_RENDERBUFFER, oldRbo);
            }
            void Begin(State& state) override
            {
                state = {};
                state.stageSpace = stage;
                XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
                XrResult poll;
                while ((poll = xrPollEvent(instance, &event)) == XR_SUCCESS)
                {
                    if (event.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) state.exitRequested = true;
                    if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED)
                    {
                        const auto& changed = reinterpret_cast<const XrEventDataSessionStateChanged&>(event);
                        sessionState = changed.state;
                        if (sessionState == XR_SESSION_STATE_READY && !running)
                        {
                            XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO}; begin.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                            Check(xrBeginSession(session, &begin), "xrBeginSession"); running = true;
                        }
                        if (sessionState == XR_SESSION_STATE_STOPPING && running)
                        { Check(xrEndSession(session), "xrEndSession"); running = false; }
                        if (sessionState == XR_SESSION_STATE_EXITING || sessionState == XR_SESSION_STATE_LOSS_PENDING) state.exitRequested = true;
                    }
                    event = {XR_TYPE_EVENT_DATA_BUFFER};
                }
                if (poll != XR_EVENT_UNAVAILABLE) Check(poll, "xrPollEvent");
                if (state.exitRequested) { running = false; return; }
                state.running = running;
                state.focused = sessionState == XR_SESSION_STATE_FOCUSED;
                if (!running) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); return; }
                XrFrameWaitInfo wait{XR_TYPE_FRAME_WAIT_INFO}; XrFrameState frame{XR_TYPE_FRAME_STATE};
                Check(xrWaitFrame(session, &wait, &frame), "xrWaitFrame"); displayTime = frame.predictedDisplayTime;
                XrFrameBeginInfo begin{XR_TYPE_FRAME_BEGIN_INFO};
                Check(xrBeginFrame(session, &begin), "xrBeginFrame"); begun = true;
                XrViewLocateInfo locate{XR_TYPE_VIEW_LOCATE_INFO}; locate.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                locate.displayTime = displayTime; locate.space = space;
                XrViewState viewState{XR_TYPE_VIEW_STATE}; uint32_t count = 0;
                Check(xrLocateViews(session, &locate, &viewState, 2, &count, views.data()), "xrLocateViews");
                const auto flags = XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_VALID_BIT;
                const bool valid = count == 2 && (viewState.viewStateFlags & flags) == flags;
                state.shouldRender = frame.shouldRender && valid;
                state.head = Locate(headSpace);
                for (unsigned i = 0; i < 2; ++i)
                {
                    state.eyes[i].pose = ConvertPose(views[i].pose, valid, floorOffset);
                    const auto& fov = views[i].fov;
                    if (valid) state.eyes[i].projection = Projection(fov.angleLeft, fov.angleRight, fov.angleDown, fov.angleUp, config.nearClip, config.farClip);
                    state.eyes[i].width = chains[i].width; state.eyes[i].height = chains[i].height;
                }
                ReadInput(state);
                ReadHandJoints(state);
            }
            void RenderEnd(State& state, const System::RenderEye& render, const Matrix4& origin, unsigned int mirror, int mirrorWidth, int mirrorHeight) override
            {
                if (!begun) return;
                std::array<XrCompositionLayerProjectionView, 2> layers{{{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}, {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}}};
                try
                {
                    if (state.shouldRender)
                    {
                        for (unsigned i = 0; i < 2; ++i)
                        {
                            auto& chain = chains[i]; uint32_t index = 0;
                            XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
                            Check(xrAcquireSwapchainImage(chain.handle, &acquire, &index), "xrAcquireSwapchainImage");
                            XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO}; wait.timeout = XR_INFINITE_DURATION;
                            Check(xrWaitSwapchainImage(chain.handle, &wait), "xrWaitSwapchainImage");
                            auto release = [&] { XrSwapchainImageReleaseInfo info{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO}; return xrReleaseSwapchainImage(chain.handle, &info); };
                            try
                            {
                                glBindFramebuffer(GL_FRAMEBUFFER, chain.framebuffer);
                                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, chain.images.at(index).image, 0);
                                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, chain.depth);
                                if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) throw std::runtime_error("OpenXR eye framebuffer incomplete");
                                glViewport(0, 0, chain.width, chain.height);
                                glClearColor(0.05f, 0.06f, 0.08f, 1);
                                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
                                render(state.eyes[i], glm::inverse(origin * PoseMatrix(state.eyes[i].pose)), chain.framebuffer);
                                glBindFramebuffer(GL_READ_FRAMEBUFFER, chain.framebuffer); glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mirror);
                                glBlitFramebuffer(0, 0, chain.width, chain.height, i * (mirrorWidth / 2), 0, (i + 1) * (mirrorWidth / 2), mirrorHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
                                glFlush();
                            }
                            catch (...) { release(); throw; }
                            Check(release(), "xrReleaseSwapchainImage");
                            layers[i].pose = views[i].pose; layers[i].fov = views[i].fov;
                            layers[i].subImage.swapchain = chain.handle;
                            layers[i].subImage.imageRect.extent = {chain.width, chain.height};
                        }
                    }
                    XrCompositionLayerProjection projection{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
                    projection.space = space; projection.viewCount = 2; projection.views = layers.data();
                    const XrCompositionLayerBaseHeader* base = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projection);
                    XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO}; end.displayTime = displayTime; end.environmentBlendMode = blend;
                    end.layerCount = state.shouldRender ? 1 : 0; end.layers = state.shouldRender ? &base : nullptr;
                    const auto result = xrEndFrame(session, &end); begun = false;
                    Check(result, "xrEndFrame");
                }
                catch (...)
                {
                    if (begun) { XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO}; end.displayTime = displayTime; end.environmentBlendMode = blend; xrEndFrame(session, &end); begun = false; }
                    throw;
                }
            }
            bool Haptic(unsigned hand, float amplitude, float seconds) override
            {
                XrHapticActionInfo info{XR_TYPE_HAPTIC_ACTION_INFO}; info.action = hapticAction; info.subactionPath = handPaths[hand];
                XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION}; vibration.amplitude = amplitude;
                vibration.duration = static_cast<XrDuration>(seconds * 1e9); vibration.frequency = XR_FREQUENCY_UNSPECIFIED;
                return xrApplyHapticFeedback(session, &info, reinterpret_cast<const XrHapticBaseHeader*>(&vibration)) == XR_SUCCESS;
            }
        };
    }
    std::unique_ptr<OpenXRBackend> CreateOpenXRBackend() { return std::make_unique<NativeBackend>(); }
}
#else
namespace Canis::VR
{
    std::unique_ptr<OpenXRBackend> CreateOpenXRBackend()
    { throw std::runtime_error("OpenXR was not compiled in. Reconfigure with -DCANIS_ENABLE_OPENXR=ON, or use --vr-sim for stereo simulation."); }
}
#endif
