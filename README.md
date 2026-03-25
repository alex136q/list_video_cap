

list_video_cap
==============

Compiling
---------

Run `make` in the folder containing the sources.

This application requires a Linux distribution running X11 and packages `ffmpeg`, `libx264`, `glfw`, `glew`.

CLI interface
-------------

Run `./list_video_cap help` for a summary of the supported commands and their arguments. At present the help text is as follows:

    Usage: ./list_video_cap <COMMAND> <ARG>...

    Commands:

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

    Flags:

    -c
	    Fallback to raw frame data channel. The absence of this flag
	    causes the application to stream H.264-encoded packets.

    -k <SIZE>
	    Size of the H.264 packets to be streamed to the graphics thread.

    -f <FPS>
	    Limit rendering frame rate to <FPS>.

    -j
	    Use planar YUV 4:2:2 encoding. Default encoding is packed YUV 4:2:2.

    -v
	    Enable debug messages.

    -b
	    Enable H.264 packet byte dumps (implies -v).

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

    -T <type>
	    Apply transforms to the frames to be rendered:
		    -T flip_v		Flip along the vertical axis (up/down).
		    -T flip_h		Flip along the horizontal axis (left/right).
		    -T gray	Convert to grayscale (skip YUV to RGBA conversion).
		    -T invert		Negate RGB color.

Other files
-----------

`extract_yuyv_y.py <PATH>` when run will write the bytes in even positions (Y' values) from `<PATH>` into `<PATH>.luma`. These are to be interpreted as a grayscale bitmap which can further be processed; `h264_test` passes `<PATH>.luma` files through `magick` (ImageMagick) to convert them to PNG files in the `h264_test_data/` folder.

`h264_test` is meant to try out `libx264`; it generates sample YUV 4:2:2 bitmaps and uses `libx264` to build a H.264 stream of frames from them. The `nals.bin` output it generates on encoding the test frames is a raw H.264 data stream that can be played with applications like `mpv` or converted to other formats with `ffmpeg`.

