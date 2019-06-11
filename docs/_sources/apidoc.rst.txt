.. psrdada-python documentation master file, created by
   sphinx-quickstart on Wed Jun  5 16:04:58 2019.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.


Api documentation
=================

.. automodule:: psrdada


Ringbuffer
----------

.. py:class:: Ringbuffer

   .. py:attribute:: isConnected

      Boolean; indicates if the Ringbuffer instance is connected to a
      running dada_db.

   .. py:attribute:: isHoldingPage

      Boolean; indicates if the Ringbuffer instance is currently holding a page;
      ie. has exclusive write access to it for Writers, or is preventing it from 
      being rewritten for Readers.

   .. py:attribute:: isEndOfData

      Boolean; indicates if the EOD flag has been set on the Ringbuffer. Used
      to implement iterators for the Reader and Writer classes.

   .. py:attribute:: header

      dict; contains a copy of the last read header. For Readers, the original
      un-parsed header is available under the __RAW_HEADER__ key.

Reader
------

.. autoclass:: Reader
   :show-inheritance:
   :members:

Writer
------

.. autoclass:: Writer
   :show-inheritance:
   :members:
