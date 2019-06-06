#ifndef __DADA_IPCBUF_H
#define __DADA_IPCBUF_H

//#include "config.h"

/* ************************************************************************

   ipcbuf_t - a struct and associated routines for creation and management
   of a ring buffer in shared memory

   ************************************************************************ */

#include <inttypes.h> 
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IPCBUF_WRITE      0   /* semaphore locks writer status */
#define IPCBUF_READ       1   /* semaphore locks reader (+clear) status */
#define IPCBUF_CONN_NSEM  2   /* total number of connection semaphores */

#define IPCBUF_SODACK       0   /* acknowledgement of start of data */
#define IPCBUF_EODACK       1   /* acknowledgement of end of data */
#define IPCBUF_FULL         2   /* semaphore counts full buffers */
#define IPCBUF_CLEAR        3   /* semaphore counts emptied buffers */
#define IPCBUF_READER_CONN  4   /* semaphore counts emptied buffers */
#define IPCBUF_DATA_NSEM    5   /* total number of data semaphores */

#define IPCBUF_XFERS 8 /* total transfers in buffer */
#define IPCBUF_READERS 8 /* maximum number of readers */

  typedef struct {

    key_t  semkey_connect;                  /* semaphore key for connecting to shared memory */
    key_t  semkey_data  [IPCBUF_READERS];   /* semaphore keys for reading/writing shared memory */

    uint64_t nbufs;    /* the number of buffers in the ring */
    uint64_t bufsz;    /* the size of the buffers in the ring */

    uint64_t w_buf;    /* count of next buffer to write */
    int w_state;       /* the state of the writer */
    uint64_t w_xfer;   /* the current write transfer number */

    uint64_t r_bufs   [IPCBUF_READERS];   /* count of next buffer to read */
    int      r_states [IPCBUF_READERS];   /* the state of the reader */
    uint64_t r_xfers  [IPCBUF_READERS];   /* the current read transfer number */
    unsigned n_readers;                   /* number of readers */

    /* the first valid buffer when sod is raised */
    uint64_t s_buf  [IPCBUF_XFERS];

    /* the first valid byte when sod is raised */
    uint64_t s_byte [IPCBUF_XFERS];

    /* end of data flag */
    char eod        [IPCBUF_XFERS];

    /* the last valid buffer when sod is raised */
    uint64_t e_buf  [IPCBUF_XFERS];

    /* the last valid byte when sod is raised */
    uint64_t e_byte [IPCBUF_XFERS];

    /* CUDA device upon which buffers may reside, -1 for host */
    int on_device_id;

  } ipcsync_t;

  typedef struct {

    int state;             /* the state of the process: writer, reader, etc. */

    int syncid;            /* sync struct shared memory id */
    int semid_connect;     /* semaphore id for shmem connect */
    int * semid_data;      /* semaphore id for shmem data */
    int * shmid;           /* ring buffer shared memory id */

    ipcsync_t* sync;       /* pointer to sync structure in shared memory */
    char**     buffer;     /* base addresses of sub-blocks in shared memory */
    void**     shm_addr;   /* shm addresses of sub-blocks in shared memory */
    char*      count;      /* the pending xfer count in each buffer in the ring */
    key_t*     shmkey;     /* shared memory keys */

    uint64_t viewbuf;      /* count of next buffer to look at (non-reader) */

    uint64_t xfer;         /* current xfer */

    uint64_t soclock_buf;  /* buffer to which the SOD is relevant */

    int iread;             /* reader count, -1 means writer */

  } ipcbuf_t;

#define IPCBUF_INIT {0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1}

  /* ////////////////////////////////////////////////////////////////////
     
     FUNCTIONS USED TO CREATE/CONNECT/DESTROY SHARED MEMORY

     //////////////////////////////////////////////////////////////////// */

  /*! Initialize an ipcbuf_t struct, creating shm and sem */
  int ipcbuf_create (ipcbuf_t*, key_t key, uint64_t nbufs, uint64_t bufsz, unsigned num_readers);
  int ipcbuf_create_work (ipcbuf_t*, key_t key, uint64_t nbufs, uint64_t bufsz, unsigned num_readers, int device_id);

  /*! Connect to an already created ipcsync_t struct in shm */
  int ipcbuf_connect (ipcbuf_t*, key_t key);

  /*! Disconnect from a previously connected ipcsync_t struct in shm */
  int ipcbuf_disconnect (ipcbuf_t*);

  /*! Destroy the ring buffer space, semaphores, and shared memory ipcbuf_t */
  int ipcbuf_destroy (ipcbuf_t*);

  /* ////////////////////////////////////////////////////////////////////
     
     FUNCTIONS USED BY THE PROCESS WRITING TO SHARED MEMORY

     //////////////////////////////////////////////////////////////////// */

  /*! Lock this process in as the data writer */
  int ipcbuf_lock_write (ipcbuf_t*);

  /*! Unlock this process in as the data writer */
  int ipcbuf_unlock_write (ipcbuf_t*);

  /*! Disable the start-of-data flag */
  int ipcbuf_disable_sod (ipcbuf_t*);

  /*! Enable the start-of-data flag */
  int ipcbuf_enable_sod (ipcbuf_t*, uint64_t st_buf, uint64_t st_byte);

  /*! Get the minimum possible buffer number that be start of data */
  uint64_t ipcbuf_get_sod_minbuf (ipcbuf_t* id);

  /*! Enable the end-of-data flag */
  int ipcbuf_enable_eod (ipcbuf_t*);

  /*! Get the next empty buffer available for writing.  The
    calling process must have locked "data writer" status with a call
    to ipcbuf_lock_write. */
  char* ipcbuf_get_next_write (ipcbuf_t*);

  int ipcbuf_zero_next_write (ipcbuf_t *id);

  /*! Return the write buffer byte offset from the start of the transfer */
  int64_t ipcbuf_tell_write (ipcbuf_t* id);

  /*! Declare that the last buffer to be returned by
    ipcbuf_get_next_write has been filled with nbytes bytes.  The
    calling process must have locked "data writer" status with a call
    to ipcbuf_lock_write.  If nbytes is less than bufsz, then end of
    data is implicitly set. */
  int ipcbuf_mark_filled (ipcbuf_t*, uint64_t nbytes);

  /*! Return true if process is in writing state */
  char ipcbuf_is_writing (ipcbuf_t*);

  /*! Return true if process is the data writer */
  char ipcbuf_is_writer (ipcbuf_t*);

  /* ////////////////////////////////////////////////////////////////////
     
     FUNCTIONS USED BY THE PROCESS READING FROM SHARED MEMORY

     //////////////////////////////////////////////////////////////////// */

  /*! Lock this process in as the data reader */
  int ipcbuf_lock_read (ipcbuf_t*);

  /*! Unlock this process in as the data reader */
  int ipcbuf_unlock_read (ipcbuf_t*);

  /*! Get the next full buffer, and the number of bytes in it */
  char* ipcbuf_get_next_read (ipcbuf_t*, uint64_t* bytes);

  /*! Return the read buffer byte offset from the start of the transfer */
  int64_t ipcbuf_tell_read (ipcbuf_t* id);

  /*! Gets the next full buffer, but does not modify any aspect of the ring buffer */
  char *ipcbuf_get_next_readable (ipcbuf_t* id, uint64_t* bytes);

  /*! Declare that the last buffer to be returned by
    ipcbuf_get_next_read has been cleared and can be recycled.  The
    process must have locked "data reader" status with a call to
    ipcbuf_lock_read */
  int ipcbuf_mark_cleared (ipcbuf_t*);

  /*! Return true if process is the data reader */
  char ipcbuf_is_reader (ipcbuf_t* id);

  /*! Return the state of the start-of-data flag */
  int ipcbuf_sod (ipcbuf_t*);

  /*! Test if the current buffer is the last buffer containing data */
  int ipcbuf_eod (ipcbuf_t*);

  /*! Return the number of bufferswritten to the ring buffer */
  uint64_t ipcbuf_get_write_count (ipcbuf_t*);
  uint64_t ipcbuf_get_write_index (ipcbuf_t* id);

  uint64_t ipcbuf_get_write_byte_xfer (ipcbuf_t* id);
  uint64_t ipcbuf_get_write_count_xfer (ipcbuf_t* id);

  /*! Return the number of buffersread from the ring buffer */
  uint64_t ipcbuf_get_read_count (ipcbuf_t*);
  uint64_t ipcbuf_get_read_count_iread (ipcbuf_t* id, unsigned iread);

  /*! Return the Data Block index of the read buffer */
  uint64_t ipcbuf_get_read_index (ipcbuf_t* id);

  /*! Return the number of buffers in the ring buffer */
  uint64_t ipcbuf_get_nbufs (ipcbuf_t*);

  /*! Return the size of each buffer in the ring */
  uint64_t ipcbuf_get_bufsz (ipcbuf_t*);

  /*! Reset the buffer count and end of data flags */
  int ipcbuf_reset (ipcbuf_t*);

  /*! Reset the buffer count and end of data flags, with extreme prejudice */
  int ipcbuf_hard_reset (ipcbuf_t*);

  /*! Lock the shared memory into physical RAM (must be su) */
  int ipcbuf_lock (ipcbuf_t*);

  /*! Unlock the shared memory from physical RAM (allow swap) */
  int ipcbuf_unlock (ipcbuf_t*);

  /*! Initialise all buffer's to 0's, pageing them into RAM */
  int ipcbuf_page (ipcbuf_t* id);

  /*! Return the number of buffers currently flagged as clear */
  uint64_t ipcbuf_get_nclear (ipcbuf_t*);
  uint64_t ipcbuf_get_nclear_iread (ipcbuf_t*, int iread);

  /*! Return the number of buffers currently flagged as full */
  uint64_t ipcbuf_get_nfull (ipcbuf_t*);
  uint64_t ipcbuf_get_nfull_iread (ipcbuf_t*, int iread);

  /*! Return the number of sodacks */
  uint64_t ipcbuf_get_sodack (ipcbuf_t* id);
  uint64_t ipcbuf_get_sodack_iread (ipcbuf_t* id, int iread);

  /*! Return the number of eodacks */
  uint64_t ipcbuf_get_eodack (ipcbuf_t* id);
  uint64_t ipcbuf_get_eodack_iread (ipcbuf_t* id, int iread);

  /*! Return the number of readers in */
  int ipcbuf_get_nreaders(ipcbuf_t* id);

  /*! Return whether reader connected, 0 == connected */
  int ipcbuf_get_reader_conn (ipcbuf_t* id);
  int ipcbuf_get_reader_conn_iread (ipcbuf_t* id, int iread);

  /*! Return the current read semaphore count */
  int ipcbuf_get_read_semaphore_count (ipcbuf_t* id);

  /*! Useful utility */
  void* shm_alloc (key_t key, size_t size, int flag, int* id);

  /*! set the start of clocking data buffer  */
  uint64_t ipcbuf_set_soclock_buf(ipcbuf_t*);

#ifdef HAVE_CUDA
  /*! return CUDA device_id for the data buffer, -1 for host */
  int ipcbuf_get_device (ipcbuf_t* id);
#endif


#ifdef __cplusplus
	   }
#endif

#endif
