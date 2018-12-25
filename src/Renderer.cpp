#include "Renderer.hpp"

GLchar* OVR_ZED_VS =
"#version 330 core\n \
			layout(location=0) in vec3 in_vertex;\n \
			layout(location=1) in vec2 in_texCoord;\n \
			uniform uint isLeft; \n \
			out vec2 b_coordTexture; \n \
			void main()\n \
			{\n \
				if (isLeft == 1U)\n \
				{\n \
					b_coordTexture = in_texCoord;\n \
					gl_Position = vec4(in_vertex.x, in_vertex.y, in_vertex.z,1);\n \
				}\n \
				else \n \
				{\n \
					b_coordTexture = vec2(1.0 - in_texCoord.x, in_texCoord.y);\n \
					gl_Position = vec4(-in_vertex.x, in_vertex.y, in_vertex.z,1);\n \
				}\n \
			}";

GLchar* OVR_ZED_FS =
"#version 330 core\n \
			uniform sampler2D u_textureZED; \n \
			in vec2 b_coordTexture;\n \
			out vec4 out_color; \n \
			void main()\n \
			{\n \
				out_color = vec4(texture(u_textureZED, b_coordTexture).bgr,1); \n \
			}";

Renderer::Renderer() {
	receiver.start_threads();
	// Initialize SDL2's context
	SDL_Init(SDL_INIT_VIDEO);
}

void Renderer::initialize(ovrSession *sess, ovrGraphicsLuid *id) {
	session = sess;
	luid = *id;

	isRussell = receiver.getIsRussell();
	int x = SDL_WINDOWPOS_CENTERED, y = SDL_WINDOWPOS_CENTERED;
	winWidth = isRussell ? 1280 : 640;
	winHeight = isRussell ? 720 : 480;
	Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
	// Create SDL2 Window
	window = SDL_CreateWindow("RPrime", x, y, winWidth, winHeight, flags);
	// Create OpenGL context
	glContext = SDL_GL_CreateContext(window);
	// Initialize GLEW
	glewInit();
	// Turn off vsync to let the compositor do its magic
	SDL_GL_SetSwapInterval(0);


	int zedWidth = isRussell ? 1280 : 640;
	int zedHeight = isRussell ? 720 : 480;

	glGenTextures(2, zedTextureID);
	for (int eye = 0; eye < 2; ++eye) {
		// Generate OpenGL texture
		glBindTexture(GL_TEXTURE_2D, zedTextureID[eye]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, zedWidth, zedHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	receiver.init_images(zedWidth, zedHeight);

	cudaError_t  err = cudaGraphicsGLRegisterImage(&cimg_l, zedTextureID[ovrEye_Left], GL_TEXTURE_2D, cudaGraphicsRegisterFlagsWriteDiscard);
	cudaError_t  err2 = cudaGraphicsGLRegisterImage(&cimg_r, zedTextureID[ovrEye_Right], GL_TEXTURE_2D, cudaGraphicsRegisterFlagsWriteDiscard);
	if (err != cudaSuccess || err2 != cudaSuccess)
		std::cerr << "ERROR: cannot create CUDA texture : " << err << std::endl;


	float pixel_density = 1.75f;
	hmdDesc = ovr_GetHmdDesc(*session);
	// Get the texture sizes of Oculus eyes
	ovrSizei textureSize0 = ovr_GetFovTextureSize(*session, ovrEye_Left, hmdDesc.DefaultEyeFov[0], pixel_density);
	ovrSizei textureSize1 = ovr_GetFovTextureSize(*session, ovrEye_Right, hmdDesc.DefaultEyeFov[1], pixel_density);

	bufferSize.w = textureSize0.w + textureSize1.w;
	bufferSize.h = std::max(textureSize0.h, textureSize1.h);

	descTextureSwap.Type = ovrTexture_2D;
	descTextureSwap.ArraySize = 1;
	descTextureSwap.Width = bufferSize.w;
	descTextureSwap.Height = bufferSize.h;
	descTextureSwap.MipLevels = 1;
	descTextureSwap.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
	descTextureSwap.SampleCount = 1;
	descTextureSwap.StaticImage = ovrFalse;
	// Create the OpenGL texture swap chain
	ovrResult result = ovr_CreateTextureSwapChainGL(*session, &descTextureSwap, &textureChain);

	if (OVR_SUCCESS(result)) {
		int length = 0;
		ovr_GetTextureSwapChainLength(*session, textureChain, &length);

		for (int i = 0; i < length; ++i) {
			GLuint chainTexId;
			ovr_GetTextureSwapChainBufferGL(*session, textureChain, i, &chainTexId);
			glBindTexture(GL_TEXTURE_2D, chainTexId);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
	} else {
		ovr_GetLastErrorInfo(&errInf);
		std::cerr << "ERROR: failed creating swap texture " << errInf.ErrorString << std::endl;
		ovr_Destroy(*session);
		ovr_Shutdown();
		SDL_GL_DeleteContext(glContext);
		SDL_DestroyWindow(window);
		SDL_Quit();
		exit(EXIT_FAILURE);
	}

	glGenFramebuffers(1, &fboID);
	glGenTextures(1, &depthBuffID);
	glBindTexture(GL_TEXTURE_2D, depthBuffID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	GLenum internalFormat = GL_DEPTH_COMPONENT24;
	GLenum type = GL_UNSIGNED_INT;
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, bufferSize.w, bufferSize.h, 0, GL_DEPTH_COMPONENT, type, NULL);

	memset(&descMirrorTexture, 0, sizeof(descMirrorTexture));
	descMirrorTexture.Width = winWidth;
	descMirrorTexture.Height = winHeight;
	descMirrorTexture.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;

	result = ovr_CreateMirrorTextureGL(*session, &descMirrorTexture, &mirrorTexture);
	if (!OVR_SUCCESS(result)) {
		ovr_GetLastErrorInfo(&errInf);
		std::cerr << "ERROR: Failed to create mirror texture " << errInf.ErrorString << std::endl;
	}

	GLuint mirrorTextureId;
	ovr_GetMirrorTextureBufferGL(*session, mirrorTexture, &mirrorTextureId);

	glGenFramebuffers(1, &mirrorFBOID);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, mirrorFBOID);
	glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mirrorTextureId, 0);
	glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	// Frame index used by the compositor, it needs to be updated each new frame
	long long frameIndex = 0;

	// FloorLevel will give tracking poses where the floor height is 0
	ovr_SetTrackingOriginType(*session, ovrTrackingOrigin_FloorLevel);


	// Create and compile the shader's sources
	oc_shader = new Shader(OVR_ZED_VS, OVR_ZED_FS);;

	// Compute the useful part of the ZED image
	unsigned int widthFinal = bufferSize.w / 2;
	float heightGL = 1.f;
	float widthGL = 1.f;
	if (zedWidth > 0.f) {
		unsigned int heightFinal = zedHeight * widthFinal / (float)zedWidth;
		// Convert this size to OpenGL viewport's frame's coordinates
		heightGL = (heightFinal) / (float)(bufferSize.h);
		widthGL = ((zedWidth * (heightFinal / (float)zedHeight)) / (float)widthFinal);
	} else {
		std::cerr << "WARNING: ZED parameters got wrong values."
			"Default vertical and horizontal FOV are used.\n"
			"Check your calibration file or check if your ZED is not too close to a surface or an object."
			<< std::endl;
	}

	// Compute the Horizontal Oculus' field of view with its parameters
	float ovrFovH = (atanf(hmdDesc.DefaultEyeFov[0].LeftTan) + atanf(hmdDesc.DefaultEyeFov[0].RightTan));
	// Compute the Vertical Oculus' field of view with its parameters
	float ovrFovV = (atanf(hmdDesc.DefaultEyeFov[0].UpTan) + atanf(hmdDesc.DefaultEyeFov[0].DownTan));

	// Compute the center of the optical lenses of the headset
	float offsetLensCenterX = ((atanf(hmdDesc.DefaultEyeFov[0].LeftTan)) / ovrFovH) * 2.f - 1.f;
	float offsetLensCenterY = ((atanf(hmdDesc.DefaultEyeFov[0].UpTan)) / ovrFovV) * 2.f - 1.f;

	// Create a rectangle with the computed coordinates and push it in GPU memory
	struct GLScreenCoordinates {
		float left, up, right, down;
	} screenCoord;

	screenCoord.up = heightGL + offsetLensCenterY;
	screenCoord.down = heightGL - offsetLensCenterY;
	screenCoord.right = widthGL + offsetLensCenterX;
	screenCoord.left = widthGL - offsetLensCenterX;

	float rectVertices[12] = { -screenCoord.left, -screenCoord.up, 0, screenCoord.right, -screenCoord.up, 0, screenCoord.right, screenCoord.down, 0, -screenCoord.left, screenCoord.down, 0 };

	glGenBuffers(1, &rectVBO[0]);
	glBindBuffer(GL_ARRAY_BUFFER, rectVBO[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(rectVertices), rectVertices, GL_STATIC_DRAW);

	float rectTexCoord[8] = { 0, 1, 1, 1, 1, 0, 0, 0 };
	glGenBuffers(1, &rectVBO[1]);
	glBindBuffer(GL_ARRAY_BUFFER, rectVBO[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(rectTexCoord), rectTexCoord, GL_STATIC_DRAW);

	unsigned int rectIndices[6] = { 0, 1, 2, 0, 2, 3 };
	glGenBuffers(1, &rectVBO[2]);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, rectVBO[2]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(rectIndices), rectIndices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// Initialize a boolean that will be used to stop the application�s loop and another one to pause/unpause rendering
	end = false;
	// This boolean is used to test if the application is focused
	isVisible = true;

	// Enable the shader
	glUseProgram(oc_shader->getProgramId());
	// Bind the Vertex Buffer Objects of the rectangle that displays ZED images

	// vertices
	glEnableVertexAttribArray(Shader::ATTRIB_VERTICES_POS);
	glBindBuffer(GL_ARRAY_BUFFER, rectVBO[0]);
	glVertexAttribPointer(Shader::ATTRIB_VERTICES_POS, 3, GL_FLOAT, GL_FALSE, 0, 0);
	// indices
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, rectVBO[2]);
	// texture coordinates
	glEnableVertexAttribArray(Shader::ATTRIB_TEXTURE2D_POS);
	glBindBuffer(GL_ARRAY_BUFFER, rectVBO[1]);
	glVertexAttribPointer(Shader::ATTRIB_TEXTURE2D_POS, 2, GL_FLOAT, GL_FALSE, 0, 0);

	cudaGraphicsMapResources(1, &cimg_l, 0);
	cudaGraphicsMapResources(1, &cimg_r, 0);
}

Renderer::~Renderer() {
	cudaGraphicsUnmapResources(1, &cimg_l);
	cudaGraphicsUnmapResources(1, &cimg_r);

	// Disable all OpenGL buffer
	glDisableVertexAttribArray(Shader::ATTRIB_TEXTURE2D_POS);
	glDisableVertexAttribArray(Shader::ATTRIB_VERTICES_POS);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
	glBindVertexArray(0);
	// Delete the Vertex Buffer Objects of the rectangle
	glDeleteBuffers(3, rectVBO);

	delete oc_shader;

	// Delete SDL, OpenGL, Oculus and ZED context
	ovr_DestroyTextureSwapChain(*session, textureChain);
	ovr_DestroyMirrorTexture(*session, mirrorTexture);
	ovr_Destroy(*session);
	ovr_Shutdown();
	SDL_GL_DeleteContext(glContext);
	SDL_DestroyWindow(window);
	SDL_Quit();
}

bool Renderer::getIsRussell() {
	return isRussell;
}

void Renderer::render_frame() {
	// While there is an event catched and not tested
	while (SDL_PollEvent(&events)) {
		// If a key is released
		if (events.type == SDL_KEYUP) {
			// If Q -> quit the application
			if (events.key.keysym.scancode == SDL_SCANCODE_Q)
				end = true;
		}
	}

	// Get texture swap index where we must draw our frame
	GLuint curTexId;
	int curIndex;
	ovr_GetTextureSwapChainCurrentIndex(*session, textureChain, &curIndex);
	ovr_GetTextureSwapChainBufferGL(*session, textureChain, curIndex, &curTexId);

	// Call ovr_GetRenderDesc each frame to get the ovrEyeRenderDesc, as the returned values (e.g. HmdToEyeOffset) may change at runtime.
	hmdToEyeOffset[ovrEye_Left] = ovr_GetRenderDesc(*session, ovrEye_Left, hmdDesc.DefaultEyeFov[ovrEye_Left]).HmdToEyePose;
	hmdToEyeOffset[ovrEye_Right] = ovr_GetRenderDesc(*session, ovrEye_Right, hmdDesc.DefaultEyeFov[ovrEye_Right]).HmdToEyePose;

	// Get eye poses, feeding in correct IPD offset
	ovr_GetEyePoses2(*session, frameIndex, ovrTrue, hmdToEyeOffset, eyeRenderPose, &sensorSampleTime);

	// If the application is focused
	if (isVisible) {
		if (receiver.has_new_frame()) {
			receiver.copy_frames(cimg_l, cimg_r);
			// Bind the frame buffer
			glBindFramebuffer(GL_FRAMEBUFFER, fboID);
			// Set its color layer 0 as the current swap texture
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, curTexId, 0);
			// Set its depth layer as our depth buffer
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthBuffID, 0);
			// Clear the frame buffer
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glClearColor(0, 0, 0, 1);

			// Render for each Oculus eye the equivalent ZED image
			for (int eye = 0; eye < 2; ++eye) {
				// Set the left or right vertical half of the buffer as the viewport
				glViewport(eye == ovrEye_Left ? 0 : bufferSize.w / 2, 0, bufferSize.w / 2, bufferSize.h);
				// Bind the left or right ZED image
				glBindTexture(GL_TEXTURE_2D, eye == ovrEye_Left ? zedTextureID[ovrEye_Left] : zedTextureID[ovrEye_Right]);
				// Bind the isLeft value
				glUniform1ui(glGetUniformLocation(oc_shader->getProgramId(), "isLeft"), eye == ovrEye_Left ? 1U : 0U);
				// Draw the ZED image
				glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
			}
			// Avoids an error when calling SetAndClearRenderSurface during next iteration.
			// Without this, during the next while loop iteration SetAndClearRenderSurface
			// would bind a framebuffer with an invalid COLOR_ATTACHMENT0 because the texture ID
			// associated with COLOR_ATTACHMENT0 had been unlocked by calling wglDXUnlockObjectsNV.
			glBindFramebuffer(GL_FRAMEBUFFER, fboID);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
			// Commit changes to the textures so they get picked up frame
			ovr_CommitTextureSwapChain(*session, textureChain);
		}
		// Do not forget to increment the frameIndex!
		frameIndex++;
	}

	ovrLayerEyeFov ld;
	ld.Header.Type = ovrLayerType_EyeFov;
	// Tell to the Oculus compositor that our texture origin is at the bottom left
	ld.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft; // Because OpenGL | Disable head tracking
															  // Set the Oculus layer eye field of view for each view
	for (int eye = 0; eye < 2; ++eye) {
		// Set the color texture as the current swap texture
		ld.ColorTexture[eye] = textureChain;
		// Set the viewport as the right or left vertical half part of the color texture
		ld.Viewport[eye] = OVR::Recti(eye == ovrEye_Left ? 0 : bufferSize.w / 2, 0, bufferSize.w / 2, bufferSize.h);
		// Set the field of view
		ld.Fov[eye] = hmdDesc.DefaultEyeFov[eye];
		// Set the pose matrix
		ld.RenderPose[eye] = eyeRenderPose[eye];
	}

	ld.SensorSampleTime = sensorSampleTime;

	ovrLayerHeader* layers = &ld.Header;
	// Submit the frame to the Oculus compositor
	// which will display the frame in the Oculus headset
	ovrResult result = ovr_SubmitFrame(*session, frameIndex, nullptr, &layers, 1);

	if (!OVR_SUCCESS(result)) {
		ovr_GetLastErrorInfo(&errInf);
		std::cout << "ERROR: failed to submit frame " << errInf.ErrorString << std::endl;
		end = true;
	}

	if (result == ovrSuccess && !isVisible) {
		std::cout << "The application is now shown in the headset." << std::endl;
	}
	isVisible = (result == ovrSuccess);

	// Copy the frame to the mirror buffer
	// which will be drawn in the SDL2 image
	glBindFramebuffer(GL_READ_FRAMEBUFFER, mirrorFBOID);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	GLint w = winWidth;
	GLint h = winHeight;
	glBlitFramebuffer(0, h, w, 0,
		0, 0, w, h,
		GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

	// Swap the SDL2 window
	SDL_GL_SwapWindow(window);

}