# -*- coding: utf-8 -*-
"""
Python bindings to the PSR Dada library.

This package provides a minimal interface to the PSRDada library.
Exported are the Reader and Writer classes to connect with
psrdada ring buffers.

Ringbuffers are used to process large data streams, in our case data generated
by radio telescopes.
A writer and (mulitple) readers can connect to the buffer and read, process,
and write data with a minimum of data copies.
This library exposes the ringbuffer as a Cython memory view, which you can then
interact with via fi. numpy.

Use cases are:
    * rapid prototyping
    * a glue layer to run CUDA kernels
    * interactive use of the telescope
"""
from psrdada.reader import Reader
from psrdada.writer import Writer
from .__version__ import __version__

__author__ = 'Jisk Attema'
__email__ = 'j.attema@esciencecenter.nl'

__all__ = ['Reader', 'Writer']
__version__ = '0.1.0'
