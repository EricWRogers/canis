// API test double for the actual OpenXR backend. This is NOT a conformant runtime
// or a device emulator. It verifies call order, cleanup, and fallback behavior.
#include <Canis/VR/VRSystem.hpp>
#include <Canis/Window.hpp>
#include <Canis/OpenGL.hpp>
#define XR_USE_GRAPHICS_API_OPENGL
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <algorithm>
#include <cstring>
#include <deque>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace Fake
{
    uintptr_t next = 10;
    template<class T> T Handle() { return reinterpret_cast<T>(next++); }
    struct Chain { GLuint texture=0; bool acquired=false, waited=false; int width=0,height=0; };
    std::map<XrSwapchain,Chain> chains;
    std::map<XrAction,std::string> actions;
    std::map<XrSpace,XrAction> spaces;
    std::map<XrHandTrackerEXT,XrHandEXT> trackers;
    bool handExtension=true, handSupport=true, handsActive=true, failRightTracker=false;
    bool badJoint=false, failLocate=false, controllerActive=true;
    std::map<std::string,XrPath> paths;
    std::deque<XrSessionState> events;
    bool instance=false, session=false, actionSet=false, running=false, waited=false, begun=false;
    bool shouldRender=true, viewsValid=true, stage=true, focused=true, failSecondChain=false;
    unsigned frames=0, projections=0, empty=0, releases=0, haptics=0, violations=0;
    XrSession sessionHandle{};
    XrResult Guard(bool condition) { if (condition) return XR_SUCCESS; ++violations; return XR_ERROR_CALL_ORDER_INVALID; }
    template<class T> XrResult Enumerate(const std::vector<T>& values,uint32_t capacity,uint32_t* count,T* output)
    {
        *count=static_cast<uint32_t>(values.size());
        if (!capacity) return XR_SUCCESS;
        if (capacity<values.size()) return XR_ERROR_SIZE_INSUFFICIENT;
        std::copy(values.begin(),values.end(),output); return XR_SUCCESS;
    }
    XrPosef Pose(float x=0) { XrPosef p{};p.orientation.w=1;p.position={x,1.65f,0};return p; }
    void Require(bool condition,const char* text) { if(!condition) throw std::runtime_error(text); }
}
extern "C"
{
XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateInstanceExtensionProperties(const char*,uint32_t n,uint32_t* count,XrExtensionProperties* p)
{
    XrExtensionProperties gl{XR_TYPE_EXTENSION_PROPERTIES};std::strcpy(gl.extensionName,XR_KHR_OPENGL_ENABLE_EXTENSION_NAME);gl.extensionVersion=1;
    XrExtensionProperties gaze{XR_TYPE_EXTENSION_PROPERTIES};std::strcpy(gaze.extensionName,XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME);gaze.extensionVersion=2;
    std::vector<XrExtensionProperties> extensions{gl,gaze};
    if(Fake::handExtension) { XrExtensionProperties hand{XR_TYPE_EXTENSION_PROPERTIES};
        std::strcpy(hand.extensionName,XR_EXT_HAND_TRACKING_EXTENSION_NAME);hand.extensionVersion=4;extensions.push_back(hand); }
    return Fake::Enumerate(extensions,n,count,p);
}
XRAPI_ATTR XrResult XRAPI_CALL xrCreateInstance(const XrInstanceCreateInfo* info,XrInstance* p)
{
    bool enabled=false;
    for(unsigned i=0;i<info->enabledExtensionCount;++i) enabled |= std::strcmp(info->enabledExtensionNames[i],XR_EXT_HAND_TRACKING_EXTENSION_NAME)==0;
    Fake::Require(enabled==Fake::handExtension,"Hand extension enabled only when advertised");
    Fake::instance=true;*p=Fake::Handle<XrInstance>();return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrDestroyInstance(XrInstance)
{ auto r=Fake::Guard(!Fake::session&&!Fake::actionSet&&Fake::chains.empty());Fake::instance=false;return r; }
XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProperties(XrInstance,XrInstanceProperties* p)
{ std::strcpy(p->runtimeName,"Canis OpenXR API test double");p->runtimeVersion=XR_MAKE_VERSION(1,0,0);return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL xrGetSystem(XrInstance,const XrSystemGetInfo*,XrSystemId* p) { *p=1;return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL xrGetSystemProperties(XrInstance,XrSystemId,XrSystemProperties* p)
{
    for(auto* next=static_cast<XrBaseOutStructure*>(p->next);next;next=next->next)
    {
        if(next->type==XR_TYPE_SYSTEM_EYE_GAZE_INTERACTION_PROPERTIES_EXT)
            reinterpret_cast<XrSystemEyeGazeInteractionPropertiesEXT*>(next)->supportsEyeGazeInteraction=XR_TRUE;
        if(next->type==XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT)
            reinterpret_cast<XrSystemHandTrackingPropertiesEXT*>(next)->supportsHandTracking=Fake::handSupport;
    }
    return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL FakeCreateHandTracker(XrSession,const XrHandTrackerCreateInfoEXT* info,XrHandTrackerEXT* p)
{
    Fake::Require(Fake::session&&Fake::handSupport&&info->handJointSet==XR_HAND_JOINT_SET_DEFAULT_EXT,"Hand tracker creation capability and session");
    if(Fake::failRightTracker&&info->hand==XR_HAND_RIGHT_EXT)return XR_ERROR_FEATURE_UNSUPPORTED;
    *p=Fake::Handle<XrHandTrackerEXT>();Fake::trackers[*p]=info->hand;return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL FakeDestroyHandTracker(XrHandTrackerEXT tracker)
{ Fake::Require(Fake::session&&Fake::trackers.erase(tracker)==1,"Destroy tracker before its session");return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL FakeLocateHandJoints(XrHandTrackerEXT tracker,const XrHandJointsLocateInfoEXT* info,XrHandJointLocationsEXT* out)
{
    Fake::Require(Fake::begun&&Fake::focused&&info->time==Fake::frames*11111111LL&&Fake::spaces.count(info->baseSpace),"Locate hand in predicted frame and reference space");
    Fake::Require(out->jointCount==XR_HAND_JOINT_COUNT_EXT,"All 26 joints requested");
    if(Fake::failLocate)return XR_ERROR_RUNTIME_FAILURE;
    out->isActive=Fake::handsActive;
    for(unsigned j=0;j<out->jointCount;++j)
    {
        auto& joint=out->jointLocations[j];joint={};
        if(!out->isActive)continue;
        joint.pose=Fake::Pose(Fake::trackers.at(tracker)==XR_HAND_LEFT_EXT ? -.2f : .2f);
        joint.pose.position.z=-.01f*j;joint.radius=.008f;
        joint.locationFlags=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
        if(j!=XR_HAND_JOINT_INDEX_TIP_EXT)joint.locationFlags|=XR_SPACE_LOCATION_POSITION_TRACKED_BIT|XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
        if(Fake::badJoint&&j==XR_HAND_JOINT_INDEX_TIP_EXT)joint.pose.orientation.w=0;
    }
    return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL FakeGraphicsRequirements(XrInstance,XrSystemId,XrGraphicsRequirementsOpenGLKHR* p)
{ p->minApiVersionSupported=XR_MAKE_VERSION(3,3,0);p->maxApiVersionSupported=XR_MAKE_VERSION(4,6,0);return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProcAddr(XrInstance,const char* name,PFN_xrVoidFunction* p)
{
    if(!std::strcmp(name,"xrGetOpenGLGraphicsRequirementsKHR"))*p=reinterpret_cast<PFN_xrVoidFunction>(FakeGraphicsRequirements);
    else if(!std::strcmp(name,"xrCreateHandTrackerEXT"))*p=reinterpret_cast<PFN_xrVoidFunction>(FakeCreateHandTracker);
    else if(!std::strcmp(name,"xrDestroyHandTrackerEXT"))*p=reinterpret_cast<PFN_xrVoidFunction>(FakeDestroyHandTracker);
    else if(!std::strcmp(name,"xrLocateHandJointsEXT"))*p=reinterpret_cast<PFN_xrVoidFunction>(FakeLocateHandJoints);
    else return XR_ERROR_FUNCTION_UNSUPPORTED;
    return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrCreateSession(XrInstance,const XrSessionCreateInfo* info,XrSession* p)
{ if(!info->next) return XR_ERROR_GRAPHICS_DEVICE_INVALID;Fake::session=true;*p=Fake::sessionHandle=Fake::Handle<XrSession>();return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL xrDestroySession(XrSession)
{ auto r=Fake::Guard(Fake::chains.empty()&&Fake::spaces.empty()&&Fake::trackers.empty()&&!Fake::begun);Fake::session=false;Fake::running=false;return r; }
XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateReferenceSpaces(XrSession,uint32_t n,uint32_t* count,XrReferenceSpaceType* p)
{ std::vector<XrReferenceSpaceType> v{XR_REFERENCE_SPACE_TYPE_LOCAL,XR_REFERENCE_SPACE_TYPE_VIEW};if(Fake::stage)v.push_back(XR_REFERENCE_SPACE_TYPE_STAGE);return Fake::Enumerate(v,n,count,p); }
XRAPI_ATTR XrResult XRAPI_CALL xrCreateReferenceSpace(XrSession,const XrReferenceSpaceCreateInfo*,XrSpace* p)
{ *p=Fake::Handle<XrSpace>();Fake::spaces[*p]=XR_NULL_HANDLE;return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL xrDestroySpace(XrSpace p) { Fake::spaces.erase(p);return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL xrStringToPath(XrInstance,const char* text,XrPath* path)
{ auto& p=Fake::paths[text];if(!p)p=Fake::paths.size();*path=p;return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL xrCreateActionSet(XrInstance,const XrActionSetCreateInfo*,XrActionSet* p)
{ *p=Fake::Handle<XrActionSet>();Fake::actionSet=true;return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL xrDestroyActionSet(XrActionSet)
{ Fake::actions.clear();Fake::actionSet=false;return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL xrCreateAction(XrActionSet,const XrActionCreateInfo* info,XrAction* p)
{ *p=Fake::Handle<XrAction>();Fake::actions[*p]=info->actionName;return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL xrSuggestInteractionProfileBindings(XrInstance,const XrInteractionProfileSuggestedBinding* p)
{ return Fake::Guard(p->countSuggestedBindings>0&&p->suggestedBindings); }
XRAPI_ATTR XrResult XRAPI_CALL xrCreateActionSpace(XrSession,const XrActionSpaceCreateInfo* info,XrSpace* p)
{ *p=Fake::Handle<XrSpace>();Fake::spaces[*p]=info->action;return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL xrAttachSessionActionSets(XrSession,const XrSessionActionSetsAttachInfo*) { return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateEnvironmentBlendModes(XrInstance,XrSystemId,XrViewConfigurationType,uint32_t n,uint32_t* count,XrEnvironmentBlendMode* p)
{ return Fake::Enumerate<XrEnvironmentBlendMode>({XR_ENVIRONMENT_BLEND_MODE_OPAQUE},n,count,p); }
XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateViewConfigurationViews(XrInstance,XrSystemId,XrViewConfigurationType,uint32_t n,uint32_t* count,XrViewConfigurationView* p)
{
    XrViewConfigurationView v{XR_TYPE_VIEW_CONFIGURATION_VIEW};v.recommendedImageRectWidth=v.maxImageRectWidth=64;
    v.recommendedImageRectHeight=v.maxImageRectHeight=64;v.recommendedSwapchainSampleCount=v.maxSwapchainSampleCount=1;
    return Fake::Enumerate<XrViewConfigurationView>({v,v},n,count,p);
}
XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateSwapchainFormats(XrSession,uint32_t n,uint32_t* count,int64_t* p)
{ return Fake::Enumerate<int64_t>({GL_SRGB8_ALPHA8},n,count,p); }
XRAPI_ATTR XrResult XRAPI_CALL xrCreateSwapchain(XrSession,const XrSwapchainCreateInfo* info,XrSwapchain* p)
{
    if(Fake::failSecondChain&&Fake::chains.size()==1)return XR_ERROR_RUNTIME_FAILURE;
    *p=Fake::Handle<XrSwapchain>();auto& c=Fake::chains[*p];c.width=info->width;c.height=info->height;
    GLint previous=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&previous);
    glGenTextures(1,&c.texture);glBindTexture(GL_TEXTURE_2D,c.texture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_SRGB8_ALPHA8,c.width,c.height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D,previous);return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrDestroySwapchain(XrSwapchain p)
{ auto c=Fake::chains.at(p);auto r=Fake::Guard(!c.acquired);glDeleteTextures(1,&c.texture);Fake::chains.erase(p);return r; }
XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateSwapchainImages(XrSwapchain chain,uint32_t n,uint32_t* count,XrSwapchainImageBaseHeader* p)
{ *count=1;if(n)reinterpret_cast<XrSwapchainImageOpenGLKHR*>(p)->image=Fake::chains.at(chain).texture;return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL xrPollEvent(XrInstance,XrEventDataBuffer* p)
{
    if(Fake::events.empty())return XR_EVENT_UNAVAILABLE;
    XrEventDataSessionStateChanged e{XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED};e.session=Fake::sessionHandle;e.state=Fake::events.front();Fake::events.pop_front();
    Fake::focused=e.state==XR_SESSION_STATE_FOCUSED;std::memcpy(p,&e,sizeof(e));return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrBeginSession(XrSession,const XrSessionBeginInfo*)
{ auto r=Fake::Guard(!Fake::running);Fake::running=true;return r; }
XRAPI_ATTR XrResult XRAPI_CALL xrEndSession(XrSession)
{ auto r=Fake::Guard(Fake::running&&!Fake::begun);Fake::running=false;return r; }
XRAPI_ATTR XrResult XRAPI_CALL xrWaitFrame(XrSession,const XrFrameWaitInfo*,XrFrameState* p)
{ auto r=Fake::Guard(Fake::running&&!Fake::begun&&!Fake::waited);Fake::waited=true;p->predictedDisplayTime=(++Fake::frames)*11111111;p->predictedDisplayPeriod=11111111;p->shouldRender=Fake::shouldRender;return r; }
XRAPI_ATTR XrResult XRAPI_CALL xrBeginFrame(XrSession,const XrFrameBeginInfo*)
{ auto r=Fake::Guard(Fake::waited&&!Fake::begun);Fake::begun=true;Fake::waited=false;return r; }
XRAPI_ATTR XrResult XRAPI_CALL xrLocateViews(XrSession,const XrViewLocateInfo*,XrViewState* state,uint32_t n,uint32_t* count,XrView* p)
{
    if(n<2)return XR_ERROR_SIZE_INSUFFICIENT;*count=2;
    state->viewStateFlags=Fake::viewsValid?(XR_VIEW_STATE_POSITION_VALID_BIT|XR_VIEW_STATE_ORIENTATION_VALID_BIT):0;
    for(unsigned i=0;i<2;++i){p[i].pose=Fake::Pose(i==0?-0.032f:0.032f);p[i].fov={-0.7f,0.8f,0.75f,-0.65f};}return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrLocateSpace(XrSpace,XrSpace,XrTime,XrSpaceLocation* p)
{ p->pose=Fake::Pose();p->locationFlags=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL xrSyncActions(XrSession,const XrActionsSyncInfo*) { return Fake::focused?XR_SUCCESS:XR_SESSION_NOT_FOCUSED; }
XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStatePose(XrSession,const XrActionStateGetInfo*,XrActionStatePose* p) { p->isActive=Fake::focused&&Fake::controllerActive;return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateFloat(XrSession,const XrActionStateGetInfo*,XrActionStateFloat* p) { p->isActive=Fake::focused;p->currentState=0.8f;return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateVector2f(XrSession,const XrActionStateGetInfo*,XrActionStateVector2f* p) { p->isActive=Fake::focused;p->currentState={0.1f,0.2f};return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateBoolean(XrSession,const XrActionStateGetInfo*,XrActionStateBoolean* p) { p->isActive=Fake::focused;p->currentState=true;return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL xrAcquireSwapchainImage(XrSwapchain s,const XrSwapchainImageAcquireInfo*,uint32_t* p)
{ auto& c=Fake::chains.at(s);auto r=Fake::Guard(Fake::begun&&!c.acquired);c.acquired=true;*p=0;return r; }
XRAPI_ATTR XrResult XRAPI_CALL xrWaitSwapchainImage(XrSwapchain s,const XrSwapchainImageWaitInfo*)
{ auto& c=Fake::chains.at(s);auto r=Fake::Guard(c.acquired&&!c.waited);c.waited=true;return r; }
XRAPI_ATTR XrResult XRAPI_CALL xrReleaseSwapchainImage(XrSwapchain s,const XrSwapchainImageReleaseInfo*)
{ auto& c=Fake::chains.at(s);auto r=Fake::Guard(c.acquired&&c.waited);c.acquired=c.waited=false;++Fake::releases;return r; }
XRAPI_ATTR XrResult XRAPI_CALL xrEndFrame(XrSession,const XrFrameEndInfo* p)
{
    bool valid=Fake::begun;
    for(auto& [key,c]:Fake::chains)valid=valid&&!c.acquired;
    if(p->layerCount)
    {
        auto* projection=reinterpret_cast<const XrCompositionLayerProjection*>(p->layers[0]);
        valid=valid&&p->layerCount==1&&projection->viewCount==2&&projection->space;
        ++Fake::projections;
    }
    else ++Fake::empty;
    Fake::begun=false;return Fake::Guard(valid);
}
XRAPI_ATTR XrResult XRAPI_CALL xrApplyHapticFeedback(XrSession,const XrHapticActionInfo*,const XrHapticBaseHeader*) { ++Fake::haptics;return XR_SUCCESS; }
}
int main()
{
    using namespace Canis;
    try
    {
        WindowOptions options; options.vrContext=true;
        Window window("OpenXR API sequence test",320,240,true,options);
        VR::System vr; VR::Config config; config.foveation=VR::FoveationMode::EyeTracked;
        std::string error;
        Fake::Require(vr.Initialize(window,config,error),error.c_str());
        Fake::Require(vr.GetDiagnostics().eyeGazeSupported,"Gaze capability query");
        Fake::Require(vr.GetDiagnostics().handTrackingSupported&&Fake::trackers.size()==2,"Create both hand trackers");
        if (!glewGetExtension("GL_NV_shading_rate_image"))
            Fake::Require(vr.GetDiagnostics().foveation == "off: GL_NV_shading_rate_image is unavailable on this GPU",
                "Experimental GLEW entry points must not enable unsupported NVIDIA foveation");
        Fake::events={XR_SESSION_STATE_READY,XR_SESSION_STATE_FOCUSED};
        Fake::Require(vr.BeginFrame(error),error.c_str());
        Fake::Require(vr.GetState().running&&vr.GetState().focused&&vr.GetState().stageSpace,"READY begins session in stage space");
        Fake::Require(vr.GetState().hands[0].active&&vr.GetState().gaze.valid,"Read action and gaze poses");
        const auto& skeleton=vr.GetState().hands[0].skeleton;
        Fake::Require(skeleton.active&&vr.GetState().hands[1].skeleton.active,"Independent left/right skeletons");
        Fake::Require(skeleton[VR::HandJoint::Wrist].pose.position.x<0&&vr.GetState().hands[1].skeleton[VR::HandJoint::Wrist].pose.position.x>0,"Hand identities preserved");
        Fake::Require(skeleton[VR::HandJoint::Wrist].tracked&&!skeleton[VR::HandJoint::IndexTip].tracked&&skeleton[VR::HandJoint::IndexTip].pose.valid,"Inferred joints remain valid without being reported as tracked");
        Fake::Require(vr.Haptic(0,0.2f,0.02f)&&Fake::haptics==1,"Submit haptic action");
        unsigned drawn=0;
        vr.SetComfortFade(1);
        Fake::Require(vr.RenderAndEndFrame([&](auto&,auto&,auto){++drawn;glClearColor(1,1,1,1);glClear(GL_COLOR_BUFFER_BIT);},error),error.c_str());
        glBindFramebuffer(GL_FRAMEBUFFER,vr.MirrorFramebuffer());
        unsigned char pixel[4]{};glReadPixels(16,16,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
        Fake::Require(pixel[0]==0&&pixel[1]==0&&pixel[2]==0,"Native eye faded before copying mirror");
        glBindFramebuffer(GL_FRAMEBUFFER,0);vr.SetComfortFade(0);
        Fake::Require(drawn==2&&Fake::projections==1&&Fake::releases==2,"Submit and release both eyes");
        Fake::events={XR_SESSION_STATE_VISIBLE};Fake::shouldRender=false;
        Fake::Require(vr.BeginFrame(error),error.c_str());
        Fake::Require(!vr.GetState().focused&&!vr.GetState().hands[0].active&&!vr.Haptic(0,1,0.1f),"Visible but unfocused clears input/haptics");
        Fake::Require(!vr.GetState().hands[0].skeleton.active&&!vr.GetState().hands[0].skeleton[VR::HandJoint::Wrist].pose.valid,"Focus loss clears joint data");
        Fake::Require(vr.RenderAndEndFrame([&](auto&,auto&,auto){++drawn;},error),error.c_str());
        Fake::Require(drawn==2&&Fake::empty==1,"shouldRender false submits no layer");
        Fake::events={XR_SESSION_STATE_FOCUSED};Fake::shouldRender=true;Fake::viewsValid=false;
        Fake::Require(vr.BeginFrame(error),error.c_str());
        Fake::Require(vr.RenderAndEndFrame([&](auto&,auto&,auto){++drawn;},error),error.c_str());
        Fake::Require(drawn==2&&Fake::empty==2,"Invalid views submit no layer");
        Fake::viewsValid=true;
        Fake::Require(vr.BeginFrame(error),error.c_str());
        Fake::Require(!vr.RenderAndEndFrame([](auto&,auto&,auto){throw std::runtime_error("draw failure");},error),"Render failure propagated");
        Fake::Require(!Fake::begun&&Fake::empty==3,"Render failure releases acquired image and ends frame");
        Fake::events={XR_SESSION_STATE_STOPPING};
        Fake::Require(vr.BeginFrame(error),error.c_str());
        Fake::Require(!vr.GetState().running&&!Fake::running,"STOPPING ends session");
        Fake::events={XR_SESSION_STATE_READY,XR_SESSION_STATE_FOCUSED};
        Fake::Require(vr.BeginFrame(error),error.c_str());
        Fake::Require(vr.RenderAndEndFrame([](auto&,auto&,auto){},error),error.c_str());
        Fake::events={XR_SESSION_STATE_EXITING};Fake::Require(vr.BeginFrame(error),error.c_str());
        Fake::Require(vr.GetState().exitRequested,"Runtime exit request");
        vr.Shutdown();
        Fake::Require(!Fake::instance&&!Fake::session&&!Fake::actionSet&&Fake::spaces.empty()&&Fake::chains.empty(),"Complete resource cleanup");
        Fake::failSecondChain=true;Fake::stage=false;
        Fake::Require(!vr.Initialize(window,config,error),"Partial swapchain initialization failure");
        Fake::Require(!Fake::instance&&!Fake::session&&Fake::chains.empty()&&Fake::spaces.empty(),"Cleanup partial initialization");
        Fake::failSecondChain=false;
        Fake::Require(vr.Initialize(window,config,error),error.c_str());
        Fake::events={XR_SESSION_STATE_READY,XR_SESSION_STATE_FOCUSED};
        Fake::Require(vr.BeginFrame(error),error.c_str());
        Fake::Require(!vr.GetState().stageSpace&&std::abs(vr.GetState().head.position.y-3.3f)<0.001f,"LOCAL fallback applies configured floor offset");
        Fake::Require(std::abs(vr.GetState().hands[0].skeleton[VR::HandJoint::Wrist].pose.position.y-3.3f)<0.001f,"Hand joints use the same LOCAL floor offset");
        Fake::Require(vr.RenderAndEndFrame([](auto&,auto&,auto){},error),error.c_str());
        vr.Shutdown();
        for(int scenario=0;scenario<3;++scenario)
        {
            Fake::handExtension=scenario!=0;Fake::handSupport=scenario!=1;Fake::failRightTracker=scenario==2;
            Fake::Require(vr.Initialize(window,config,error),"Optional hand tracking must not prevent VR initialization");
            Fake::Require(Fake::trackers.size()==(scenario==2?1:0),"Unsupported/partial hand tracker fallback");
            vr.Shutdown();
        }
        Fake::handExtension=Fake::handSupport=true;Fake::failRightTracker=false;
        Fake::Require(vr.Initialize(window,config,error),error.c_str());
        Fake::events={XR_SESSION_STATE_READY,XR_SESSION_STATE_FOCUSED};
        Fake::controllerActive=false;
        for(int scenario=0;scenario<5;++scenario)
        {
            Fake::handsActive=scenario!=1;Fake::badJoint=scenario==2;Fake::failLocate=scenario==3;
            Fake::Require(vr.BeginFrame(error),error.c_str());
            Fake::Require(vr.GetState().hands[0].skeleton.active==(scenario==0||scenario==4),"Tracking loss, invalid joints and locate failures clear the pose; recovery resumes it");
            Fake::Require(!vr.GetState().hands[0].active,"Skeleton input does not require an active controller");
            Fake::Require(vr.RenderAndEndFrame([](auto&,auto&,auto){},error),error.c_str());
        }
        vr.Shutdown();
        Fake::Require(Fake::violations==0,"No invalid OpenXR call sequences");
        return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
