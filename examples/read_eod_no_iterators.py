#!/usr/bin/env python
"""
Show how to read multiple datasets, separated by EODs.

Version with iterors.
"""
import numpy as np

from psrdada import Reader

reader = Reader(0xdada)

nheader = 0
while True:
    header = reader.getHeader()
    nheader += 1

    del header['__RAW_HEADER__']  # prettier output
    print(nheader, header)

    if 'QUIT' in reader.header.keys():
        print('Header contains the QUIT key, so we quit')
        break

    # we iterate manually through the buffer,
    # so we also need to manually reset the eod flag
    reader.isEndOfData = False

    # loop over the pages until EOD is encountered
    npages = 0
    while not reader.isEndOfData:
        # read the page as numpy array
        page = reader.getNextPage()

        npages += 1
        data = np.asarray(page)
        print("page:", npages, np.sum(data))

        reader.markCleared()

reader.disconnect()
