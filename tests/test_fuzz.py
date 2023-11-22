import random
import string
import unittest
import uuid
from datetime import datetime
from io import BytesIO
from typing import Any, Callable, Dict, List, Tuple
from uuid import UUID

from tsv.helper import Generator, Parser

_random_map: Dict[type, Callable[[], Any]] = {
    bytes: lambda: bytes(random.randint(0, 255) for _ in range(100)),
    datetime: lambda: datetime.now().replace(microsecond=0),
    float: lambda: random.random(),
    int: lambda: random.randint(-(2**32), 2 * 32),
    str: lambda: (
        "".join(
            random.choices(
                string.ascii_uppercase + string.ascii_lowercase + string.digits,
                k=random.randint(0, 100),
            )
        )
    ),
    UUID: lambda: uuid.uuid4(),
    bool: lambda: random.random() > 0.5,
}


def random_any(typ: type) -> Any:
    return _random_map[typ]()


class TestFuzz(unittest.TestCase):
    def test_fuzz(self) -> None:
        all_types = (bytes, datetime, float, int, str, UUID, bool)
        row_count = 1000
        column_count = 100

        column_types = tuple(random.choices(all_types, k=column_count))

        generated_rows: List[Tuple[Any, ...]] = [
            tuple(random_any(c) for c in column_types) for _ in range(row_count)
        ]

        generator = Generator()
        with BytesIO() as f:
            for row in generated_rows:
                f.write(generator.generate_line(row))
                f.write(b"\n")
            tsv_data = f.getvalue()

        try:
            parser = Parser(column_types)
            with BytesIO(tsv_data) as f:
                parsed_rows = parser.parse_file(f)
        except ValueError:
            with open("dump.tsv", "wb") as f:
                f.write(tsv_data)
            raise

        for generated_row, parsed_row in zip(generated_rows, parsed_rows):
            self.assertEqual(generated_row, parsed_row)


if __name__ == "__main__":
    unittest.main()
