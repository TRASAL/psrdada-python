
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>

#include <time.h>

#include "config.h"
#include "ipcbuf.h"
#include "tmutil.h"
#include "ipcutil.h"

#ifdef HAVE_CUDA
#include "ipcutil_cuda.h"
#endif

// #define _DEBUG 1

/* semaphores */

// now defined in ipcbuf.h

/* process states */

#define IPCBUF_DISCON  0  /* disconnected */
#define IPCBUF_VIEWER  1  /* connected */

#define IPCBUF_WRITER  2  /* one process that writes to the buffer */
#define IPCBUF_WRITING 3  /* start-of-data flag has been raised */
#define IPCBUF_WCHANGE 4  /* next operation will change writing state */

#define IPCBUF_READER  5  /* one process that reads from the buffer */
#define IPCBUF_READING 6  /* start-of-data flag has been raised */
#define IPCBUF_RSTOP   7  /* end-of-data flag has been raised */

#define IPCBUF_VIEWING 8  /* currently viewing */
#define IPCBUF_VSTOP   9  /* end-of-data while viewer */

/* *************************************************************** */
/*!
  creates the shared memory block that may be used as an ipcsync_t struct

  \param key the shared memory key
  \param flag the flags to pass to shmget
*/
int ipcsync_get (ipcbuf_t* id, key_t key, uint64_t nbufs, int flag)
{
  size_t required = sizeof(ipcsync_t) + nbufs + sizeof(key_t) * nbufs;

  if (!id)
  {
    fprintf (stderr, "ipcsync_get: invalid ipcbuf_t*\n");
    return -1;
  }

  id->sync = ipc_alloc (key, required, flag, &(id->syncid));
  if (id->sync == 0)
  {
    fprintf (stderr, "ipcsync_get: ipc_alloc error\n");
    return -1;
  }

  if (nbufs == 0)
    nbufs = id->sync->nbufs;

  id->count = (char*) (id->sync + 1);

#ifdef _DEBUG
  fprintf (stderr, "SYNC=%p COUNT=%p\n", id->sync, id->count);
#endif

  id->shmkey = (key_t*) (id->count + nbufs);
  id->state = 0;
  id->viewbuf = 0;

  return 0;
}

int ipcbuf_get (ipcbuf_t* id, int flag, int n_readers)
{
  int retval = 0;
  ipcsync_t* sync = 0;
  uint ibuf = 0;
  unsigned iread = 0;

  if (!id)
  {
    fprintf (stderr, "ipcbuf_get: invalid ipcbuf_t*\n");
    return -1;
  }

  sync = id->sync;

#ifdef _DEBUG
  fprintf (stderr, "ipcbuf_get: semkey_connect=0x%x shmkey=0x%x\n",
                   sync->semkey_connect, id->shmkey[0]);
#endif

  /* shared memory connection semaphores */
  id->semid_connect = semget (sync->semkey_connect, IPCBUF_CONN_NSEM, flag);
  if (id->semid_connect < 0)
  {
    fprintf (stderr, "ipcbuf_get: semget(0x%x, %d, 0x%x): %s\n",
                     sync->semkey_connect, IPCBUF_CONN_NSEM, flag, strerror(errno));
    retval = -1;
  }

#ifdef _DEBUG
  fprintf (stderr, "ipcbuf_get: semid=%d\n", id->semid_connect);
#endif

  /* shared memory data semphores */
  id->semid_data = (int *) malloc(sizeof(int) * sync->n_readers);
  assert(id->semid_data != 0);
  for (iread=0; iread < sync->n_readers; iread++)
  {
    id->semid_data[iread] = semget (sync->semkey_data[iread], IPCBUF_DATA_NSEM, flag);
    if (id->semid_data[iread] < 0)
    {
      fprintf (stderr, "ipcbuf_get: semget(0x%x, %d, 0x%x): %s\n",
                       sync->semkey_data[iread], IPCBUF_DATA_NSEM, flag, strerror(errno));
      retval = -1;
    }
#ifdef _DEBUG
    fprintf (stderr, "ipcbuf_get: semid_data[%d]=%d\n", iread, id->semid_data[iread]);
#endif
  }

  id->buffer = (char**) malloc (sizeof(char*) * sync->nbufs);
  assert (id->buffer != 0);
  id->shm_addr = (void**) malloc (sizeof(void*) * sync->nbufs);
  assert (id->shm_addr != 0);
  id->shmid = (int*) malloc (sizeof(int) * sync->nbufs);
  assert (id->shmid != 0);

  for (ibuf=0; ibuf < sync->nbufs; ibuf++)
  {
#ifdef HAVE_CUDA
    if (sync->on_device_id >= 0)
    {
      id->buffer[ibuf] = ipc_alloc_cuda (id->shmkey[ibuf], sync->bufsz,
                                         flag, id->shmid + ibuf, &(id->shm_addr[ibuf]),
                                         sync->on_device_id);
    }
    else
#endif
    {
      id->buffer[ibuf] = ipc_alloc (id->shmkey[ibuf], sync->bufsz, 
            flag, id->shmid + ibuf);
      id->shm_addr[ibuf] = id->buffer[ibuf];
    }

    if ( id->buffer[ibuf] == 0 )
    {
      fprintf (stderr, "ipcbuf_get: ipc_alloc buffer[%u] %s\n",
                       ibuf, strerror(errno));
      retval = -1;
      break;
    }
  }

  return retval;
}


/* *************************************************************** */
/*!
  Creates a new ring buffer in shared memory

  \return pointer to a new ipcbuf_t ring buffer struct
  \param nbufs
  \param bufsz
  \param key
*/

/* start with some random key for all of the pieces */
static int key_increment  = 0x00010000;

int ipcbuf_create (ipcbuf_t* id, key_t key, uint64_t nbufs, uint64_t bufsz, unsigned n_readers)
{
  return ipcbuf_create_work (id, key, nbufs, bufsz, n_readers, -1); 
}

int ipcbuf_create_work (ipcbuf_t* id, key_t key, uint64_t nbufs, uint64_t bufsz, unsigned n_readers, int device_id)
{
  uint64_t ibuf = 0;
  uint64_t iread = 0;
  int flag = IPCUTIL_PERM | IPC_CREAT | IPC_EXCL;

#ifdef _DEBUG
  fprintf (stderr, "ipcbuf_create: key=%d nbufs=%"PRIu64" bufsz=%"PRIu64" n_readers=%d\n",
                   key, nbufs, bufsz, n_readers);
#endif

  if (ipcsync_get (id, key, nbufs, flag) < 0)
  {
    fprintf (stderr, "ipcbuf_create: ipcsync_get error\n");
    return -1;
  }

  id->sync->nbufs     = nbufs;
  id->sync->bufsz     = bufsz;
  id->sync->n_readers = n_readers;
#ifdef HAVE_CUDA
  id->sync->on_device_id = device_id;
#else
  id->sync->on_device_id = -1;
#endif

  for (ibuf = 0; ibuf < IPCBUF_XFERS; ibuf++)
  {
    id->sync->s_buf  [ibuf] = 0;
    id->sync->s_byte [ibuf] = 0;
    id->sync->e_buf  [ibuf] = 0;
    id->sync->e_byte [ibuf] = 0;
    id->sync->eod    [ibuf] = 1;
  }

  // set semkey for access control to shared memory
  key += key_increment;
  id->sync->semkey_connect = key;

  // set semkey for each reader
  for (iread = 0; iread < IPCBUF_READERS; iread++)
  {
    key += key_increment;
    id->sync->semkey_data[iread] = key;
  }

  for (ibuf = 0; ibuf < nbufs; ibuf++)
  {
    id->count[ibuf] = 0;
    key += key_increment;
    id->shmkey[ibuf] = key;
  }

  id->sync->w_buf = 0;
  id->sync->w_xfer = 0;
  id->sync->w_state = IPCBUF_DISCON;

  for (iread = 0; iread < IPCBUF_READERS; iread++)
  {
    id->sync->r_bufs[iread] = 0;
    id->sync->r_xfers[iread] = 0;
    id->sync->r_states[iread] = IPCBUF_DISCON;
  }

  id->buffer      = 0;
  id->viewbuf     = 0;
  id->xfer        = 0;
  id->soclock_buf = 0;
  id->iread       = -1;

  if (ipcbuf_get (id, flag, n_readers) < 0)
  {
    fprintf (stderr, "ipcbuf_create: ipcbuf_get error\n");
    return -1;
  }

#ifdef _DEBUG
  fprintf (stderr, "ipcbuf_create: syncid=%d semid=%d\n",
                   id->syncid, id->semid_connect);
#endif

  /* ready to be locked by writer and reader processes */
  if (ipc_semop (id->semid_connect, IPCBUF_WRITE, 1, 0) < 0)
  {
    fprintf (stderr, "ipcbuf_create: error incrementing IPCBUF_WRITE\n");
    return -1;
  }

  if (ipc_semop (id->semid_connect, IPCBUF_READ, id->sync->n_readers, 0) < 0)
  {
    fprintf (stderr, "ipcbuf_create: error incrementing IPCBUF_READ\n");
    return -1;
  }

  /* ready for writer to decrement when it needs to set SOD/EOD */
  for (iread = 0; iread < n_readers; iread++)
  {
    if (ipc_semop (id->semid_data[iread], IPCBUF_SODACK, IPCBUF_XFERS, 0) < 0)
    {
      fprintf (stderr, "ipcbuf_create: error incrementing IPCBUF_SODACK for reader %lu\n", iread);
      return -1;
    }
    if (ipc_semop (id->semid_data[iread], IPCBUF_EODACK, IPCBUF_XFERS, 0) < 0)
    {
      fprintf (stderr, "ipcbuf_create: error incrementing IPCBUF_EODACK for reader %lu\n", iread);
      return -1;
    }
    if (ipc_semop (id->semid_data[iread], IPCBUF_READER_CONN, 1, 0) < 0)
    {
      fprintf (stderr, "ipcbuf_create: error incrementing IPCBUF_READER_CONN for reader %lu\n", iread);
      return -1;
    }
  }

  id->state = IPCBUF_VIEWER;
  return 0;
}

int ipcbuf_connect (ipcbuf_t* id, key_t key)
{
  int flag = IPCUTIL_PERM;

  if (ipcsync_get (id, key, 0, flag) < 0)
  {
    fprintf (stderr, "ipcbuf_connect: ipcsync_get error\n");
    return -1;
  }

#ifdef _DEBUG
  fprintf (stderr, "ipcbuf_connect: key=0x%x nbufs=%"PRIu64" bufsz=%"PRIu64"\n",
                    key, id->sync->nbufs, id->sync->bufsz);
#endif

  id->buffer = 0;

  // we are connecting, so dont create (-1)
  if (ipcbuf_get (id, flag, -1) < 0)
  {
    fprintf (stderr, "ipcbuf_connect: ipcbuf_get error\n");
    return -1;
  }

#ifdef _DEBUG
  fprintf (stderr, "ipcbuf_connect: syncid=%d semid_connect=%d\n",
                   id->syncid, id->semid_connect);
#endif

  id->state = IPCBUF_VIEWER;
  return 0;
}


int ipcbuf_disconnect (ipcbuf_t* id)
{
  uint64_t ibuf = 0;

#ifdef _DEBUG
  fprintf (stderr, "ipcbuf_disconnect: iread=%d\n", id->iread);
#endif

  if (!id)
  {
    fprintf (stderr, "ipcbuf_disconnect: invalid ipcbuf_t\n");
    return -1;
  }

  for (ibuf = 0; ibuf < id->sync->nbufs; ibuf++)
  {
#ifdef HAVE_CUDA
    if (id->sync->on_device_id >= 0)
    {
      if (id->buffer[ibuf])
      {
        if (ipc_disconnect_cuda (id->buffer[ibuf])< 0)  
          fprintf (stderr, "ipc_disconnect_cuda failed on buffer[%lu]\n", ibuf);
      }
      if (id->shm_addr[ibuf] && shmdt (id->shm_addr[ibuf]) < 0)
      {
        fprintf (stderr, "ipcbuf_disconnect: shmdt(shm_addr[%lu]) failed\n", ibuf);
        perror ("ipcbuf_disconnect: shmdt(buffer)");
      }
    } 
    else
#endif
    {
      if (id->buffer[ibuf] && shmdt (id->buffer[ibuf]) < 0)
        perror ("ipcbuf_disconnect: shmdt(buffer)");
    }
  }

  if (id->buffer) free (id->buffer); id->buffer = 0;
  if (id->shm_addr) free (id->shm_addr); id->shm_addr = 0;
  if (id->shmid) free (id->shmid); id->shmid = 0;
  if (id->semid_data) free (id->semid_data); id->semid_data = 0;

  if (id->sync && shmdt (id->sync) < 0)
    perror ("ipcbuf_disconnect: shmdt(sync)");

  id->sync = 0;

  id->state = IPCBUF_DISCON; 
  id->iread = -1;

  return 0;
}

int ipcbuf_destroy (ipcbuf_t* id)
{
  uint64_t ibuf = 0;
  int iread = 0;

  if (!id)
  {
    fprintf (stderr, "ipcbuf_destroy: invalid ipcbuf_t\n");
    return -1;
  }

#ifdef _DEBUG
  fprintf (stderr, "ipcbuf_destroy: semid_connect=%d\n", id->semid_connect);
#endif

  if (id->semid_connect>-1 && semctl (id->semid_connect, 0, IPC_RMID) < 0)
    perror ("ipcbuf_destroy: semctl");
  id->semid_connect = -1;

  for (iread = 0; iread < id->sync->n_readers; iread++)
  {
    if (id->semid_data[iread]>-1 && semctl (id->semid_data[iread], 0, IPC_RMID) < 0)
        perror ("ipcbuf_destroy: semctl");
    id->semid_data[iread] = -1;
  }

  for (ibuf = 0; ibuf < id->sync->nbufs; ibuf++)
  {
#ifdef _DEBUG
    fprintf (stderr, "ipcbuf_destroy: id[%"PRIu64"]=%x\n",
             ibuf, id->shmid[ibuf]);
#endif

#ifdef HAVE_CUDA
    if (id->sync->on_device_id >= 0)
      ipc_dealloc_cuda (id->buffer[ibuf], id->sync->on_device_id);
#endif
    if (id->buffer)
      id->buffer[ibuf] = 0;

    if (id->shmid[ibuf]>-1 && shmctl (id->shmid[ibuf], IPC_RMID, 0) < 0)
      perror ("ipcbuf_destroy: buf shmctl");

  }

  if (id->buffer) free (id->buffer); id->buffer = 0;
  if (id->shmid) free (id->shmid); id->shmid = 0;
  if (id->semid_data) free (id->semid_data); id->semid_data = 0;

#ifdef _DEBUG
  fprintf (stderr, "ipcbuf_destroy: syncid=%d\n", id->syncid);
#endif

  if (id->syncid>-1 && shmctl (id->syncid, IPC_RMID, 0) < 0)
    perror ("ipcbuf_destroy: sync shmctl");

  id->sync = 0;
  id->syncid = -1;

  return 0;
}

/*! Lock this process in as the designated writer */
int ipcbuf_lock_write (ipcbuf_t* id)
{
  if (id->state != IPCBUF_VIEWER)
  {
    fprintf (stderr, "ipcbuf_lock_write: not connected\n");
    return -1;
  }

#ifdef _DEBUG
  fprintf (stderr, "ipcbuf_lock_write: decrement WRITE=%d addr=%x\n",
                   semctl (id->semid_connect, IPCBUF_WRITE, GETVAL), id);
#endif

  /* decrement the write semaphore (only one can) */
  if (ipc_semop (id->semid_connect, IPCBUF_WRITE, -1, SEM_UNDO) < 0)
  {
    fprintf (stderr, "ipcbuf_lock_write: error decrement IPCBUF_WRITE\n");
    return -1;
  }

  /* WCHANGE is a special state that means the process will change into the
     WRITING state on the first call to get_next_write */

  if (id->sync->w_state == 0)
    id->state = IPCBUF_WCHANGE;
  else
    id->state = IPCBUF_WRITING;

  id->xfer = id->sync->w_xfer % IPCBUF_XFERS;

  return 0;
}

int ipcbuf_unlock_write (ipcbuf_t* id)
{
  if (!ipcbuf_is_writer (id))
  {
    fprintf (stderr, "ipcbuf_unlock_write: state != WRITER\n");
    return -1;
  }

#ifdef _DEBUG
  fprintf (stderr, "ipcbuf_unlock_write: increment WRITE=%d\n",
                   semctl (id->semid_connect, IPCBUF_WRITE, GETVAL));
#endif

  if (ipc_semop (id->semid_connect, IPCBUF_WRITE, 1, SEM_UNDO) < 0)
  {
    fprintf (stderr, "ipcbuf_unlock_write: error increment IPCBUF_WRITE\n");
    return -1;
  }

  id->state = IPCBUF_VIEWER;

  return 0;
}

char ipcbuf_is_writing (ipcbuf_t* id)
{
  return id->state == IPCBUF_WRITING;
}
 

int ipcbuf_enable_eod (ipcbuf_t* id)
{
  /* must be the designated writer */
  if (id->state != IPCBUF_WRITING)
  {
    fprintf (stderr, "ipcbuf_enable_eod: not writing\n");
    return -1;
  }

  id->state = IPCBUF_WCHANGE;

  return 0;
}

int ipcbuf_disable_sod (ipcbuf_t* id)
{
  /* must be the designated writer */
  if (id->state != IPCBUF_WCHANGE)
  {
    fprintf (stderr, "ipcbuf_disable_sod: not able to change writing state\n");
    return -1;
  }

  id->state = IPCBUF_WRITER;

  return 0;
}

uint64_t ipcbuf_get_sod_minbuf (ipcbuf_t* id)
{
  ipcsync_t* sync = id->sync;

  /* Since we may have multiple transfers, the minimum sod will be relative
   * to the first buffer we clocked data onto */
  uint64_t new_bufs_written = sync->w_buf - id->soclock_buf;

  if (new_bufs_written < sync->nbufs) 
    return id->soclock_buf;
  else
    return sync->w_buf - sync->nbufs + 1;
    
}

  /* start buf should be the buffer to begin on, and should be aware 
   * of previously filled bufs, filled within the same observation
   * or from a previous observation */

int ipcbuf_enable_sod (ipcbuf_t* id, uint64_t start_buf, uint64_t start_byte)
{
  ipcsync_t* sync = id->sync;
  uint64_t new_bufs = 0;
  uint64_t bufnum = 0;
  int iread = 0;

  /* must be the designated writer */
  if (id->state != IPCBUF_WRITER && id->state != IPCBUF_WCHANGE)
  {
    fprintf (stderr, "ipcbuf_enable_sod: not writer state=%d\n", id->state);
    return -1;
  }

#ifdef _DEBUG
  fprintf (stderr, "ipcbuf_enable_sod: start buf=%"PRIu64" w_buf=%"PRIu64"\n",
                   start_buf, sync->w_buf);
#endif

  /* start_buf must be less than or equal to the number of buffers written */
  if (start_buf > sync->w_buf)
  {
    fprintf (stderr,
       "ipcbuf_enable_sod: start_buf=%"PRIu64" > w_buf=%"PRIu64"\n",
       start_buf, sync->w_buf);
    return -1;
  }

  if (start_buf < ipcbuf_get_sod_minbuf (id))
  {
    fprintf (stderr,
       "ipcbuf_enable_sod: start_buf=%"PRIu64" < start_min=%"PRIu64"\n",
       start_buf, ipcbuf_get_sod_minbuf (id));
    return -1;
  }

  /* start_byte must be less than or equal to the size of the buffer */
  if (start_byte > sync->bufsz)
  {
    fprintf (stderr,
       "ipcbuf_enable_sod: start_byte=%"PRIu64" > bufsz=%"PRIu64"\n",
       start_byte, sync->bufsz);
    return -1;
  }

  for (iread = 0; iread < sync->n_readers; iread++)
  {

#ifdef _DEBUG
    fprintf (stderr, "ipcbuf_enable_sod: decrement SODACK[%d]=%d\n",
                     iread, semctl (id->semid_data[iread], IPCBUF_SODACK, GETVAL));
#endif

    /* decrement the start-of-data acknowlegement semaphore */
    if (ipc_semop (id->semid_data[iread], IPCBUF_SODACK, -1, 0) < 0)
    {
      fprintf (stderr, "ipcbuf_enable_sod: error decrement SODACK[%d]\n", iread);
      return -1;
    }
  }
  id->xfer = sync->w_xfer % IPCBUF_XFERS;

  sync->s_buf  [id->xfer] = start_buf;
  sync->s_byte [id->xfer] = start_byte;

  /* changed by AJ to fix a bug where a reader is still reading this xfer
   * and the writer wants to start writing to it... */
  if (sync->w_buf == 0) 
    sync->eod [id->xfer] = 0;

#ifdef _DEBUG
  fprintf (stderr, "ipcbuf_enable_sod: xfer=%"PRIu64
	                 " start buf=%"PRIu64" byte=%"PRIu64"\n", sync->w_xfer,
                   sync->s_buf[id->xfer], sync->s_byte[id->xfer]);
#endif

#ifdef _DEBUG
  fprintf (stderr, "ipcbuf_enable_sod: w_buf=%"PRIu64"\n", sync->w_buf);
#endif

  for (new_bufs = sync->s_buf[id->xfer]; new_bufs < sync->w_buf; new_bufs++)
  {
    bufnum = new_bufs % sync->nbufs;
    id->count[bufnum] ++;
#ifdef _DEBUG
    fprintf (stderr, "ipcbuf_enable_sod: count[%"PRIu64"]=%u\n",
                      bufnum, id->count[bufnum]);
#endif
  }

  new_bufs = sync->w_buf - sync->s_buf[id->xfer];

#ifdef _DEBUG
  fprintf (stderr, "ipcbuf_enable_sod: new_bufs=%"PRIu64"\n", new_bufs);
#endif

  id->state = IPCBUF_WRITING;
  id->sync->w_state = IPCBUF_WRITING;

  for (iread = 0; iread < sync->n_readers; iread++)
  {

#ifdef _DEBUG
    fprintf (stderr, "ipcbuf_enable_sod: increment FULL[%d]=%d by %"PRIu64"\n",
                      iread, 
                      semctl (id->semid_data[iread], IPCBUF_FULL, GETVAL), new_bufs);
#endif

    /* increment the buffers written semaphore */
    if (new_bufs && ipc_semop (id->semid_data[iread], IPCBUF_FULL, new_bufs, 0) < 0)
    {
      fprintf (stderr, "ipcbuf_enable_sod: error increment FULL\n");
      return -1;
    }
  }

  return 0;
}

char ipcbuf_is_writer (ipcbuf_t* id)
{
  int who = id->state;
  return who==IPCBUF_WRITER || who==IPCBUF_WCHANGE || who==IPCBUF_WRITING;
}

char* ipcbuf_get_next_write (ipcbuf_t* id)
{
  int iread = 0;
  uint64_t bufnum = 0;
  ipcsync_t* sync = id->sync;

  /* must be the designated writer */
  if (!ipcbuf_is_writer(id))
  {
    fprintf (stderr, "ipcbuf_get_next_write: process is not writer\n");
    return NULL;
  }

  if (id->state == IPCBUF_WCHANGE)
  {
#ifdef _DEBUG
  fprintf (stderr, "ipcbuf_get_next_write: WCHANGE->WRITING enable_sod"
	                 " w_buf=%"PRIu64"\n", id->sync->w_buf);
#endif

    if (ipcbuf_enable_sod (id, id->sync->w_buf, 0) < 0)
    {
      fprintf (stderr, "ipcbuf_get_next_write: ipcbuf_enable_sod error\n");
      return NULL;
    }
  }

  bufnum = sync->w_buf % sync->nbufs;

  while (id->count[bufnum])
  {
#ifdef _DEBUG
    fprintf (stderr, "ipcbuf_get_next_write: count[%"PRIu64"]=%u\n",
                     bufnum, id->count[bufnum]);
#endif
    /* decrement the buffers read semaphore */
    for (iread = 0; iread < sync->n_readers; iread++ )
    {
#ifdef _DEBUG
      fprintf (stderr, "ipcbuf_get_next_write: decrement CLEAR=%d\n",
                       semctl (id->semid_data[iread], IPCBUF_CLEAR, GETVAL));
#endif
      if (ipc_semop (id->semid_data[iread], IPCBUF_CLEAR, -1, 0) < 0)
      {
        fprintf (stderr, "ipcbuf_get_next_write: error decrement CLEAR\n");
        return NULL;
      }
    }

    id->count[bufnum] --;
  }

  return id->buffer[bufnum];
}

/* memset the contents of the next write buffer to zero, after it has been marked cleared */
int ipcbuf_zero_next_write (ipcbuf_t *id)
{
  ipcsync_t* sync = id->sync;

  /* must be the designated writer */
  if (!ipcbuf_is_writer(id))
  {
    fprintf (stderr, "ipcbuf_get_next_write: process is not writer\n");
    return -1;
  }

  // get the next buffer to be written
  uint64_t next_buf = (sync->w_buf + 1) % sync->nbufs;

  char have_cleared = 0;
  unsigned iread;
  while (!have_cleared)
  {
    have_cleared = 1;
    // check that each reader has 1 clear buffer at least
    for (iread = 0; iread < sync->n_readers; iread++ )
    {
      if (semctl (id->semid_data[iread], IPCBUF_CLEAR, GETVAL) == 0)
        have_cleared = 0;
    }
    if (!have_cleared)
    {
      float_sleep(0.01);
    }
  }

  // zap bufnum
#ifdef HAVE_CUDA
  if (id->sync->on_device_id >= 0)
    ipc_zero_buffer_cuda (id->buffer[next_buf], id->sync->bufsz);
  else
#endif
    bzero (id->buffer[next_buf], id->sync->bufsz);
  return 0;
}

int ipcbuf_mark_filled (ipcbuf_t* id, uint64_t nbytes)
{
  ipcsync_t* sync = 0;
  uint64_t bufnum = 0;
  int iread = 0;

  /* must be the designated writer */
  if (!ipcbuf_is_writer(id))
  {
    fprintf (stderr, "ipcbuf_mark_filled: process is not writer\n");
    return -1;
  }

  /* increment the buffers written semaphore only if WRITING */
  if (id->state == IPCBUF_WRITER)
  {
    id->sync->w_buf ++;
    return 0;
  }

  sync = id->sync;

  if (id->state == IPCBUF_WCHANGE || nbytes < sync->bufsz)
  {
#ifdef _DEBUG
    if (id->state == IPCBUF_WCHANGE)
      fprintf (stderr, "ipcbuf_mark_filled: end xfer #%"PRIu64"->%"PRIu64"\n",
                       sync->w_xfer, id->xfer);
#endif

    for (iread = 0; iread < sync->n_readers; iread++)
    {
#ifdef _DEBUG
      fprintf (stderr, "ipcbuf_mark_filled: decrement EODACK=%d\n",
                       semctl (id->semid_data[iread], IPCBUF_EODACK, GETVAL));
#endif
    
      if (ipc_semop (id->semid_data[iread], IPCBUF_EODACK, -1, 0) < 0)
      {
        fprintf (stderr, "ipcbuf_mark_filled: error decrementing EODACK\n");
        return -1;
      }
    }

    sync->e_buf  [id->xfer] = sync->w_buf;
    sync->e_byte [id->xfer] = nbytes;
    sync->eod    [id->xfer] = 1;

#ifdef _DEBUG
      fprintf (stderr, "ipcbuf_mark_filled:"
                       " end buf=%"PRIu64" byte=%"PRIu64"\n",
                       sync->e_buf[id->xfer], sync->e_byte[id->xfer]);
#endif

    sync->w_xfer++;
    id->xfer = sync->w_xfer % IPCBUF_XFERS;

    id->state = IPCBUF_WRITER;
    id->sync->w_state = 0;
  }

  bufnum = sync->w_buf % sync->nbufs;

  id->count[bufnum] ++;
  sync->w_buf ++;

#ifdef _DEBUG
  fprintf (stderr, "ipcbuf_mark_filled: count[%"PRIu64"]=%u w_buf=%"PRIu64"\n",
                   bufnum, id->count[bufnum], sync->w_buf);
#endif

  for (iread = 0; iread < sync->n_readers; iread++)
  {
#ifdef _DEBUG
    fprintf (stderr, "ipcbuf_mark_filled: increment FULL=%d\n",
                     semctl (id->semid_data[iread], IPCBUF_FULL, GETVAL));
#endif
    if (ipc_semop (id->semid_data[iread], IPCBUF_FULL, 1, 0) < 0)
    {
      fprintf (stderr, "ipcbuf_mark_filled: error increment FULL\n");
      return -1;
    }
  }
  
  return 0;
}

/*! Lock this process in as the designated reader */
int ipcbuf_lock_read (ipcbuf_t* id)
{
  int iread = 0;
  if (id->state != IPCBUF_VIEWER)
  {
    fprintf (stderr, "ipcbuf_lock_read: not connected\n");
    return -1;
  }

  if (id->iread != -1)
  {
    fprintf (stderr, "ipcbuf_lock_read: iread initialized unexpectedly\n");
    return -1;
  }

#ifdef _DEBUG
  fprintf (stderr, "ipcbuf_lock_read: decrement READ=%d\n",
                   semctl (id->semid_connect, IPCBUF_READ, GETVAL));
#endif
  /* decrement the read semaphore */
  if (ipc_semop (id->semid_connect, IPCBUF_READ, -1, SEM_UNDO) < 0)
  {
    fprintf (stderr, "ipcbuf_lock_read: error decrement READ\n");
    return -1;
  }

#ifdef _DEBUG
  fprintf (stderr, "ipcbuf_lock_read: reader status locked\n");
#endif

  /* determine the reader index based on reader state */
#ifdef _DEBUG
  fprintf (stderr, "ipcbuf_lock_read: id->iread=%d\n", id->iread);
#endif

  // order the reader's based on xfer number
  uint64_t xfers_val [IPCBUF_READERS];
  unsigned xfers_pos [IPCBUF_READERS];
  unsigned ipos = 0;
  for (ipos=0; ipos < id->sync->n_readers; ipos++)
  {
    xfers_val[ipos] = UINT64_MAX;
    for (iread = 0; iread < id->sync->n_readers; iread++)
    {
      if (id->sync->r_bufs[iread] < xfers_val[ipos])
      {
        char used = 0;
        unsigned ipos2;
        for (ipos2=0; ipos2<ipos; ipos2++)
          if (xfers_pos[ipos2] == iread)
            used=1;
        if (!used)
        {
          xfers_val[ipos] = id->sync->r_bufs[iread];
          xfers_pos[ipos] = iread;
        }
      }
    }
  }

  for (iread = 0; ((id->iread == -1) && (iread < id->sync->n_readers)); iread++)
  {
    unsigned oldest_iread = xfers_pos[iread];
#ifdef _DEBUG
    fprintf (stderr, "ipcbuf_lock_read: BEFORE decrement [%d] READER_CONN=%d\n",
                      oldest_iread, semctl (id->semid_data[oldest_iread], IPCBUF_READER_CONN, GETVAL));
#endif
    //  try to decrement the reader connected semaphore for this reader
    if (ipc_semop (id->semid_data[oldest_iread], IPCBUF_READER_CONN, -1, IPC_NOWAIT | SEM_UNDO) < 0)
    {
      if ( errno == EAGAIN )
      {
#ifdef _DEBUG
        fprintf (stderr, "ipcbuf_lock_read: skipping oldest_read=%d\n", oldest_iread);
#endif
      }
      else
      {
        fprintf (stderr, "ipcbuf_lock_read: error decrement READER_CONN\n");
        return -1;
      }
    }
    // we did decrement the READER_CONN, assign the iread
    else
    {
#ifdef _DEBUG
      fprintf (stderr, "ipcbuf_lock_read: assigning id->iread=%d\n", oldest_iread);
#endif
      id->iread = oldest_iread;
    }
  }
  if (id->iread == -1)
  {
    fprintf (stderr, "ipcbuf_lock_read: error could not find available read index\n");
    return -1;
  }

  // To facilitate a reader connecting to an XFER that already has 
  // start of data raised.
#ifdef _DEBUG
  fprintf (stderr, "ipcbuf_lock_read: r_states[%d]=%d\n", id->iread, id->sync->r_states[id->iread]);
#endif
  if (id->sync->r_states[id->iread] == IPCBUF_DISCON)
    id->state = IPCBUF_READER;
  else
    id->state = IPCBUF_READING;

  id->xfer = id->sync->r_xfers[id->iread] % IPCBUF_XFERS;

#ifdef _DEBUG
  fprintf (stderr, "ipcbuf_lock_read: xfer=%"PRIu64
           " start buf=%"PRIu64" byte=%"PRIu64"\n", id->sync->r_xfers[id->iread],
           id->sync->s_buf[id->xfer], id->sync->s_byte[id->xfer]);
#endif

  return 0;
}

char ipcbuf_is_reader (ipcbuf_t* id)
{
  int who = id->state;
  return who==IPCBUF_READER || who==IPCBUF_READING || who==IPCBUF_RSTOP;
}

int ipcbuf_unlock_read (ipcbuf_t* id)
{
  if (!ipcbuf_is_reader(id))
  {
    fprintf (stderr, "ipcbuf_unlock_read: state != READER\n");
    return -1;
  }

  if ((id->iread < 0) || (id->iread >= id->sync->n_readers))
  {
    fprintf (stderr, "ipcbuf_lock_read: iread not initialized\n");
    return -1;
  }
#ifdef _DEBUG
  fprintf (stderr, "ipcbuf_unlock_read[%d]: increment READER_CONN=%d\n",
                   id->iread, semctl (id->semid_data[id->iread], IPCBUF_READER_CONN, GETVAL));
#endif
  if (ipc_semop (id->semid_data[id->iread], IPCBUF_READER_CONN, 1, SEM_UNDO) < 0)
  {
    fprintf (stderr, "ipcbuf_disconnect: error increment READER_CONN\n");
    return -1;
  }

#ifdef _DEBUG
  fprintf (stderr, "ipcbuf_unlock_read[%d]: increment READ=%d\n",
                   id->iread,
                   semctl (id->semid_connect, IPCBUF_READ, GETVAL));
#endif
  if (ipc_semop (id->semid_connect, IPCBUF_READ, 1, SEM_UNDO) < 0)
  {
    fprintf (stderr, "ipcbuf_unlock_read: error increment READ\n");
    return -1;
  }

  id->state = IPCBUF_VIEWER;
#ifdef _DEBUG
  int iread = id->iread;
#endif
  id->iread = -1;

#ifdef _DEBUG
    fprintf (stderr, "ipcbuf_unlock_read[%d]: id->sync->r_states[%d]=%d\n",
                     iread, iread, id->sync->r_states[iread]);
#endif

  return 0;
}

char* ipcbuf_get_next_read_work (ipcbuf_t* id, uint64_t* bytes, int flag)
{
  int iread = -1;
  uint64_t bufnum;
  uint64_t start_byte = 0;
  ipcsync_t* sync = 0;

  if (ipcbuf_eod (id))
    return NULL;

  sync = id->sync;

  if (ipcbuf_is_reader (id))
  {
    iread = id->iread;
#ifdef _DEBUG
    fprintf (stderr, "ipcbuf_get_next_read: decrement[%d] FULL=%d\n", iread,
                     semctl (id->semid_data[iread], IPCBUF_FULL, GETVAL));
#endif

    /* decrement the buffers written semaphore */
    if (ipc_semop (id->semid_data[iread], IPCBUF_FULL, -1, flag) < 0) {
      fprintf (stderr, "ipcbuf_get_next_read: error decrement FULL\n");
      return NULL;
    }
#ifdef _DEBUG
    fprintf (stderr, "ipcbuf_get_next_read: decrement[%d] FULL worked!\n", iread);
#endif

    if (id->state == IPCBUF_READER)
    {
      id->xfer = sync->r_xfers[iread] % IPCBUF_XFERS;

#ifdef _DEBUG
      fprintf (stderr, "ipcbuf_get_next_read: xfer=%"PRIu64
	       " start buf=%"PRIu64" byte=%"PRIu64"\n", sync->r_xfers[iread],
	       sync->s_buf[id->xfer], 
	       sync->s_byte[id->xfer]);
#endif

      id->state = IPCBUF_READING;
      id->sync->r_states[iread] = IPCBUF_READING;

      sync->r_bufs[iread] = sync->s_buf[id->xfer];
      start_byte = sync->s_byte[id->xfer];

#ifdef _DEBUG
    fprintf (stderr, "ipcbuf_get_next_read: increment SODACK=%d\n",
                     semctl (id->semid_data[iread], IPCBUF_SODACK, GETVAL));
#endif

      /* increment the start-of-data acknowlegement semaphore */
      if (ipc_semop (id->semid_data[iread], IPCBUF_SODACK, 1, flag) < 0) {
        fprintf (stderr, "ipcbuf_get_next_read: error increment SODACK\n");
        return NULL;
      }

    }

    bufnum = sync->r_bufs[iread];

  }
  else
  {
#ifdef _DEBUG
    fprintf (stderr, "ipcbuf_get_next_read: not reader\n");
#endif
    // TODO - check if we should always just use id->iread = 0 for this??
    iread = 0;

    if (id->state == IPCBUF_VIEWER)
    {
#ifdef _DEBUG
      fprintf (stderr, "ipcbuf_get_next_read: start viewing xfer=%"PRIu64
               " start buf=%"PRIu64" byte=%"PRIu64"\n", sync->r_xfers[iread],
               sync->s_buf[id->xfer], sync->s_byte[id->xfer]);
#endif

      id->xfer = sync->r_xfers[iread] % IPCBUF_XFERS;
      id->state = IPCBUF_VIEWING;

      id->viewbuf = sync->s_buf[id->xfer];
      start_byte = sync->s_byte[id->xfer];

      /*
        In the following, the viewer seeks to the end of data.
        This is probably the best default behaviour for most monitors,
        which should present the most current data.

        It is also necessary for proper behaviour of the dada_hdu class.
        The header block is never closed; the xfer is always 0 and each
        new transfer is simply a step forward in the ring buffer.
        Therefore, if a viewer always takes the first buffer of the
        current transfer (as above), then it will be reading out-of-date
        header information.  The following lines correct this problem.

        Willem van Straten - 22 Oct 2008
      */
      if (sync->w_buf > id->viewbuf + 1)
      {
#ifdef _DEBUG
        fprintf (stderr, "ipcbuf_get_next_read: viewer seek to end of data\n");
#endif
        id->viewbuf = sync->w_buf - 1;
        start_byte = 0;
      }
    }

#ifdef _DEBUG
    fprintf (stderr, "ipcbuf_get_next_read: sync->w_buf=%"PRIu64" id->viewbuf=%"PRIu64"\n",
             sync->w_buf, id->viewbuf);
#endif

    /* Viewers wait until w_buf is incremented without semaphore operations */
    while (sync->w_buf <= id->viewbuf)
    {
#ifdef _DEBUG
      fprintf (stderr, "ipcbuf_get_next_read: sync->eod[%d]=%d sync->r_bufs[%d]=%"PRIu64" sync->e_buf[%d]=%"PRIu64"\n",
                        id->xfer, sync->eod[id->xfer], iread, sync->r_bufs[iread], id->xfer, sync->e_buf[id->xfer]); 
#endif

      // AJ added: sync->r_bufs[iread] to ensure that a buffer has been read by a reader
      if (sync->eod[id->xfer] && sync->r_bufs[iread] && sync->r_bufs[iread] == sync->e_buf[id->xfer])
      {
        id->state = IPCBUF_VSTOP;
        break;
      }

      float_sleep (0.1);
    }

    if (id->viewbuf + sync->nbufs < sync->w_buf)
      id->viewbuf = sync->w_buf - sync->nbufs + 1;

    bufnum = id->viewbuf;
    id->viewbuf ++;
#ifdef _DEBUG
    fprintf (stderr, "ipcbuf_get_next_read: bufnum=%"PRIu64"\n", bufnum);
#endif
  }

  bufnum %= sync->nbufs;

  if (bytes)
  {
    if (sync->eod[id->xfer] && sync->r_bufs[iread] == sync->e_buf[id->xfer])
    {
#ifdef _DEBUG
      fprintf (stderr, "ipcbuf_get_next_read xfer=%d EOD=true and r_buf="
                       "%"PRIu64" == e_buf=%"PRIu64"\n", (int)id->xfer, 
                       sync->r_bufs[iread], sync->e_buf[id->xfer]);
#endif
      *bytes = sync->e_byte[id->xfer] - start_byte;
    }
    else
      *bytes = sync->bufsz - start_byte;
  }

#ifdef _DEBUG
  fprintf (stderr, "ipcbuf_get_next_read: returning ptr=%p + %lu\n",
           (void *) (id->buffer[bufnum]), start_byte);
#endif

  return id->buffer[bufnum] + start_byte;
}


char* ipcbuf_get_next_read (ipcbuf_t* id, uint64_t* bytes)
{
  return ipcbuf_get_next_read_work (id, bytes, 0);
}

char* ipcbuf_get_next_readable (ipcbuf_t* id, uint64_t* bytes)
{
  return ipcbuf_get_next_read_work (id, bytes, SEM_UNDO);
}

uint64_t ipcbuf_tell (ipcbuf_t* id, uint64_t bufnum)
{
  ipcsync_t* sync = id->sync;

#ifdef _DEBUG
  fprintf (stderr, 
           "ipcbuf_tell: bufnum=%"PRIu64" xfer=%"PRIu64", s_buf=%"PRIu64" s_byte=%"PRIu64"\n",
           bufnum, id->xfer, sync->s_buf[id->xfer], sync->s_byte[id->xfer]);
#endif

  if (bufnum <= sync->s_buf[id->xfer])
    return 0;

  bufnum -= sync->s_buf[id->xfer];

  return bufnum*sync->bufsz - sync->s_byte[id->xfer];
}

int64_t ipcbuf_tell_write (ipcbuf_t* id)
{
  if (ipcbuf_eod (id))
    return -1;

  if (!ipcbuf_is_writer (id))
    return -1;

  return ipcbuf_tell (id, id->sync->w_buf);
}

int64_t ipcbuf_tell_read (ipcbuf_t* id)
{
  if (ipcbuf_eod (id))
    return -1;

  if (id->state == IPCBUF_READING)
    return ipcbuf_tell (id, id->sync->r_bufs[id->iread]);
  else if (id->state == IPCBUF_VIEWING)
    return ipcbuf_tell (id, id->viewbuf);
  else
    return 0;
}

int ipcbuf_mark_cleared (ipcbuf_t* id)
{
  ipcsync_t* sync = 0;
  int iread = id->iread;

  if (!id)
  {
    fprintf (stderr, "ipcbuf_mark_cleared: no ipcbuf!\n");
    return -1;
  }

  if (id->state != IPCBUF_READING)
  {
    fprintf (stderr, "ipcbuf_mark_cleared: not reading\n");
    return -1;
  }

  sync = id->sync;

#ifdef _DEBUG
  fprintf (stderr, "ipcbuf_mark_cleared: increment CLEAR=%d\n",
                   semctl (id->semid_data[iread], IPCBUF_CLEAR, GETVAL));
#endif

  /* increment the buffers cleared semaphore */
  if (ipc_semop (id->semid_data[iread], IPCBUF_CLEAR, 1, 0) < 0)
    return -1;

  if (sync->eod[id->xfer] && sync->r_bufs[iread] == sync->e_buf[id->xfer])
  {
#ifdef _DEBUG
    fprintf (stderr, "ipcbuf_mark_cleared: increment EODACK=%d; CLEAR=%d\n",
                     semctl (id->semid_data[iread], IPCBUF_EODACK, GETVAL),
                     semctl (id->semid_data[iread], IPCBUF_CLEAR, GETVAL));
#endif

    id->state = IPCBUF_RSTOP;
    id->sync->r_states[iread] = IPCBUF_DISCON;

    sync->r_xfers[iread] ++;
    id->xfer = sync->r_xfers[iread] % IPCBUF_XFERS;

    if (ipc_semop (id->semid_data[iread], IPCBUF_EODACK, 1, 0) < 0) {
      fprintf (stderr, "ipcbuf_mark_cleared: error incrementing EODACK\n");
      return -1;
    }
  }
  else
  {
#ifdef _DEBUG
    fprintf (stderr, "ipcbuf_mark_cleared: increment r_buf=%"PRIu64"\n",
             sync->r_bufs[iread]);
#endif
    sync->r_bufs[iread] ++;
  }

  return 0;
}

int ipcbuf_reset (ipcbuf_t* id)
{
  uint64_t ibuf = 0;
  int iread = 0;
  uint64_t nbufs = ipcbuf_get_nbufs (id);
  ipcsync_t* sync = id->sync;
  unsigned ix = 0;

  /* if the reader has reached end of data, reset the state */
  if (id->state == IPCBUF_RSTOP)
  {
    id->state = IPCBUF_READER;
    return 0;
  }

  /* otherwise, must be the designated writer */
  if (!ipcbuf_is_writer(id))  {
    fprintf (stderr, "ipcbuf_reset: invalid state=%d\n", id->state);
    return -1;
  }

  if (sync->w_buf == 0)
    return 0;

  for (ibuf = 0; ibuf < nbufs; ibuf++)
  {
    while (id->count[ibuf])
    {
      for (iread = 0; iread < sync->n_readers; iread++)
      {
#ifdef _DEBUG
        fprintf (stderr, "ipcbuf_reset: decrement CLEAR=%d\n",
                         semctl (id->semid_data[iread], IPCBUF_CLEAR, GETVAL));
#endif

        /* decrement the buffers cleared semaphore */
        if (ipc_semop (id->semid_data[iread], IPCBUF_CLEAR, -1, 0) < 0)
        {
          fprintf (stderr, "ipcbuf_reset: error decrementing CLEAR\n");
          return -1;
        }
      }

      id->count[ibuf] --;
    }
  }

  for (iread = 0; iread < sync->n_readers; iread++)
  {
#ifdef _DEBUG
    fprintf (stderr, "ipcbuf_reset: decrement SODACK=%d\n",
                     semctl (id->semid_data[iread], IPCBUF_SODACK, GETVAL));
#endif

    if (ipc_semop (id->semid_data[iread], IPCBUF_SODACK, -IPCBUF_XFERS, 0) < 0)
    {
      fprintf (stderr, "ipcbuf_reset: error decrementing SODACK\n");
      return -1;
    }

#ifdef _DEBUG
    fprintf (stderr, "ipcbuf_reset: decrement EODACK=%d\n",
                     semctl (id->semid_data[iread], IPCBUF_EODACK, GETVAL));
#endif

    if (ipc_semop (id->semid_data[iread], IPCBUF_EODACK, -IPCBUF_XFERS, 0) < 0)
    {
      fprintf (stderr, "ipcbuf_reset: error decrementing EODACK\n");
      return -1;
    }

    if (ipc_semop (id->semid_data[iread], IPCBUF_SODACK, IPCBUF_XFERS, 0) < 0)
    {
      fprintf (stderr, "ipcbuf_reset: error resetting SODACK\n");
      return -1;
    }

    if (ipc_semop (id->semid_data[iread], IPCBUF_EODACK, IPCBUF_XFERS, 0) < 0)
    {
      fprintf (stderr, "ipcbuf_reset: error resetting EODACK\n");
      return -1;
    }

    sync->r_bufs[iread] = 0;
    sync->r_xfers[iread] = 0;
  }
  sync->w_buf = 0;
  sync->w_xfer = 0;

  for (ix=0; ix < IPCBUF_XFERS; ix++)
    sync->eod[ix] = 1;

  return 0;
}

/* reset the buffer count and end of data flags, without prejudice */
int ipcbuf_hard_reset (ipcbuf_t* id)
{
  ipcsync_t* sync = id->sync;
  unsigned ix = 0;
  int val = 0;
  int iread = 0;

  sync->w_buf = 0;
  sync->w_xfer = 0;

  for (ix=0; ix < IPCBUF_XFERS; ix++)
    sync->eod[ix] = 1;

  for (iread = 0; iread < sync->n_readers; iread++)
  {
    sync->r_bufs[iread] = 0;
    sync->r_xfers[iread] = 0;

    if (semctl (id->semid_data[iread], IPCBUF_FULL, SETVAL, val) < 0)
    {
      perror ("ipcbuf_hard_reset: semctl (IPCBUF_FULL, SETVAL)");
      return -1;
    }

    if (semctl (id->semid_data[iread], IPCBUF_CLEAR, SETVAL, val) < 0)
    {
      perror ("ipcbuf_hard_reset: semctl (IPCBUF_FULL, SETVAL)");
      return -1;
    }
  }
  return 0;
}

int ipcbuf_lock (ipcbuf_t* id)
{
#ifdef SHM_LOCK

  uint64_t ibuf = 0;

  if (id->syncid < 0 || id->shmid == 0)
    return -1;

  if (shmctl (id->syncid, SHM_LOCK, 0) < 0)
  {
    perror ("ipcbuf_lock: shmctl (syncid, SHM_LOCK)");
    return -1;
  }

  for (ibuf = 0; ibuf < id->sync->nbufs; ibuf++)
    if (shmctl (id->shmid[ibuf], SHM_LOCK, 0) < 0)
    {
      perror ("ipcbuf_lock: shmctl (shmid, SHM_LOCK)");
      return -1;
    }

  return 0;

#else

  fprintf(stderr, "ipcbuf_lock does nothing on this platform!\n");
  return -1;

#endif

}

int ipcbuf_unlock (ipcbuf_t* id)
{
#ifdef SHM_UNLOCK

  uint64_t ibuf = 0;

  if (id->syncid < 0 || id->shmid == 0)
    return -1;

  if (shmctl (id->syncid, SHM_UNLOCK, 0) < 0)
  {
    perror ("ipcbuf_lock: shmctl (syncid, SHM_UNLOCK)");
    return -1;
  }

  for (ibuf = 0; ibuf < id->sync->nbufs; ibuf++)
    if (shmctl (id->shmid[ibuf], SHM_UNLOCK, 0) < 0)
    {
      perror ("ipcbuf_lock: shmctl (shmid, SHM_UNLOCK)");
      return -1;
    }

  return 0;

#else

  fprintf(stderr, "ipcbuf_unlock does nothing on this platform!\n");
  return -1;

#endif

}

// Initialize each buffer
int ipcbuf_page (ipcbuf_t* id)
{
  uint64_t ibuf = 0;
  if (id->syncid < 0 || id->shmid == 0)
    return -1;

  for (ibuf = 0; ibuf < id->sync->nbufs; ibuf++)
  {
#ifdef HAVE_CUDA
    if (id->sync->on_device_id >= 0)
      ipc_zero_buffer_cuda( id->buffer[ibuf], id->sync->bufsz );
    else
#endif
      bzero (id->buffer[ibuf], id->sync->bufsz); 
  }

  return 0;
}



int ipcbuf_eod (ipcbuf_t* id)
{
  if (!id)
  {
    fprintf (stderr, "ipcbuf_eod: invalid ipcbuf_t*\n");
    return -1;
  }

  return ( (id->state == IPCBUF_RSTOP) || (id->state == IPCBUF_VSTOP) );
}


int ipcbuf_sod (ipcbuf_t* id)
{
  return id->state == IPCBUF_READING || id->state == IPCBUF_WRITING;
}

// return the total bytes written for the current XFER in id
uint64_t ipcbuf_get_write_byte_xfer (ipcbuf_t* id)
{
  if (id->sync->eod[id->xfer])
    return id->sync->e_byte[id->xfer];
  else
    return ipcbuf_tell (id, id->sync->w_buf);
}

uint64_t ipcbuf_get_write_count_xfer (ipcbuf_t* id)
{
  if (id->sync->w_xfer == id->xfer)
    return id->sync->w_buf;
  else
    return id->sync->e_byte[id->xfer];
}


uint64_t ipcbuf_get_write_count (ipcbuf_t* id)
{
  return id->sync->w_buf;
}

uint64_t ipcbuf_get_write_index (ipcbuf_t* id)
{
  return id->sync->w_buf % id->sync->nbufs;
}

uint64_t ipcbuf_get_read_count (ipcbuf_t* id)
{
  if (id->iread == -1)
    return ipcbuf_get_read_count_iread(id, 0);
  else
    return ipcbuf_get_read_count_iread(id, id->iread);
}

uint64_t ipcbuf_get_read_count_iread (ipcbuf_t* id, unsigned iread)
{
  return id->sync->r_bufs[iread];
}


uint64_t ipcbuf_get_read_index (ipcbuf_t* id)
{
  return ipcbuf_get_read_count (id) % id->sync->nbufs;
}

uint64_t ipcbuf_get_nbufs (ipcbuf_t* id)
{
  return id->sync->nbufs;
}

uint64_t ipcbuf_get_bufsz (ipcbuf_t* id)
{
  return id->sync->bufsz;
}

uint64_t ipcbuf_get_nfull (ipcbuf_t* id)
{
  return ipcbuf_get_nfull_iread (id, -1);
}

uint64_t ipcbuf_get_nfull_iread (ipcbuf_t* id, int iread)
{
  if (iread >= 0)
    return semctl (id->semid_data[iread], IPCBUF_FULL, GETVAL);
  else if (id->iread == -1)
    return semctl (id->semid_data[0], IPCBUF_FULL, GETVAL);
  else
  {
    unsigned i=0;
    uint64_t max_nfull = 0;
    uint64_t nfull = 0;
    for (i = 0; i < id->sync->n_readers; i++)
    {
      nfull = semctl (id->semid_data[i], IPCBUF_FULL, GETVAL);
      if (nfull > max_nfull)
        max_nfull = nfull;
    }
    return nfull;
  }
}

uint64_t ipcbuf_get_nclear (ipcbuf_t* id)
{
  return ipcbuf_get_nclear_iread (id, -1);
}

uint64_t ipcbuf_get_nclear_iread (ipcbuf_t* id, int iread)
{
  if (iread >= 0)
    return semctl (id->semid_data[iread], IPCBUF_CLEAR, GETVAL);
  else if (id->iread == -1)
    return semctl (id->semid_data[0], IPCBUF_CLEAR, GETVAL);
  else
  {
    unsigned i=0;
    uint64_t max_nclear = 0;
    uint64_t nclear = 0;
    for (i = 0; i < id->sync->n_readers; i++)
    {
      nclear = semctl (id->semid_data[i], IPCBUF_CLEAR, GETVAL);
      if (nclear > max_nclear)
        max_nclear = nclear;
    }
    return nclear;
  }
}

uint64_t ipcbuf_get_sodack (ipcbuf_t* id)
{
  return ipcbuf_get_sodack_iread (id, -1);
}
uint64_t ipcbuf_get_sodack_iread (ipcbuf_t* id, int iread)
{
  if (iread >= 0)
    return semctl (id->semid_data[iread], IPCBUF_SODACK, GETVAL);
  else if (id->iread == -1)
    return semctl (id->semid_data[0], IPCBUF_SODACK, GETVAL);
  else
  {
    unsigned i=0;
    uint64_t max_sodack = 0;
    uint64_t sodack = 0;
    for (i = 0; i < id->sync->n_readers; i++)
    {
      sodack = semctl (id->semid_data[i], IPCBUF_SODACK, GETVAL);
      if (sodack > max_sodack)
        max_sodack = sodack;
    }
    return sodack;
  }
}

uint64_t ipcbuf_get_eodack (ipcbuf_t* id)
{
  return ipcbuf_get_eodack_iread (id, -1);
}

uint64_t ipcbuf_get_eodack_iread (ipcbuf_t* id, int iread)
{
  if (iread >= 0)
    return semctl (id->semid_data[iread], IPCBUF_EODACK, GETVAL);
  else if (id->iread == -1)
    return semctl (id->semid_data[0], IPCBUF_EODACK, GETVAL);
  else
  {
    unsigned i=0;
    uint64_t max_eodack = 0;
    uint64_t eodack = 0;
    for (i = 0; i < id->sync->n_readers; i++)
    {
      eodack = semctl (id->semid_data[i], IPCBUF_EODACK, GETVAL);
      if (eodack > max_eodack)
        max_eodack = eodack;
    }
    return eodack;
  }
}

int ipcbuf_get_nreaders (ipcbuf_t* id)
{
  return id->sync->n_readers;
}

int ipcbuf_get_reader_conn (ipcbuf_t* id)
{
  return ipcbuf_get_reader_conn_iread (id, -1);
}

int ipcbuf_get_reader_conn_iread (ipcbuf_t* id, int iread)
{
  if (iread >= 0)
    return semctl (id->semid_data[iread], IPCBUF_READER_CONN, GETVAL);
  else if (id->iread == -1)
    return semctl (id->semid_data[0], IPCBUF_READER_CONN, GETVAL);
  else
  {
    unsigned i=0;
    uint64_t min_reader_conn = 1;
    uint64_t reader_conn = 0;
    for (i = 0; i < id->sync->n_readers; i++)
    {
      reader_conn = semctl (id->semid_data[i], IPCBUF_READER_CONN, GETVAL);
      if (reader_conn < min_reader_conn)
        min_reader_conn = reader_conn;
    }
    return reader_conn;
  }
}

int ipcbuf_get_read_semaphore_count (ipcbuf_t* id)
{
  return semctl (id->semid_connect, IPCBUF_READ, GETVAL);
}

/* Sets the buffer at which clocking began.  */
uint64_t ipcbuf_set_soclock_buf (ipcbuf_t* id)
{
  if (id->sync->w_xfer > 0) 
    id->soclock_buf = id->sync->e_buf[(id->sync->w_xfer-1) % IPCBUF_XFERS] + 1;
  else
    id->soclock_buf = 0;

  return id->soclock_buf;
}

#ifdef HAVE_CUDA
int ipcbuf_get_device (ipcbuf_t* id)
{
  return id->sync->on_device_id;
}
#endif
