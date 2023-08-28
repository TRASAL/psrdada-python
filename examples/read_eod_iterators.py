#!/usr/bin/env python
"""
Show how to read multiple datasets, separated by EODs.

Version with iterors.
"""
import numpy as np

from psrdada import Reader


reader = Reader(0xdada)

nheader = 0
done = False
while not done:
    nheader += 1
    header = reader.getHeader()

    del header['__RAW_HEADER__']  # prettier output

    print(nheader, header)
    if 'QUIT' in header.keys():
        print('Header contains the QUIT key, so we quit')
        done = True
    else:
        # loop over the pages until EOD is encountered
        # the EOD flag is (like mark_cleared etc.) automagically kept in sync
        npages = 0
        for page in reader:
            npages += 1
            data = np.asarray(page)
            print("page:", npages, np.sum(data))

reader.disconnect()
