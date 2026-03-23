#include "headers.h"

void acquire_lock(int *lock) {
  syscall(SYS_futex, lock, FUTEX_WAIT, 1, NULL);
}

void release_lock(int *lock) {
  *lock = 0;
  syscall(SYS_futex, lock, FUTEX_WAKE, 1);
}

void sleep_ms(unsigned int milliseconds) {
  struct timespec *sleep_request = malloc(sizeof(struct timespec));
  sleep_request->tv_sec = milliseconds / 1000;
  sleep_request->tv_nsec = (milliseconds % 1000) * 1000000;
  nanosleep(sleep_request, NULL);
  free(sleep_request);
}

