#ifndef IMG_REC
#define IMG_REC

#pragma once
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

#define BUFLEN 65000
#define LPORT 8000
#define RPORT 8001

class ImageReceiver
{
public:
	ImageReceiver();
	~ImageReceiver();
	void init_images(int, int);
	void start_threads();
	bool has_new_frame();
	bool getIsRussell();
	void copy_frames(cudaGraphicsResource *cimg_l, cudaGraphicsResource *cimg_r);
	// Packed data for threaded computation
	struct ThreadData {
		sl::Mat zed_image;
		std::mutex mtx;
		bool isRussell;
		std::mutex hasFrame;
		bool run;
		bool new_frame;
	};
private:
	bool isRussell;
	// Create a struct which contains the sl::Camera and the associated data
	ThreadData thread_data[2];
	std::thread runnerLeft, runnerRight;
};
#endif