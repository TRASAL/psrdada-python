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
from os import environ, path

with open('README.md') as readme_file:
    README = readme_file.read()

with open(path.join('psrdada', '__version__.py')) as version_file:
    version = {}
    exec(version_file.read(), version)
    PROJECT_VERSION = version['__version__']

# Get the header locations from the environment
INCLUDE_DIRS = []
if "CPATH" in environ:
    flags = environ["CPATH"].split(':')
    for flag in flags:
        # when usingn spack, there is no -I prefix
        INCLUDE_DIRS.append(flag)

if "CFLAGS" in environ:
    flags = environ["CFLAGS"].split(' ')
    for flag in flags:
        if flag[0:2] == '-I':
            # when usingn spack, there is no -I prefix
            INCLUDE_DIRS.append(flag[2:-1])

# keep the original order
INCLUDE_DIRS.reverse()

# Get the header locations from the environment
LIBRARY_DIRS = []
if "LD_LIBRARY_PATH" in environ:
    flags = environ["LD_LIBRARY_PATH"].split(':')
    for flag in flags:
        # when usingn spack, there is no -I prefix
        LIBRARY_DIRS.append(flag)

    # keep the original order
    LIBRARY_DIRS.reverse()

EXTENSIONS = [
    Extension(
        "psrdada.ringbuffer",
        ["psrdada/ringbuffer.pyx"],
        libraries=["psrdada"],
        library_dirs=LIBRARY_DIRS,
        include_dirs=INCLUDE_DIRS
        ),
    Extension(
        "psrdada.reader",
        ["psrdada/reader.pyx"],
        libraries=["psrdada"],
        library_dirs=LIBRARY_DIRS,
        include_dirs=INCLUDE_DIRS
        ),
    Extension(
        "psrdada.writer",
        ["psrdada/writer.pyx"],
        libraries=["psrdada"],
        library_dirs=LIBRARY_DIRS,
        include_dirs=INCLUDE_DIRS
        ),
    Extension(
        "psrdada.viewer",
        ["psrdada/viewer.pyx"],
        libraries=["psrdada"],
        library_dirs=LIBRARY_DIRS,
        include_dirs=INCLUDE_DIRS
        ),
    ]

setup(
    name='psrdada',
    version=PROJECT_VERSION,
    description="Python3 bindings to the ringbuffer implementation in PSRDada",
    long_description=README + '\n\n',
    author="Jisk Attema",
    author_email='j.attema@esciencecenter.nl',
    url='https://github.com/NLeSC/psrdada-python',
    packages=['psrdada',],
    package_dir={'psrdada': 'psrdada'},
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
        'Programming Language :: Python :: 3.12',
    ],
    test_suite='tests',
    ext_modules=cythonize(EXTENSIONS),
)
