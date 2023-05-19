#!/usr/bin/env python

import os
import sys
import platform

# Try to detect the host platform automatically.
# This is used if no `platform` argument is passed
if sys.platform.startswith("linux"):
    host_platform = "linux"
elif sys.platform == "darwin":
    host_platform = "osx"
elif sys.platform == "win32" or sys.platform == "msys":
    host_platform = "windows"
else:
    raise ValueError("Could not detect platform automatically, please specify with " "platform=<platform>")

env = Environment(ENV=os.environ)

opts = Variables([], ARGUMENTS)

# Define our options
opts.Add(EnumVariable("target", "Compilation target", "debug", ["e", "editor", "d", "debug", "r", "release"]))
opts.Add(EnumVariable("platform", "Compilation platform", host_platform, ["", "windows", "x11", "linux", "osx"]))
opts.Add(
    EnumVariable("p", "Compilation target, alias for 'platform'", host_platform, ["", "windows", "x11", "linux", "osx"])
)
opts.Add(EnumVariable("bits", "Target platform bits", "64", ("32", "64")))
opts.Add(BoolVariable("use_llvm", "Use the LLVM / Clang compiler", "no"))
opts.Add(BoolVariable("dev_build", "Debug symbols", "yes"))

opts.Update(env)

# Local dependency paths, adapt them to your setup
godot_headers_path = "godot-cpp/gdextension/"
cpp_bindings_path = "godot-cpp/"
cpp_library = "libgodot-cpp"

# only support 64 at this time..
bits = 64

# Generates help for the -h scons option.
Help(opts.GenerateHelpText(env))

architecture_array = ["", "universal", "x86_32", "x86_64", "arm32", "arm64", "rv64", "ppc32", "ppc64", "wasm32"]
architecture_aliases = {
    "x64": "x86_64",
    "amd64": "x86_64",
    "armv7": "arm32",
    "armv8": "arm64",
    "arm64v8": "arm64",
    "aarch64": "arm64",
    "rv": "rv64",
    "riscv": "rv64",
    "riscv64": "rv64",
    "ppcle": "ppc32",
    "ppc": "ppc32",
    "ppc64le": "ppc64",
}
opts.Add(EnumVariable("arch", "CPU architecture", "", architecture_array, architecture_aliases))

opts.Update(env)
Help(opts.GenerateHelpText(env))

if env['arch'] == "":
    # No architecture specified. Default to arm64 if building for Android,
    # universal if building for macOS or iOS, wasm32 if building for web,
    # otherwise default to the host architecture.
    if env["platform"] in ["osx", "ios"]:
        env["arch"] = "universal"
    elif env["platform"] == "android":
        env["arch"] = "arm64"
    elif env["platform"] == "javascript":
        env["arch"] = "wasm32"
    else:
        host_machine = platform.machine().lower()
        if host_machine in architecture_array:
            env["arch"] = host_machine
        elif host_machine in architecture_aliases.keys():
            env["arch"] = architecture_aliases[host_machine]
        elif "86" in host_machine:
            # Catches x86, i386, i486, i586, i686, etc.
            env["arch"] = "x86_32"
        else:
            print("Unsupported CPU architecture: " + host_machine)
            Exit()

# We use this to re-set env["arch"] anytime we call opts.Update(env).
env_arch = env["arch"]

# This makes sure to keep the session environment variables on Windows.
# This way, you can run SCons in a Visual Studio 2017 prompt and it will find
# all the required tools
if host_platform == "windows" and env["platform"] != "android":
    if env["bits"] == "64":
        env = Environment(TARGET_ARCH="amd64")
        env['msvc_arch'] = 'X64'
    elif env["bits"] == "32":
        env = Environment(TARGET_ARCH="x86")
        env['msvc_arch'] = 'X86'
    opts.Update(env)

# Process some arguments
if env["use_llvm"]:
    env["CC"] = "clang"
    env["CXX"] = "clang++"

if env["p"] != "":
    env["platform"] = env["p"]

if env["platform"] == "":
    print("No valid target platform selected.")
    quit()

if env["platform"] == "freebsd":
    env["platform"] = "linux"

if env["target"] in ("debug", "editor"):
    env.Append(CPPDEFINES=["DEBUG_ENABLED", "DEBUG_METHODS_ENABLED"])

if env["platform"] in ("x11", "linux"):
    cpp_library += ".linux"
    env.Append(CCFLAGS=["-fPIC"])
    env.Append(CXXFLAGS=["-std=c++17"])
    if env["target"] in ("debug", "d", "editor", "e"):
        env.Append(CCFLAGS=["-g", "-O2"])
    else:
        env.Append(CCFLAGS=["-g", "-O3"])
elif env["platform"] == "windows":
    cpp_library += ".windows"
    # This makes sure to keep the session environment variables on windows,
    # that way you can run scons in a vs 2017 prompt and it will find all the required tools
    env.Append(ENV=os.environ)
    #env.Append(CPPDEFINES=["WIN32", "_WIN32", "_WINDOWS", "_CRT_SECURE_NO_WARNINGS", "_ITERATOR_DEBUG_LEVEL=2"])
    env.Append(CCFLAGS=["-W3", "-GR"])
    env.Append(CXXFLAGS=["-std:c++17"])
    if env["target"] in ("debug", "d", "editor", "e"):
        env.Append(CPPDEFINES=["_DEBUG"])
        env.Append(CCFLAGS=["-EHsc", "-MDd", "-ZI", "-FS"])
        env.Append(LINKFLAGS=["-DEBUG", "/MACHINE:" + env['msvc_arch']])
    else:
        env.Append(CPPDEFINES=["NDEBUG"])
        env.Append(CCFLAGS=["-O2", "-EHsc", "-MD"])
    if not(env["use_llvm"]):
        env.Append(CPPDEFINES=["TYPED_METHOD_BIND"])

if env["target"] in ("debug", "d"):
    cpp_library += ".template_debug"
elif env["target"] in ("editor", "e"):
    cpp_library += ".editor"
else:
    cpp_library += ".template_release"

if env["dev_build"]:
    cpp_library += ".dev"

cpp_library += "." + str(env_arch)

# make sure our binding library is properly includes
env.Append(LIBPATH=["lib/" + env["platform"] + "/" + env_arch + "/lib", cpp_bindings_path + "bin/"])
env.Append(CPPPATH=["lib/" + env["platform"] + "/" + env_arch + "/include", godot_headers_path, cpp_bindings_path + "include/", cpp_bindings_path + "gen/include/"])
env.Append(LIBS=[cpp_library])

# tweak this if you want to use different folders, or more folders, to store your source code in.
env.Append(CPPPATH=["src/"])

Export('env')

sources = Glob("src/*.cpp") + Glob("lib/*.c")

suffix = env["SHLIBSUFFIX"]
if env["platform"] == "windows":
    suffix = ".dll"

library = env.SharedLibrary("bin/" + env["platform"] + "/libffmpegmediadecoder.64" + suffix, source=sources)

env.Prepend(LIBS=["avformat", "avcodec", "avutil", "swresample", "swscale"])

Default(library)
