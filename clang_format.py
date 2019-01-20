import os
import sys

def clangFormat():
    if os.name == "nt":
        osPathName = "windows-x86_64"
        exeName = "clang-format.exe"
    elif os.name == "posix":
        osPathName = "linux-x86_64"
        exeName = "clang-format"

    if os.environ.get("ANDROID_NDK_HOME") != None:
        ndkHome = str(os.environ.get("ANDROID_NDK_HOME"))
    elif os.environ.get("ANDROID_NDK_ROOT") != None:
        ndkHome = str(os.environ.get("ANDROID_NDK_ROOT"))
    else:
        print("ANDROID_NDK_HOME environment variable needs to be set")
        sys.exit(1)

    scriptPath = os.path.dirname(os.path.realpath(__file__))
    srcDir = os.path.join(scriptPath, "app", "src", "main", "cpp")
    clangFormatExe = os.path.join(ndkHome, "toolchains", "llvm", "prebuilt", osPathName, "bin", exeName)

    for file in os.listdir(srcDir):
        if file.endswith(".c") or file.endswith(".cpp") or file.endswith(".h") or file.endswith(".hpp"):
            exe = clangFormatExe + " -i -style=file " + str(os.path.join(srcDir, file))
            print(exe)
            os.system(exe)

if __name__ == '__main__':
  clangFormat()
