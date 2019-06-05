# file: reader.pyx
"""
Reader class.

Implements reading header and data from a running PSRDada ringbuffer.
"""
from cpython.buffer cimport PyBUF_READ
cimport dada_hdu
from .ringbuffer cimport Ringbuffer

import re
from .exceptions import PSRDadaError
from .ringbuffer import Ringbuffer

cdef extern from "<Python.h>":
    ctypedef struct PyObject:
        pass
    PyObject *PyMemoryView_FromMemory(char *mem, Py_ssize_t size, int flags)

cdef class Reader(Ringbuffer):
    """
    Reader class.

    Implements reading header and data from a running PSRDada ringbuffer.
    """
    def __init__(self):
        self.isEndOfData = False
        self.isHoldingPage = False

    def connect(self, key):
        """
        Connect to a PSRDada ringbuffer with the specified key, and lock it for reading.

        :param key: Identifier of the ringbuffer, typically 0xdada
        """
        super().connect(key)

        if dada_hdu.dada_hdu_lock_read(self._c_dada_hdu) < 0:
            raise PSRDadaError("ERROR in dada_hdu_lock_read")

        self.isEndOfData = dada_hdu.ipcbuf_eod (<dada_hdu.ipcbuf_t *> self._c_dada_hdu.data_block)

    def disconnect(self):
        """Disconnect from the PSRDada ringbuffer"""

        # if we are still holding a page, mark it cleared
        cdef dada_hdu.ipcbuf_t *ipcbuf = <dada_hdu.ipcbuf_t *> self._c_dada_hdu.data_block
        if self.isHoldingPage:
            dada_hdu.ipcbuf_mark_cleared (ipcbuf)
            self.isHoldingPage = False

        dada_hdu.dada_hdu_unlock_read(self._c_dada_hdu)

        super().disconnect()

    def getHeader(self):
        """
        Read a header from the ringbuffer and return it as a dict.

        We reimplement the parsing logic from the PSRDada code:
         * A page is read from the PSRDada header block, and parsed line by line.
           The page must contain ASCII text.
         * Each line (separated by newlines or backslashes) contains a
           key-value pair (separated by tabs or spaces).

        The original header is also added to the dict using the special key *__RAW_HEADER__*
        The read is blocking; it will wait for a header page to become available.
        If EndOfData has been set on the buffer, it is cleared and the buffer is reset.

        .. note:: The last-read header is available as *reader.header*.

        :returns: A dict
        """
        # read and parse the header
        cdef dada_hdu.uint64_t bufsz
        cdef char * c_string = NULL

        c_string = NULL
        while c_string == NULL:
            c_string = dada_hdu.ipcbuf_get_next_read(self._c_dada_hdu.header_block, &bufsz)
            if not bufsz:
                dada_hdu.ipcbuf_mark_cleared (self._c_dada_hdu.header_block)

                isEndOfHeaderData = dada_hdu.ipcbuf_eod (<dada_hdu.ipcbuf_t *> self._c_dada_hdu.header_block)
                if isEndOfHeaderData:
                    dada_hdu.ipcbuf_reset(<dada_hdu.ipcbuf_t *> self._c_dada_hdu.header_block)

        py_string = c_string[:bufsz].decode('ascii')
        dada_hdu.ipcbuf_mark_cleared (self._c_dada_hdu.header_block)

        # remove all keys from the old header (if present)
        self.header = dict()
        self.header['__RAW_HEADER__'] = py_string

        # split lines on newline and backslash
        lines = re.split('\n|\\\\', py_string)

        for line in lines:
            # split key value on tab and space
            keyvalue = line.replace('\t', ' ').split(' ', 1)
            if len(keyvalue) == 2:
                self.header[keyvalue[0]] = keyvalue[1]

        return self.header

    def getNextPage(self):
        """
        Return a memoryview on the next available ringbuffer page.

        The read is blocking; it will wait for a page to become available.

        .. note:: The view is readonly, and a direct mapping of the ringbuffer page.
                  So, no memory copies, and no garbage collector.

        :returns: a PyMemoryView
        """

        if self.isHoldingPage:
            raise PSRDadaError("Error in getNextPage: previous page not cleared.")

        cdef char * c_page = dada_hdu.ipcbuf_get_next_read (<dada_hdu.ipcbuf_t *> self._c_dada_hdu.data_block, &self._bufsz)
        if c_page == NULL:
            raise PSRDadaError("Error in getNextPage: NULL page read")

        self.isHoldingPage = True

        return <object> PyMemoryView_FromMemory(c_page, self._bufsz, PyBUF_READ)

    def markCleared(self):
        """
        Release a page back to the ringbuffer.

        Mark the current page as cleared, so it can be reused by the ringbuffer.
        This is called automatically when iterating over the ringbuffer.
        """
        if self.isHoldingPage:
            dada_hdu.ipcbuf_mark_cleared (<dada_hdu.ipcbuf_t *> self._c_dada_hdu.data_block)

        self.isHoldingPage = False
        self.isEndOfData = dada_hdu.ipcbuf_eod (<dada_hdu.ipcbuf_t *> self._c_dada_hdu.data_block)

        if self.isEndOfData:
            # this is necessary to be able to restart reading later
            dada_hdu.ipcbuf_reset(<dada_hdu.ipcbuf_t *> self._c_dada_hdu.data_block)

    def __iter__(self):
        return self

    def __next__(self):
        # when not using iterators, the user shoud issue markCleared(),
        # and reset the isEndOfData flag at the right moment.
        self.markCleared()

        if self.isEndOfData:
            self.isEndOfData = False
            raise StopIteration

        return self.getNextPage()
