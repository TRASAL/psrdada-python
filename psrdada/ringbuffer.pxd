# file: ringbuffer.pxd

cimport dada_hdu

cdef class Ringbuffer:
    cdef dada_hdu.key_t _key
    cdef dada_hdu.dada_hdu_t *_c_dada_hdu
    cpdef bint isConnected
    cdef dada_hdu.uint64_t _bufsz
