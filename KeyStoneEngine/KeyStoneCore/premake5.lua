project "KeyStoneCore"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"

    targetdir ("../build/bin/%{cfg.buildcfg}")
    objdir ("../build/obj/%{cfg.buildcfg}")

    files { 
        "./include/**.h",
        "./include/**.inl",
        "./src/**.cpp" 
    }

    includedirs { 
        "./src",
        "./include",
        vcpkg.includedirs
    }

    libdirs {
        vcpkg.libdirs
    }

    links {
        "lua"
    }

    filter "system:windows"
        system "windows"
        defines { 
            "WINDOWS",
            "_WINDOWS"
        }

    filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"
      
    filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"

