# file: reader.pyx

import re
cimport dada_hdu

from .exceptions import PSRDadaError

cdef class Reader:
    cdef dada_hdu.key_t _key
    cdef dada_hdu.dada_hdu_t *_c_dada_hdu
    header = {}

    def __cinit__(self):
        print "Initializing _c_dada_hdu"
        self._c_dada_hdu = dada_hdu.dada_hdu_create(NULL)
        self._key = 0xdada

    def __dealloc__(self):
        print "Destroying _c_dada_hdu"
        dada_hdu.dada_hdu_destroy(self._c_dada_hdu)

    def connect(self, key):
        """Connect to a PSR DADA ringbuffer with the specified key"""

        # store parameters on the Reader object
        self._key = key

        dada_hdu.dada_hdu_set_key(self._c_dada_hdu, self._key)

        if dada_hdu.dada_hdu_connect(self._c_dada_hdu) < 0:
            raise PSRDadaError("ERROR in dada_hdu_connect")

        if dada_hdu.dada_hdu_lock_read(self._c_dada_hdu) < 0:
            raise PSRDadaError("ERROR in dada_hdu_lock_read")

    def getHeader(self):
        # read and parse the header
        cdef dada_hdu.uint64_t bufsz
        cdef char * c_string = NULL

        c_string = dada_hdu.ipcbuf_get_next_read (self._c_dada_hdu.header_block, &bufsz)
        py_string = c_string[:bufsz].decode('ascii')

        dada_hdu.ipcbuf_mark_cleared (self._c_dada_hdu.header_block)

        # split lines on newline and backslash
        lines = re.split('\n|\\\\', py_string)

        for line in lines:
            # split key value on tab and space
            keyvalue = line.replace('\t', ' ').split(' ', 1)
            if len(keyvalue) == 2:
                self.header[keyvalue[0]] = keyvalue[1]

        return self.header

    def disconnect(self):
        """Disconnect from PRS DADA ringbuffer"""
        dada_hdu.dada_hdu_unlock_read(self._c_dada_hdu)
        dada_hdu.dada_hdu_disconnect(self._c_dada_hdu)


