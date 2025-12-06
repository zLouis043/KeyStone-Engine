project "KeyStoneTests"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++20"

   targetdir ("../build/bin/%{cfg.buildcfg}")
   objdir ("../build/obj/%{cfg.buildcfg}")

   files { 
      "main.cpp",
      "src/**.cpp",
      "include/**.h"
   }
   
   includedirs {
      "../KeyStoneCore/include/",
      "../KeyStoneCore/",
      vcpkg.includedir
   }

   libdirs {
      "../build/bin/%{cfg.buildcfg}",
      vcpkg.libdir
   }

   links {
      "KeyStoneCore"
   }

   debugargs { "-d" }
   
   filter "system:windows"
      buildoptions { "/utf-8", "/Zc:preprocessor" }
      defines { "_CRT_SECURE_NO_WARNINGS" }

   filter "configurations:Debug"
      defines { "DEBUG", "_DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"