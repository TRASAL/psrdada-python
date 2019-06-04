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

TEST_SIZE = 524288
TEST_DATA = np.random.randint(0, 255, TEST_SIZE)


class TestReadWriteData(unittest.TestCase):
    """
    Test for reading and writing data.

    Start a ringbuffer instance and write some data, then read it back.
    """

    def setUp(self):
        """Test setup."""
        os.system("dada_db -d 2>/dev/null;dada_db -b %i -k dada" % (TEST_SIZE))

        self.writer = Writer()
        self.writer.connect(0xdada)

        self.reader = Reader()
        self.reader.connect(0xdada)

    def tearDown(self):
        """Test teardown."""
        self.writer.disconnect()
        self.reader.disconnect()

        os.system("dada_db -d -k dada 2> /dev/null")

    def test_writing_data(self):
        """
        Data reading and writing test.

        Read a previously written header from the ringbuffer,
        and test if the headers are equal.
        """
        # get a page from the writer, fill it with data, and mark it filled
        # test by manually calling the right methods in order
        page = self.writer.getNextPage()
        data = np.asarray(page)
        data[...] = TEST_DATA[...]
        self.writer.markEndOfData()
        self.writer.markFilled()

        page = self.reader.getNextPage()
        data = np.asarray(page)

        # compare the written and read data
        self.assertTrue((data == TEST_DATA).all())

        self.reader.markCleared()


class TestReadWriteDataIterator(unittest.TestCase):
    """
    Test for reading and writing data.

    Start a ringbuffer instance and write some data, then read it back.
    """

    def setUp(self):
        """Test setup."""
        os.system("dada_db -d 2>/dev/null;dada_db -b %i -k dada" % (TEST_SIZE))

        self.writer = Writer()
        self.writer.connect(0xdada)

        self.reader = Reader()
        self.reader.connect(0xdada)

    def tearDown(self):
        """Test teardown."""
        self.writer.disconnect()
        self.reader.disconnect()

        os.system("dada_db -d -k dada 2> /dev/null")

    def test_writing_data(self):
        """
        Data reading and writing test.

        Read a previously written header from the ringbuffer,
        and test if the headers are equal.
        """
        # get a page from the writer, fill it with data, and mark it filled
        for page in self.writer:
            data = np.asarray(page)
            data[...] = TEST_DATA[...]
            self.writer.markEndOfData()

        # get a page from the reader
        for page in self.reader:
            # test the Iterator interface for the reader class
            data = np.asarray(page)

            # compare the written and read data
            self.assertTrue((data == TEST_DATA).all())


if __name__ == '__main__':
    sys.exit(unittest.main())
