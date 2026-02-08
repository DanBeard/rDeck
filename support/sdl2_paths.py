"""
SDL2 path detection for PlatformIO native build.
Finds SDL2 includes and library paths using pkg-config.
"""
Import("env")
import subprocess
import sys

def get_sdl2_flags():
    """Get SDL2 compiler and linker flags via pkg-config"""
    try:
        # Get cflags (includes)
        cflags = subprocess.check_output(
            ["pkg-config", "--cflags", "sdl2"],
            stderr=subprocess.DEVNULL
        ).decode().strip().split()

        # Get libs
        libs = subprocess.check_output(
            ["pkg-config", "--libs", "sdl2"],
            stderr=subprocess.DEVNULL
        ).decode().strip().split()

        return cflags, libs
    except (subprocess.CalledProcessError, FileNotFoundError):
        print("Warning: pkg-config not found or SDL2 not installed")
        print("Trying common paths...")
        # Fallback for common installations
        return ["-I/usr/include/SDL2"], ["-lSDL2"]

cflags, libs = get_sdl2_flags()

# Add include paths
for flag in cflags:
    if flag.startswith("-I"):
        env.Append(CPPPATH=[flag[2:]])
    else:
        env.Append(CCFLAGS=[flag])

# Add library paths and libs
for flag in libs:
    if flag.startswith("-L"):
        env.Append(LIBPATH=[flag[2:]])
    elif flag.startswith("-l"):
        env.Append(LIBS=[flag[2:]])
    else:
        env.Append(LINKFLAGS=[flag])

print(f"SDL2 CFLAGS: {cflags}")
print(f"SDL2 LIBS: {libs}")
