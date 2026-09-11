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

project "Emulator6502"
    kind "ConsoleApp"
    language "C"
    cdialect "C11"
    staticruntime "on"

    warnings "Extra" 
    buildoptions { "-WX", "/wd4100" }

    links {
        "SDL3-static",
        "winmm",
        "version",
        "imm32"
    }

    dependson {
        "SDL3-static",
    }

    targetdir("bin/" .. outputdir .. "/%{prj.name}")
    objdir("bin-int/" .. outputdir .. "/%{prj.name}")
    
    files {
        "Emulator/src/**.c",
        "Emulator/include/**.h"
    }

    includedirs {
        "Emulator/include",
        "Emulator/vendor/CppUtils/single_include",
        -- SDL
        "Emulator/vendor/sdl/include",
        -- Nuklear
        "Emulator/vendor/nuklear/"
    }

    prelinkcommands {
        "copy /y \"Emulator\\vendor\\sdl\\Build\\%{cfg.buildcfg}\\Sdl3-static.lib\" \"%{cfg.targetdir}\\freetype.dll\"",

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