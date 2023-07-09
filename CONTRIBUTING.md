# Parse and generate tab-separated values (TSV) data

This C extension module uses the Python [Limited API](https://docs.python.org/3/c-api/stable.html) targeting Python 3.8 and later.

## Building from source

This project uses [cibuildwheel](https://cibuildwheel.readthedocs.io/en/stable/) to create packages for PyPI:

```sh
cibuildwheel --platform linux
```

Scan for possible ABI3 violations and inconsistencies with [abi3audit](https://github.com/trailofbits/abi3audit):

```sh
abi3audit wheelhouse/*-cp3*.whl
```
