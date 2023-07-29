import random
import unittest
from datetime import datetime, timezone
from timeit import timeit
from typing import Any, Callable, Tuple
from uuid import UUID

from tsv.helper import types_to_format_str
from tsv.parser import parse_line, parse_record


def parse_datetime(s: bytes) -> datetime:
    return (
        datetime.fromisoformat(s.decode("ascii").replace("Z", "+00:00"))
        .astimezone(timezone.utc)
        .replace(tzinfo=None)
    )


def parse_str(s: bytes) -> str:
    return (
        s.replace(b"\\0", b"\0")
        .replace(b"\\b", b"\b")
        .replace(b"\\f", b"\f")
        .replace(b"\\n", b"\n")
        .replace(b"\\r", b"\r")
        .replace(b"\\t", b"\t")
        .replace(b"\\v", b"\v")
        .decode("utf-8")
    )


def parse_uuid(s: bytes) -> UUID:
    return UUID(s.decode("ascii"))


def type_to_converter(typ: type) -> Callable[[bytes], Any]:
    if typ is bool:
        return bool
    elif typ is int:
        return int
    elif typ is float:
        return float
    elif typ is str:
        return parse_str
    elif typ is datetime:
        return parse_datetime
    elif typ is UUID:
        return parse_uuid
    elif typ is bytes:
        return bytes
    else:
        raise TypeError(f"conversion for type `{typ}` is not supported")


def types_to_converters(fields: Tuple[type, ...]) -> Tuple[Callable[[bytes], Any], ...]:
    return tuple(type_to_converter(typ) for typ in fields)


def process_record_python(
    converters: Tuple[Callable[[bytes], Any], ...], tsv_record: tuple
) -> tuple:
    return tuple(
        converter(field) if field != b"\N" else None
        for (converter, field) in zip(converters, tsv_record)
    )


def process_line_python(
    converters: Tuple[Callable[[bytes], Any], ...], tsv_line: bytes
) -> tuple:
    return tuple(
        converter(field) if field != b"\N" else None
        for (converter, field) in zip(converters, tsv_line.split(b"\t"))
    )


def process_record_c(converters: str, tsv_record: tuple) -> tuple:
    return parse_record(converters, tsv_record)


def process_line_c(converters: str, tsv_line: bytes) -> tuple:
    return parse_line(converters, tsv_line)


class Tester:
    tsv_types: Tuple[type, ...]
    tsv_record: Tuple[Any, ...]
    tsv_line: bytes
    converters: Tuple[Callable[[bytes], Any], ...]
    format_str: str

    def __init__(
        self, tsv_types: Tuple[type, ...], tsv_record: Tuple[Any, ...]
    ) -> None:
        self.tsv_types = tsv_types
        self.tsv_record = tsv_record
        self.tsv_line = b"\t".join(self.tsv_record)
        self.converters = types_to_converters(tuple(typ for typ in self.tsv_types))
        self.format_str = types_to_format_str(tuple(typ for typ in self.tsv_types))


class TestPerformance(unittest.TestCase):
    iterations: int = 100000

    tsv_types: tuple = (str, datetime, float, int, str, str, UUID, bool)
    tsv_record: tuple = (
        "árvíztűrő tükörfúrógép".encode("utf-8"),
        b"1989-10-23T23:59:59Z",
        b"0.5",
        b"-56",
        "árvíztűrő \\r\\n tükörfúrógép".encode("utf=8"),
        b"Etiam pulvinar diam et diam lacinia, in consectetur neque consequat. Cras pharetra ut metus ac lobortis. Vestibulum interdum euismod odio sed cursus. Integer orci magna, mollis et mattis non, dignissim eu ante. Pellentesque habitant morbi tristique senectus et netus et malesuada fames ac turpis egestas. Maecenas sit amet lacinia enim. Quisque porttitor turpis eu tristique cursus. Pellentesque aliquam dui sit amet laoreet porta.",
        str(UUID("f81d4fae-7dec-11d0-a765-00a0c91e6bf6")).encode("ascii"),
        b"true",
    )

    tester: Tester

    def __init__(self, methodName: str = "runTest") -> None:
        super().__init__(methodName)
        self.tester = Tester(self.tsv_types, self.tsv_record)

    def test_parse_record(self) -> None:
        self.assertEqual(
            process_record_python(self.tester.converters, self.tsv_record),
            process_record_c(self.tester.format_str, self.tsv_record),
        )

        print()
        print("Parsing records...")
        time_py = timeit(
            lambda: process_record_python(self.tester.converters, self.tsv_record),
            number=self.iterations,
        )
        time_c = timeit(
            lambda: process_record_c(self.tester.format_str, self.tsv_record),
            number=self.iterations,
        )
        percent = 100 * (1 - time_c / time_py)
        print(f"Python interpreter took {time_py:.2f} s")
        print(f"C extension took {time_c:.2f} s")
        print(f"{percent:.2f}% savings")

    def test_parse_line(self) -> None:
        self.assertEqual(
            process_line_python(self.tester.converters, self.tester.tsv_line),
            process_line_c(self.tester.format_str, self.tester.tsv_line),
        )

        print()
        print("Parsing lines...")
        time_py = timeit(
            lambda: process_line_python(self.tester.converters, self.tester.tsv_line),
            number=self.iterations,
        )
        time_c = timeit(
            lambda: process_line_c(self.tester.format_str, self.tester.tsv_line),
            number=self.iterations,
        )
        percent = 100 * (1 - time_c / time_py)
        print(f"Python interpreter took {time_py:.2f} s")
        print(f"C extension took {time_c:.2f} s")
        print(f"{percent:.2f}% savings")


class TestPerformanceManyFields(unittest.TestCase):
    iterations: int = 100000

    def test_parse_many_fields(self) -> None:
        tsv_types: Tuple[type, ...] = tuple([int] * 100)
        tsv_record: Tuple[Any, ...] = tuple(
            str(random.randint(0, 10000)).encode("ascii") for i in range(100)
        )
        tester = Tester(tsv_types, tsv_record)
        self.assertEqual(
            process_line_python(tester.converters, tester.tsv_line),
            process_line_c(tester.format_str, tester.tsv_line),
        )

        print()
        print("Parsing lines with many fields...")
        time_py = timeit(
            lambda: process_line_python(tester.converters, tester.tsv_line),
            number=self.iterations,
        )
        time_c = timeit(
            lambda: process_line_c(tester.format_str, tester.tsv_line),
            number=self.iterations,
        )
        percent = 100 * (1 - time_c / time_py)
        print(f"Python interpreter took {time_py:.2f} s")
        print(f"C extension took {time_c:.2f} s")
        print(f"{percent:.2f}% savings")


if __name__ == "__main__":
    unittest.main()
