Basic example
-------------

Using iterators
~~~~~~~~~~~~~~~

Connect to and read from a PSRDada ringbuffer in a pythonic way, using
the Iterator interface:

.. literalinclude:: ../examples/sum_with_iterator.py

Explicit ringbuffer control 
~~~~~~~~~~~~~~~~~~~~~~~~~~~

For usecases that require a more finegrained flow control, there is a
more C-like API:

.. literalinclude:: ../examples/sum_no_iterator.py

More examples
~~~~~~~~~~~~~

For more examples, see the *examples* and *tests* directories in the repo.
Examples cover writing to the ringbuffer, reading and writing header data, and dealing with EndOfData.
EndOfData is used by *dada_dbevent* to send independent datasets through a ringbuffer, instead of a single continuous stream.
