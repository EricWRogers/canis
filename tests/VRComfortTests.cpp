#include <Canis/VR/ComfortMotion.hpp>
#include <Canis/VR/Interaction.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
#include <stdexcept>
using namespace Canis;
using namespace Canis::VR;
void Check(bool test,const char* message) { if(!test) throw std::runtime_error(message); }
int main()
{
    const auto directory = std::filesystem::temp_directory_path()/
        ("canis-vr-settings-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try
    {
        PlayerSettings settings; ComfortMotion motion; std::string error;
        Check(ValidatePlayerSettings(settings,error),"Defaults valid");
        Check(motion.Queue({{1,0,2},1}),"Queue teleport");
        Check(!motion.Queue({}),"No overlapping moves");
        Check(!motion.Update(0.04f,true,settings)&&motion.Alpha()>0&&motion.Alpha()<1,"Fade before moving");
        auto move = motion.Update(0.05f,true,settings);
        Check(move&&move->position.x==1&&motion.Alpha()==1,"Move only when black");
        Check(!motion.Update(5,true,settings)&&motion.Alpha()==1,"Large delta keeps black frame");
        Check(!motion.Update(5,true,settings)&&!motion.Busy()&&motion.Alpha()==0,"Fade completes without repeated move");
        motion.Queue({}); motion.Update(0.04f,true,settings);
        Check(!motion.Update(1,false,settings)&&!motion.Busy()&&motion.Alpha()==0,"Loss cancels pending move");
        Check(!motion.Update(1,true,settings),"Restoring tracking never commits old move");
        TeleportGesture gesture;
        Check(!gesture.Update(1,true)&&gesture.Aiming(),"Trigger begins aiming");
        Check(!gesture.Update(0.5f,true)&&gesture.Aiming(),"Trigger hysteresis retains aim");
        Check(gesture.Update(0.1f,true)&&!gesture.Aiming(),"Release commits once");
        Check(!gesture.Update(0.1f,true),"Release cannot repeat");
        gesture.Update(1,true); gesture.Update(1,false);
        Check(!gesture.Update(1,true)&&!gesture.Aiming(),"Tracking regain requires neutral trigger");
        Check(!gesture.Update(0,true),"Neutral after loss does not teleport");
        RigPose rig{{2,0.4f,3},0.7f}; Pose head{{0.3f,1.2f,0.1f},{1,0,0,0},true};
        auto turn = TurnAroundHead(rig,head,PI/2);
        auto world = [](const RigPose& r,const Pose& h){return r.position+glm::angleAxis(r.yaw,Vector3(0,1,0))*h.position;};
        Check(glm::distance(world(rig,head),world(turn,head))<0.0001f,"Snap pivot does not translate head");
        auto teleported=TeleportHeadTo(rig,head,{1,0,1}); auto newHead=world(teleported,head);
        Check(std::abs(newHead.x-1)<0.0001f&&std::abs(newHead.z-1)<0.0001f&&teleported.position.y==rig.position.y,"Teleport preserves seated lift");
        Check(!SurfacePlacement({0,2,0},{0.06f,0.06f,0.06f},{0,1,0},{0.3f,0.04f,0.3f}),"No snapping ingredient down from above");
        Check(!SurfacePlacement({0.28f,1.1f,0},{0.06f,0.06f,0.06f},{0,1,0},{0.3f,0.04f,0.3f}),"Ingredient must fit surface");
        Check(SurfacePlacement({0,1.12f,0},{0.06f,0.06f,0.06f},{0,1,0},{0.3f,0.04f,0.3f}).has_value(),"Near-surface release allowed");
        std::filesystem::create_directories(directory); const auto file=directory/"vr.canis";
        Check(LoadPlayerSettings(file.string(),settings,error),"Missing file uses defaults");
        auto write=[&](const char* text){std::ofstream out(file);out<<text;};
        write("version: 1\ndominantHand: left\nheightOffset: 0.4\nsnapDegrees: 45\n");
        Check(LoadPlayerSettings(file.string(),settings,error)&&settings.dominantHand==0&&settings.heightOffset==0.4f&&settings.snapDegrees==45,"Partial settings override");
        for(const char* invalid:{"heightOffset: 0.2\nsnapDegrees: .nan\n","dominantHand: both\n","fadeOutSeconds: 0\n","version: 2\n","snapDegrees: 30\nsnapDegrees: 45\n","typo: 1\n","[1,2]\n"})
        {
            write(invalid); Check(!LoadPlayerSettings(file.string(),settings,error),"Reject malformed settings");
            Check(settings.heightOffset==0.4f&&settings.snapDegrees==45,"Invalid override is transactional");
        }
        std::filesystem::remove_all(directory); return 0;
    }
    catch(const std::exception& e){std::filesystem::remove_all(directory);std::cerr<<e.what()<<'\n';return 1;}
}
