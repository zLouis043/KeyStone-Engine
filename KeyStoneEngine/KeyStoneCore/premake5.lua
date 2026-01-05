project("KeyStoneCore")
	kind("SharedLib")
	language("C++")
	cppdialect("C++20")
	staticruntime "On"

	targetdir("../build/bin/%{cfg.buildcfg}")
	objdir("../build/obj/%{cfg.buildcfg}")

	files({
		"**.h",
		"**.hpp",
		"**.cpp",
		"./include/**.h",
		"./include/**.hpp",
		"./include/**.inl",
		"./src/**.cpp",
	})

	includedirs({
		"./src",
		"./include",
		vcpkg.includedir,
	})

	libdirs({
		vcpkg.libdir
	})

	links({
		"lua",
		"ffi"
	})

	defines({
		"KS_EXPORT",
		"FMT_HEADER_ONLY",
	})

	characterset("Unicode")

	filter "system:windows"
        system "windows"
        defines { "WINDOWS", "_WINDOWS" }
        buildoptions { "/utf-8", "/Zc:preprocessor" }

    filter "system:linux"
        system "linux"
        links { "dl" }
        buildoptions { "-fPIC" }

    filter "system:macosx"
        system "macosx"
        
    filter "configurations:Debug"
        defines { "DEBUG" , "KS_ENABLE_PROFILING", "KS_DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG", "KS_RELEASE" }
        optimize "On"
