# file: ringbuffer.pyx

cimport dada_hdu

from .exceptions import PSRDadaError

cdef class Ringbuffer:
    def __cinit__(self):
        self._multilog = dada_hdu.multilog_open("psrdada-python", 1)
        dada_hdu.multilog(self._multilog, 1, "psrdada-python starting")
        # multilog_add(self._multilog, FILE*fptr)

        self._c_dada_hdu = dada_hdu.dada_hdu_create(self._multilog)
        self._key = 0xdada
        self.isConnected = False
        self.isHoldingPage = False
        self.isEndOfData = False
        self.header = dict()

    def __dealloc__(self):
        dada_hdu.multilog(self._multilog, 1, "psrdada-python shutting down")
        dada_hdu.multilog_close(self._multilog)
        self._multilog = NULL
        dada_hdu.dada_hdu_destroy(self._c_dada_hdu)
        if self.isConnected:
            raise PSRDadaError("Ringbuffer destroyed without disconnecting first")

    def connect(self, key):
        """Connect to a PSR DADA ringbuffer with the specified key"""
        if self.isConnected:
            raise PSRDadaError("Ringbuffer has already been connected")

        # store parameters on the Reader object
        self._key = key

        dada_hdu.dada_hdu_set_key(self._c_dada_hdu, self._key)

        if dada_hdu.dada_hdu_connect(self._c_dada_hdu) < 0:
            raise PSRDadaError("ERROR in dada_hdu_connect")

        self.isConnected = True
        self.isHoldingPage = False

    def disconnect(self):
        """Disconnect from PRS DADA ringbuffer"""
        dada_hdu.dada_hdu_disconnect(self._c_dada_hdu)
        self.isConnected = False
