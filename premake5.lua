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

project "Emulator6502"
    kind "ConsoleApp"
    language "C"
    cdialect "C11"
    staticruntime "on"

    targetdir("bin/" .. outputdir .. "/%{prj.name}")
    objdir("bin-int/" .. outputdir .. "/%{prj.name}")

    files {
        "Emulator/src/**.c",
        "Emulator/include/**.h"
        -- "Animations/vendor/GLFW/include/GLFW/glfw3.h",
        -- "Animations/vendor/GLFW/include/GLFW/glfw3native.h",
        -- "Animations/vendor/GLFW/src/glfw_config.h",
        -- "Animations/vendor/GLFW/src/context.c",
        -- "Animations/vendor/GLFW/src/init.c",
        -- "Animations/vendor/GLFW/src/input.c",
        -- "Animations/vendor/GLFW/src/monitor.c",
        -- "Animations/vendor/GLFW/src/vulkan.c",
        -- "Animations/vendor/GLFW/src/window.c",
        -- "Animations/vendor/glad/include/glad/glad.h",
        -- "Animations/vendor/glad/include/glad/KHR/khrplatform.h",
		-- "Animations/vendor/glad/src/glad.c"
		-- "Animations/vendor/glad/src/glad.c"
    }

    includedirs {
        "Emulator/include",
        "Emulator/vendor/CppUtils/single_include"
    }

    filter "system:windows"
        -- buildoptions { "-lgdi32" }
        systemversion "latest"

       -- files {
       --     -- "Animations/vendor/GLFW/src/win32_init.c",
       --     -- "Animations/vendor/GLFW/src/win32_joystick.c",
       --     -- "Animations/vendor/GLFW/src/win32_monitor.c",
       --     -- "Animations/vendor/GLFW/src/win32_time.c",
       --     -- "Animations/vendor/GLFW/src/win32_thread.c",
       --     -- "Animations/vendor/GLFW/src/win32_window.c",
       --     -- "Animations/vendor/GLFW/src/wgl_context.c",
       --     -- "Animations/vendor/GLFW/src/egl_context.c",
       --     -- "Animations/vendor/GLFW/src/osmesa_context.c",
       -- }

        -- defines  {
        --     "_CRT_SECURE_NO_WARNINGS"
        -- }

    filter { "configurations:Debug" }
        buildoptions "/MTd"
        runtime "Debug"
        symbols "on"

    filter { "configurations:Release" }
        buildoptions "/MT"
        runtime "Release"
        optimize "on"

        defines {
            "_RELEASE"
        }