#include "OculusApp.hpp"


#pragma comment(lib, "ws2_32.lib")


void message_receiver_loop(Receiver_thread_info *info) {
	SOCKET s;
	struct sockaddr_in receiver, si_other;
	int slen = sizeof(si_other);
	int recv_len;
	char buf[256];
	WSADATA wsa;

	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		std::cerr << "Failed. Error Code : " <<  WSAGetLastError() << std::endl;
		exit(EXIT_FAILURE);
	}

	//Create a socket
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET)
	{
		std::cerr << "Could not create socket : " << WSAGetLastError() << std::endl;
	}

	//Prepare the sockaddr_in structure
	receiver.sin_family = AF_INET;
	receiver.sin_addr.s_addr = INADDR_ANY;
	receiver.sin_port = htons(RECEIVING_PORT);
	int rumbles = 0;
	//Bind
	if (bind(s, (struct sockaddr *)&receiver, sizeof(receiver)) == SOCKET_ERROR)
	{
		std::cerr << "Bind failed with error code : " << WSAGetLastError() << std::endl;
		exit(EXIT_FAILURE);
	}
	while (info->run) {
		//clear the buffer by filling null, it might have previously received data
		memset(buf, '\0', 256);

		//try to receive some data, this is a blocking call
		if ((recv_len = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr *) &si_other, &slen)) == SOCKET_ERROR)
		{
			std::cerr << "recvfrom() failed with error code : " << WSAGetLastError() << std::endl;
			exit(EXIT_FAILURE);
		}
		rumbles++;
		info->mtx.lock();
		info->has_message = true;
		info->mtx.unlock();
	}
	closesocket(s);
	WSACleanup();
}

OculusApp::OculusApp(int argc, char **argv)
{
	rec_info.run = true;
	rec_info.has_message = false;
	rec_info.leftRumb = false;
	rec_info.rightRumb = false;
	receiving_thread = std::thread(message_receiver_loop, &rec_info);
	int rumbs = 0;

	// initialize oculus
	ovrResult result = ovr_Initialize(nullptr);
	if (OVR_FAILURE(result)) {
		std::cerr << "ERROR: Failed to initialize libOVR" << std::endl;
		SDL_Quit();
		exit(EXIT_FAILURE);
	}

	// Connect to the Oculus headset
	ovrGraphicsLuid luid;
	result = ovr_Create(&session, &luid);
	if (OVR_FAILURE(result)) {
		std::cerr << "ERROR: Oculus Rift not detected" << std::endl;
		ovr_Shutdown();
		SDL_Quit();
		exit(EXIT_FAILURE);
	}
	broadcaster.initialize(argc, argv, &session);
	renderer.initialize(&session, &luid);
}


OculusApp::~OculusApp()
{
	receiving_thread.join();
}

void OculusApp::run() {
	// Main loop
	while (!end) {
		// check for messages
		//	if (rec_info.has_message) {
		rec_info.mtx.lock();
		bool leftRumb = rec_info.leftRumb;
		bool rightRumb = rec_info.rightRumb;
		rec_info.has_message = false;
		rec_info.leftRumb = false;
		rec_info.rightRumb = false;
		rec_info.mtx.unlock();
		//if (leftRumb) {
		// haptic not working yet
		//	bool rightRumb = inputState.Buttons & ovrButton_A ? true : false;
		//	bool leftRumb = false;
		ovr_SetControllerVibration(session, ovrControllerType_LTouch, leftRumb ? 1.0f : 0.0f, leftRumb ? 1.0f : 0.0f);
		//std::cerr << "sent it and got " << OVR_SUCCESS(res) << " \n";
		//}
		//if (rightRumb) {
		//fprintf(stderr, "right rumbled\n");
		ovr_SetControllerVibration(session, ovrControllerType_RTouch, rightRumb ? 1.0f : 0.0f, rightRumb ? 1.0f : 0.0f);
		//}
		//}
		//else {
		//	ovr_SetControllerVibration(session, ovrControllerType_LTouch, 0.0f, 0.0f);
		//	ovr_SetControllerVibration(session, ovrControllerType_RTouch, 0.0f, 0.0f);
		//}
		renderer.render_frame();
		broadcaster.send_position();
	}

}