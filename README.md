Project that works in conjunction with the R Prime Robot

Maxwell Tung

// ****** Prerequisites ****** //

- Windows 64 bits
- Oculus Rift CV1
- ZED SDK (https://stereolabs.com/developer)
- Oculus SDK (https://developer.oculus.com/downloads/package/oculus-sdk-for-windows/) (1.17 or later)
- GLEW (http://glew.sourceforge.net) included in the ZED SDK dependencies folder
- SDL (http://libsdl.org/download-2.0.php)
- OpenCV (https://opencv.org/releases.html)

// ****** Building the program with CMake  ****** //

 - Clone this repo
 - Open cmake-gui and select the source and build folders
 - Generate the Visual Studio `Win64` solution
 - Build solution
 
 Note: you may have to edit CMakeLists.txt to match the location of your installed libraries.


// ****** Running the Program ****** //

Program can be run through the command line with the explicit IP address and port number 
of the target device like so:

RPrime.exe [number of targets ] [IP ADDRESS 0] [PORT NUMBER 0] ... [IP ADDRESS n] [PORT NUMBER n]

Alternatively, the default locations of the robot's targets can be used with the names Rufus and Rachel:

RPrime.exe Rufus Rachel

If no command-line arguments are supplied the program renders the video feed without broadcasting
position data.
