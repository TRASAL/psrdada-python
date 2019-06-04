#!/usr/bin/env python
import sys
import os
import unittest
import numpy as np

from psrdada import Reader

reader = Reader()

reader.connect(0xdbda)

nheader = 0
while True:
    header = reader.getHeader()
    nheader += 1
    print(nheader, header)

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
