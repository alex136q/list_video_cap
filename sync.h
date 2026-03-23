#ifndef INCLUDE_SYNC_H
#define INCLUDE_SYNC_H

void acquire_lock(int *lock);
void release_lock(int *lock);

void sleep_ms(unsigned int milliseconds);

#endif
