#!/bin/bash

# PSRDada does not bundle a shared library
# a hackish way to get a libpsrdada.so that contains engough functionality for psrdada-python,
# you can do the following

# set the psrdada root directory here
PSRDADA=psrdada

# set the destination directory for the library
LIBDIR=lib

# --- no changes below this line ---

cd $PSRDADA

SOURCES=ascii_header.o command_parse.o command_parse_server.o dada_affinity.o dada_client.o dada_generator.o dada_hdu.o dada_ni.o dada_pwc_main_multi.o dada_pwc_main.o dada_pwc_nexus_config.o dada_pwc_nexus_header_parse.o dada_pwc_nexus.o dada_pwc.o dada_udp.o daemon.o diff_time.o disk_array.o fileread.o filesize.o ipcbuf.o ipcio.o ipcutil.o monitor.o multilog.o multilog_server.o nexus.o node_array.o sock.o string_array.o tmutil.o cd psrdada/src 

rm -f *.o 

CPPFLAGS=-fPIC make 

gcc *.o -shared -o libpsrdada.so

mv libpsrdada.so ${LIBDIR}
