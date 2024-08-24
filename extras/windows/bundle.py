#!/usr/bin/env python3

import os
import subprocess
import shutil
import sys

# Define the paths as a list
search_paths = [
# MSYS2 paths
    '/mingw32/bin',
    '/mingw64/bin',
    '/ucrt64/bin',
    '/clang32/bin',
    '/clang64/bin',
    '/clangarm64/bin',
# Fedora toolchain paths
    '/usr/i686-w64-mingw32/bin',
    '/usr/x86_64-w64-mingw32/bin',
    '/usr/x86_64-w64-mingw32ucrt/bin',
# Fedora MINGW toolchain paths
    '/usr/i686-w64-mingw32/sys-root/mingw/bin',
    '/usr/x86_64-w64-mingw32/sys-root/mingw/bin',
    '/usr/x86_64-w64-mingw32ucrt/sys-root/mingw/bin',
# MXE toolchain paths
    '/usr/lib/mxe/usr/x86_64-w64-mingw32.shared/bin'
]

# Join the paths with a colon to form the environment variable
os.environ['MINGW_BUNDLEDLLS_SEARCH_PATH'] = ':'.join(search_paths)

def usage():
    print(f"usage: {os.path.basename(sys.argv[0])} [-d] (Verbose output)")
    print("DLL Bundler for Windows")
    print("This script is called by 'meson install' if building for Windows.")
    print("The intention is for this wrapper to behave in a similar manner to the bundle.py script in extras/macos.")
    print(f"usage: {os.path.basename(sys.argv[0])} [-d] (Verbose output)")
    sys.exit(1)

verbose = False

# Parse command-line arguments
args = sys.argv[1:]
while args:
    arg = args.pop(0)
    if arg == '-d':
        verbose = True
    else:
        usage()

# Search for DLLs in subproject directories
subproj_dirs = [os.path.join(os.getenv('MESON_SOURCE_ROOT'), 'subprojects'),
                os.path.join(os.getenv('MESON_BUILD_ROOT'), 'subprojects')]
for subproj_dir in subproj_dirs:
    if os.path.isdir(subproj_dir):
        for root, dirs, files in os.walk(subproj_dir):
            os.environ['MINGW_BUNDLEDLLS_SEARCH_PATH'] += f':{root}'

# Run mingw-bundledlls to get DLL list
dll_list_cmd = [sys.executable, os.path.join(os.getenv('MESON_SOURCE_ROOT'), 'extras/windows/mingw-bundledlls/mingw-bundledlls'),
                os.path.join(os.getenv('MESON_BUILD_ROOT'), 'naev.exe')]

if verbose:
    print("Executing command:", dll_list_cmd)
    print("Working directory:", os.getcwd())

dll_list_proc = subprocess.Popen(dll_list_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
dll_list_out, dll_list_err = dll_list_proc.communicate()

if verbose:
    print(dll_list_out.decode())
    print(dll_list_err.decode())

# Copy DLLs to installation directory
dll_list = dll_list_out.decode().splitlines()
for dll in dll_list:
    dll_path = os.path.join(os.getenv('MESON_INSTALL_DESTDIR_PREFIX'), os.path.basename(dll))
    shutil.copy(dll, dll_path)
