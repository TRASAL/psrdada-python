"""
Definiton of exceptions that can be raised by the PSRDada code
"""

class PSRDadaError(Exception):
    def __init__(self, value):
        self.value = value

    def __str__(self):
        return repr(self.value)
