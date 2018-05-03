#!/usr/bin/env python
# -*- coding: utf-8 -*-

from setuptools import setup
from distutils.extension import Extension
from Cython.Build import cythonize

with open('README.md') as readme_file:
    readme = readme_file.read()

with open('VERSION') as version_file:
    project_version = version_file.read()

extensions = [
    Extension("psrdada", ["psrdada/reader.pyx", "psrdada/writer.pyx"],
        include_dirs = ["/home/jiska/Code/AA/install/include"],
        libraries = ["psrdada"],
        library_dirs = ["/home/jiska/Code/AA/install/lib"]),
    ]
setup(
    name='psrdada',
    version=project_version,
    description="Python3 bindings to the ringbuffer implementation in PSRDada",
    long_description=readme + '\n\n',
    author="Jisk Attema",
    author_email='j.attema@esciencecenter.nl',
    url='https://github.com/jiskattema/psrdada_python',
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
    ext_modules = cythonize(extensions),
)
