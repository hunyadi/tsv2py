from typing import Any, BinaryIO, List, Tuple

def parse_record(field_types: str, record: Tuple[bytes, ...]) -> Tuple[Any, ...]:
    """
    Parses a tuple of byte arrays representing a TSV record into a tuple of Python objects,
    according to a type specification.

    The following type specification characters are supported:
        * `b` for `bytes` (pass-through mode)
        * `d` for `datetime` (naive, assumed as if in UTC)
        * `f` for `float`
        * `i` for `int`
        * `s` for `str`
        * `z` for `bool`

    :param field_types: Identifies the expected type of each field.
    :param record: A tuple of `bytes` objects, each corresponding to the represented value of a field.
    :returns: A tuple of Python objects, each corresponding to the parsed value of a field.
    """

    ...

def parse_line(field_types: str, line: bytes) -> Tuple[Any, ...]:
    """
    Parses a line representing a TSV record into a tuple of Python objects.

    :param field_types: Identifies the expected type of each field.
    :param line: A `bytes` object of character data, representing a full record in TSV.
    :returns: A tuple of Python objects, each corresponding to the parsed value of a field.
    """

    ...

def parse_file(field_types: str, file: BinaryIO) -> List[Tuple[Any, ...]]:
    """
    Parses a TSV file into a list of tuples of Python objects.

    :param field_types: Identifies the expected type of each field in a record.
    :param file: A file-like object opened in binary mode.
    :returns: A list of tuples, in which each tuple element is a Python object.
    """

    ...
