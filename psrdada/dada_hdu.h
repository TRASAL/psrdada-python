#ifndef __DADA_HDU_H
#define __DADA_HDU_H

/* ************************************************************************

   dada_hdu_t - a struct and associated routines for creation and
   management of a DADA Header plus Data Unit

   ************************************************************************ */

#include "multilog.h"
#include "ipcio.h"

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct dada_hdu {

    /*! The status and error logging interface */
    multilog_t* log;

    /*! The Data Block interface */
    ipcio_t* data_block;

    /*! The Header Block interface */
    ipcbuf_t* header_block;

    /* The header */
    char* header;

    /* The size of the header */
    uint64_t header_size;

    /* The Data Block key */
    key_t data_block_key;

    /* The Header Block key */
    key_t header_block_key;

  } dada_hdu_t;

  /*! Create a new DADA Header plus Data Unit */
  dada_hdu_t* dada_hdu_create (multilog_t* log);

  /*! Set the key of the DADA Header plus Data Unit */
  void dada_hdu_set_key (dada_hdu_t* hdu, key_t key);

  /*! Destroy a DADA Header plus Data Unit */
  void dada_hdu_destroy (dada_hdu_t* hdu);

  /*! Connect the DADA Header plus Data Unit */
  int dada_hdu_connect (dada_hdu_t* hdu);

  /*! Connect the DADA Header plus Data Unit */
  int dada_hdu_disconnect (dada_hdu_t* hdu);

  /*! Lock DADA Header plus Data Unit designated reader */
  int dada_hdu_lock_read (dada_hdu_t* hdu);

  /*! Unlock DADA Header plus Data Unit designated reader */
  int dada_hdu_unlock_read (dada_hdu_t* hdu);

  /*! Lock DADA Header plus Data Unit designated writer */
  int dada_hdu_lock_write (dada_hdu_t* hdu);

  /*! Unlock DADA Header plus Data Unit designated writer */
  int dada_hdu_unlock_write (dada_hdu_t* hdu);

  /*! Lock DADA Header plus Data Unit designated writer */
  int dada_hdu_lock_write_spec (dada_hdu_t* hdu, char writemode);

  /*! Open the DADA Header plus Data Unit for passive viewing */
  int dada_hdu_open_view (dada_hdu_t* hdu);

  /*! Close the DADA Header pluse Data Unit passive viewing mode */
  int dada_hdu_close_view (dada_hdu_t* hdu);

  /*! Read the next header from the struct */
  int dada_hdu_open (dada_hdu_t* hdu);

  /*! Return base addresses of data block buffers, nbufs and bufsz */
  char ** dada_hdu_db_addresses(dada_hdu_t * hdu, uint64_t * nbufs, uint64_t * bufsz);

  /*! Return base addresses of header block buffers, nbufs and bufsz */
  char ** dada_hdu_hb_addresses(dada_hdu_t * hdu, uint64_t * nbufs, uint64_t * bufsz);

#ifdef __cplusplus
	   }
#endif

#endif
