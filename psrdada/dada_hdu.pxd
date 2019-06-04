# file: dada_hdu.pxd

#include "ipcio.h"

# following: http://cython.readthedocs.io/en/latest/src/tutorial/clibraries.html#defining-external-declarations

ctypedef unsigned long int uint64_t

#cdef extern from "<sys/types.h>":
#    ctypedef struct key_t:
#        pass
ctypedef unsigned long key_t

cdef extern from "multilog.h":
    ctypedef struct multilog_t:
        pass

    multilog_t* multilog_open(const char* program_name, char syslog)
    int multilog_close(multilog_t* m)
    int multilog (multilog_t* m, int priority, const char* format, ...)
    # int multilog_add(multilog* m, FILE* fptr)

cdef extern from "ipcbuf.h":
    ctypedef struct ipcbuf_t:
        pass

    char* ipcbuf_get_next_read (ipcbuf_t* ipcbuf, uint64_t* bytes)
    char* ipcbuf_get_next_write (ipcbuf_t* ipcbuf)
    uint64_t ipcbuf_get_bufsz (ipcbuf_t* ipcbuf)
    int ipcbuf_mark_filled (ipcbuf_t* ipcbuf, uint64_t nbytes)
    int ipcbuf_mark_cleared (ipcbuf_t* ipcbuf)
    int ipcbuf_enable_eod (ipcbuf_t*)
    int ipcbuf_eod (ipcbuf_t*)
    int ipcbuf_reset (ipcbuf_t*)

cdef extern from "ipcio.h":
    ctypedef struct ipcio_t:
        pass

cdef extern from "dada_hdu.h":
    ctypedef struct dada_hdu_t:
        # The status and error logging interface
        multilog_t* log

        # The Data Block interface
        ipcio_t* data_block

        # The Header Block interface
        ipcbuf_t* header_block

        # The header
        char* header

        # The size of the header
        uint64_t header_size

        # The Data Block key
        key_t data_block_key

        # The Header Block key
        key_t header_block_key
     
    # Create a new DADA Header plus Data Unit
    dada_hdu_t* dada_hdu_create (multilog_t* log)

    # Set the key of the DADA Header plus Data Unit
    void dada_hdu_set_key (dada_hdu_t* hdu, key_t key)

    # Destroy a DADA Header plus Data Unit
    void dada_hdu_destroy (dada_hdu_t* hdu)

    # Connect the DADA Header plus Data Unit 
    int dada_hdu_connect (dada_hdu_t* hdu)

    # Connect the DADA Header plus Data Unit
    int dada_hdu_disconnect (dada_hdu_t* hdu)

    # Lock DADA Header plus Data Unit designated reader
    int dada_hdu_lock_read (dada_hdu_t* hdu)

    # Unlock DADA Header plus Data Unit designated reader
    int dada_hdu_unlock_read (dada_hdu_t* hdu)

    # Lock DADA Header plus Data Unit designated writer
    int dada_hdu_lock_write (dada_hdu_t* hdu)

    # Unlock DADA Header plus Data Unit designated writer
    int dada_hdu_unlock_write (dada_hdu_t* hdu)

    # Lock DADA Header plus Data Unit designated writer
    int dada_hdu_lock_write_spec (dada_hdu_t* hdu, char writemode)

    # Open the DADA Header plus Data Unit for passive viewing
    int dada_hdu_open_view (dada_hdu_t* hdu)

    # Close the DADA Header pluse Data Unit passive viewing mode
    int dada_hdu_close_view (dada_hdu_t* hdu)

    # Read the next header from the struct
    int dada_hdu_open (dada_hdu_t* hdu)

    # Return base addresses of data block buffers, nbufs and bufsz
    char ** dada_hdu_db_addresses(dada_hdu_t * hdu, uint64_t * nbufs, uint64_t * bufsz)

    # Return base addresses of header block buffers, nbufs and bufsz
    char ** dada_hdu_hb_addresses(dada_hdu_t * hdu, uint64_t * nbufs, uint64_t * bufsz)

