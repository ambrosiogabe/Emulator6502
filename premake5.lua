workspace "Emulator6502"
    architecture "x64"

    configurations {
        "Debug",
        "Release",
        "Dist"
    }

    startproject "Emulator6502"

-- This is a helper variable, to concatenate the sys-arch
outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

externalproject "SDL3-static"
    location "Emulator/vendor/sdl/Build/"
    uuid "B54DE593-B77B-3E9C-B20A-8B7EA6A88814"
    kind "StaticLib"
    language "C"
    cdialect "C11"

    -- Explicitly pass the C11 flag to MSVC because premake is stupid
    filter "toolset:msc*"
        buildoptions { "/std:c11" }         

project "tree-sitter"
    kind "StaticLib"
    language "C"
    cdialect "C11"
    staticruntime "on"

    targetdir("bin/" .. outputdir .. "/%{prj.name}")
    objdir("bin-int/" .. outputdir .. "/%{prj.name}")

    -- Explicitly pass the C11 flag to MSVC because premake is stupid
    filter "toolset:msc*"
        buildoptions { "/std:c11" }    

    files {
        "Emulator/vendor/tree-sitter/lib/src/lib.c"
    }

    includedirs {
        "Emulator/vendor/tree-sitter/lib/src",
        "Emulator/vendor/tree-sitter/lib/include"
    }    

project "tinyfiledialogs"
    kind "StaticLib"
    language "C"
    cdialect "C11"
    staticruntime "on"

    targetdir("bin/" .. outputdir .. "/%{prj.name}")
    objdir("bin-int/" .. outputdir .. "/%{prj.name}")

    -- Explicitly pass the C11 flag to MSVC because premake is stupid
    filter "toolset:msc*"
        buildoptions { "/std:c11" }    

    files {
        "Emulator/vendor/tinyfiledialog/tinyfiledialogs.c"
    }

    includedirs {
        "Emulator/vendor/tinyfiledialog"
    }  

project "imgui"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "on"

    targetdir("bin/" .. outputdir .. "/%{prj.name}")
    objdir("bin-int/" .. outputdir .. "/%{prj.name}")

    -- Explicitly pass the C11 flag to MSVC because premake is stupid
    filter "toolset:msc*"
        buildoptions { "/std:c++17" }    

    files {
        "Emulator/vendor/dear_bindings/generated/dcimgui.cpp",
        "Emulator/vendor/dear_bindings/generated/dcimgui.h",
        "Emulator/vendor/dear_bindings/generated/dcimgui_internal.cpp",
        "Emulator/vendor/dear_bindings/generated/dcimgui_internal.h",
        "Emulator/vendor/dear_bindings/generated/backends/dcimgui_impl_sdl3.cpp",
        "Emulator/vendor/dear_bindings/generated/backends/dcimgui_impl_sdl3.h",
        "Emulator/vendor/dear_bindings/generated/backends/dcimgui_impl_sdlrenderer3.cpp",
        "Emulator/vendor/dear_bindings/generated/backends/dcimgui_impl_sdlrenderer3.h",
        "Emulator/vendor/imgui/imconfig.h",
        "Emulator/vendor/imgui/imgui.cpp",
        "Emulator/vendor/imgui/imgui.h",
        "Emulator/vendor/imgui/imgui_demo.cpp",
        "Emulator/vendor/imgui/imgui_draw.cpp",
        "Emulator/vendor/imgui/imgui_internal.h",
        "Emulator/vendor/imgui/imgui_tables.cpp",
        "Emulator/vendor/imgui/imgui_widgets.cpp",
        "Emulator/vendor/imgui/imstb_rectpack.h",
        "Emulator/vendor/imgui/imstb_textedit.h",
        "Emulator/vendor/imgui/imstb_truetype.h",
        "Emulator/vendor/imgui/backends/imgui_impl_sdl3.cpp",
        "Emulator/vendor/imgui/backends/imgui_impl_sdl3.h",
        "Emulator/vendor/imgui/backends/imgui_impl_sdlrenderer3.cpp",
        "Emulator/vendor/imgui/backends/imgui_impl_sdlrenderer3.h",
    }

    includedirs {
        "Emulator/vendor/dear_bindings/generated",
        "Emulator/vendor/imgui",
        "Emulator/vendor/imgui/backends",
        "Emulator/vendor/sdl/include",
    }      

project "Emulator6502"
    kind "ConsoleApp"
    language "C"
    cdialect "C11"
    staticruntime "on"

    warnings "Extra" 
    buildoptions { "-WX", "/wd4100", "/wd5287", "/wd4206" }

    -- Explicitly pass the C11 flag to MSVC because premake is stupid
    filter "toolset:msc*"
        buildoptions { "/std:c11" }        

    links {
        "SDL3-static",
        "tree-sitter",
        "winmm",
        "version",
        "imm32",
        "tinyfiledialogs",
        "imgui"
    }

    dependson {
        "SDL3-static",
        "tree-sitter",
        "tinyfiledialogs",
        "imgui"
    }

    targetdir("bin/" .. outputdir .. "/%{prj.name}")
    objdir("bin-int/" .. outputdir .. "/%{prj.name}")
    
    files {
        "Emulator/src/**.c",
        "Emulator/include/**.h",
        -- Tree sitter parser for 6502
        "Emulator/vendor/tree-sitter-asm6502/src/parser.c",
    }

    includedirs {
        "Emulator/include",
        "Emulator/vendor/CppUtils/single_include",
        -- SDL
        "Emulator/vendor/sdl/include",
        -- Tree-sitter
        "Emulator/vendor/tree-sitter/lib/include",
        -- Tree-sitter-asm6502
        "Emulator/vendor/tree-sitter-asm6502/src",
        -- Tiny File dialog
        "Emulator/vendor/tinyfiledialog",
        -- imgui C bindings
        "Emulator/vendor/dear_bindings/generated",
        "Emulator/vendor/imgui"
    }

    filter "system:windows"
        systemversion "latest"

        defines  {
            "_CRT_SECURE_NO_WARNINGS"
        }

    filter { "configurations:Debug" }
        buildoptions "/MDd"
        runtime "Debug"
        symbols "on"

    filter { "configurations:Release" }
        buildoptions "/MD"
        runtime "Release"
        optimize "on"

        defines {
            "_RELEASE"
        }

project "Emulator6502_Tests"
    kind "ConsoleApp"
    language "C"
    cdialect "C11"
    staticruntime "on"

    -- Explicitly pass the C11 flag to MSVC because premake is stupid
    filter "toolset:msc*"
        buildoptions { "/std:c11" }        

    warnings "Extra" 
    buildoptions { "-WX", "/wd4100", "/wd4028" }

    targetdir("bin/" .. outputdir .. "/%{prj.name}")
    objdir("bin-int/" .. outputdir .. "/%{prj.name}")
    
    files {
        "Emulator/tests/**.h",
        "Emulator/tests/**.c",
        "Emulator/src/**.c",
        "Emulator/include/**.h"
    }

    removefiles {
        "Emulator/src/main.c"
    }

    includedirs {
        "Emulator/tests",
        "Emulator/include",
        "Emulator/vendor/CppUtils/single_include"
    }

    filter "system:windows"
        systemversion "latest"

        defines  {
            "_CRT_SECURE_NO_WARNINGS"
        }

    filter { "configurations:Debug" }
        buildoptions "/MDd"
        runtime "Debug"
        symbols "on"

    filter { "configurations:Release" }
        buildoptions "/MD"
        runtime "Release"
        optimize "on"

        defines {
            "_RELEASE"
        }