# file: writer.pyx
"""
Writer class.

Implements writing header and data from a running PSRDada ringbuffer.
Create a new writer instance::

    writer = Writer()

Create a new reader instance, and connect it directly to a running ringbuffer::

    writer = Writer(0xdada)
"""

from cpython.buffer cimport PyBUF_WRITE
cimport dada_hdu
from .ringbuffer cimport Ringbuffer

from .exceptions import PSRDadaError

cdef extern from "<Python.h>":
    ctypedef struct PyObject:
        pass
    PyObject *PyMemoryView_FromMemory(char *mem, Py_ssize_t size, int flags)

cdef extern from "<string.h>":
    char *strncpy(char *dest, const char *src, size_t n)

cdef class Writer(Ringbuffer):
    """
    Writer class.

    Implements writing header and data to a running PSRDada ringbuffer.
    """
    def __init__(self, key=None):
        """
        Creates a new Reader instance, and optionally connects to a ringbuffer.

        :param key: Optional. Identifier of the ringbuffer, typically 0xdada
        """
        if key:
            self.connect(key)

    def connect(self, key):
        """
        Connect to a PSR DADA ringbuffer with the specified key, and lock it for writing

        :param key: Identifier of the ringbuffer, typically 0xdada
        """
        super().connect(key)

        if dada_hdu.dada_hdu_lock_write(self._c_dada_hdu) < 0:
            raise PSRDadaError("ERROR in dada_hdu_lock_write")

    def disconnect(self):
        """Disconnect from PSRDada ringbuffer"""
        if not self.isEndOfData:
            dada_hdu.dada_hdu_unlock_write(self._c_dada_hdu)

        super().disconnect()

    def markEndOfData(self):
        """
        Set the EOD (end-of-data) flag, and mark the page as filled.

        .. note :: This will also raise a StopIteration exception when using
                   iterators.

        .. seealso:: markFilled
        """
        if not self.isConnected:
            raise PSRDadaError("Error in setEndOfData: not connected")

        if not self.isHoldingPage:
            raise PSRDadaError("Error in setEndOfData: not writing")

        if dada_hdu.ipcbuf_enable_eod(<dada_hdu.ipcbuf_t *> self._c_dada_hdu.data_block) < 0:
            raise PSRDadaError("Error in setEndOfData: cannot mark end of data")

        self.isEndOfData = True
        self.markFilled()

        # like dada_dbevent, unlock to ensure EOD is written
        dada_hdu.dada_hdu_unlock_write(self._c_dada_hdu)

    def setHeader(self, header):
        """
        Write a dict to a ringbuffer header page.

        We reimplement the parsing logic from the PSRDada code:
         * Each key-value pair in the dict is formatted as an ASCII string,
           using a ' ' as separator;
         * All strings are concattenated (with an additional newline)
           and written to the header page.

        The write is blocking; it will wait for a header page to become available.
        If EndOfData has been set on the buffer, it is cleared and the buffer is reset.

        .. note:: The last-written header is available as *writer.header*.

        :returns: A dict
        """
        # when sending a header, assume we will also send data later
        # this also simplifies sending multiple dataset
        if self.isEndOfData:
            # recover from a previous EOD signal
            dada_hdu.dada_hdu_lock_write(self._c_dada_hdu)
            dada_hdu.ipcbuf_reset(<dada_hdu.ipcbuf_t *> self._c_dada_hdu.data_block)
            self.isEndOfData = False

        cdef char * c_string = dada_hdu.ipcbuf_get_next_write (self._c_dada_hdu.header_block)
        bufsz = dada_hdu.ipcbuf_get_bufsz(self._c_dada_hdu.header_block)

        # remove all keys from the old header (if present)
        self.header = dict()

        lines = []
        for key in header:
            # join key value on a space
            line = key + ' ' + header[key]
            lines.append(line)

            # keep a copy for the Writer class
            self.header[key] = header[key]

        if len(header) > 0:
            # join lines on newline, convert to ascii bytes
            # and add a final newline
            py_string = ('\n'.join(lines) + '\n').encode('ascii')
        else:
            py_string = ''.encode('ascii')

        # copy to the header page and done
        strncpy(c_string, py_string, bufsz)
        dada_hdu.ipcbuf_mark_filled(self._c_dada_hdu.header_block, len(py_string))

    def getNextPage(self):
        """
        Return a memoryview on the next available ringbuffer page.

        The call is blocking; it will wait for a page to become available.
        Use *data = np.asarray(page)* to convert it to something more useable.

        .. note:: The view is a direct mapping of the ringbuffer page.
                  So, no memory copies, and no garbage collector.

        :returns: a PyMemoryView
        """
        if self.isEndOfData:
            # recover from a previous EOD signal
            dada_hdu.dada_hdu_lock_write(self._c_dada_hdu)
            dada_hdu.ipcbuf_reset(<dada_hdu.ipcbuf_t *> self._c_dada_hdu.data_block)
            self.isEndOfData = False

        if self.isHoldingPage:
            raise PSRDadaError("Error in getNextPage: previous page not cleared.")

        cdef dada_hdu.ipcbuf_t *ipcbuf = <dada_hdu.ipcbuf_t *> self._c_dada_hdu.data_block
        cdef char * c_page = dada_hdu.ipcbuf_get_next_write (ipcbuf)
        self.isHoldingPage = True

        self._bufsz = dada_hdu.ipcbuf_get_bufsz(ipcbuf)
        return <object> PyMemoryView_FromMemory(c_page, self._bufsz, PyBUF_WRITE)

    def markFilled(self):
        """
        Release a page back to the ringbuffer.

        Mark the current page as filled and and return it to the ringbuffer.
        This is called automatically when iterating over the ringbuffer.

        .. seealso:: markEndOfData
        """
        if self.isHoldingPage:
            dada_hdu.ipcbuf_mark_filled (<dada_hdu.ipcbuf_t *> self._c_dada_hdu.data_block, self._bufsz)

        self.isHoldingPage = False

    def __iter__(self):
        return self

    def __next__(self):
        self.markFilled()

        if self.isEndOfData:
            raise StopIteration

        return self.getNextPage()
