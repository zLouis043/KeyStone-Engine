project "KeyStoneTests"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++20"

   targetdir ("../build/bin/%{cfg.buildcfg}")
   objdir ("../build/obj/%{cfg.buildcfg}")

   files { 
      "main.cpp",
      "src/**.cpp",
      "src/**.h"
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

   debugargs { "-s -d" }
   
   filter "configurations:Debug"
      defines { "DEBUG", "_DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"