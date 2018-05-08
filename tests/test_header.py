#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
PSRDada header test.

Tests for handling header data using the Reader and Writer classess from
the `psrdada` module.
"""

import sys
import os
import unittest

from psrdada import Reader
from psrdada import Writer

class TestReadWriteData(unittest.TestCase):
    """
    Test for reading and writing header data

    Start a ringbuffer instance and write some data to the header block, then read it back.
    """

    def setUp(self):
        """Test setup."""
        os.system("dada_db -d ; dada_db")

        self.writer = Writer()
        self.writer.connect(0xdada)

        self.reader = Reader()
        self.reader.connect(0xdada)

    def tearDown(self):
        """Test teardown."""
        self.writer.disconnect()
        self.reader.disconnect()

    def test_writing_header(self):
        """
        Header reading and writing test.

        Read a previously written header from the ringbuffer,
        and test if the headers are equal.
        """
        self.writer.setHeader(HEADER_TEST_DATA)
        header = self.reader.getHeader()

        self.assertDictEqual(header, HEADER_TEST_DATA)

if __name__ == '__main__':
    sys.exit(unittest.main())
