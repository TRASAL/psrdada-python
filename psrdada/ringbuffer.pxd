# file: ringbuffer.pxd

cimport dada_hdu

cdef class Ringbuffer:
    cdef dada_hdu.key_t _key
    cdef dada_hdu.dada_hdu_t *_c_dada_hdu
    cdef dada_hdu.multilog_t *_multilog
    cdef dada_hdu.uint64_t _bufsz
    cdef public bint isConnected
    cdef public bint isHoldingPage
    cdef public bint isEndOfData
    cdef public dict header
