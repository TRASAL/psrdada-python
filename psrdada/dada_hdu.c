#include "dada_hdu.h"
#include "dada_def.h"
#include "ascii_header.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

/*! Create a new DADA Header plus Data Unit */
dada_hdu_t* dada_hdu_create (multilog_t* log)
{
  dada_hdu_t* hdu = malloc (sizeof(dada_hdu_t));
  assert (hdu != 0);

  hdu -> log = log;
  hdu -> data_block = 0;
  hdu -> header_block = 0;

  hdu -> header = 0;
  hdu -> header_size = 0;

  dada_hdu_set_key( hdu, DADA_DEFAULT_BLOCK_KEY );
  return hdu;
}

/*! Set the key of the DADA Header plus Data Unit */
void dada_hdu_set_key (dada_hdu_t* hdu, key_t key)
{
  hdu -> data_block_key = key;
  hdu -> header_block_key = key + 1;
}

/*! Destroy a DADA primary write client main loop */
void dada_hdu_destroy (dada_hdu_t* hdu)
{
  assert (hdu != 0);

  if (hdu->data_block)
    dada_hdu_disconnect (hdu);

  free (hdu);
}

/*! Connect the DADA Header plus Data Unit */
int dada_hdu_connect (dada_hdu_t* hdu)
{
  ipcbuf_t ipcbuf_init = IPCBUF_INIT;
  ipcio_t ipcio_init = IPCIO_INIT;

  assert (hdu != 0);

  if (hdu->data_block) {
    fprintf (stderr, "dada_hdu_connect: already connected\n");
    return -1;
  }

  hdu->header_block = malloc (sizeof(ipcbuf_t));
  assert (hdu->header_block != 0);
  *(hdu->header_block) = ipcbuf_init;

  hdu->data_block = malloc (sizeof(ipcio_t));
  assert (hdu->data_block != 0);
  *(hdu->data_block) = ipcio_init;

  /* connect to the shared memory */
  if (ipcbuf_connect (hdu->header_block, hdu->header_block_key) < 0)
  {
    multilog (hdu->log, LOG_ERR, "Failed to connect to Header Block\n");
    free (hdu->header_block);
    hdu->header_block = 0;
    free (hdu->data_block);
    hdu->data_block = 0;
    return -1;
  }

  if (ipcio_connect (hdu->data_block, hdu->data_block_key) < 0)
  {
    multilog (hdu->log, LOG_ERR, "Failed to connect to Data Block\n");
    free (hdu->header_block);
    hdu->header_block = 0;
    free (hdu->data_block);
    hdu->data_block = 0;
    return -1;
  }

  return 0;
}


/*! Disconnect the DADA Header plus Data Unit */
int dada_hdu_disconnect (dada_hdu_t* hdu)
{
  int status = 0;

  assert (hdu != 0);

  if (!hdu->data_block) {
    fprintf (stderr, "dada_hdu_disconnect: not connected\n");
    return -1;
  }

  if (ipcio_disconnect (hdu->data_block) < 0) {
    multilog (hdu->log, LOG_ERR, "Failed to disconnect from Data Block\n");
    status = -1;
  }

  if (ipcbuf_disconnect (hdu->header_block) < 0) {
    multilog (hdu->log, LOG_ERR, "Failed to disconnect from Header Block\n");
    status = -1;
  }

  free (hdu->header_block);
  hdu->header_block = 0;
  free (hdu->data_block);
  hdu->data_block = 0;
  
  return status;
}

/*! Lock DADA Header plus Data Unit designated reader */
int dada_hdu_lock_read (dada_hdu_t* hdu)
{
  assert (hdu != 0);

  if (!hdu->data_block) {
    fprintf (stderr, "dada_hdu_disconnect: not connected\n");
    return -1;
  }

  if (ipcbuf_lock_read (hdu->header_block) < 0) {
    multilog (hdu->log, LOG_ERR, "Could not lock Header Block for reading\n");
    return -1;
  }

  if (ipcio_open (hdu->data_block, 'R') < 0) {
    multilog (hdu->log, LOG_ERR, "Could not lock Data Block for reading\n");
    return -1;
  }

  return 0;
}

/*! Unlock DADA Header plus Data Unit designated reader */
int dada_hdu_unlock_read (dada_hdu_t* hdu)
{
  assert (hdu != 0);

  if (!hdu->data_block)
  {
    fprintf (stderr, "dada_hdu_unlock_read: not connected\n");
    return -1;
  }

#ifdef _DEBUG
  fprintf (stderr, "dada_hdu_unlock_read: ipcio_close (hdu->data_block)\n");
#endif
  if (ipcio_close (hdu->data_block) < 0)
  {
    multilog (hdu->log, LOG_ERR, "Could not unlock Data Block read\n");
    return -1;
  }

  if (hdu->header)
  {
#ifdef _DEBUG
    fprintf (stderr, "dada_hdu_unlock_read: free (hdu->header)\n");
#endif
    free (hdu->header);
    hdu->header = 0;
    if (ipcbuf_is_reader (hdu->header_block))
    {
#ifdef _DEBUG
      fprintf (stderr, "dada_hdu_unlock_read: ipcbuf_mark_cleared (hdu->header_block)\n");
#endif
      ipcbuf_mark_cleared (hdu->header_block);
    }
  }

#ifdef _DEBUG
  fprintf (stderr, "dada_hdu_unlock_read: ipcbuf_unlock_read (hdu->header_block)\n");
#endif
  if (ipcbuf_unlock_read (hdu->header_block) < 0) {
    multilog (hdu->log, LOG_ERR,"Could not unlock Header Block read\n");
    return -1;
  }

  return 0;
}

/*! Lock DADA Header plus Data Unit designated writer */
int dada_hdu_lock_write (dada_hdu_t* hdu)
{
   return dada_hdu_lock_write_spec (hdu, 'W');
}

/*! Lock DADA Header plus Data Unit designated writer with specified mode */
int dada_hdu_lock_write_spec (dada_hdu_t* hdu, char writemode)
{
  assert (hdu != 0);

  if (!hdu->data_block) {
    fprintf (stderr, "dada_hdu_disconnect: not connected\n");
    return -1;
  }

  if (ipcbuf_lock_write (hdu->header_block) < 0) {
    multilog (hdu->log, LOG_ERR, "Could not lock Header Block for writing\n");
    return -1;
  }

  if (ipcio_open (hdu->data_block, writemode) < 0) {
    multilog (hdu->log, LOG_ERR, "Could not lock Data Block for writing\n");
    return -1;
  }

  return 0;
}

/*! Unlock DADA Header plus Data Unit designated writer */
int dada_hdu_unlock_write (dada_hdu_t* hdu)
{
  assert (hdu != 0);

  if (!hdu->data_block) {
    fprintf (stderr, "dada_hdu_disconnect: not connected\n");
    return -1;
  }

  if (ipcio_is_open (hdu->data_block))
  {
#ifdef _DEBUG
    fprintf (stderr, "dada_hdu_unlock_write: ipcio_close (hdu->data_block)\n");
#endif
    if (ipcio_close (hdu->data_block) < 0) {
      multilog (hdu->log, LOG_ERR, "Could not unlock Data Block write\n");
      return -1;
    }
  }

#ifdef _DEBUG
    fprintf (stderr, "dada_hdu_unlock_write: ipcbuf_unlock_write (hdu->header_block)\n");
#endif
  if (ipcbuf_unlock_write (hdu->header_block) < 0) {
    multilog (hdu->log, LOG_ERR, "Could not unlock Header Block write\n");
    return -1;
  }

  return 0;
}

/*! Lock DADA Header plus Data Unit designated reader */
int dada_hdu_open_view (dada_hdu_t* hdu)
{
  assert (hdu != 0);

  if (!hdu->data_block)
  {
    fprintf (stderr, "dada_hdu_open_view: not connected\n");
    return -1;
  }

  if (ipcio_open (hdu->data_block, 'r') < 0)
  {
    multilog (hdu->log, LOG_ERR, "Could not open Data Block for viewing\n");
    return -1;
  }

  return 0;
}

/*! Unlock DADA Header plus Data Unit designated reader */
int dada_hdu_close_view (dada_hdu_t* hdu)
{
  assert (hdu != 0);

  if (!hdu->data_block)
  {
    fprintf (stderr, "dada_hdu_close_view: not connected\n");
    return -1;
  }

  if (ipcio_close (hdu->data_block) < 0)
  {
    multilog (hdu->log, LOG_ERR, "Could not close Data Block view\n");
    return -1;
  }

  return 0;
}

int dada_hdu_open (dada_hdu_t* hdu)
{
  /* pointer to the status and error logging facility */
  multilog_t* log = 0;

  /* The header from the ring buffer */
  char* header = 0;
  uint64_t header_size = 0;

  /* header size, as defined by HDR_SIZE attribute */
  uint64_t hdr_size = 0;

  assert (hdu != 0);
  assert (hdu->header == 0);

  log = hdu->log;

  while (!header_size)
  {
    /* Wait for the next valid header sub-block */
    header = ipcbuf_get_next_read (hdu->header_block, &header_size);

    if (!header) {
      multilog (log, LOG_ERR, "Could not get next header\n");
      return -1;
    }

    if (!header_size)
    {
      if (ipcbuf_is_reader (hdu->header_block))
        ipcbuf_mark_cleared (hdu->header_block);

      if (ipcbuf_eod (hdu->header_block))
      {
        multilog (log, LOG_INFO, "End of data on header block\n");
        if (ipcbuf_is_reader (hdu->header_block))
          ipcbuf_reset (hdu->header_block);
      }
      else
      {
        multilog (log, LOG_ERR, "Empty header block\n");
        return -1;
      }
    }
  }

  header_size = ipcbuf_get_bufsz (hdu->header_block);

  /* Check that header is of advertised size */
  if (ascii_header_get (header, "HDR_SIZE", "%"PRIu64, &hdr_size) != 1) {
    multilog (log, LOG_ERR, "Header with no HDR_SIZE. Setting to %"PRIu64"\n",
              header_size);
    hdr_size = header_size;
    if (ascii_header_set (header, "HDR_SIZE", "%"PRIu64, hdr_size) < 0) {
      multilog (log, LOG_ERR, "Error setting HDR_SIZE\n");
      return -1;
    }
  }

  if (hdr_size < header_size)
    header_size = hdr_size;

  else if (hdr_size > header_size) {
    multilog (log, LOG_ERR, "HDR_SIZE=%"PRIu64
              " > Header Block size=%"PRIu64"\n", hdr_size, header_size);
    multilog (log, LOG_DEBUG, "ASCII header dump\n%s", header);
    return -1;
  }

  /* Duplicate the header */
  if (header_size > hdu->header_size)
  {
    hdu->header = realloc (hdu->header, header_size);
    assert (hdu->header != 0);
    hdu->header_size = header_size;
  }

  memcpy (hdu->header, header, header_size);
  return 0;
}

// return the base addresses and sizes of the datablock
char ** dada_hdu_db_addresses(dada_hdu_t * hdu, uint64_t * nbufs, uint64_t * bufsz)
{
  ipcbuf_t *db = (ipcbuf_t *) hdu->data_block;
  *nbufs = ipcbuf_get_nbufs (db);
  *bufsz = ipcbuf_get_bufsz (db);

  return db->buffer;
}

// return the base addresses and sizes of the datablock
char ** dada_hdu_hb_addresses(dada_hdu_t * hdu, uint64_t * nbufs, uint64_t * bufsz)
{
  ipcbuf_t *hb = hdu->header_block;
  *nbufs = ipcbuf_get_nbufs (hb);
  *bufsz = ipcbuf_get_bufsz (hb);

  return hb->buffer;
}


