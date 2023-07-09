import unittest
from datetime import datetime, timezone
from timeit import timeit
from typing import Any, Callable, Tuple
from uuid import UUID

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


converters: Tuple[Callable[[bytes], Any], ...] = (
    bytes,
    parse_datetime,
    float,
    int,
    parse_str,
    parse_uuid,
    bool,
)


def process_record_python(tsv_record: tuple) -> tuple:
    return tuple(
        converter(field) if field != b"\N" else None
        for (converter, field) in zip(converters, tsv_record)
    )


def process_line_python(tsv_line: bytes) -> tuple:
    return tuple(
        converter(field) if field != b"\N" else None
        for (converter, field) in zip(converters, tsv_line.split(b"\t"))
    )


def process_record_c(tsv_record: tuple) -> tuple:
    return parse_record("bdfisuz", tsv_record)


def process_line_c(tsv_line: bytes) -> tuple:
    return parse_line("bdfisuz", tsv_line)


class TestPerformance(unittest.TestCase):
    iterations: int = 1000000

    tsv_record: tuple = (
        "árvíztűrő tükörfúrógép".encode("utf-8"),
        b"1989-10-23T23:59:59Z",
        b"0.5",
        b"-56",
        "árvíztűrő \\r\\n tükörfúrógép".encode("utf=8"),
        str(UUID("f81d4fae-7dec-11d0-a765-00a0c91e6bf6")).encode("ascii"),
        b"true",
    )
    tsv_line: bytes

    def __init__(self, methodName: str = "runTest") -> None:
        super().__init__(methodName)
        self.tsv_line = b"\t".join(self.tsv_record)

    def test_parse_record(self) -> None:
        self.assertEqual(
            process_record_python(self.tsv_record), process_record_c(self.tsv_record)
        )

        print()
        print("Parsing records...")
        time_py = timeit(
            lambda: process_record_python(self.tsv_record), number=self.iterations
        )
        time_c = timeit(
            lambda: process_record_c(self.tsv_record), number=self.iterations
        )
        percent = 100 * (1 - time_c / time_py)
        print(f"Python interpreter took {time_py:.2f} s")
        print(f"C extension took {time_c:.2f} s")
        print(f"{percent:.2f}% savings")

    def test_parse_line(self) -> None:
        self.assertEqual(
            process_line_python(self.tsv_line), process_line_c(self.tsv_line)
        )

        print()
        print("Parsing lines...")
        time_py = timeit(
            lambda: process_line_python(self.tsv_line),
            number=self.iterations,
        )
        time_c = timeit(
            lambda: process_line_c(self.tsv_line),
            number=self.iterations,
        )
        percent = 100 * (1 - time_c / time_py)
        print(f"Python interpreter took {time_py:.2f} s")
        print(f"C extension took {time_c:.2f} s")
        print(f"{percent:.2f}% savings")


if __name__ == "__main__":
    unittest.main()
