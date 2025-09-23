project "KeyStone-CLI"
   kind "ConsoleApp"
   language "C++"

   targetdir ("../build/bin/%{cfg.buildcfg}")
   objdir ("../build/obj/%{cfg.buildcfg}")

   files { 
      "./main.cpp",
      "./project_manager.*",
      "./dependency_manager.*",
      "./deployment_manager.*" 
   }
   
   includedirs {
      vcpkg.includedirs,
      "../KeyStoneCore/include"
   }

   libdirs {
      vcpkg.libdirs,
      "../build/bin/%{cfg.buildcfg}" 
   }

   links {
      "KeyStoneCore"
   }
   
   cppdialect "C++20"
   
   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"
      
   filter "configurations:Release"

      defines { "NDEBUG" }
      optimize "On"