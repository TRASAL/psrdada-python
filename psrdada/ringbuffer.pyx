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

    def getbufstats(self):
        """Get monitoring statistics
           Based on dada_dbmonitor"""

        # These 2 pointers can be moved to the __cinit__
        cdef dada_hdu.ipcbuf_t * _c_hb = self._c_dada_hdu.header_block
        cdef dada_hdu.ipcbuf_t * _c_db =\
                <dada_hdu.ipcbuf_t*> self._c_dada_hdu.data_block

        # Number of header buffers
        hdr_nbufs  = dada_hdu.ipcbuf_get_nbufs(_c_hb)

        # Number of data buffers
        data_nbufs = dada_hdu.ipcbuf_get_nbufs(_c_db)

        # Number of data readers
        n_readers  = dada_hdu.ipcbuf_get_nreaders(_c_db)

        # Get header statistics
        bufs_written = dada_hdu.ipcbuf_get_write_count(_c_hb)
        bufs_read    = dada_hdu.ipcbuf_get_read_count(_c_hb)
        full_bufs    = dada_hdu.ipcbuf_get_nfull(_c_hb)
        clear_bufs   = dada_hdu.ipcbuf_get_nclear(_c_hb)
        available_bufs = (hdr_nbufs - full_bufs)

        hdr_stats = dict()
        hdr_stats['FREE']   = available_bufs
        hdr_stats['FULL']   = full_bufs
        hdr_stats['CLEAR']  = clear_bufs
        hdr_stats['NWRITE'] = bufs_written
        hdr_stats['NREAD']  = bufs_read

        # Get data statistics
        data_stats_list = list() # a dictionary for every reader
        for iread in range(n_readers):
            bufs_written = dada_hdu.ipcbuf_get_write_count(_c_db)
            bufs_read    = dada_hdu.ipcbuf_get_read_count_iread(_c_db, iread)
            full_bufs    = dada_hdu.ipcbuf_get_nfull_iread(_c_db, iread)
            clear_bufs   = dada_hdu.ipcbuf_get_nclear_iread(_c_db, iread)
            available_bufs = (data_nbufs - full_bufs)

            data_stats = dict()
            data_stats['FREE']   = available_bufs
            data_stats['FULL']   = full_bufs
            data_stats['CLEAR']  = clear_bufs
            data_stats['NWRITE'] = bufs_written
            data_stats['NREAD']  = bufs_read
            data_stats_list.append(data_stats)

        return hdr_stats, data_stats_list
