#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
test_psrdada_python
----------------------------------

Tests for passing header data using the Reader and Writer classess from the `psrdada` module.
"""

# Note that tests first are sorted alfabetically, so be careful of the test names.
# We follow the 'init' style to facilitate ordering

import sys
import os
import unittest

from psrdada import Reader
from psrdada import Writer

header_test_data = {
        "program": "PSRDada",
        "description": "Python bindings to PSRDada"
        }

class TestPsrdada_10_writer(unittest.TestCase):
    """Start a ringbuffer instance and write some data to the header block"""

    def setUp(self):
        os.system("dada_db -d ; dada_db")
        self.writer = Writer()
        self.writer.connect(0xdada)

    def tearDown(self):
        self.writer.disconnect()

    def test_writing_header(self):
        header = self.writer.setHeader(header_test_data)

class TestPsrdada_20_reader(unittest.TestCase):
    """Read data from the header block"""

    def setUp(self):
        self.reader = Reader()
        self.reader.connect(0xdada)

    def tearDown(self):
        self.reader.disconnect()
        os.system("dada_db -d")

    def test_reading_header(self):
        header = self.reader.getHeader()
        self.assertDictEqual(header, header_test_data)

if __name__ == '__main__':
    sys.exit(unittest.main())
