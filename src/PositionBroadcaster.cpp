#include "PositionBroadcaster.hpp"


#define STATUS_FORMAT "{ \"head\": { \"orientation\": { \"roll\":%f, \"pitch\":%f, \"yaw\":%f }, \"position\": { \"x\": %f, \"y\": %f, \"z\": %f} }, \"hands\": { \"left\": { \"position\": { \"x\": %f, \"y\": %f, \"z\": %f }, \"orientation\": { \"roll\": %f, \"pitch\": %f, \"yaw\": %f } },  \"right\": { \"position\": { \"x\": %f, \"y\": %f, \"z\": %f }, \"orientation\": { \"roll\": %f, \"pitch\": %f, \"yaw\": %f }  } }, \"buttons\": { \"a\": %d, \"b\": %d, \"x\": %d, \"y\": %d, \"menu\": %d, \"leftTrig\": %f, \"rightTrig\": %f, \"leftGrip\": %f, \"rightGrip\": %f, \"left_jstick\": { \"x\": %f, \"y\": %f, \"button\": %d }, \"right_jstick\": { \"x\": %f, \"y\": %f, \"button\": %d } }}"

#define MAX_SOCKETS 4

void setup_socket(PositionBroadcaster::location loc, struct sockaddr_in& server, SOCKET *sd) {
	WSADATA w;  // Used to open Windows connection 

	if (WSAStartup(0x0101, &w) != 0) {
		std::cerr << "Could not open Windows connection.\n";
		exit(0);
	}

	// Open a datagram socket 
	*sd = socket(AF_INET, SOCK_DGRAM, 0);
	if (*sd == INVALID_SOCKET) {
		std::cerr << "Could not create socket.\n";
		WSACleanup();
		exit(0);
	}

	// Clear out server struct 
	memset((void *)&server, '\0', sizeof(struct sockaddr_in));

	// Set family and port 
	server.sin_family = AF_INET;
	server.sin_port = htons(loc.port);

	// Set server address 
	server.sin_addr.S_un.S_un_b.s_b1 = (unsigned char)loc.a1;
	server.sin_addr.S_un.S_un_b.s_b2 = (unsigned char)loc.a2;
	server.sin_addr.S_un.S_un_b.s_b3 = (unsigned char)loc.a3;
	server.sin_addr.S_un.S_un_b.s_b4 = (unsigned char)loc.a4;
}

void quat_to_euler(ovrQuatf ori, float *roll, float *pitch, float *yaw) {
	float y = ori.y; float x = ori.x; float z = ori.z; float w = ori.w;
	*roll = asin(2 * x*y + 2 * z*w);
	*pitch = atan2(2 * x*w - 2 * y*z, 1 - 2 * x*x - 2 * z*z);
	*yaw = atan2(2 * y*w - 2 * x*z, 1 - 2 * y*y - 2 * z*z);
}

// function for broadcasting position
void position_broadcast_loop(PositionBroadcaster::Broadcast_thread_info *info) {
	int server_length = sizeof(struct sockaddr_in);
	char send_buffer[SIZE];
	struct sockaddr_in servers[MAX_SOCKETS];		// Information about the server 

	SOCKET sd[MAX_SOCKETS];

	for (int i = 0; i < info->argc; i++) {
		setup_socket(info->locs[i], servers[i], &sd[i]);
	}

	while (info->run) {

		info->mtx.lock();
		ovrTrackingState ts = info->ts;
		ovrInputState inputState = info->inputState;
		info->mtx.unlock();

		// find head pose
		ovrQuatf ori = ts.HeadPose.ThePose.Orientation;
		ovrVector3f pose = ts.HeadPose.ThePose.Position;

		// convert quaternion to euler angles
		float roll, pitch, yaw;
		quat_to_euler(ori, &roll, &pitch, &yaw);

		// hand poses
		ovrVector3f handPoses[2];
		ovrQuatf handOris[2];

		handPoses[0] = ts.HandPoses[0].ThePose.Position;
		handPoses[1] = ts.HandPoses[1].ThePose.Position;
		handOris[0] = ts.HandPoses[0].ThePose.Orientation;
		handOris[1] = ts.HandPoses[1].ThePose.Orientation;

		float handroll[2], handpitch[2], handyaw[2];
		quat_to_euler(handOris[0], &handroll[0], &handpitch[0], &handyaw[0]);
		quat_to_euler(handOris[1], &handroll[1], &handpitch[1], &handyaw[1]);

		// set up string to send in JSON
		sprintf_s(send_buffer, sizeof(send_buffer), STATUS_FORMAT, \
			roll, pitch, yaw, -pose.z, -pose.x, pose.y,
			-handPoses[0].z, -handPoses[0].x, handPoses[0].y, handroll[0], handpitch[0], handyaw[0],\
			-handPoses[1].z, -handPoses[1].x, handPoses[1].y, handroll[1], handpitch[1], handyaw[1], \
			inputState.Buttons & ovrButton_A, inputState.Buttons & ovrButton_B, inputState.Buttons & ovrButton_X, inputState.Buttons & ovrButton_Y, inputState.Buttons & ovrButton_Enter,\
			inputState.IndexTrigger[0], inputState.IndexTrigger[1], inputState.HandTrigger[0], inputState.HandTrigger[1], \
			inputState.Thumbstick[0].x, inputState.Thumbstick[0].y, inputState.Buttons & ovrButton_LThumb, inputState.Thumbstick[1].x, inputState.Thumbstick[1].y, inputState.Buttons & ovrButton_RThumb);

		//fprintf(stderr, "%s\n", send_buffer); // uncomment for debugging

		for (int i = 0; i < info->argc; i++) {
			// broadcast and ensure no errors
			if (sendto(sd[i], send_buffer, (int)strlen(send_buffer), 0, (struct sockaddr *)&servers[i], server_length) == -1)
			{
				int err = WSAGetLastError();
				std::cerr << "Error tansmitting data with socket " << i << ". error code is " << err << std::endl;
				closesocket(sd[i]);
				WSACleanup();
				exit(0);
			}
		}
		Sleep(10);

	}

	for (int i = 0; i < info->argc; i++)
		closesocket(sd[i]);
	WSACleanup();
}

PositionBroadcaster::PositionBroadcaster() {
	// Initialize data for broadcast loop
	send_info.run = true;
	send_info.argc = 1;
	send_info.locs = locations;
	location rufus_loc;
	rufus_loc.a1 = 130;	rufus_loc.a2 = 64; rufus_loc.a3 = 190; rufus_loc.a4 = 55;
	rufus_loc.port = 8000;
	location rachel_loc;
	rachel_loc.a1 = 130; rachel_loc.a2 = 64; rachel_loc.a3 = 143; rachel_loc.a4 = 97;
	rachel_loc.port = 8000;

	targets.insert(std::make_pair("rachel", rachel_loc));
	targets.insert(std::make_pair("Rachel", rachel_loc));
	targets.insert(std::make_pair("rufus", rufus_loc));
	targets.insert(std::make_pair("Rufus", rufus_loc));
}

void PositionBroadcaster::initialize(int argc, char **argv, ovrSession *sess) {
	if (argc == 1) {
		std::cout << "Running program without position broadcasting.\n";
		num_locations = 0;
	} else if (argv[1][0] < 58 && argv[1][0] > 47) { // is a number
		sscanf_s(argv[1], "%d", &num_locations);

		if (num_locations > MAX_LOCATIONS) {
			std::cerr << "Too many locations asked for.\n%s", "wrong usage\n";
			exit(EXIT_FAILURE);
		} else if (num_locations > (argc - 2) / 2) {
			std::cerr << "Not enough locations provided.\n%s", "wrong usage\n";
			exit(EXIT_FAILURE);
		}

		for (int i = 0; i < num_locations; i++) {
			sscanf_s(argv[i * 2 + 2], "%d.%d.%d.%d", &locations[i].a1, &locations[i].a2, &locations[i].a3, &locations[i].a4);
			sscanf_s(argv[i * 2 + 3], "%hu", &locations[i].port);
		}

	} else {
		num_locations = argc - 1;
		if (argc > MAX_LOCATIONS + 1) {
			std::cerr << "Too many locations provided. Maximum is " << MAX_LOCATIONS << ".\n";
			exit(EXIT_FAILURE);
		}
		for (int i = 1; i < argc && i < MAX_LOCATIONS + 1; i++) {
			if (targets.count(argv[i]) > 0) {
				locations[i - 1] = targets.find(argv[i])->second;
			} 
			else {
				std::cerr << "Location '" << argv[i] << "' not recognized.\n";
				exit(EXIT_FAILURE);
			}
		}
	}
	session = sess;
	send_info.argc = num_locations;
	send_info.locs = locations;
	broadcast_thread = std::thread(position_broadcast_loop, &(send_info));
}

PositionBroadcaster::~PositionBroadcaster() {
	send_info.run = false;
	broadcast_thread.join();
}

void PositionBroadcaster::send_position() {
	// get positioning to broadcast
	ovrTrackingState ts = ovr_GetTrackingState(*session, ovr_GetTimeInSeconds(), ovrTrue);
	ovrInputState inputState;
	ovr_GetInputState(*session, ovrControllerType_Touch, &inputState);
	send_info.mtx.lock();
	send_info.ts = ts;
	send_info.inputState = inputState;
	send_info.mtx.unlock();
}