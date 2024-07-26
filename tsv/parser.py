"Parses TSV fields into a tuple of Python objects."

from typing import Any, BinaryIO, List, Tuple

from . import cpu_features

if cpu_features.has_avx2():
    try:
        from . import parser_avx2 as parser_native  # type: ignore
    except ImportError:
        from . import parser_plain as parser_native  # type: ignore
else:
    from . import parser_plain as parser_native  # type: ignore


def parse_record(field_types: str, record: Tuple[bytes, ...]) -> Tuple[Any, ...]:
    """
    Parses a tuple of byte arrays representing a TSV record into a tuple of Python objects,
    according to a type specification.

    The following type specification characters are supported:
        * `b` for `bytes` (pass-through mode)
        * `d` for `date`
        * `t` for `time` (naive, assumed as if in UTC)
        * `T` for `datetime` (naive, assumed as if in UTC)
        * `f` for `float`
        * `i` for `int`
        * `s` for `str`
        * `z` for `bool`
        * `u` for `UUID`
        * `.` for `Decimal`
        * `4` for `IPv4Address`
        * `6` for `IPv6Address`
        * 'n' for network address (either IPv4 or IPv6)
        * `j` for serialized JSON
        * `_` to skip a field without parsing

    :param field_types: Identifies the expected type of each field.
    :param record: A tuple of `bytes` objects, each corresponding to the represented value of a field.
    :returns: A tuple of Python objects, each corresponding to the parsed value of a field.
    """

    raise NotImplementedError()


def parse_line(field_types: str, line: bytes) -> Tuple[Any, ...]:
    """
    Parses a line representing a TSV record into a tuple of Python objects.

    :param field_types: Identifies the expected type of each field.
    :param line: A `bytes` object of character data, representing a full record in TSV.
    :returns: A tuple of Python objects, each corresponding to the parsed value of a field.
    """

    raise NotImplementedError()


def parse_file(field_types: str, file: BinaryIO) -> List[Tuple[Any, ...]]:
    """
    Parses a TSV file into a list of tuples of Python objects.

    :param field_types: Identifies the expected type of each field in a record.
    :param file: A file-like object opened in binary mode.
    :returns: A list of tuples, in which each tuple element is a Python object.
    """

    raise NotImplementedError()


parse_record = parser_native.parse_record  # noqa: F811
parse_line = parser_native.parse_line  # noqa: F811
parse_file = parser_native.parse_file  # noqa: F811
