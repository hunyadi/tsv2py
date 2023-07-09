set -e
cibuildwheel --platform linux
abi3audit wheelhouse/*-cp3*.whl
python3 -m twine upload wheelhouse/*
python3 -m build
python3 -m twine upload dist/*
