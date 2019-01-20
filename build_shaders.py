import os
import sys

def buildShaders():
    if os.name == "nt":
        osPathName = "windows-x86_64"
        exeName = "glslc.exe"
    elif os.name == "posix":
        osPathName = "linux-x86_64"
        exeName = "glslc"

    if os.environ.get("ANDROID_NDK_HOME") != None:
        ndkHome = str(os.environ.get("ANDROID_NDK_HOME"))
    elif os.environ.get("ANDROID_NDK_ROOT") != None:
        ndkHome = str(os.environ.get("ANDROID_NDK_ROOT"))
    else:
        print("ANDROID_NDK_HOME environment variable needs to be set")
        sys.exit(1)

    scriptPath = os.path.dirname(os.path.realpath(__file__))
    srcDir = os.path.join(scriptPath, "app", "src", "main", "assets", "shaders")
    glslcExe = os.path.join(ndkHome, "shader-tools", osPathName, exeName)

    for file in os.listdir(srcDir):
        if not file.endswith(".spv"):
            filePath = str(os.path.join(srcDir, file))
            exe = glslcExe + " " + filePath + " -o " + filePath + ".spv"
            print(exe)
            os.system(exe)

if __name__ == '__main__':
  buildShaders()
