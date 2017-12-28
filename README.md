# MediaSoup - native client

This project is a native C++ client for MediaSoup.
It is still a work in progress.

## Dependencies

1) You need libwebrtc, you can download an installer here: https://drive.google.com/drive/folders/0B398g_p42xgrbUF3VjlFNnNxb3M
2) Boost, version 1.66 or higher is needed.


## Instructions for MacOS build

Requirements: Install brew, CMake, Xcode, boost, and libwebrtc (from the link in this README).

To build: `cd` into the project; `mkdir build`, `cd build`, `cmake ..`, and then `make`.

You should get an executable file in build/example.
