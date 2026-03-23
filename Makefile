
CCFLAGS := -Wall -O2 -march=native -lpthread -lglfw -lGLEW -lGL -lGLX -lX11 -lx264 -lc -lm

x264_test:
	$(CC) $(CCFLAGS) -o h264_test h264.c

all:
	$(CC) $(CCFLAGS) -o list_video_cap *.c

run: all
	./list_video_cap watch -d /dev/video0 -i 0 -f 60

run_verbose: all
	./list_video_cap watch -d /dev/video0 -i 0 -f 60 -v

run_nofpslimit: all
	./list_video_cap watch -d /dev/video0 -i 0 -f 0

clean:
	@echo [BUILD] Deleting object files
	rm list_video_cap
	rm *o

run_list:
	@echo [BUILD] Listing cameras
	./list_video_cap list | tee local_video_devices.txt

run_save:
	@echo [BUILD] Grabbing one frame
	./list_video_cap save -d /dev/video0 -i 0 -o frame_out

run_watch:
	@echo [BUILD] Grabbing stream
	./list_video_cap watch -d /dev/video0 -i 0 -f 120 -v

