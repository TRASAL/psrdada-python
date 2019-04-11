PSRDada Python
===============================
Python3 bindings to the ringbuffer implementation in PSRDada

This is a proof-of-concept implementation, only functions immediately necessary for my project will be implemented.
The code is inspired by the example code in the [cython userguide](http://cython.readthedocs.io/en/latest/src/userguide/buffer.html)

Documentation
-------------
Work in progress.

Installation
------------
clone the repository  
    `git clone git@github.com:AA-ALERT/psrdada-python.git`
change into the top-level directory  
    `cd psrdada-python`  
set the correct paths for psrdada in setup.py
install using  
    `make && make install`

Dependencies
------------
 * Python 3.5
 * PSRDada dada\_db exectuable in the PATH
 * PSRDada libpsrdada.so in LD\_LIBRARY\_PATH

Note: see the `make_libpsrdada.sh` script on how to get that library

License
-------
Copyright (c) 2018, Jisk Attema

Apache Software License 2.0

Contributing
------------
Contributing authors so far:
* Jisk Attema


