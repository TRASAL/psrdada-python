#!/usr/bin/env python
"""
Monitor statistics of a ringbuffer
"""
from psrdada import Viewer
import time

viewer = Viewer(0xdada)

print("HEADER               Data")
print("FRE FUL CLR  W  R    FRE FUL CLR     W     R")
for i in range(10):
    hdr_mon, data_mon_list = viewer.getbufstats()
    data_mon = data_mon_list[0] #Assuming only 1 reader

    hdr_stats = [hdr_mon['FREE'], hdr_mon['FULL'], hdr_mon['CLEAR'],
            hdr_mon['NWRITE'],hdr_mon['NREAD']]
    data_stats = [data_mon['FREE'], data_mon['FULL'], data_mon['CLEAR'],
            data_mon['NWRITE'],data_mon['NREAD']]

    print("%3i %3i %3i %2i %2i    %3i %3i %3i %5i %5i"
            %(*hdr_stats, *data_stats))
    time.sleep(1)

viewer.disconnect()
