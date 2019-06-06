#include "ipcutil.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>

// #define _DEBUG 1

/* *************************************************************** */
/*!
  Returns a shared memory block and its shmid
*/
void* ipc_alloc (key_t key, size_t size, int flag, int* shmid)
{
  void* buf = 0;
  int id = 0;

  id = shmget (key, size, flag);
  if (id < 0) {
    fprintf (stderr, "ipc_alloc: shmget (key=%x, size=%ld, flag=%x) %s\n",
             key, size, flag, strerror(errno));
    return 0;
  }

#ifdef _DEBUG
  fprintf (stderr, "ipc_alloc: shmid=%d\n", id);
#endif

  buf = shmat (id, 0, flag);

  if (buf == (void*)-1) {
    fprintf (stderr,
	     "ipc_alloc: shmat (shmid=%d) %s\n"
	     "ipc_alloc: after shmget (key=%x, size=%ld, flag=%x)\n",
	     id, strerror(errno), key, size, flag);
    return 0;
  }

#ifdef _DEBUG
  fprintf (stderr, "ipc_alloc: shmat=%p\n", buf);
#endif

  if (shmid)
    *shmid = id;

  return buf;
}

int ipc_semop (int semid, short num, short op, short flag)
{
  struct sembuf semopbuf;

  semopbuf.sem_num = num;
  semopbuf.sem_op = op;
  semopbuf.sem_flg = flag;
 
  if (semop (semid, &semopbuf, 1) < 0) {
    if (!(flag | IPC_NOWAIT))
      perror ("ipc_semop: semop");
    return -1;
  }
  return 0;
}
