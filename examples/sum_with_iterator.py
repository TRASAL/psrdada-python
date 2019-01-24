#!/usr/bin/env python3
import numpy as np
from psrdada import Reader

# Create a reader instace
reader = Reader()

# Connect to a running ringbuffer with key 'dada'
reader.connect(0xdada)

# loop over the pages
for page in reader:
    # read the page as numpy array
    data = np.asarry(page)
    print (np.sum(data))

reader.disconnect()
