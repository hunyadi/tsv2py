from typing import Any, Tuple

def parse(field_types: str, record: Tuple[bytes, ...]) -> Tuple[Any, ...]:
    """
    Parses a tuple of fields into a tuple of Python objects, according to a type specification.

    The following type specification characters are supported:
        * `b` for `bytes` (pass-through mode)
        * `d` for `datetime` (naive, assumed as if in UTC)
        * `f` for `float`
        * `i` for `int`
        * `s` for `str`
        * `z` for `bool`

    :param field_types: Identifies the expected type of each field.
    :param record: A tuple of `bytes` objects, each corresponding to the represented value of a field.
    :returns: A tuple of Python objects, each corresponding to the parsed value of a field.
    """

    ...
