project("KeyStoneCore")
kind("SharedLib")
language("C++")
cppdialect("C++20")

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
	vcpkg.libdir,
	vcpkg.bindir,
})

filter("system:linux")
links({ "lua5.4" })
filter("system:windows")
links({ "lua" })
filter({})

links({
	"fmt",
	"spdlog",
})

defines({
	"KS_EXPORT",
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
