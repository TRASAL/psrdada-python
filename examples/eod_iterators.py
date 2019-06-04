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

    # loop over the pages until EOD is encountered
    # the EOD flag is (like mark_cleared etc.) automagically kept in sync
    npages = 0
    for page in reader:
        npages += 1
        data = np.asarray(page)
        print("page:", npages, np.sum(data))
