import datetime
import typing
import uuid
from typing import Any, BinaryIO, List, Tuple

from . import parser


def escape(s: bytes) -> bytes:
    "Replaces special characters in a string with their escape sequences."

    return (
        s.replace(b"\\", b"\\\\")
        .replace(b"\x00", b"\\0")
        .replace(b"\b", b"\\b")
        .replace(b"\f", b"\\f")
        .replace(b"\n", b"\\n")
        .replace(b"\r", b"\\r")
        .replace(b"\t", b"\\t")
        .replace(b"\v", b"\\v")
    )


def unescape(s: bytes) -> bytes:
    "Replaces escape sequences in a string with the characters they correspond to."

    return (
        s.replace(b"\\0", b"\x00")
        .replace(b"\\b", b"\b")
        .replace(b"\\f", b"\f")
        .replace(b"\\n", b"\n")
        .replace(b"\\r", b"\r")
        .replace(b"\\t", b"\t")
        .replace(b"\\v", b"\v")
        .replace(b"\\\\", b"\\")
    )


def type_to_format_char(typ: type) -> str:
    "Returns the type format character for a Python type."

    if typ is bool:
        return "z"
    elif typ is int:
        return "i"
    elif typ is float:
        return "f"
    elif typ is str:
        return "s"
    elif typ is datetime.datetime:
        return "d"
    elif typ is uuid.UUID:
        return "u"
    elif typ is bytes:
        return "b"
    else:
        raise TypeError(f"conversion for type `{typ}` is not supported")


def types_to_format_str(fields: Tuple[type, ...]) -> str:
    "Returns the type format string for a tuple of Python types."

    return "".join(type_to_format_char(typ) for typ in fields)


def generate_value(val: Any) -> bytes:
    "Returns the TSV representation of a Python object."

    typ = type(val)
    if typ is bool:
        return b"true" if val else b"false"
    elif typ is int or typ is float or typ is uuid.UUID:
        return str(val).encode("ascii")
    elif typ is str:
        return escape(typing.cast(str, val).encode("utf-8"))
    elif typ is datetime.datetime:
        return (
            typing.cast(datetime.datetime, val)
            .replace(tzinfo=datetime.timezone.utc)
            .isoformat()
            .encode("ascii")
            .replace(b"+00:00", b"Z")
        )
    elif typ is bytes:
        return escape(typing.cast(bytes, val))
    else:
        raise TypeError(f"conversion for type `{typ}` is not supported")


class Generator:
    "Generates TSV data from Python objects."

    def generate_record(self, record: Tuple[Any, ...]) -> Tuple[bytes, ...]:
        return tuple(generate_value(field) for field in record)

    def generate_line(self, record: Tuple[Any, ...]) -> bytes:
        return b"\t".join(generate_value(field) for field in record)


class Parser:
    "Parses TSV data into Python objects."

    _format: str

    def __init__(self, fields: Tuple[type, ...]) -> None:
        self._format = types_to_format_str(fields)

    def parse_record(self, record: Tuple[bytes, ...]) -> Tuple[Any, ...]:
        """
        Parses a tuple of byte arrays representing a TSV record into a tuple of Python objects.

        :param record: A tuple of `bytes` objects, in which each tuple element corresponds to a field.
        :returns: A tuple of Python objects, corresponding to a TSV record.
        """

        return parser.parse_record(self._format, record)

    def parse_line(self, line: bytes) -> Tuple[Any, ...]:
        """
        Parses a line representing a TSV record into a tuple of Python objects.

        Equivalent to
        ```
        return self.parse_record(tuple(line.split(b"\\t")))
        ```

        :param line: A `bytes` object of character data, corresponding to a full record in TSV.
        :returns: A tuple of Python objects, corresponding to a TSV record.
        """

        return parser.parse_line(self._format, line)

    def parse_file(self, file: BinaryIO) -> List[Tuple[Any, ...]]:
        """
        Parses a TSV file into a list of tuples of Python objects.

        Equivalent to
        ```
        return [self.parse_line(line.rstrip()) for line in file]
        ```

        :param file: A file-like object opened in binary mode.
        :returns: A list of tuples, in which each tuple element is a Python object.
        """

        return parser.parse_file(self._format, file)
