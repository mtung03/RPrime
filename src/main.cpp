
#define NOMINMAX

#include <iostream>
// UDP STUFF
#include <winsock2.h>
#include <Windows.h>

#include <GL/glew.h>

#include <stddef.h>

#include <opencv2/opencv.hpp>
/*Depending on the SDL version you are using, you may have to include SDL2/SDL.h or directly SDL.h (2.0.7)*/
#include <SDL.h>
#include <SDL_syswm.h>

#include <Extras/OVR_Math.h>
#include <OVR_CAPI.h>
#include <OVR_CAPI_GL.h>

#include <cuda.h>
#include <cuda_runtime.h>
#include <cuda_gl_interop.h>

#include <sl/Camera.hpp>
#include <string>
#include "Shader.hpp"
#include <atomic>
#include <mutex>
#include "OculusApp.hpp"


#pragma comment(lib, "ws2_32.lib")

int main(int argc, char **argv) {
    OculusApp app(argc, argv);
    app.run();

    return 0;
}
