#include <Canis/Canis.hpp>
#include <Canis/Window.hpp>
#include <Canis/InputManager.hpp>
#include <SDL3/SDL.h>
#include <iostream>
#include <stdexcept>
using namespace Canis;
static void Check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main() {
    try {
        Check(InputManager::WindowMouseToPixels(Vector2(160,120),Vector2(320,240),Vector2(640,480))==Vector2(320,240),
              "High-DPI mouse coordinates did not match rendered menu pixels");
        Check(InputManager::WindowMouseToPixels(Vector2(200,100),Vector2(800,600),Vector2(1000,750))==Vector2(250,125),
              "Fractional-DPI mouse coordinates did not match rendered menu pixels");
        SetEditorRuntimeEnabled(false);
        Window window("Input focus regression",320,240,true);
        InputManager input;
        InputDocument document;std::string error;
        Check(ParseInputDocument(R"(version: 1
maps:
- id: 1
  name: Menu
  actions:
  - id: 2
    name: Confirm
    type: Button
    bindings:
    - {id: 3, scheme: KeyboardMouse, path: Keyboard/Enter}
)",document,error),error.c_str());
        Check(input.Actions().RegisterSchema(document,error),error.c_str());
        Check(input.Actions().QueueDocument(document,error),error.c_str());input.EnableMap({1});input.EvaluateActions();
        SDL_FlushEvents(SDL_EVENT_FIRST,SDL_EVENT_LAST);
        const Uint32 id=SDL_GetWindowID(static_cast<SDL_Window*>(window.GetSDLWindow()));
        auto event=[&](Uint32 type){SDL_Event e{};e.type=type;e.window.windowID=id;SDL_PushEvent(&e);};
        event(SDL_EVENT_WINDOW_FOCUS_GAINED);event(SDL_EVENT_WINDOW_OCCLUDED);
        Check(input.Update(&window),"Unexpected quit");input.EvaluateActions();
        Check(input.GameInputFocused(),"Occlusion incorrectly disabled focused game input");
        SDL_Event key{};key.type=SDL_EVENT_KEY_DOWN;key.key.windowID=id;key.key.scancode=SDL_SCANCODE_RETURN;
        key.key.key=SDLK_RETURN;key.key.down=true;SDL_PushEvent(&key);
        SDL_Event click{};click.type=SDL_EVENT_MOUSE_BUTTON_DOWN;click.button.windowID=id;click.button.button=SDL_BUTTON_LEFT;
        click.button.down=true;click.button.x=160;click.button.y=120;SDL_PushEvent(&click);
        input.Update(&window);input.EvaluateActions();
        Check(input.Action({2}).pressed,"Enter lost after focused-window occlusion");
        Check(input.JustLeftClicked(),"Mouse click lost after focused-window occlusion");
        event(SDL_EVENT_WINDOW_FOCUS_LOST);input.Update(&window);input.EvaluateActions();
        Check(!input.GameInputFocused(),"Actual focus loss did not suspend input");
        event(SDL_EVENT_WINDOW_FOCUS_GAINED);input.Update(&window);input.EvaluateActions();
        Check(input.GameInputFocused(),"Focus gain did not recover input");
        event(SDL_EVENT_WINDOW_MINIMIZED);input.Update(&window);input.EvaluateActions();
        Check(!input.GameInputFocused(),"Minimize did not suspend input");
        event(SDL_EVENT_WINDOW_RESTORED);input.Update(&window);input.EvaluateActions();
        Check(input.GameInputFocused(),"Restore did not recover input");
        std::cout<<"INPUT_FOCUS_PASS focused occlusion preserves Enter/click; actual focus loss/minimize suspend input\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
