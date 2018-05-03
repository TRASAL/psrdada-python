# file: writer.pyx

cimport dada_hdu

cdef class Writer:
    cdef dada_hdu.key_t _key
    cdef dada_hdu.dada_hdu_t *_c_dada_hdu

    def __cinit__(self):
        print "Initializing _c_dada_hdu"
        self._c_dada_hdu = dada_hdu.dada_hdu_create(NULL)
        self._isConnected = False
        self._key = 0xdada
        self._header = {}

    def __dealloc__(self):
        print "Destroying _c_dada_hdu"
        dada_hdu.dada_hdu_destroy(self._c_dada_hdu)

    def setHeader(self, header):
        cdef char * c_string = NULL

        c_string = dada_hdu.ipcbuf_get_next_write (self._c_dada_hdu.header_block)
        bufsz = dada_hdu.ipcbuf_get_bufsz(self._c_dada_hdu.header_block)

        lines = []
        for key in header:
            # join key value on a space
            line = key + ' ' + header[key]
            lines.append(line)

        # join lines on newline
        py_string = '\n'.join(lines)

        c_string[:] = py_string

    def connect(self, key):
        """Connect to a PSR DADA ringbuffer with the specified key"""

        # store parameters on the Reader object
        self._key = key

        dada_hdu.dada_hdu_set_key(self._c_dada_hdu, self._key)

        if dada_hdu.dada_hdu_connect(self._c_dada_hdu) < 0:
            raise "ERROR in dada_hdu_connect"

        if dada_hdu.dada_hdu_lock_write(self._c_dada_hdu) < 0:
            raise "ERROR in dada_hdu_lock_write"

        self._isConnected = True

    def disconnect(self, key):
        """Disconnect from PRS DADA ringbuffer"""
        dada_hdu.dada_hdu_unlock_write(self._c_dada_hdu)
        dada_hdu.dada_hdu_disconnect(self._c_dada_hdu)
        self._isConnected = False


