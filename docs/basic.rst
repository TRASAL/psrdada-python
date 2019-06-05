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

