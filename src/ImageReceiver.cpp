#include "ImageReceiver.hpp"


ImageReceiver::ImageReceiver() {
	// Set thread variables
	thread_data[LEFT_EYE].run = true;
	thread_data[LEFT_EYE].new_frame = true;
	thread_data[RIGHT_EYE].run = true;
	thread_data[RIGHT_EYE].new_frame = true;
}

ImageReceiver::~ImageReceiver() {
	for (int eye = 0; eye < 2; eye++)
		thread_data[eye].zed_image.free();

	thread_data[LEFT_EYE].run = false;
	thread_data[RIGHT_EYE].run = false;
	runnerLeft.join();
	runnerRight.join();
}

bool ImageReceiver::getIsRussell() {
	thread_data[LEFT_EYE].hasFrame.lock(); // will hang until thread has determined cam identity
	return thread_data[LEFT_EYE].isRussell;
}

void ImageReceiver::init_images(int width, int height) {
	sl::uchar4 dark_bckgrd(44, 44, 44, 255);
	for (int eye = 0; eye < 2; ++eye) {
		// Set size and default value to texture
		thread_data[eye].zed_image.alloc(width, height, sl::MAT_TYPE_8U_C4, sl::MEM_GPU);
		thread_data[eye].zed_image.setTo(dark_bckgrd, sl::MEM_GPU);
	}
}

// ZED image grab thread
void zed_runner(ImageReceiver::ThreadData *thread_data, bool isLeft) {
	thread_data->hasFrame.lock();
	SOCKET s;
	struct sockaddr_in receiver, si_other;
	int rec_len, slen = sizeof(si_other);
	char buf[BUFLEN];
	WSADATA wsa;

	if (0 != WSAStartup(MAKEWORD(2, 2), &wsa)) {
		std::cerr << "Initialization failed. Error Code : " << WSAGetLastError() << "\n";
		return;
	}

	if (INVALID_SOCKET == (s = socket(AF_INET, SOCK_DGRAM, 0))) {
		std::cerr << "Could not create socket. Error Code : " << WSAGetLastError() << "\n";
		return;
	}

	receiver.sin_family = AF_INET;
	receiver.sin_addr.s_addr = INADDR_ANY;
	receiver.sin_port = htons(isLeft ? LPORT : RPORT);

	if (SOCKET_ERROR == bind(s, (struct sockaddr *)&receiver, sizeof(receiver))) {
		std::cerr << "Bind failed. Error Code : " << WSAGetLastError() << "\n";
		return;
	}

	// determine which camera
	memset(buf, '\0', BUFLEN);

	if (SOCKET_ERROR == (rec_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen))) {
		std::cerr << "recvfrom() failed. Error Code : " << WSAGetLastError() << "\n";
		return;
	}
	std::vector<char> temp(&buf[0], &buf[BUFLEN]);
	cv::Mat frame = cv::imdecode(cv::Mat(temp), cv::IMREAD_UNCHANGED);

	thread_data->isRussell = frame.rows < 500 ? false : true; // should be 720 for russ, 480 for royce
	thread_data->hasFrame.unlock(); // signal camera identity is known

	// Loop while the main loop is not over
	while (thread_data->run) {
		memset(buf, '\0', BUFLEN);

		if (SOCKET_ERROR == (rec_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen))) {
			std::cerr << "recvfrom() failed. Error Code : " << WSAGetLastError() << "\n";
			return;
		}

		std::vector<char> data(&buf[0], &buf[BUFLEN]);
		cv::Mat frame = cv::imdecode(cv::Mat(data), cv::IMREAD_UNCHANGED);

		cv::Mat cvAlph;
		cv::cvtColor(frame, cvAlph, cv::COLOR_RGB2RGBA);

		sl::Mat cpuMat = sl::Mat(cvAlph.cols, cvAlph.rows, sl::MAT_TYPE_8U_C4, cvAlph.data, (thread_data->isRussell ? 1280 : 640) * 4, sl::MEM_CPU);
		thread_data->mtx.lock();
		sl::ERROR_CODE e = cpuMat.copyTo(thread_data->zed_image, sl::COPY_TYPE_CPU_GPU);

		thread_data->mtx.unlock();
		thread_data->new_frame = true;
	}

	thread_data->zed_image.free();
}

void ImageReceiver::start_threads() {
	// Launch ZED grab thread
	runnerLeft = std::thread(zed_runner, &(thread_data[LEFT_EYE]), true);
	runnerRight = std::thread(zed_runner, &(thread_data[RIGHT_EYE]), false);
}

bool ImageReceiver::has_new_frame() {
	return (thread_data[LEFT_EYE].new_frame || thread_data[RIGHT_EYE].new_frame);
}

void ImageReceiver::copy_frames(cudaGraphicsResource *cimg_l, cudaGraphicsResource *cimg_r) {
	thread_data[LEFT_EYE].mtx.lock();
	thread_data[RIGHT_EYE].mtx.lock();

	cudaArray_t arrIm;
	cudaGraphicsSubResourceGetMappedArray(&arrIm, cimg_l, 0, 0);
	cudaMemcpy2DToArray(arrIm, 0, 0, thread_data[LEFT_EYE].zed_image.getPtr<sl::uchar1>(sl::MEM_GPU), thread_data[LEFT_EYE].zed_image.getStepBytes(sl::MEM_GPU), thread_data[LEFT_EYE].zed_image.getWidth() * 4, thread_data[LEFT_EYE].zed_image.getHeight(), cudaMemcpyDeviceToDevice);

	cudaGraphicsSubResourceGetMappedArray(&arrIm, cimg_r, 0, 0);
	cudaMemcpy2DToArray(arrIm, 0, 0, thread_data[RIGHT_EYE].zed_image.getPtr<sl::uchar1>(sl::MEM_GPU), thread_data[RIGHT_EYE].zed_image.getStepBytes(sl::MEM_GPU), thread_data[RIGHT_EYE].zed_image.getWidth() * 4, thread_data[RIGHT_EYE].zed_image.getHeight(), cudaMemcpyDeviceToDevice);

	thread_data[LEFT_EYE].mtx.unlock();
	thread_data[RIGHT_EYE].mtx.unlock();
	thread_data[LEFT_EYE].new_frame = false;
	thread_data[RIGHT_EYE].new_frame = false;
}