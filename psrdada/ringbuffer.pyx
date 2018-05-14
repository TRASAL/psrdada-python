# file: ringbuffer.pyx

cimport dada_hdu

from .exceptions import PSRDadaError

cdef class Ringbuffer:
    header = {}

    def __cinit__(self):
        self._c_dada_hdu = dada_hdu.dada_hdu_create(NULL)
        self._key = 0xdada

    def __dealloc__(self):
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

    def disconnect(self):
        """Disconnect from PRS DADA ringbuffer"""
        dada_hdu.dada_hdu_disconnect(self._c_dada_hdu)
        self.isConnected = False
