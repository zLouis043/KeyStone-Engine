project "KeyStoneCore"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"

    targetdir ("../build/bin/%{cfg.buildcfg}")
    objdir ("../build/obj/%{cfg.buildcfg}")

    files { 
        "**.h",
        "**.hpp",
        "**.cpp",
        "./include/**.h",
        "./include/**.hpp",
        "./include/**.inl",
        "./src/**.cpp" 
    }

    includedirs { 
        "./src",
        "./include",
        vcpkg.includedir
    }

    libdirs {
        vcpkg.libdir,
        vcpkg.bindir
    }

    links {
        "lua", "fmt", "spdlog"
    }

    defines {
        "KS_EXPORT"
    }

    postbuildcommands {
        "{COPYFILE} %{vcpkg.bindir}/spdlog.dll %{cfg.targetdir}",
        "{COPYFILE} %{vcpkg.bindir}/fmt.dll %{cfg.targetdir}",
        "{COPYFILE} %{vcpkg.bindir}/lua.dll %{cfg.targetdir}",
    }

    characterset ("Unicode")

    filter "system:windows"
        system "windows"
        defines { 
            "WINDOWS",
            "_WINDOWS"
        }
        buildoptions { "/utf-8" }

    filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"
      
    filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"

