#ifndef __DADA_IPCUTIL_H
#define __DADA_IPCUTIL_H

#include <sys/types.h>

/* ************************************************************************

   utilities for creation of shared memory and operations on semaphores

   ************************************************************************ */

#define IPCUTIL_PERM 0666 /* default: read/write permissions for all */

#ifdef __cplusplus
extern "C" {
#endif

  /* allocate size bytes in shared memory with the specified flags and key.
     returns the pointer to the base address and the shmid, id */
  void* ipc_alloc (key_t key, size_t size, int flag, int* id);

  /* operate on the specified semaphore */
  int ipc_semop (int semid, short num, short incr, short flag);

#ifdef __cplusplus
	   }
#endif

#endif
