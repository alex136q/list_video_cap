#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

#include <dirent.h>
#include <sys/mman.h>
#include <pthread.h>

#include <linux/futex.h>
#include <syscall.h>

#include <fcntl.h>
#include <sys/ioctl.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "sync.h"
#include "video_msg.h"
#include "queue.h"
#include "debug.h"
#include "record.h"
#include "display.h"
#include "cli.h"
#include "config.h"

