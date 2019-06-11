#!/usr/bin/env python3
"""Example program showing how to read from a ringbuffer."""
import numpy as np
from psrdada import Reader


def read_untill_end():
    """Connect, read all pages, and disconnect from a ringbuffer."""
    # Create a reader instace and connect to a running ringbuffer
    reader = Reader(0xdada)

    # loop over the pages until EOD is encountered
    while not reader.isEndOfData:
        # read the page as numpy array
        page = reader.getNextPage()

        data = np.asarray(page)
        print(np.sum(data))

        reader.markCleared()

    reader.disconnect()


if __name__ == '__main__':
    read_untill_end()
