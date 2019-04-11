#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
Setup script for the PSRDada python bindings.

Build and install the package using distutils.
"""

# pylint: disable=all
from Cython.Build import cythonize
from setuptools import setup
from distutils.extension import Extension

with open('README.md') as readme_file:
    README = readme_file.read()

with open('VERSION') as version_file:
    PROJECT_VERSION = version_file.read()

PSRDADA_LIBDIR = "/usr/lib"
PSRDADA_LIBS = "psrdada"
PSRDADA_INCLUDE = "/usr/include"

EXTENSIONS = [
    Extension(
        "psrdada.ringbuffer",
        ["psrdada/ringbuffer.pyx"],
        include_dirs=[PSRDADA_INCLUDE],
        libraries=[PSRDADA_LIBS],
        library_dirs=[PSRDADA_LIBDIR]),
    Extension(
        "psrdada.reader",
        ["psrdada/reader.pyx"],
        include_dirs=[PSRDADA_INCLUDE],
        libraries=[PSRDADA_LIBS],
        library_dirs=[PSRDADA_LIBDIR]),
    Extension(
        "psrdada.writer",
        ["psrdada/writer.pyx"],
        include_dirs=[PSRDADA_INCLUDE],
        libraries=[PSRDADA_LIBS],
        library_dirs=[PSRDADA_LIBDIR]),
]

setup(
    name='psrdada',
    version=PROJECT_VERSION,
    description="Python3 bindings to the ringbuffer implementation in PSRDada",
    long_description=README + '\n\n',
    author="Jisk Attema",
    author_email='j.attema@esciencecenter.nl',
    url='https://github.com/NLeSC/psrdada-python',
    packages=[
        'psrdada',
    ],
    package_dir={'psrdada':
                 'psrdada'},
    include_package_data=True,
    license="Apache Software License 2.0",
    zip_safe=False,
    keywords='psrdada',
    classifiers=[
        'Development Status :: 2 - Pre-Alpha',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: Apache Software License',
        'Natural Language :: English',
        "Programming Language :: Python :: 2",
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.5',
    ],
    test_suite='tests',
    ext_modules=cythonize(EXTENSIONS),
)
