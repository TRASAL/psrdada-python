PSRDada Python
===============================
Python3 bindings to the ringbuffer implementation in [PSRDada](http://psrdada.sourceforge.net/)

It allows you to connect to a ringbuffer, read header info, and read/write ringbuffer pages as numpy arrays.
I aim to be interoperable, and bug-for-bug compattible, with PSRDADA.
It also supports reading multiple datasets form a ringbuffer for connecting to dada\_dbevent.

This is a proof-of-concept implementation, only functions from PSRDADA immediately necessary for my project will be implemented.
The code is inspired by the example code in the [cython userguide](http://cython.readthedocs.io/en/latest/src/userguide/buffer.html)

Error messages, together with a start-up and shutdown message, are written to the syslog.
Depending on your OS, you can read those using:

```bash
 $ grep psrdada-python /var/log/syslog
 $ journalctl --since "10 minutes ago" | grep psrdada-python
```

Documentation
-------------
See the two scripts in the examples directory, and the tests.

```python
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
    data = np.asarray(page)
    print(np.sum(data))

reader.disconnect()
```

Installation
------------
clone the repository  
    `git clone git@github.com:AA-ALERT/psrdada-python.git`
change into the top-level directory  
    `cd psrdada-python`  
install using  
    `make && make install`

Dependencies
------------
 * Python 3.5
 * PSRDada dada\_db exectuable in the PATH

License
-------
Copyright (c) 2018, Jisk Attema

Apache Software License 2.0

Contributing
------------
Contributing authors so far:
* Jisk Attema


