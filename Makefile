
CCFLAGS := -Wall -O2 -march=native -lpthread -lglfw -lGLEW -lGL -lGLX -lX11 -lc -lm

run: all
	./list_video_cap watch -d /dev/video0 -i 0 -f 60

run_verbose: all
	./list_video_cap watch -d /dev/video0 -i 0 -f 60 -v

run_nofpslimit: all
	./list_video_cap watch -d /dev/video0 -i 0 -f 0

all:
	$(CC) $(CCFLAGS) -o list_video_cap *.c

link: compile
	@echo [BUILD] Linking
	$(CC) $(CCFLAGS) -o list_video_cap debug.o sync.o video_msg.o queue.o display.o record.o cli.o

clean:
	@echo [BUILD] Deleting object files
	rm list_video_cap
	rm *o

compile: sync queue video_msg debug display record cli

cli: cli.c *.h
	@echo [BUILD] Building [cli] object file
	$(CC) $(CCFLAGS) -c cli.o cli.c -O2 -march=native

queue: queue.c *.h
	@echo [BUILD] Building [queue] object file
	$(CC) $(CCFLAGS) -c queue.o queue.c

sync: sync.c *.h
	@echo [BUILD] Building [sync] object file
	$(CC) $(CCFLAGS) -c sync.o sync.c

video_msg: video_msg.c *.h
	@echo [BUILD] Building [video_msg] object file
	$(CC) $(CCFLAGS) -c video_msg.o video_msg.c

debug: debug.c *.h
	@echo [BUILD] Building [debug] object file
	$(CC) $(CCFLAGS) -c debug.o debug.c

record: record.c *.h
	@echo [BUILD] Building [record] object file
	$(CC) $(CCFLAGS) -c record.o record.c

display: display.c *.h
	@echo [BUILD] Building [display] object file
	$(CC) $(CCFLAGS) -c display.o display.c

run_list:
	@echo [BUILD] Listing cameras
	./list_video_cap list | tee local_video_devices.txt

run_save:
	@echo [BUILD] Grabbing one frame
	./list_video_cap save -d /dev/video0 -i 0 -o frame_out

run_watch:
	@echo [BUILD] Grabbing stream
	./list_video_cap watch -d /dev/video0 -i 0 -f 120 -v

