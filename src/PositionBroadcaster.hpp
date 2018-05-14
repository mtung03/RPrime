#ifndef POS_BCAST
#define POS_BCAST
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
#include <string>
#include <map>
#include "Renderer.hpp"

#define SIZE 1024
#define RECEIVING_PORT 8008
#define MAX_LOCATIONS 4

class PositionBroadcaster {
public:
	PositionBroadcaster();
	void initialize(int, char**, ovrSession*);
	~PositionBroadcaster();
	void send_position();
	struct location {
		int a1, a2, a3, a4; // ip address
		unsigned short port; // port number
	};
	// info shared between main thread and broadcasting thread
	struct Broadcast_thread_info {
		std::mutex mtx;
		ovrTrackingState ts;
		ovrInputState    inputState;
		std::atomic<bool> run;
		int argc;
		location *locs;
	};
private:
	ovrSession *session;
	std::multimap<std::string, location> targets;
	Broadcast_thread_info send_info;
	std::thread broadcast_thread;
	location locations[MAX_LOCATIONS];
	int num_locations = 1;
};
#endif