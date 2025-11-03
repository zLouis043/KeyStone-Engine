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
	vcpkg.static.includedir,
})

libdirs({
	vcpkg.static.libdir,
	vcpkg.static.bindir,
})

links({
	"lua"
})

defines({
	"KS_EXPORT",
	"FMT_HEADER_ONLY",     -- fmt diventa header-only, niente linking
	"SPDLOG_HEADER_ONLY",  -- spdlog diventa header-only, niente linking
})

characterset("Unicode")

filter("system:windows")
system("windows")
defines({
	"WINDOWS",
	"_WINDOWS",
})
buildoptions({ "/utf-8" })

filter("configurations:Debug")
defines({ "DEBUG" })
symbols("On")

filter("configurations:Release")
defines({ "NDEBUG" })
optimize("On")
