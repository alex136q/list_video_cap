

list_video_cap
==============

Compiling
---------

Run `make` in the folder containing the sources.

This application requires a Linux distribution and the presence of the following dynamic libraries:

- `libglfw.so` (packaged under `glfw` or `glfw-x11` or `glfw-wayland`)
- `libGL.so`, `libGLdispatch.so`, `libGLX.so` (`libglvnd`, ships with X11)
- `libX11.so`, `libxcb.so`, `libXau.so`, `libXdmcp.so` (part of X11)
- `libc.so`, `libm.so` (ubiquitous)

CLI interface
-------------

Run `./list_video_cap help` for a summary of the supported commands and their arguments. At present the help text is as follows:

    Usage:


    ./list_video_cap help
	    Shows this text.

    ./list_video_cap list
	    Lists the capabilities of available video devices.

    ./list_video_cap list -d <path>
	    Lists the capabilities of the specified device.

    ./list_video_cap list -i <integer>
	    Lists the capabilities of the given capture input for all devices.

    ./list_video_cap list -d <path> -i <integer>
	    Lists only the capabilities of the given input and device.

    ./list_video_cap save -d <path> -i <integer> -o <path>
	    Save a single frame as grayscale (Y' component).

    ./list_video_cap watch -d <path> -i <integer>
	    Opens a window displaying the camera feed.

    Other options:

    -f <FPS>
	    Limit rendering frame rate to <FPS>.

    -v
	    Enable debug messages.

    -s <width> <height>
	    Video capture frame and window size hints for the V4L2 driver.

    -R
	    Allow window to be resized by the user.

    -b
	    Show a border around the video feed.

    -m
	    Show memory dumps in debug messages.

    -t
	    Test OpenGL by rendering dummy frames.

    -F <type>
	    Overlay solid color patterns over the captured frame (can be combined):
		    -F full		Fill whole frame.
		    -F half		Fill lower half of frame.
		    -F pixel	Color top left pixel.
		    -F diag		Fill main diagonal.
		    -F horiz	Fill two lines.

Other files
-----------

`extract_yuyv_y.py <PATH>` when run will write the bytes in even positions (Y' values) from `<PATH>` into `<PATH>.luma`. These are to be interpreted as a grayscale bitmap which can further be processed; `h264_test` passes `<PATH>.luma` files through `magick` (ImageMagick) to convert them to PNG files in the `h264_test_data/` folder.

`h264_test` is meant to try out `libx264`; it generates sample YUV 4:2:2 bitmaps and uses `libx264` to build a H.264 stream of frames from them. The `nals.bin` output it generates on encoding the test frames is a raw H.264 data stream that can be played with applications like `mpv`.

