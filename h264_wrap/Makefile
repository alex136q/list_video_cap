
CCFLAGS = -g -O2 -lx264 -lavcodec -lavutil -lc -lm

h264_test: h264_lib h264_bin
	@echo Build done

h264_lib:
	$(CC) $(CCFLAGS) -c h264.o h264.c
	ar r h264.a h264.o

h264_bin:
	$(CC) $(CCFLAGS) -o h264_test h264_test.c h264.a

clean:
	rm h264_test *.o

run_h264_test: h264_test
	./h264_test

