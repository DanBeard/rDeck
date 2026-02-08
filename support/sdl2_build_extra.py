"""
SDL2 post-build configuration for PlatformIO native build.
Ensures SDL2 is properly linked.
"""
Import("env")

# Ensure SDL2 is linked (may be redundant with sdl2_paths.py but safe)
env.Append(LIBS=["SDL2"])

# Add pthread for std::thread on Linux
import platform
if platform.system() == "Linux":
    env.Append(LIBS=["pthread"])
    env.Append(CCFLAGS=["-pthread"])
    env.Append(LINKFLAGS=["-pthread"])

# Add any additional platform-specific flags
if platform.system() == "Darwin":  # macOS
    # macOS may need framework linking
    pass
