#ifndef OCU_APP
#define OCU_APP
#pragma once

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
#include "PositionBroadcaster.hpp"
#include "Renderer.hpp"

#define SIZE 1024
#define RECEIVING_PORT 8008
#define MAX_LOCATIONS 4

// info shared between main thread and receiving thread
struct Receiver_thread_info {
	std::mutex mtx;
	int leftRumb = 0, leftDur, rightRumb = 0, rightDur; // dur in milliseconds
	std::atomic<bool> has_message;
	std::atomic<bool> run;
};

class OculusApp
{
public:
	OculusApp(int, char**);
	~OculusApp();
	void run();
private:
	Renderer renderer;
	PositionBroadcaster broadcaster;
	std::thread  receiving_thread;

	bool end = false;
	bool isVisible, isRussell;
	Receiver_thread_info rec_info;
	ovrSession session;
};
#endif