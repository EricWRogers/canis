#include <Canis/IOManager.hpp>
#include <SDL3/SDL.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>
using namespace Canis;
namespace fs=std::filesystem;
static void Check(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
int main()
{
    SDL_Init(0);
    const auto root=fs::temp_directory_path()/("canis-asset-watch-"+std::to_string(SDL_GetTicksNS()));
    try
    {
        fs::create_directories(root/"nested");
        const auto model=root/"nested/road.gltf";
        std::ofstream(model)<<"original";
        std::ofstream(model.string()+".meta")<<"metadata";
        std::ofstream(root/".hidden")<<"hidden";
        auto first=ReadAssetDirectorySnapshot(root.string());
        Check(first->paths.size()==1 && first->added.size()==1,"Watcher included metadata/hidden files or lost nested models");
        auto same=ReadAssetDirectorySnapshot(root.string(),first);
        Check(same->added.empty()&&same->changed.empty()&&same->removed.empty(),"Unchanged assets generated reload work");
        fs::last_write_time(model,first->writeTimes.at(model.generic_string())+std::chrono::seconds(1));
        auto job=std::async(std::launch::async,ReadAssetDirectorySnapshot,root.string(),same);
        const auto changed=job.get();
        Check(changed->changed==std::vector<std::string>{model.generic_string()},"Worker did not report a modified asset");
        Check(first->writeTimes.at(model.generic_string())!=changed->writeTimes.at(model.generic_string()),"Worker mutated the editor's previous snapshot");
        const auto renamed=root/"nested/renamed.gltf";fs::rename(model,renamed);
        const auto moved=ReadAssetDirectorySnapshot(root.string(),changed);
        Check(moved->added==std::vector<std::string>{renamed.generic_string()} &&
              moved->removed==std::vector<std::string>{model.generic_string()},"Rename did not update asset additions/removals");
        fs::remove(renamed);
        auto deleted=ReadAssetDirectorySnapshot(root.string(),moved);
        Check(deleted->paths.empty()&&deleted->removed.size()==1,"Deleted asset retained a watcher entry");
        fs::remove_all(root);SDL_Quit();
        std::cout<<"ASSET_WATCH_PASS unchanged snapshots, worker isolation, modifications, rename and deletion\n";
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';fs::remove_all(root);SDL_Quit();return 1;}
}
