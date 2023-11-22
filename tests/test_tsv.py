import datetime
import time
import typing
import unittest
from decimal import Decimal
from io import BytesIO
from ipaddress import IPv4Address, IPv6Address
from uuid import UUID

from tsv.helper import Parser
from tsv.parser import parse_line, parse_record


class TestParseRecord(unittest.TestCase):
    def test_format(self) -> None:
        tsv_record = (
            "árvíztűrő tükörfúrógép".encode("utf-8"),
            b"1984-01-01",
            b"23:59:59Z",
            b"1989-10-23T23:59:59Z",
            b"0.5",
            b"-56",
            b"multi-line\\r\\nstring",
            b"f81d4fae-7dec-11d0-a765-00a0c91e6bf6",
            b"true",
            b"3.14159265358979323846264338327950288419716939937510",
            b"192.0.2.0",
            b"2001:DB8:0:0:8:800:200C:417A",
            b'["one","two","three"]',
            b'{"string": "value", "number": 87}',
            b"<!-- omitted field -->",
        )
        py_record = (
            "árvíztűrő tükörfúrógép".encode("utf-8"),
            datetime.date(1984, 1, 1),
            datetime.time(23, 59, 59),
            datetime.datetime(1989, 10, 23, 23, 59, 59),
            0.5,
            -56,
            "multi-line\r\nstring",
            UUID("f81d4fae-7dec-11d0-a765-00a0c91e6bf6"),
            True,
            Decimal("3.14159265358979323846264338327950288419716939937510"),
            IPv4Address("192.0.2.0"),
            IPv6Address("2001:DB8:0:0:8:800:200C:417A"),
            ["one", "two", "three"],
            {"string": "value", "number": 87},
            None,
        )
        signature = "".join(
            ["b", "d", "t", "T", "f", "i", "s", "u", "z", ".", "4", "6", "j", "j", "_"]
        )
        self.assertEqual(parse_record(signature, tsv_record), py_record)

    def test_none(self) -> None:
        typ = ["b", "d", "t", "T", "f", "i", "s", "u", "z", ".", "4", "6", "j", "_"]
        tsv_record = tuple(rb"\N" for _ in range(len(typ)))
        py_record = tuple(None for _ in range(len(typ)))
        signature = "".join(typ)
        self.assertEqual(parse_record(signature, tsv_record), py_record)

    def test_integer(self) -> None:
        tsv_record = (
            b"-56",
            b"0",
            b"56",
            b"+56",
            b"2147483647",
            b"-2147483648",
            b"9223372036854775807",
            b"-9223372036854775808",
            b"9999999999999999999",
        )
        py_record = (
            -56,
            0,
            56,
            56,
            2147483647,
            -2147483648,
            9223372036854775807,
            -9223372036854775808,
            9999999999999999999,
        )
        self.assertEqual(parse_record("i" * len(tsv_record), tsv_record), py_record)

    def test_datetime(self) -> None:
        tsv_record = (
            b"1989-10-23T23:59:59Z",
            b"1989-10-23T23:59:59.1Z",
            b"1989-10-23T23:59:59.12Z",
            b"1989-10-23T23:59:59.123Z",
            b"1989-10-23T23:59:59.1234Z",
            b"1989-10-23T23:59:59.12345Z",
            b"1989-10-23T23:59:59.123456Z",
            b"1989-10-23T23:59:59.000001Z",
        )
        py_record = (
            datetime.datetime(1989, 10, 23, 23, 59, 59),
            datetime.datetime(1989, 10, 23, 23, 59, 59, 100000),
            datetime.datetime(1989, 10, 23, 23, 59, 59, 120000),
            datetime.datetime(1989, 10, 23, 23, 59, 59, 123000),
            datetime.datetime(1989, 10, 23, 23, 59, 59, 123400),
            datetime.datetime(1989, 10, 23, 23, 59, 59, 123450),
            datetime.datetime(1989, 10, 23, 23, 59, 59, 123456),
            datetime.datetime(1989, 10, 23, 23, 59, 59, 1),
        )
        self.assertEqual(parse_record("T" * len(tsv_record), tsv_record), py_record)

    def test_time(self) -> None:
        tsv_record = (
            b"23:59:59Z",
            b"23:59:59.1Z",
            b"23:59:59.12Z",
            b"23:59:59.123Z",
            b"23:59:59.1234Z",
            b"23:59:59.12345Z",
            b"23:59:59.123456Z",
            b"23:59:59.000001Z",
        )
        py_record = (
            datetime.time(23, 59, 59),
            datetime.time(23, 59, 59, 100000),
            datetime.time(23, 59, 59, 120000),
            datetime.time(23, 59, 59, 123000),
            datetime.time(23, 59, 59, 123400),
            datetime.time(23, 59, 59, 123450),
            datetime.time(23, 59, 59, 123456),
            datetime.time(23, 59, 59, 1),
        )
        self.assertEqual(parse_record("t" * len(tsv_record), tsv_record), py_record)

    def test_uuid(self) -> None:
        tsv_record = (
            b"f81d4fae7dec11d0a76500a0c91e6bf6",
            b"f81d4fae-7dec-11d0-a765-00a0c91e6bf6",
        )
        py_record = (
            UUID("f81d4fae-7dec-11d0-a765-00a0c91e6bf6"),
            UUID("f81d4fae-7dec-11d0-a765-00a0c91e6bf6"),
        )
        self.assertEqual(parse_record("uu", tsv_record), py_record)

    def test_json(self) -> None:
        bs = "\\"  # a single backslash character
        tsv_record = (
            b"[]",
            b"{}",
            b"[0]",
            b"[1,2]",
            b'["string"]',
            # both TSV unescape and JSON parser have `\` as the escape character
            f'["backslash: {bs}{bs}{bs}{bs}", "newline: {bs}{bs}n"]'.encode("utf-8"),
        )
        py_record: typing.Tuple[list, dict, list, list, list, list] = (
            [],
            {},
            [0],
            [1, 2],
            ["string"],
            ["backslash: \\", "newline: \n"],
        )
        self.assertEqual(parse_record("j" * len(tsv_record), tsv_record), py_record)


class TestParseLine(unittest.TestCase):
    def test_format(self) -> None:
        tsv_record = b"\t".join(
            [
                "árvíztűrő tükörfúrógép".encode("utf-8"),
                b"1984-01-01",
                b"23:59:59Z",
                b"1989-10-23T23:59:59Z",
                b"0.5",
                b"-56",
                b"multi-line\\r\\nstring",
                b"f81d4fae-7dec-11d0-a765-00a0c91e6bf6",
                b"true",
                b"3.14159265358979323846264338327950288419716939937510",
                b"192.0.2.0",
                b"2001:DB8:0:0:8:800:200C:417A",
                b'{"string":"value","number":87}',
                b"<!-- omitted field -->",
            ]
        )
        py_record = (
            "árvíztűrő tükörfúrógép".encode("utf-8"),
            datetime.date(1984, 1, 1),
            datetime.time(23, 59, 59),
            datetime.datetime(1989, 10, 23, 23, 59, 59),
            0.5,
            -56,
            "multi-line\r\nstring",
            UUID("f81d4fae-7dec-11d0-a765-00a0c91e6bf6"),
            True,
            Decimal("3.14159265358979323846264338327950288419716939937510"),
            IPv4Address("192.0.2.0"),
            IPv6Address("2001:DB8:0:0:8:800:200C:417A"),
            {"string": "value", "number": 87},
            None,
        )
        signature = "".join(
            ["b", "d", "t", "T", "f", "i", "s", "u", "z", ".", "4", "6", "j", "_"]
        )
        self.assertEqual(parse_line(signature, tsv_record), py_record)

    def test_none(self) -> None:
        typ = ["b", "d", "t", "T", "f", "i", "s", "u", "z", ".", "4", "6", "j", "_"]
        tsv_record = b"\t".join(rb"\N" for _ in range(len(typ)))
        py_record = tuple(None for _ in range(len(typ)))
        signature = "".join(typ)
        self.assertEqual(parse_line(signature, tsv_record), py_record)

    def test_field_count(self) -> None:
        tsv_record = b"0"
        parse_line("i", tsv_record)
        with self.assertRaises(ValueError):
            parse_line("ii", tsv_record)

        tsv_record = b"\t".join([b"1", b"2"])
        with self.assertRaises(ValueError):
            parse_line("i", tsv_record)
        parse_line("ii", tsv_record)
        with self.assertRaises(ValueError):
            parse_line("iii", tsv_record)

    def test_field_length(self) -> None:
        # insufficient characters, no SIMD operation is executed
        tsv_record = b"string"
        parse_line("s", tsv_record)
        tsv_record = b"\t\t\t\t\t"
        parse_line("ssssss", tsv_record)

        # no delimiter is found with SIMD operation
        tsv_record = b"12345678901234567890123456789012\t..."
        parse_line("ss", tsv_record)

        # one delimiter is found with SIMD operation
        tsv_record = b"1234567890123456789012345678901\t..."
        parse_line("ss", tsv_record)

        # several delimiters found with SIMD operation
        tsv_record = b"1\t12\t123\t1234\t12345\t...12345678901234567890123456789012"
        parse_line("ssssss", tsv_record)

    def test_string_escape(self) -> None:
        tsv_record = b""
        parse_line("s", tsv_record)

        tsv_record = (
            r"árvíztűrő \0, \b, \f, \n, \r, \t and \v \\\\ tükörfúrógép".encode("utf-8")
        )
        parse_line("s", tsv_record)

        tsv_record = r"árvíztűrő \N tükörfúrógép".encode("utf-8")
        with self.assertRaises(ValueError):
            parse_line("s", tsv_record)


class TestParseFile(unittest.TestCase):
    tsv_data: bytes

    def setUp(self) -> None:
        tsv_record = (
            "árvíztűrő tükörfúrógép",
            "1984-01-01",
            "1989-10-23T23:59:59Z",
            "0.5",
            "-56",
            r"multi-line\r\nstring",
            "f81d4fae-7dec-11d0-a765-00a0c91e6bf6",
            "true",
            "192.0.2.0",
            "2001:DB8:0:0:8:800:200C:417A",
        )

        with BytesIO() as f:
            for _ in range(100000):
                f.write(b"\t".join(field.encode("utf-8") for field in tsv_record))
                f.write(b"\n")
            self.tsv_data = f.getvalue()

    def test_file(self) -> None:
        parser = Parser(
            (
                bytes,
                datetime.date,
                datetime.datetime,
                float,
                int,
                str,
                UUID,
                bool,
                IPv4Address,
                IPv6Address,
            )
        )

        start = time.perf_counter_ns()
        with BytesIO(self.tsv_data) as f:
            py_records = parser.parse_file(f)
        end = time.perf_counter_ns()

        elapsed_time = (end - start) / 10**9
        print(f"Parsing file took {elapsed_time:.03f} s.")

        py_record = (
            "árvíztűrő tükörfúrógép".encode("utf-8"),
            datetime.date(1984, 1, 1),
            datetime.datetime(1989, 10, 23, 23, 59, 59),
            0.5,
            -56,
            "multi-line\r\nstring",
            UUID("f81d4fae-7dec-11d0-a765-00a0c91e6bf6"),
            True,
            IPv4Address("192.0.2.0"),
            IPv6Address("2001:DB8:0:0:8:800:200C:417A"),
        )
        self.assertEqual(py_records, [py_record] * 100000)

    def test_line(self) -> None:
        parser = Parser(
            (
                bytes,
                datetime.date,
                datetime.datetime,
                float,
                int,
                str,
                UUID,
                bool,
                IPv4Address,
                IPv6Address,
            )
        )
        with BytesIO(self.tsv_data) as f:
            for line in f:
                parser.parse_line(line[:-1])


if __name__ == "__main__":
    unittest.main()
