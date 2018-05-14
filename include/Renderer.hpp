#ifndef RENDERER
#define RENDERER

#pragma once
#include <iostream>
// UDP STUFF
#include <winsock2.h>
#include <Windows.h>

#include <GL/glew.h>

#include <stddef.h>

#include <opencv2/opencv.hpp>
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
#include "ImageReceiver.hpp"

class Renderer
{
public:
	Renderer();
	~Renderer();
	bool getIsRussell();
	void render_frame();
	void initialize(ovrSession*, ovrGraphicsLuid*);
private:
	ImageReceiver receiver;

	bool end, isVisible, isRussell;

	ovrErrorInfo errInf;
	// Compute the final size of the render buffer
	ovrSizei bufferSize;
	// SDL variable that will be used to store input events
	SDL_Event events;

	ovrSession *session;
	ovrGraphicsLuid luid;
	ovrTextureSwapChain textureChain = nullptr;
	ovrTextureSwapChainDesc descTextureSwap = {};
	GLuint fboID;
	GLuint depthBuffID;
	ovrMirrorTextureDesc descMirrorTexture;
	ovrHmdDesc hmdDesc;
	GLuint zedTextureID[2];
	ovrPosef eyeRenderPose[2];
	ovrPosef hmdToEyeOffset[2];
	cudaGraphicsResource *cimg_l;
	cudaGraphicsResource *cimg_r;
	int frameIndex = 0;
	GLuint mirrorFBOID;
	int winWidth, winHeight;
	double sensorSampleTime;
	Shader *oc_shader;
	SDL_Window* window;
	SDL_GLContext glContext;
	ovrMirrorTexture mirrorTexture = nullptr;
	GLuint rectVBO[3];

};

#endif