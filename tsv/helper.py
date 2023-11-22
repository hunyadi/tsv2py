import datetime
import decimal
import enum
import ipaddress
import json
import sys
import types
import typing
import uuid
from typing import Any, BinaryIO, Dict, Iterable, List, Tuple, Union

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


if sys.version_info >= (3, 10):

    def is_union_like(typ: object) -> bool:
        "True if type is a union such as `Union[T1, T2, ...]` or a union type `T1 | T2`."

        return typing.get_origin(typ) is Union or isinstance(typ, types.UnionType)

else:

    def is_union_like(typ: object) -> bool:
        "True if type is a union such as `Union[T1, T2, ...]` or a union type `T1 | T2`."

        return typing.get_origin(typ) is Union


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
    elif typ is bytes:
        return "b"
    elif typ is datetime.datetime:
        return "T"
    elif typ is datetime.date:
        return "d"
    elif typ is datetime.time:
        return "t"
    elif typ is decimal.Decimal:
        return "."
    elif typ is uuid.UUID:
        return "u"
    elif typ is ipaddress.IPv4Address:
        return "4"
    elif typ is ipaddress.IPv6Address:
        return "6"
    elif typ is list or typ is dict:  # serialized JSON
        return "j"
    elif typ is type(None):
        return "_"

    if is_union_like(typ):
        args = typing.get_args(typ)
        if (
            len(args) == 2
            and ipaddress.IPv4Address in args
            and ipaddress.IPv6Address in args
        ):
            return "n"

    raise TypeError(f"conversion for type `{typ}` is not supported")


def types_to_format_str(fields: Tuple[type, ...]) -> str:
    "Returns the type format string for a tuple of Python types."

    return "".join(type_to_format_char(typ) for typ in fields)


def generate_value(val: Any) -> bytes:
    "Returns the TSV representation of a Python object."

    if val is None:
        return rb"\N"
    elif isinstance(val, bool):
        return b"true" if val else b"false"
    elif isinstance(val, bytes):
        return escape(val)
    elif isinstance(val, (int, float, uuid.UUID)):
        return str(val).encode("ascii")
    elif isinstance(val, str):
        return escape(val.encode("utf-8"))
    elif isinstance(val, (datetime.time, datetime.datetime)):
        return (
            val.replace(tzinfo=datetime.timezone.utc)
            .isoformat()
            .encode("ascii")
            .replace(b"+00:00", b"Z")
        )
    elif isinstance(val, datetime.date):
        return val.isoformat().encode("ascii")
    elif isinstance(val, decimal.Decimal):
        return str(val).encode("ascii")
    elif isinstance(val, enum.Enum):
        return generate_value(val.value)
    elif isinstance(val, (ipaddress.IPv4Address, ipaddress.IPv6Address)):
        return val.compressed.encode("ascii")
    elif isinstance(val, (dict, list)):  # serialized JSON
        return escape(
            json.dumps(
                val, ensure_ascii=False, check_circular=False, separators=(",", ":")
            ).encode("utf-8")
        )
    else:
        raise TypeError(
            f"conversion for value `{val}` of type `{type(val)}` is not supported"
        )


class Generator:
    "Generates TSV data from Python objects."

    def generate_record(self, record: Tuple[Any, ...]) -> Tuple[bytes, ...]:
        return tuple(generate_value(field) for field in record)

    def generate_line(self, record: Tuple[Any, ...]) -> bytes:
        return b"\t".join(generate_value(field) for field in record)

    def generate_file(self, file: BinaryIO, items: Iterable[Tuple[Any, ...]]) -> None:
        for item in items:
            file.write(self.generate_line(item))
            file.write(b"\n")


class Parser:
    "Parses TSV data into Python objects."

    _format: str

    def __init__(self, fields: Tuple[type, ...]) -> None:
        """
        Creates a new parser.

        :param fields: Column types, in order of occurrence.
        """

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


class AutoDetectParser(Parser):
    "Parses TSV data into Python tuples auto-detecting types based on a header."

    columns: Tuple[str, ...]

    def __init__(self, names_to_types: Dict[str, type], header: bytes) -> None:
        """
        Creates a new parser.

        :param names_to_types: Associates column names with column types.
        :param header: The table header, usually the first line in a file.
        """

        self.columns = tuple(col.decode("utf-8") for col in header.split(b"\t"))
        self._format = types_to_format_str(
            tuple(names_to_types[name] for name in self.columns)
        )
