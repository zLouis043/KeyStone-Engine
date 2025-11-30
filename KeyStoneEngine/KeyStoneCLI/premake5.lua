project "KeyStone-CLI"
   kind "ConsoleApp"
   language "C++"

   targetdir ("../build/bin/%{cfg.buildcfg}")
   objdir ("../build/obj/%{cfg.buildcfg}")

   files { 
      "main.cpp"
   }
   
   includedirs {
      vcpkg.includedir,
      "../KeyStoneCore/",
      "../KeyStoneCore/include/"
   }

   libdirs {
      vcpkg.libdir,
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

   filter "system:windows"
      system "windows"
      defines { 
         "WINDOWS",
         "_WINDOWS"
      }
      buildoptions { "/utf-8" }
