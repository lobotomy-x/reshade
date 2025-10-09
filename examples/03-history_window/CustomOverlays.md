# Making Custom ImGui Overlays with ReShade
ReShade offers addon developers the ability to create ImGui-based overlays for games and other applications without needing to compile the library or handle any of the hooks or rendering implementation. This is especially useful with newer games that require D3D12 and force the use of Nvidia Streamline, AMD Fidelity FX, and Intel XESS/XELL plugins. Its entirely possible to use ReShade for this purpose without even needing to make use of ReShade's other API features. However the standard method registering overlays with ReShade does require that users open the full ReShade GUI and while custom overlays are possible they are not recommended and no known examples exist. However there is an easy and extremely reliable workaround I happened upon while developing GUI enhancements for ReShade itself. I have implemented this workaround in an existing example addon that previously had a standard overlay visible only with the main ReShade UI opened. Let's start by reviewing the basics of registering an overlay. This will assume you at least have some basic minimal knowledge of C++ and ImGui and have read the [ReShade addon documentation](https://reshade.me/docs). You can clone the Reshade repo and modify an example addon or build a Windows dll project in Visual Studio. Either way the first thing you must do is include ImGui.h and reshade.hpp with ReShade coming after ImGui. After registering the addon itself with ReShade in Dllmain you can register an overlay.


## ReShade's Overlay Registration System
Registering a ReShade overlay through the standard system is the best and easiest choice with the caveat that your overlay will only display while the ReShade overlay is open. That also means that while its technically possible to setup a dedicated key for your overlay it would still require the ReShade overlay open. Not very user friendly since that would actually require two inputs, the ReShade GUI key and then your custom one. Its possible to use another addon event to do your own custom rendering, but this is complicated and still relies on ReShade events. You also cannot invoke any ImGui functions from unrelated ReShade addon events like reshade_present and attempting to do so may compile, but will surely crash the application or at best fail to do anything.

Typically you would register your own overlay like so
```C++
  reshade::register_overlay("History", draw_history_window);
```

ReShade usually only tries to render ImGui widgets while the main ReShade GUI is open with the exceptions being the invisible background viewport for docking, the Splash and Message windows used for displaying alerts and shader compilation progress, and the OnScreen Display (OSD) that is used for showing optional statistics like framerate and is shown even with the overlay closed. Those familiar with ImGui are likely aware that its possible to append to an existing window within context by calling ImGui::Begin/End with the same ID as the target window, although flags and open state cannot be changed by this additional call. Perhaps you see where I'm going with this but hold lets finish covering the ReShade callback system before getting back to our example.

ReShade's ImGui implementation has some notable limitations with functions like FindWindowByID not exported to addons, but the ability to append to windows has been retained and enhanced by allowing addons to register an overlay with an existing title and get the same effect of appending to an exisiting page. ReShade handles this behind the scenes by calling ImGui::Begin with the registered window title for each addon widget, in order of registration. This is done each frame immediately after ReShade finishes calling its own windows which means that you can only add to and not modify or override the preexisting content. 

You can register as many overlays as you like from a single addon and can add content to ReShade's main windows (Home###home, Add-ons###addons, Log###log, Statistics###statistics, Settings###settings, About###about) by registering with the same title. For those specific windows you do need to include the ###name component which is a little ImGui trick that ignores anything before the ### for its internal ID system, meaning you could also make a window called Home without appending to the ReShade Home. You also are not required to use the registration system for all your windows. If for example you have some data you wish to pass between two existing windows while isolating to function scope you could manually call ImGui::Begin, just be aware that the registered window needs to be active for this to work.

You can also register with other addons if you know the title of their window! But do keep in mind that if the addon is not present you'll end up spawning a new window with your added content so be sure to check for presence of the targeted addon. Addons are required to export a NAME and DESCRIPTION so you could enumerate all modules but if you are targeting a specific addon you could just call GetModuleHandle. In such case you should definitely use a static bool to check this so that you're not referencing the module every single frame.

```C++
    static bool is_loaded = false
    if (GetModuleHandleA("shader_dump.addon64") != nullptr)
        is_loaded = true;
    ImGui::Text(is_loaded ? "shader_dump addon is loaded " : "shader dump addon is not loaded");

```
I won't show how to enumerate all module handles since that will depend on whether you do it from dllmain or handle it later on. The last thing to note on this topic is that addons are loaded in alphabetical order and if you try to hook an addon window that doesn't exist yet, you will end up making that window first, so the content you wanted to append to the bottom of the window will go to the top. This may actually be ideal for some use cases so keep in mind the order does matter.

Back to the matter at hand, our goal is to register an overlay that can exist independently of ReShade's main overlay. Actually I've already mentioned it briefly, but there is a window we can hook that is (almost) always active.

## The Workaround

Taking our history window addon again, we'll now change the register_overlay call to use the "OSD" window instead of its own window.
    
```C++
  // reshade::register_overlay("History", draw_history_window);
  reshade::register_overlay("OSD", osd_draw);
```
However if we just do that without any other changes it will result in our history items being noninteractive and positioned next to the FPS display. So instead of passing the draw_history_window function in the register_overlay call, we'll instead make a new function with the same signature `static void callback(reshade::api::effect_runtime *)`
and inside of that function we'll make our own window.

 ```C++
        ImGui::Begin("History", nullptr, ImGuiWindowFlags_None);
        draw_history_window(runtime);
        ImGui::End();
```
And that does the trick. we still have to wait a few seconds for the message window to go away at startup but other than that we have an always active window. So next we need to make it toggleable. I won't include the full code here (which I yoinked from crosire's implementation in the main ReShade source anyway) but I've added a widget to allow binding a key in the settings menu and defined a global variable at the top of the file to hold the value and a static bool to hold our toggled state. Within the same callback function where we draw the window we can simply check for any keypresses since this is called every frame. This is now pretty much feature complete.
 ```C++
    if (toggle_key[0] != 0 && runtime->is_key_released(toggle_key[0]))
    {
        draw_window = !draw_window;
    }

    if (draw_window) {
        ImGui::Begin("History", nullptr, ImGuiWindowFlags_None);
        draw_history_window(runtime);
        ImGui::End();
    }
```

But let's go ahead and take it one step further before discussing the remaining issues. There is no limit to how many overlays we can register so I'm going to pass `nullptr` as the the window name to make a panel in the Addon settings page like so `reshade::register_overlay(nullptr, draw_settings);`.

As I mentioned this is also where I have the input key box but I'm not going to focus on that part.

```C++
    if (ImGui::Button(draw_window ? "Attach Panel" : "Detach Panel"))
        draw_window = !draw_window;
    {
        set_toggle_key(saved_key);
        reshade::set_config_value(nullptr, "history_window", "toggle_key", toggle_key[0]);
    }

    if (!draw_window) {
        draw_history_window(runtime);
    }
```
So as you can see I have an additional button here that attaches or detaches the panel. This is nice to have as a GUI option if your hotkey has issues or you already have the menu open but we're taking it a step further. When the window is not being draw in the OSD we'll actually call our existing function from the draw_settings callback. The result is a fully dockable pane allowing us to tether the UI to ReShade's overlay when we don't want it while retaining the ability to open it with our hotkey as well. 

