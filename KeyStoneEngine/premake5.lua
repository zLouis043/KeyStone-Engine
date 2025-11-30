local function setupVcpkg()
    local local_vcpkg = path.join(_MAIN_SCRIPT_DIR, "..", "vendor", "vcpkg")
    
    if not os.isdir(local_vcpkg) then
        print("ERROR: Vcpkg directory not found at " .. local_vcpkg)
        return nil
    end

    local triplet = ""
    if os.target() == "windows" then
        triplet = "x64-windows"
    elseif os.target() == "linux" then
        triplet = "x64-linux"
    elseif os.target() == "macosx" then
        if os.isdir(path.join(local_vcpkg, "installed", "arm64-osx")) then
            triplet = "arm64-osx"
        else
            triplet = "x64-osx"
        end
    end

    local install_root = path.join(local_vcpkg, "installed")
    
    local static_triplet = triplet .. "-static"
    local static_path = path.join(install_root, static_triplet)
    
    local base_path = path.join(install_root, triplet)

    local final_path = ""
    
    if os.isdir(static_path) then
        final_path = static_path
        print("Vcpkg: Using static triplet path: " .. final_path)
    elseif os.isdir(base_path) then
        final_path = base_path
        print("Vcpkg: Using standard triplet path: " .. final_path)
    else
        print("WARNING: Could not find installed libraries for triplet " .. triplet .. " or " .. static_triplet)
        print("Did you run Scripts/setup.[bat|sh]?")
        return { includedir = "", libdir = "", bindir = "" }
    end

    return {
        includedir = path.join(final_path, "include"),
        libdir = path.join(final_path, "lib"),
        bindir = path.join(final_path, "bin")
    }
end
vcpkg = setupVcpkg()

workspace("Keystone-Engine")
configurations({ "Debug", "Release" })
platforms({ "x64" })
startproject("KeyStoneTests")

include("./KeyStoneCore/premake5.lua")
include("./KeyStoneCLI/premake5.lua")
include("./KeyStoneTests/premake5.lua")

newaction {
        trigger = "clean",
        description = "Remove all build files.",
        execute = function()
            print("Cleaning build directory...")
            os.rmdir("build")
            os.rmdir("bin") 
            os.rmdir("bin-int")
            
            if os.target() == "windows" then
                os.execute("del /s /q *.sln *.vcxproj *.vcxproj.user *.vcxproj.filters 2>nul")
            else
                os.execute("find . -name 'Makefile' -delete")
                os.execute("find . -name '*.make' -delete")
            end
            print("Done.")
        end
    }
