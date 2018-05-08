# -*- coding: utf-8 -*-
"""
Python bindings to the PSR Dada library.

This package provides a minimal interface to the PSRDada library.
Exported are the Reader and Writer classes to connect with
psrdada ring buffers.
"""

from psrdada.reader import Reader
from psrdada.writer import Writer

__author__ = 'Jisk Attema'
__email__ = 'j.attema@esciencecenter.nl'

__all__ = ['Reader', 'Writer']
