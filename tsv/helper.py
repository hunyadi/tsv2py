import datetime
import uuid
from typing import Any, BinaryIO, List, Tuple

from . import parser


def type_to_format_char(typ: type) -> str:
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
    return "".join(type_to_format_char(typ) for typ in fields)


class Parser:
    _format: str

    def __init__(self, fields: Tuple[type, ...]) -> None:
        self._format = types_to_format_str(fields)

    def parse_record(self, record: Tuple[bytes, ...]) -> Tuple[Any, ...]:
        """
        Parses a tuple of byte arrays representing a TSV record into a tuple of Python objects.
        """

        return parser.parse_record(self._format, record)

    def parse_line(self, line: bytes) -> Tuple[Any, ...]:
        """
        Parses a line representing a TSV record into a tuple of Python objects.

        Equivalent to
        ```
        return self.parse_record(tuple(line.split(b"\\t")))
        ```
        """

        return parser.parse_line(self._format, line)

    def parse_file(self, file: BinaryIO) -> List[Tuple[Any, ...]]:
        return [self.parse_line(line) for line in file]
