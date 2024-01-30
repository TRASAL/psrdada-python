# file: viewer.pyx
"""
Viewer class.

Implements passive viewing of a running PSRDada ringbuffer for monitoring 
purposes
Create a new viewer instance::

    viewer = Viewer()

Create a new viewe instance, and connect it directly to a running ringbuffer::

    viewer = Viewer(0xdada)

"""
from . cimport dada_hdu
from .ringbuffer cimport Ringbuffer

from .exceptions import PSRDadaError


cdef class Viewer(Ringbuffer):
    """
    Viewer class.

    Implements monitoring buffer statistics from a running PSRDada ringbuffer.
    """
    cdef dada_hdu.ipcbuf_t *_c_hb
    cdef dada_hdu.ipcbuf_t *_c_db
    cdef readonly int hdr_nbufs
    cdef readonly int data_nbufs
    cdef readonly int n_readers
    def __init__(self, key=None):
        """
        Creates a new Viewer instance, and optionally connects to a ringbuffer

        :param key: Optional. Hex identifier of the ring buffer
        """

        self.hdr_nbufs = 0
        self.data_nbufs = 0
        self.n_readers = 0

        if key:
            self.connect(key)

    def connect(self, key):
        """
        Passively connect to a PSRDada ring buffer with the specified key

        :param key: Hex identifier of the ring buffer
        """
        super().connect(key)

        self._populate_stats()


    cdef _populate_stats(self):
        """Populate internal ringbuffer variables for later monitoring"""
        # Pointers to IPCbuf 
        self._c_hb = self._c_dada_hdu.header_block
        self._c_db = <dada_hdu.ipcbuf_t*> self._c_dada_hdu.data_block

        # Number of header buffers
        self.hdr_nbufs = dada_hdu.ipcbuf_get_nbufs(self._c_hb)

        # Number of data buffers
        self.data_nbufs = dada_hdu.ipcbuf_get_nbufs(self._c_db)

        # Number of data readers
        self.n_readers = dada_hdu.ipcbuf_get_nreaders(self._c_db)


    def getbufstats(self):
        """
        Get monitoring statistics. This 
        subroutine is based on *psrdada*'s *dada_dbmonitor*

        This call can be made in a loop for consistent monitoring

        .. note:: Every dict returned by this function has 5 keys:

                  - *FREE*: Current number of free header/data buffers
                  - *FULL*: Current number of full header/data buffers
                  - *CLEAR*: Current number of header/data buffers cleared for
                    writing
                  - *NWRITE*: Total number of header/data pages that 
                    have been written
                  - *NREAD*: Total number of header/data pages that have 
                    been read
        
        :returns: 2-element tuple containing:

                  * A dict for header buffer statistics

                  * A list of dict for data buffer statistics. The list is 
                    for every reader connected to the ringbuffer

        """
        if not self.isConnected:
            raise PSRDadaError("Error in getbufstats: not connected")
        # Get header statistics
        bufs_written = dada_hdu.ipcbuf_get_write_count(self._c_hb)
        bufs_read    = dada_hdu.ipcbuf_get_read_count(self._c_hb)
        full_bufs    = dada_hdu.ipcbuf_get_nfull(self._c_hb)
        clear_bufs   = dada_hdu.ipcbuf_get_nclear(self._c_hb)
        available_bufs = (self.hdr_nbufs - full_bufs)

        hdr_stats = dict()
        hdr_stats['FREE']   = available_bufs
        hdr_stats['FULL']   = full_bufs
        hdr_stats['CLEAR']  = clear_bufs
        hdr_stats['NWRITE'] = bufs_written
        hdr_stats['NREAD']  = bufs_read

        # Get data statistics
        data_stats_list = list() # a dictionary for every reader
        for iread in range(self.n_readers):
            bufs_written = dada_hdu.ipcbuf_get_write_count(self._c_db)
            bufs_read    = dada_hdu.ipcbuf_get_read_count_iread(self._c_db, 
                    iread)
            full_bufs    = dada_hdu.ipcbuf_get_nfull_iread(self._c_db, 
                    iread)
            clear_bufs   = dada_hdu.ipcbuf_get_nclear_iread(self._c_db, 
                    iread)
            available_bufs = (self.data_nbufs - full_bufs)

            data_stats = dict()
            data_stats['FREE']   = available_bufs
            data_stats['FULL']   = full_bufs
            data_stats['CLEAR']  = clear_bufs
            data_stats['NWRITE'] = bufs_written
            data_stats['NREAD']  = bufs_read
            data_stats_list.append(data_stats)

        return hdr_stats, data_stats_list
