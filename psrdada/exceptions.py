"""
PSRDada Exceptions.

Definiton of exceptions that can be raised by the PSRDada code
"""


class PSRDadaError(Exception):
    """
    PSRDadaError Exceptions.

    Raised when a runtime error occurs in libpsrdada.  Note that the
    library itself outputs error messages to stderr.
    """

    def __init__(self, value):
        """Initialize exception."""
        super().__init__()
        self.value = value

    def __str__(self):
        """Convert exception to human readable string."""
        return repr(self.value)
