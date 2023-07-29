import os.path
import unittest
from datetime import datetime
from uuid import UUID

from tsv.helper import Parser
from tsv.parser import parse_line, parse_record


class TestParseRecord(unittest.TestCase):
    def test_format(self) -> None:
        tsv_record = (
            "árvíztűrő tükörfúrógép".encode("utf-8"),
            b"1989-10-23T23:59:59Z",
            b"0.5",
            b"-56",
            b"multi-line\\r\\nstring",
            b"f81d4fae-7dec-11d0-a765-00a0c91e6bf6",
            b"true",
        )
        py_record = (
            "árvíztűrő tükörfúrógép".encode("utf-8"),
            datetime(1989, 10, 23, 23, 59, 59),
            0.5,
            -56,
            "multi-line\r\nstring",
            UUID("f81d4fae-7dec-11d0-a765-00a0c91e6bf6"),
            True,
        )
        self.assertEqual(parse_record("bdfisuz", tsv_record), py_record)

    def test_none(self) -> None:
        tsv_record = (
            b"\N",
            b"\N",
            b"\N",
            b"\N",
            b"\N",
            b"\N",
            b"\N",
        )
        py_record = (
            None,
            None,
            None,
            None,
            None,
            None,
            None,
        )
        self.assertEqual(parse_record("bdfisuz", tsv_record), py_record)

    def test_integer(self) -> None:
        tsv_record = (
            b"-56",
            b"0",
            b"56",
            b"+56",
        )
        py_record = (
            -56,
            0,
            56,
            56,
        )
        self.assertEqual(parse_record("iiii", tsv_record), py_record)

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


class TestParseLine(unittest.TestCase):
    def test_format(self) -> None:
        tsv_record = b"\t".join(
            [
                "árvíztűrő tükörfúrógép".encode("utf-8"),
                b"1989-10-23T23:59:59Z",
                b"0.5",
                b"-56",
                b"multi-line\\r\\nstring",
                b"f81d4fae-7dec-11d0-a765-00a0c91e6bf6",
                b"true",
            ]
        )
        py_record = (
            "árvíztűrő tükörfúrógép".encode("utf-8"),
            datetime(1989, 10, 23, 23, 59, 59),
            0.5,
            -56,
            "multi-line\r\nstring",
            UUID("f81d4fae-7dec-11d0-a765-00a0c91e6bf6"),
            True,
        )
        self.assertEqual(parse_line("bdfisuz", tsv_record), py_record)

    def test_none(self) -> None:
        tsv_record = b"\t".join(
            [
                b"\N",
                b"\N",
                b"\N",
                b"\N",
                b"\N",
                b"\N",
                b"\N",
            ]
        )
        py_record = (
            None,
            None,
            None,
            None,
            None,
            None,
            None,
        )
        self.assertEqual(parse_line("bdfisuz", tsv_record), py_record)

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
    tsv_path: str

    def __init__(self, methodName: str = "runTest") -> None:
        super().__init__(methodName)
        self.tsv_path = os.path.join(os.path.dirname(__file__), "test.tsv")

    def setUp(self) -> None:
        tsv_record = (
            "árvíztűrő tükörfúrógép",
            "1989-10-23T23:59:59Z",
            "0.5",
            "-56",
            r"multi-line\r\nstring",
            "f81d4fae-7dec-11d0-a765-00a0c91e6bf6",
            "true",
        )

        with open(self.tsv_path, "wb") as f:
            f.write(b"\t".join(field.encode("utf-8") for field in tsv_record))

    def tearDown(self) -> None:
        os.remove(self.tsv_path)

    def test_file(self) -> None:
        parser = Parser((bytes, datetime, float, int, str, UUID, bool))
        with open(self.tsv_path, "rb") as f:
            py_records = parser.parse_file(f)

        py_record = (
            "árvíztűrő tükörfúrógép".encode("utf-8"),
            datetime(1989, 10, 23, 23, 59, 59),
            0.5,
            -56,
            "multi-line\r\nstring",
            UUID("f81d4fae-7dec-11d0-a765-00a0c91e6bf6"),
            True,
        )
        self.assertEqual(py_records, [py_record])

    def test_line(self) -> None:
        parser = Parser((bytes, datetime, float, int, str, UUID, bool))
        with open(self.tsv_path, "rb") as f:
            for line in f:
                parser.parse_line(line)


if __name__ == "__main__":
    unittest.main()
