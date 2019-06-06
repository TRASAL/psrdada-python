#ifndef __DADA_IPCIO_H
#define __DADA_IPCIO_H

/* ************************************************************************

   ipcio_t - a struct and associated routines for creating and managing,
   as well as reading and writing to and from an ipcbuf_t ring buffer

   ************************************************************************ */

#include "ipcbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct {

    ipcbuf_t  buf;      /* ipcio_t struct on which this is based */

    char*  curbuf;      /* pointer to the current buffer in the ring */
    uint64_t curbufsz;  /* size of the current buffer */

    uint64_t bytes;     /* number of bytes into current buffer */

    char rdwrt;         /* == r read;  == w write */

    char marked_filled; /* flag set when curbuf has been marked filled */

    char sod_pending;   /* flag set when sod flag has been raised */
    uint64_t sod_buf;   /* buffer of start-of-data */
    uint64_t sod_byte;  /* byte of start-of-data */

  } ipcio_t;

  static const ipcio_t IPCIO_INIT = { IPCBUF_INIT, 0,0, 0, 0, 0, 0,0,0 };

  /*! create a new shared memory block and initialize an ipcio_t struct */
  int ipcio_create (ipcio_t* ipc, key_t key, uint64_t nbufs, uint64_t bufsz, unsigned num_read);

  /*! create a new shared memory block and initialize an ipcio_t struct, on GPU device_id */
  int ipcio_create_work (ipcio_t* ipc, key_t key, uint64_t nbufs, uint64_t bufsz, unsigned num_read, int device_id);

  /*! connect to an already created ipcbuf_t struct in shared memory */
  int ipcio_connect (ipcio_t* ipc, key_t key);

  /*! disconnect from an already connected ipcio_t struct */
  int ipcio_disconnect (ipcio_t* ipc);

  /*! start reading/writing to an ipcbuf */
  int ipcio_open (ipcio_t* ipc, char rdwrt);

  /*! stop reading/writing to an ipcbuf */
  int ipcio_close (ipcio_t* ipc);

  /*! return true if already open */
  int ipcio_is_open (ipcio_t* ipc);

  /*! free all resources reserved for the ring buffer */
  int ipcio_destroy (ipcio_t* ipc);

  /*! get the minimum byte that may be passed to ipcio_start */
  uint64_t ipcio_get_start_minimum (ipcio_t* ipc);

  /*! enable start of data on the specified byte */
  int ipcio_start (ipcio_t* ipc, uint64_t byte);

  /*! write an end of data marker; may continue writing to ring buffer */
  int ipcio_stop (ipcio_t* ipc);

  /*! write bytes to ipcbuf */
  ssize_t ipcio_write (ipcio_t* ipc, char* ptr, size_t bytes);

  /*! read bytes from ipcbuf */
  ssize_t ipcio_read (ipcio_t* ipc, char* ptr, size_t bytes);

  /*! seek into ipcbuf - valid only for reading for now */
  int64_t ipcio_seek (ipcio_t* ipc, int64_t offset, int whence);

  /*! tell the last byte written to the ring buffer */
  uint64_t ipcio_tell (ipcio_t* ipc);

  /*! tell how many bytes can be written into the ring buffer before its full */
  int64_t ipcio_space_left (ipcio_t* ipc);

  /*! tell how many bytes can be written into the ring buffer before its full */
  float ipcio_percent_full(ipcio_t* ipc);

  uint64_t ipcio_get_soclock_byte(ipcio_t* ipc);

  char * ipcio_open_block_read (ipcio_t *ipc, uint64_t *curbufsz, uint64_t *block_id);

  ssize_t ipcio_close_block_read (ipcio_t *ipc, uint64_t bytes);

  char * ipcio_open_block_write (ipcio_t *ipc, uint64_t *block_id);

  ssize_t ipcio_update_block_write (ipcio_t *ipc, uint64_t bytes);

  ssize_t ipcio_close_block_write (ipcio_t *ipc, uint64_t bytes);

  int ipcio_zero_next_block (ipcio_t *ipc);

#ifdef __cplusplus
	   }
#endif

#endif
