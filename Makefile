
CCFLAGS := -g -Wall -O2 -march=native -lpthread -lglfw -lGLEW -lGL -lGLX -lX11 -lx264 -lavcodec -lavutil -lc -lm


all: build_list_video_cap build_h264_wrap

build_list_video_cap: build_h264_wrap
	$(CC) $(CCFLAGS) -o list_video_cap *.c h264_wrap/h264.a

build_h264_wrap:
	make -C h264_wrap

clean:
	@echo [BUILD] Deleting object files
	rm list_video_cap
	rm *o
	make -C h264_wrap clean


run: run_list_video_cap run_h264_wrap

run_h264_wrap:
	make -C h264_wrap run_h264_wrap

run_list: list_video_cap
	@echo [BUILD] Listing cameras
	./list_video_cap list | tee local_video_devices.txt

run_save: list_video_cap
	@echo [BUILD] Grabbing one frame
	./list_video_cap save -d /dev/video0 -i 0 -o frame_out

run_watch: list_video_cap
	@echo [BUILD] Grabbing stream
	./list_video_cap watch -d /dev/video0 -i 0 -f 120 -v

run_verbose: list_video_cap
	./list_video_cap watch -d /dev/video0 -i 0 -f 60 -v

run_nofpslimit: list_video_cap
	./list_video_cap watch -d /dev/video0 -i 0 -f 0

