#!/usr/bin/env python
"""
Show how to send multiple datasets, separated by EODs.

Version with iterors.
"""
import time
from random import randint, choice
from string import ascii_lowercase
import numpy as np

from psrdada import Writer


def random_string(length=10):
    """Generate a random string of given length."""
    return ''.join(choice(ascii_lowercase) for i in range(length))


writer = Writer()

writer.connect(0xdbda)

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

    npage = 0
    for page in writer:
        data = np.asarray(page)
        data.fill(npage)
        npage += 1

        # at the last iteration, mark the page with EOD;
        # this will also raise a StopIteration exception
        if npage == npages:
            writer.markEndOfData()
        else:
            # like this, you can see better what happens using dada_dbmonitor
            time.sleep(0.5)

# Send a message to the reader that we're done
writer.setHeader({'QUIT': 'YES'})

writer.disconnect()
