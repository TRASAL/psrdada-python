# -*- coding: utf-8 -*-
"""Python bindings to the PSR Dada library"""

__author__ = 'Jisk Attema'
__email__ = 'j.attema@esciencecenter.nl'

from psrdada.reader import Reader
from psrdada.writer import Writer

__all__  = ['Reader','Writer']
