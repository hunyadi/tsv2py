# Parse and generate tab-separated values (TSV) data

[Tab-separated values](https://en.wikipedia.org/wiki/Tab-separated_values) (TSV) is a simple and popular format for data storage, data transfer, exporting data from and importing data to relational databases. For example, PostgreSQL [COPY](https://www.postgresql.org/docs/current/sql-copy.html) moves data between PostgreSQL tables and standard file-system files or in-memory stores, and its `text` format (a text file with one line per table row) is a generic version of TSV. Meanwhile, packages like [asyncpg](https://magicstack.github.io/asyncpg/current/index.html) help efficiently insert, update or query data in bulk with binary data transfer between Python and PostgreSQL.

This package offers a high-performance alternative to convert data between a TSV text file and Python objects. The parser can read a TSV record into a Python tuple consisting of built-in Python types, one for each field. The generator can produce a TSV record from a tuple.

## Quick start

```python
from tsv.helper import Parser

# specify the column structure
parser = Parser(fields=(bytes, date, datetime, float, int, str, UUID, bool))

# read and parse an entire file
with open(tsv_path, "rb") as f:
    py_records = parser.parse_file(f)

# read and parse a file line by line
with open(tsv_path, "rb") as f:
    for line in f:
        py_record = parser.parse_line(line)
```

## TSV format

Text format is a simple tabular format in which each record (table row) occupies a single line.

* Output always begins with a header row, which lists data field names.
* Fields (table columns) are delimited by *tab* characters.
* Non-printable characters and special values are escaped with *backslash* (`\`), as shown below:

| Escape | Interpretation               |
| ------ | ---------------------------- |
| `\N`   | NULL value                   |
| `\0`   | NUL character (ASCII 0)      |
| `\b`   | Backspace (ASCII 8)          |
| `\f`   | Form feed (ASCII 12)         |
| `\n`   | Newline (ASCII 10)           |
| `\r`   | Carriage return (ASCII 13)   |
| `\t`   | Tab (ASCII 9)                |
| `\v`   | Vertical tab (ASCII 11)      |
| `\\`   | Backslash (single character) |

This format allows data to be easily imported into a database engine, e.g. with PostgreSQL [COPY](https://www.postgresql.org/docs/current/sql-copy.html).

Output in this format is transmitted as media type `text/plain` or `text/tab-separated-values` in UTF-8 encoding.

## Parser

The parser understands the following Python types:

* `None`. This special value is returned for the TSV escape sequence `\N`.
* `bool`. A literal `true` or `false` is converted into a boolean value.
* `bytes`. TSV escape sequences are reversed before the data is passed to Python as a `bytes` object. NUL bytes are permitted.
* `datetime`. The input has to comply with RFC 3339 and ISO 8601. The timezone must be UTC (a.k.a. suffix `Z`).
* `date`. The input has to conform to the format `YYYY-MM-DD`.
* `time`. The input has to conform to the format `hh:mm:ssZ` with no fractional seconds, or `hh:mm:ss.ffffffZ` with fractional seconds. Fractional seconds allow up to 6 digits of precision.
* `float`. Interpreted as double precision floating point numbers.
* `int`. Arbitrary-length integers are allowed.
* `str`. TSV escape sequences are reversed before the data is passed to Python as a `str`. NUL bytes are not allowed.
* `uuid.UUID`. The input has to comply with RFC 4122, or be a string of 32 hexadecimal digits.
* `decimal.Decimal`. Interpreted as arbitrary precision decimal numbers.
* `ipaddress.IPv4Address`.
* `ipaddress.IPv6Address`.
* `list` and `dict`, which are understood as JSON, and invoke the equivalent of `json.loads` to parse a serialized JSON string.

The backslash character `\` is both a TSV and a JSON escape sequence initiator. When JSON data is written to TSV, several backslash characters may be needed, e.g. `\\n` in a quoted JSON string translates to a single newline character. First, `\\` in `\\n` is understood as an escape sequence by the TSV parser to produce a single `\` character followed by an `n` character, and in turn `\n` is understood as a single newline embedded in a JSON string by the JSON parser. Specifically, you need four consecutive backslash characters in TSV to represent a single backslash in a JSON quoted string.

Internally, the implementation uses AVX2 instructions to

* parse RFC 3339 date-time strings into Python `datetime` objects,
* parse RFC 4122 UUID strings or 32-digit hexadecimal strings into Python `UUID` objects,
* and find `\t` delimiters between fields in a line.

For parsing integers up to the range of the `long` type, the parser calls the C standard library function [strtol](https://en.cppreference.com/w/c/string/byte/strtol).

For parsing IPv4 and IPv6 addresses, the parser calls the C function [inet_pton](https://man7.org/linux/man-pages/man3/inet_pton.3.html) in libc or Windows Sockets (WinSock2).

If installed, the parser employs [orjson](https://github.com/ijl/orjson) to improve parsing speed of nested JSON structures. If not available, the library falls back to the [built-in JSON decoder](https://docs.python.org/3/library/json.html).

### Date-time format

```
YYYY-MM-DDThh:mm:ssZ
YYYY-MM-DDThh:mm:ss.fZ
YYYY-MM-DDThh:mm:ss.ffZ
YYYY-MM-DDThh:mm:ss.fffZ
YYYY-MM-DDThh:mm:ss.ffffZ
YYYY-MM-DDThh:mm:ss.fffffZ
YYYY-MM-DDThh:mm:ss.ffffffZ
```

### Date format

```
YYYY-MM-DD
```

### Time format

```
hh:mm:ssZ
hh:mm:ss.fZ
hh:mm:ss.ffZ
hh:mm:ss.fffZ
hh:mm:ss.ffffZ
hh:mm:ss.fffffZ
hh:mm:ss.ffffffZ
```

## Performance

Depending on the field types, *tsv2py* is up to 7 times faster to parse TSV records than a functionally equivalent Python implementation based on the Python standard library. Savings in execution time are more substantial for dates, UUIDs and longer strings with special characters (up to 90% savings), and they are more moderate for simple types like small integers (approx. 60% savings).
