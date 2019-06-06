#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "ipcio.h"

#ifdef HAVE_CUDA
#include <cuda_runtime.h>
#endif

// #define _DEBUG 1

void ipcio_init (ipcio_t* ipc)
{
  ipc -> bytes = 0;
  ipc -> rdwrt = 0;
  ipc -> curbuf = 0;

  ipc -> marked_filled = 0;

  ipc -> sod_pending = 0;
  ipc -> sod_buf  = 0;
  ipc -> sod_byte = 0;
}

/* create a new shared memory block and initialize an ipcio_t struct */
int ipcio_create (ipcio_t* ipc, key_t key, uint64_t nbufs, uint64_t bufsz, unsigned num_read)
{
  return ipcio_create_work (ipc, key, nbufs, bufsz, num_read, -1);
}

int ipcio_create_work (ipcio_t* ipc, key_t key, uint64_t nbufs, uint64_t bufsz, unsigned num_read, int device_id)
{
  if (ipcbuf_create_work ((ipcbuf_t*)ipc, key, nbufs, bufsz, num_read, device_id) < 0)
  {
    fprintf (stderr, "ipcio_create: ipcbuf_create error\n");
    return -1;
  }
  ipcio_init (ipc);
  return 0;
}

/* connect to an already created ipcbuf_t struct in shared memory */
int ipcio_connect (ipcio_t* ipc, key_t key)
{
  if (ipcbuf_connect ((ipcbuf_t*)ipc, key) < 0) {
    fprintf (stderr, "ipcio_connect: ipcbuf_connect error\n");
    return -1;
  }
  ipcio_init (ipc);
  return 0;
}

/* disconnect from an already connected ipcbuf_t struct in shared memory */
int ipcio_disconnect (ipcio_t* ipc)
{
  if (ipcbuf_disconnect ((ipcbuf_t*)ipc) < 0) {
    fprintf (stderr, "ipcio_disconnect: ipcbuf_disconnect error\n");
    return -1;
  }
  ipcio_init (ipc);
  return 0;
}

int ipcio_destroy (ipcio_t* ipc)
{
  ipcio_init (ipc);
  return ipcbuf_destroy ((ipcbuf_t*)ipc);
}

/* start reading/writing to an ipcbuf */
int ipcio_open (ipcio_t* ipc, char rdwrt)
{
  if (rdwrt != 'R' && rdwrt != 'r' && rdwrt != 'w' && rdwrt != 'W') {
    fprintf (stderr, "ipcio_open: invalid rdwrt = '%c'\n", rdwrt);
    return -1;
  }

  ipc -> rdwrt = 0;
  ipc -> bytes = 0;
  ipc -> curbuf = 0;

  if (rdwrt == 'w' || rdwrt == 'W') {

    /* read from file, write to shm */
    if (ipcbuf_lock_write ((ipcbuf_t*)ipc) < 0) {
      fprintf (stderr, "ipcio_open: error ipcbuf_lock_write\n");
      return -1;
    }

    if (rdwrt == 'w' && ipcbuf_disable_sod((ipcbuf_t*)ipc) < 0) {
      fprintf (stderr, "ipcio_open: error ipcbuf_disable_sod\n");
      return -1;
    }

    ipc -> rdwrt = rdwrt;
    return 0;

  }

  if (rdwrt == 'R') {
    if (ipcbuf_lock_read ((ipcbuf_t*)ipc) < 0) {
      fprintf (stderr, "ipcio_open: error ipcbuf_lock_read\n");
      return -1;
    }
  }

  ipc -> rdwrt = rdwrt;
  return 0;
}

uint64_t ipcio_get_start_minimum (ipcio_t* ipc)
{
  uint64_t bufsz  = ipcbuf_get_bufsz ((ipcbuf_t*)ipc);
  uint64_t minbuf = ipcbuf_get_sod_minbuf ((ipcbuf_t*)ipc);
  return minbuf * bufsz;
}


/* Checks if a SOD request has been requested, if it has, then it enables
 * SOD */
int ipcio_check_pending_sod (ipcio_t* ipc)
{
  ipcbuf_t* buf = (ipcbuf_t*) ipc;

  /* If the SOD flag has not been raised return 0 */
  if (ipc->sod_pending == 0) 
    return 0;

  /* The the buffer we wish to raise SOD on has not yet been written, then
   * don't raise SOD */
  if (ipcbuf_get_write_count (buf) <= ipc->sod_buf)
    return 0;

  /* Try to enable start of data on the sod_buf & sod byte */
  if (ipcbuf_enable_sod (buf, ipc->sod_buf, ipc->sod_byte) < 0) {
    fprintf (stderr, "ipcio_check_pendind_sod: fail ipcbuf_enable_sod\n");
    return -1;
  }
  
  ipc->sod_pending = 0;
  return 0;
}

/* start writing valid data to an ipcbuf. byte is the absolute byte relative to
 * the start of the data block */
int ipcio_start (ipcio_t* ipc, uint64_t byte)
{
  ipcbuf_t* buf  = (ipcbuf_t*) ipc;
  uint64_t bufsz = ipcbuf_get_bufsz (buf);

  if (ipc->rdwrt != 'w') {
    fprintf (stderr, "ipcio_start: invalid ipcio_t (%c)\n",ipc->rdwrt);
    return -1;
  }

  ipc->sod_buf  = byte / bufsz;
  ipc->sod_byte = byte % bufsz;
  ipc->sod_pending = 1;
  ipc->rdwrt = 'W';

  return ipcio_check_pending_sod (ipc);
}

/* stop reading/writing to an ipcbuf */
int ipcio_stop_close (ipcio_t* ipc, char unlock)
{
  if (ipc -> rdwrt == 'W') {

#ifdef _DEBUG
    if (ipc->curbuf)
      fprintf (stderr, "ipcio_close:W buffer:%"PRIu64" %"PRIu64" bytes. "
                       "buf[0]=%x\n", ipc->buf.sync->w_buf, ipc->bytes, 
                       (void *) ipc->curbuf);
#endif

    if (ipcbuf_is_writing((ipcbuf_t*)ipc)) {

      if (ipcbuf_enable_eod ((ipcbuf_t*)ipc) < 0) {
        fprintf (stderr, "ipcio_close:W error ipcbuf_enable_eod\n");
        return -1;
      }

      if (ipcbuf_mark_filled ((ipcbuf_t*)ipc, ipc->bytes) < 0) {
        fprintf (stderr, "ipcio_close:W error ipcbuf_mark_filled\n");
        return -1;
      }

      if (ipcio_check_pending_sod (ipc) < 0) {
        fprintf (stderr, "ipcio_close:W error ipcio_check_pending_sod\n");
        return -1;
      }

      /* Ensure that mark_filled is not called again for this buffer
         in ipcio_write */
      ipc->marked_filled = 1;

      if (ipc->bytes == ipcbuf_get_bufsz((ipcbuf_t*)ipc)) {
#ifdef _DEBUG
        fprintf (stderr, "ipcio_close:W last buffer was filled\n");
#endif
        ipc->curbuf = 0;
      }

    }

    ipc->rdwrt = 'w';

    if (!unlock)
      return 0;

  }

  if (ipc -> rdwrt == 'w') {

    /* Removed to allow a writer to write more than one transfer to the 
     * data block */
    /*
#ifdef _DEBUG
    fprintf (stderr, "ipcio_close:W calling ipcbuf_reset\n");
#endif

    if (ipcbuf_reset ((ipcbuf_t*)ipc) < 0) {
      fprintf (stderr, "ipcio_close:W error ipcbuf_reset\n");
      return -1;
    }*/

    if (ipc->buf.sync->w_xfer > 0) {

      uint64_t prev_xfer = ipc->buf.sync->w_xfer - 1;
      /* Ensure the w_buf pointer is pointing buffer after the 
       * most recent EOD */
      ipc->buf.sync->w_buf = ipc->buf.sync->e_buf[prev_xfer % IPCBUF_XFERS]+1;

      // TODO CHECK IF WE NEED TO DECREMENT the count??
      
    }

    if (ipcbuf_unlock_write ((ipcbuf_t*)ipc) < 0) {
      fprintf (stderr, "ipcio_close:W error ipcbuf_unlock_write\n");
      return -1;
    }

    ipc -> rdwrt = 0;
    return 0;

  }

  if (ipc -> rdwrt == 'R') {

#ifdef _DEBUG
    fprintf (stderr, "ipcio_close:R ipcbuf_unlock_read()\n");
#endif
    if (ipcbuf_unlock_read ((ipcbuf_t*)ipc) < 0) {
      fprintf (stderr, "ipcio_close:R error ipcbuf_unlock_read\n");
      return -1;
    }

    ipc -> rdwrt = 0;
    return 0;

  }

  fprintf (stderr, "ipcio_close: invalid ipcio_t\n");
  return -1;
}

/* stop writing valid data to an ipcbuf */
int ipcio_stop (ipcio_t* ipc)
{
  if (ipc->rdwrt != 'W') {
    fprintf (stderr, "ipcio_stop: not writing!\n");
    return -1;
  }
  return ipcio_stop_close (ipc, 0);
}

/* stop reading/writing to an ipcbuf */
int ipcio_close (ipcio_t* ipc)
{
  return ipcio_stop_close (ipc, 1);
}

/* return 1 if the ipcio is open for reading or writing */
int ipcio_is_open (ipcio_t* ipc)
{
  char rdwrt = ipc->rdwrt;
  return rdwrt == 'R' || rdwrt == 'r' || rdwrt == 'w' || rdwrt == 'W';
}

/* write bytes to ipcbuf */
ssize_t ipcio_write (ipcio_t* ipc, char* ptr, size_t bytes)
{

  size_t space = 0;
  size_t towrite = bytes;

  if (ipc->rdwrt != 'W' && ipc->rdwrt != 'w') {
    fprintf (stderr, "ipcio_write: invalid ipcio_t (%c)\n",ipc->rdwrt);
    return -1;
  }

#ifdef HAVE_CUDA
  int device_id = ipcbuf_get_device ((ipcbuf_t*)ipc);
  cudaError_t err;
  if (device_id >= 0)
  {
#ifdef _DEBUG
    fprintf (stderr, "ipcio_write: cudaSetDevice(%d)\n", device_id);
#endif
    err = cudaSetDevice (device_id);
    if (err != cudaSuccess)
    {
      fprintf (stderr, "ipcio_write: cudaSetDevice failed %s\n",
               cudaGetErrorString(err));
      return -1;
    }
  }
  else
  {
#ifdef _DEBUG
    fprintf (stderr, "ipcio_write: device_id=%d\n", device_id);
#endif
  }
#endif

  while (bytes) {

    /*
      The check for a full buffer is done at the start of the loop
      so that if ipcio_write exactly fills a buffer before exiting,
      the end-of-data flag can be raised before marking the buffer
      as filled in ipcio_stop
    */
    if (ipc->bytes == ipcbuf_get_bufsz((ipcbuf_t*)ipc)) {

      if (!ipc->marked_filled) {

#ifdef _DEBUG
        fprintf (stderr, "ipcio_write buffer:%"PRIu64" mark_filled\n",
                 ipc->buf.sync->w_buf);
#endif
        
        /* the buffer has been filled */
        if (ipcbuf_mark_filled ((ipcbuf_t*)ipc, ipc->bytes) < 0) {
          fprintf (stderr, "ipcio_write: error ipcbuf_mark_filled\n");
          return -1;
        }

        if (ipcio_check_pending_sod (ipc) < 0) {
          fprintf (stderr, "ipcio_write: error ipcio_check_pending_sod\n");
          return -1;
        }

      }

      ipc->curbuf = 0;
      ipc->bytes = 0;
      ipc->marked_filled = 1;

    }

    if (!ipc->curbuf) {

#ifdef _DEBUG
      fprintf (stderr, "ipcio_write buffer:%"PRIu64" ipcbuf_get_next_write\n",
               ipc->buf.sync->w_buf);
#endif

      ipc->curbuf = ipcbuf_get_next_write ((ipcbuf_t*)ipc);

#ifdef _DEBUG
      fprintf (stderr, "ipcio_write: ipcbuf_get_next_write returns\n");
#endif

      if (!ipc->curbuf) {
        fprintf (stderr, "ipcio_write: ipcbuf_next_write\n");
        return -1;
      }

      ipc->marked_filled = 0;
      ipc->bytes = 0;
    }

    space = ipcbuf_get_bufsz((ipcbuf_t*)ipc) - ipc->bytes;
    if (space > bytes)
      space = bytes;

#ifdef _DEBUG
    fprintf (stderr, "ipcio_write space=%"PRIu64"\n", space);
#endif

    if (space > 0) {

#ifdef _DEBUG
      fprintf (stderr, "ipcio_write buffer:%"PRIu64" offset:%"PRIu64
	       " count=%"PRIu64"\n", ipc->buf.sync->w_buf, ipc->bytes, space);
#endif

#ifdef HAVE_CUDA
      if (device_id >= 0)
      {
#ifdef _DEBUG
        fprintf (stderr, "ipcio_write: cudaMemcpy (%p, %p, :%"PRIu64", cudaMemcpyHostToDevice\n",
	         (void *) ipc->curbuf + ipc->bytes, (void *) ptr, space);
#endif
        err = cudaMemcpy(ipc->curbuf + ipc->bytes, ptr, space, cudaMemcpyHostToDevice);
        if (err != cudaSuccess)
        {
          fprintf (stderr, "ipcio_write: cudaMemcpy failed %s\n", cudaGetErrorString(err));
        }
#ifdef _DEBUG
        fprintf (stderr, "ipcio_write: cudaDeviceSynchronize()\n");
#endif
        cudaDeviceSynchronize();
      }
      else
#endif
      {
#ifdef _DEBUG
        fprintf (stderr, "ipcio_write: memcpy (%p, %p, :%"PRIu64"\n",
                 (void *) ipc->curbuf + ipc->bytes, (void *) ptr, space);
#endif
        memcpy (ipc->curbuf + ipc->bytes, ptr, space);
      } 
      ipc->bytes += space;
      ptr += space;
      bytes -= space;
    }
  }

  return towrite;
}

/* 
 * Open next Data Block unit for "direct" read access. Returns a pointer to the
 * DB unit and set the number of bytes in the block and the block index 
 */
char * ipcio_open_block_read (ipcio_t *ipc, uint64_t *curbufsz, uint64_t *block_id)
{

  if (ipc->bytes != 0)
  {
    fprintf (stderr, "ipcio_open_block_read: ipc->bytes != 0\n");
    return 0;
  }

  if (ipc->curbuf)
  {
    fprintf(stderr, "ipcio_open_block_read: ipc->curbuf != 0\n");
    return 0;
  }

  if (ipc -> rdwrt != 'r' && ipc -> rdwrt != 'R')
  {
    fprintf(stderr, "ipcio_open_block_read: ipc -> rdwrt != [rR]\n");
    return 0;
  }

  // test for EOD
  if (ipcbuf_eod((ipcbuf_t*)ipc))
  {
    fprintf(stderr, "ipcio_open_block_read: ipcbuf_eod true, returning null ptr\n");
    return 0;
  }

  ipc->curbuf = ipcbuf_get_next_read ((ipcbuf_t*)ipc, &(ipc->curbufsz));

  if (!ipc->curbuf)
  {
    fprintf (stderr, "ipcio_open_block_read: could not get next block rdwrt=%c\n", ipc -> rdwrt);
    return 0;
  }

  *block_id = ipcbuf_get_read_index ((ipcbuf_t*)ipc);
  *curbufsz = ipc->curbufsz;

  ipc->bytes = 0;

  return ipc->curbuf;
}

/*
 * Close the Data Block unit from a "direct" read access, updating the number
 * of bytes read. Return 0 on success, -1 on failure
 */
ssize_t ipcio_close_block_read (ipcio_t *ipc, uint64_t bytes)
{

  if (ipc->bytes != 0)
  {
    fprintf (stderr, "ipcio_close_block_read: ipc->bytes != 0\n");
    return -1;
  }

  if (!ipc->curbuf)
  {
    fprintf (stderr, "ipcio_close_block_read: ipc->curbuf == 0\n");
    return -1;
  }

  if (ipc -> rdwrt != 'R')
  {
    fprintf (stderr, "ipcio_close_block_read: ipc->rdwrt != W\n");
    return -1;
  }

  // the reader should always have read the required number of bytes
  if (bytes != ipc->curbufsz)
  {
    fprintf (stderr, "ipcio_close_block_read: WARNING! bytes [%"PRIu64"] != ipc->curbufsz [%"PRIu64"]\n", 
              bytes, ipc->curbufsz);
  }

  // increment the bytes counter by the number of bytes used
  ipc->bytes += bytes;

  // if all the requested bytes have been read
  if (ipc->bytes == ipc->curbufsz)
  {

    if (ipcbuf_mark_cleared ((ipcbuf_t*)ipc) < 0)
    {
      fprintf (stderr, "ipcio_close_block: error ipcbuf_mark_filled\n");
      return -1;
    }

    ipc->curbuf = 0;
    ipc->bytes = 0;
  }

  return 0;
}


/* 
 * Open next Data Block unit for "direct" write access. Returns a pointer to the
 * DB unit and set the block index 
 */
char * ipcio_open_block_write (ipcio_t *ipc, uint64_t *block_id)
{

  if (ipc->bytes != 0) 
  {
    fprintf (stderr, "ipcio_open_block_write: ipc->bytes != 0\n");
    return 0;
  }

  if (ipc->curbuf) 
  {
    fprintf(stderr, "ipcio_open_block_write: ipc->curbuf != 0\n");
    return 0;
  }

  if (ipc -> rdwrt != 'W')
  {
    fprintf(stderr, "ipcio_open_block_write: ipc -> rdwrt != W\n");
    return 0;
  } 

  ipc->curbuf = ipcbuf_get_next_write ((ipcbuf_t*)ipc);

  if (!ipc->curbuf)
  {
    fprintf (stderr, "ipcio_open_block_write: could not get next block rdwrt=%c\n", ipc -> rdwrt);
    return 0;
  }

  *block_id = ipcbuf_get_write_index ((ipcbuf_t*)ipc);

  ipc->marked_filled = 0;
  ipc->bytes = 0;

  return ipc->curbuf;
}

int ipcio_zero_next_block (ipcio_t *ipc)
{
  if (ipc -> rdwrt != 'W')
  {
    fprintf(stderr, "ipcio_open_block_write: ipc -> rdwrt != W\n");
    return -1;
  }

  return ipcbuf_zero_next_write ((ipcbuf_t*)ipc);
}

/*
 * Update the number of bytes written to a Data Block unit that was opened
 * for "direct" write access. This does not mark the buffer as filled. 
 */
ssize_t ipcio_update_block_write (ipcio_t *ipc, uint64_t bytes)
{
  if (ipc->bytes != 0)
  {
    fprintf (stderr, "ipcio_update_block_write: ipc->bytes [%"PRIu64"] != 0\n", ipc->bytes);
    return -1;
  }

  if (!ipc->curbuf)
  {
    fprintf (stderr, "ipcio_update_block_write: ipc->curbuf == 0\n");
    return -1;
  }

  if (ipc->rdwrt != 'W')
  {
    fprintf(stderr, "ipcio_update_block_write: ipc->rdwrt != W\n");
    return -1;
  }

  if (ipc->bytes + bytes > ipcbuf_get_bufsz((ipcbuf_t*)ipc))
  {
    fprintf(stderr, "ipcio_update_block_write: wrote more bytes than there was space for! [%"PRIu64" + %"PRIu64"] > %"PRIu64"\n",
                    ipc->bytes, bytes, ipcbuf_get_bufsz((ipcbuf_t*)ipc));
    return -1;
  }

  // increment the bytes counter by the number of bytes used
  ipc->bytes += bytes;

  return 0;
}

/*
 * Close the Data Block unit from a "direct" write access, updating the number
 * of bytes written. Return 0 on success, -ve on failure
 */
ssize_t ipcio_close_block_write (ipcio_t *ipc, uint64_t bytes)
{

  // update the number of bytes written
  if (ipcio_update_block_write (ipc, bytes) < 0)
  {
    fprintf (stderr, "ipcio_close_block_write: ipcio_update_block_write failed\n");
    return -1;
  }

  // if this buffer has not yet been marked as filled
  if (!ipc->marked_filled) 
  {
    if (ipcbuf_mark_filled ((ipcbuf_t*)ipc, ipc->bytes) < 0) 
    {
      fprintf (stderr, "ipcio_close_block_write: error ipcbuf_mark_filled\n");
      return -2;
    }

    if (ipcio_check_pending_sod (ipc) < 0)
    {
      fprintf (stderr, "ipcio_close_block_write: error ipcio_check_pending_sod\n");
      return -3;
    }

    ipc->marked_filled = 1;
    ipc->curbuf = 0;
    ipc->bytes = 0;
  }
  return 0;
}

/* read bytes from ipcbuf */
ssize_t ipcio_read (ipcio_t* ipc, char* ptr, size_t bytes)
{
  size_t space = 0;
  size_t toread = bytes;

  if (ipc -> rdwrt != 'r' && ipc -> rdwrt != 'R')
  {
    fprintf (stderr, "ipcio_read: invalid ipcio_t (rdwrt=%c)\n", ipc->rdwrt);
    return -1;
  }

#ifdef HAVE_CUDA
  int device_id = ipcbuf_get_device ((ipcbuf_t*)ipc);
  cudaError_t err;
  if (device_id >= 0)
  {
#ifdef _DEBUG
    fprintf (stderr, "ipcio_read: cudaSetDevice(%d)\n", device_id);
#endif
    err = cudaSetDevice (device_id);
    if (err != cudaSuccess)
    {
      fprintf (stderr, "ipcio_read: cudaSetDevice failed %s\n",
               cudaGetErrorString(err));
      return -1;
    }
  }
  else
  {
#ifdef _DEBUG
    fprintf (stderr, "ipcio_read: device_id=%d\n", device_id);
#endif
  }

#endif

  while (!ipcbuf_eod((ipcbuf_t*)ipc))
  {
    if (!ipc->curbuf)
    {
      ipc->curbuf = ipcbuf_get_next_read ((ipcbuf_t*)ipc, &(ipc->curbufsz));

#ifdef _DEBUG
      fprintf (stderr, "ipcio_read buffer:%"PRIu64" %"PRIu64" bytes. buf[0]=%p\n",
               ipc->buf.sync->r_bufs[0], ipc->curbufsz, (void *) (ipc->curbuf));
#endif

      if (!ipc->curbuf)
      {
        fprintf (stderr, "ipcio_read: error ipcbuf_next_read\n");
        return -1;
      }

      ipc->bytes = 0;
    }

    if (bytes)
    {
      space = ipc->curbufsz - ipc->bytes;
      if (space > bytes)
        space = bytes;

      if (ptr)
      {
#ifdef HAVE_CUDA
        if (device_id >= 0)
        {
#ifdef _DEBUG
          fprintf (stderr, "ipcio_read: cudaMemcpy (%p, %p, :%"PRIu64", cudaMemcpyHostToDevice\n",
                   (void *) ptr, (void *) ipc->curbuf + ipc->bytes, space);
#endif
          err = cudaMemcpy(ptr, ipc->curbuf + ipc->bytes, space, cudaMemcpyDeviceToHost);
          if (err != cudaSuccess)
          {
            fprintf (stderr, "ipcio_read: cudaMemcpy failed %s\n", cudaGetErrorString(err));
          }
#ifdef _DEBUG
          fprintf (stderr, "ipcio_write: cudaDeviceSynchronize()\n");
#endif
          cudaDeviceSynchronize();
        }
        else
#endif
        {
#ifdef _DEBUG
        fprintf (stderr, "ipcio_read: memcpy (%p, %p, :%"PRIu64"\n",
                         (void *) ptr, (void *) ipc->curbuf + ipc->bytes, space);
#endif
          memcpy (ptr, ipc->curbuf + ipc->bytes, space);
        }
        ptr += space;
      }

      ipc->bytes += space;
      bytes -= space;
    }

    if (ipc->bytes == ipc->curbufsz)
    {
      if (ipc -> rdwrt == 'R' && ipcbuf_mark_cleared ((ipcbuf_t*)ipc) < 0)
      {
        fprintf (stderr, "ipcio_read: error ipcbuf_mark_filled\n");
        return -1;
      }

      ipc->curbuf = 0;
      ipc->bytes = 0;
    }
    else if (!bytes)
      break;
  }

  return toread - bytes;
}



uint64_t ipcio_tell (ipcio_t* ipc)
{
  int64_t current = -1;

  if (ipc -> rdwrt == 'R' || ipc -> rdwrt == 'r')
    current = ipcbuf_tell_read ((ipcbuf_t*)ipc);
  else if (ipc -> rdwrt == 'W' || ipc -> rdwrt == 'w')
    current = ipcbuf_tell_write ((ipcbuf_t*)ipc);

  if (current < 0)
  {
//#ifdef _DEBUG
    fprintf (stderr, "ipcio_tell: failed ipcbuf_tell"
             " mode=%c current=%"PRIi64"\n", ipc->rdwrt, current);
//#endif
    return 0;
  }

  return current + ipc->bytes;
}



int64_t ipcio_seek (ipcio_t* ipc, int64_t offset, int whence)
{
  /* the absolute value of the offset */
  uint64_t current = ipcio_tell (ipc);

#ifdef _DEBUG
  fprintf (stderr, "ipcio_seek: offset=%"PRIi64" tell=%"PRIu64"\n",
           offset, current);
#endif

  if (whence == SEEK_CUR)
    offset += ipcio_tell (ipc);

  if (current < offset)
  {
    /* seeking forward is just like reading without the memcpy */
    if (ipcio_read (ipc, 0, offset - current) < 0)
    {
      fprintf (stderr, "ipcio_seek: empty read %"PRIi64" bytes error\n",
               offset-current);
      return -1;
    }

  }

  else if (offset < current)
  {
    /* can only go back to the beginning of the current buffer ... */
    offset = current - offset;
    if (offset > ipc->bytes)
    {
      fprintf (stderr, "ipcio_seek: %"PRIu64" > max backwards %"PRIu64"\n",
               offset, ipc->bytes);
      return -1;
    }
    ipc->bytes -= offset;
  }

  return ipcio_tell (ipc);
}

/* Returns the number of bytes available in the ring buffer */
int64_t ipcio_space_left(ipcio_t* ipc)
{
  uint64_t bufsz = ipcbuf_get_bufsz ((ipcbuf_t *)ipc);
  uint64_t nbufs = ipcbuf_get_nbufs ((ipcbuf_t *)ipc);
  uint64_t full_bufs = ipcbuf_get_nfull((ipcbuf_t*) ipc);
  int64_t available_bufs = (nbufs - full_bufs);

#ifdef _DEBUG
  uint64_t clear_bufs = ipcbuf_get_nclear((ipcbuf_t*) ipc);
  fprintf (stderr,"ipcio_space_left: full_bufs = %"PRIu64", clear_bufs = %"
                  PRIu64", available_bufs = %"PRIu64", sum = %"PRIu64"\n",
                  full_bufs, clear_bufs, available_bufs, available_bufs*bufsz);
#endif

  return available_bufs * bufsz;

}

/* Returns the percentage of space left in the ring buffer */
float ipcio_percent_full(ipcio_t* ipc) {

  uint64_t nbufs = ipcbuf_get_nbufs ((ipcbuf_t *)ipc);
  uint64_t full_bufs = ipcbuf_get_nfull((ipcbuf_t*) ipc);
  return ((float)full_bufs) / ((float)nbufs);

}


/* Returns the byte corresponding the start of data clocking/recording */
uint64_t ipcio_get_soclock_byte(ipcio_t* ipc) {
  uint64_t bufsz = ipcbuf_get_bufsz ((ipcbuf_t *)ipc);
  return bufsz * ((ipcbuf_t*)ipc)->soclock_buf;
}

