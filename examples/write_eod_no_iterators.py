#!/usr/bin/env python
"""
Show how to send multiple datasets, separated by EODs.

Version without iterors
"""
import time
from random import randint, choice
from string import ascii_lowercase
import numpy as np

from psrdada import Writer


def random_string(length=10):
    """Generate a random string of given length."""
    return ''.join(choice(ascii_lowercase) for i in range(length))


writer = Writer(0xdada)

# send 10 datasets, separated by an EOD
for ndataset in range(10):
    npages = randint(1, 10)

    # setting a new header also resets the buffer: isEndOfData = False
    writer.setHeader({
        'DATASET': str(ndataset),
        'PAGES': str(npages),
        'MAGIC': random_string(20)
    })
    print(writer.header)

    for npage in range(npages):
        page = writer.getNextPage()
        data = np.asarray(page)
        data.fill(npage)

        # marking a page filled will send it on the ringbuffer,
        # and then we cant set the 'EndOfData' flag anymore.
        # so we need to treat the last page differently
        if npage < npages - 1:
            writer.markFilled()
        else:
            time.sleep(0.5)

    # mark the last page with EOD flag
    # this will also mark it filled, and send it
    writer.markEndOfData()


# Send a message to the reader that we're done
writer.setHeader({'QUIT': 'YES'})

writer.disconnect()
