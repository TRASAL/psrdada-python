#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
PSRDada data test.

Tests for handling data using the Reader and Writer classess from
the `psrdada` module.
"""

import sys
import os
import unittest
import numpy as np

from psrdada import Reader
from psrdada import Writer


class TestReadWriteData(unittest.TestCase):
    """
    Test for reading and writing data.

    Start a ringbuffer instance and write some data, then read it back.
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

    def test_writing_data(self):
        """
        Data reading and writing test.

        Read a previously written header from the ringbuffer,
        and test if the headers are equal.
        """
        # get a page from the writer, fill it with data, and mark it filled
        page = self.writer.getNextPage()
        data = np.asarray(page)
        test_data = np.random.randint(0, 255, len(data))
        data[...] = test_data[...]
        self.writer.markFilled()

        # get a page from the reader
        page = self.reader.getNextPage()
        data = np.asarray(page)

        # compare the written and read data
        self.assertTrue((data == test_data).all())

if __name__ == '__main__':
    sys.exit(unittest.main())
