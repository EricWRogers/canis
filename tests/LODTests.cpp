#include <Canis/LOD.hpp>
#include <iostream>
#include <stdexcept>
#include <cmath>
using namespace Canis;
static void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main()
{
    try {
        std::vector<LODLevel> levels={{"",0,.2f},{"",1,.08f},{"",2,.02f},{"",3,0}};
        Check(SelectLOD(levels,.3f,-1,.15f)==0,"Near selection");
        Check(SelectLOD(levels,.1f,-1,.15f)==1,"Medium selection");
        Check(SelectLOD(levels,.03f,-1,.15f)==2,"Far selection");
        Check(SelectLOD(levels,.001f,-1,.15f)==3,"Last level stays available");
        Check(SelectLOD(levels,.19f,0,.15f)==0,"Downward hysteresis");
        Check(SelectLOD(levels,.16f,0,.15f)==1,"Downward threshold");
        Check(SelectLOD(levels,.21f,1,.15f)==1,"Upward hysteresis");
        Check(SelectLOD(levels,.24f,1,.15f)==0,"Upward threshold");
        auto projection=glm::perspective(glm::radians(84.f),1.8f,.1f,1000.f);
        auto model=glm::translate(Matrix4(1),Vector3(0,0,-200));
        Vector3 low(-10),high(10);
        float normal=ProjectedLODHeight(low,high,model,Matrix4(1),projection);
        float fov=2*std::atan(std::tan(glm::radians(84.f)/2)/8);
        float scoped=ProjectedLODHeight(low,high,model,Matrix4(1),glm::perspective(fov,1.8f,.1f,1000.f));
        Check(std::abs(scoped/normal-8)<.001f,"Zoom uses the actual projection");
        Check(ProjectedLODHeight(low,high,glm::translate(Matrix4(1),Vector3(0,0,-5)),Matrix4(1),projection)==1,"Near-plane crossing retains detail");
        float large=ProjectedLODHeight(low,high,model*glm::scale(Matrix4(1),Vector3(2)),Matrix4(1),projection);
        Check(std::abs(large/normal-2)<.001f,"World scale affects selection");
        Check(SelectLOD(levels,std::max(normal,scoped),-1,.15f)==0,"Stereo chooses the more detailed eye");
        Check(SelectLOD(levels,normal,-1,.15f)>SelectLOD(levels,scoped,-1,.15f),"Independent views can select different detail");
        std::cout<<"LOD_RULES_PASS screen size, scaling, zoom, hysteresis, near plane and stereo\n";
    } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
