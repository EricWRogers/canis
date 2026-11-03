#include <Canis/Dialog.hpp>
#include <yaml-cpp/yaml.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <cmath>
#include <algorithm>

namespace Canis {
DialogDocument DialogDocument::Load(const std::string& path) {
    auto root=YAML::LoadFile(path);auto settings=root["Dialog"];
    DialogDocument result;
    result.id=settings["id"].as<std::string>(std::filesystem::path(path).stem().string());
    result.allowEarlyContinue=settings["allowEarlyContinue"].as<bool>(true);
    result.waitForUserInput=settings["waitForUserInput"].as<bool>(true);
    result.charactersPerSecond=settings["charactersPerSecond"].as<float>(60);
    result.initialDelay=settings["initialDelay"].as<float>(.1f);
    result.messages=root["Messages"].as<std::vector<std::string>>();
    result.Validate();return result;
}
void DialogDocument::Validate() const {
    if(id.empty() || id.find_first_of("\r\n")!=std::string::npos)throw std::runtime_error("Dialog needs a single-line completion ID.");
    if(!std::isfinite(charactersPerSecond)||charactersPerSecond<1||charactersPerSecond>300 ||
       !std::isfinite(initialDelay)||initialDelay<0||initialDelay>10)throw std::runtime_error("Invalid dialog timing.");
    if(messages.empty()||messages.size()>128)throw std::runtime_error("Dialog needs 1 to 128 messages.");
    for(const auto& message:messages)if(message.find_first_not_of(" \t\r\n")==std::string::npos||message.size()>8192)
        throw std::runtime_error("Messages must contain text and be no longer than 8192 bytes.");
}
void DialogDocument::Save(const std::string& path) const {
    Validate();
    if(std::filesystem::path(path).extension()!=".dialog")throw std::runtime_error("Use a .dialog filename.");
    YAML::Node root;auto d=root["Dialog"];d["id"]=id;d["allowEarlyContinue"]=allowEarlyContinue;
    d["waitForUserInput"]=waitForUserInput;d["charactersPerSecond"]=charactersPerSecond;d["initialDelay"]=initialDelay;
    root["Messages"]=messages;
    const auto temp=path+".tmp";
    {std::ofstream out(temp);out<<root<<'\n';out.flush();if(!out)throw std::runtime_error("Could not write dialog.");}
    std::filesystem::rename(temp,path);
}
namespace {
std::size_t Prefix(const std::string& text,std::size_t characters) {
    std::size_t end=0;
    while(end<text.size()&&characters--) {
        ++end;while(end<text.size()&&(static_cast<unsigned char>(text[end])&0xc0)==0x80)++end;
    }
    return end;
}
}
void DialogPlayback::Start(DialogDocument value) {value.Validate();document=std::move(value);index=0;m_age=0;m_revealed=false;}
bool DialogPlayback::Revealed() const {
    return Finished()||m_revealed||Prefix(document.messages[index],std::size_t(std::max(0.f,m_age-document.initialDelay)*document.charactersPerSecond))>=document.messages[index].size();
}
std::string DialogPlayback::VisibleText() const {
    if(Finished())return {};
    const auto& text=document.messages[index];
    return Revealed()?text:text.substr(0,Prefix(text,std::size_t(std::max(0.f,m_age-document.initialDelay)*document.charactersPerSecond)));
}
void DialogPlayback::Tick(float dt) {
    if(Finished()||!std::isfinite(dt)||dt<=0)return;
    m_age+=dt;
    if(!document.waitForUserInput && Revealed())Continue();
}
bool DialogPlayback::Continue() {
    if(Finished())return true;
    if(!Revealed()){if(document.allowEarlyContinue)m_revealed=true;return false;}
    ++index;m_age=0;m_revealed=false;return Finished();
}
}
