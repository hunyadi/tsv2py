set -e
if [ -d build ]; then rm -rf build; fi
if [ -d wheelhouse ]; then rm -rf wheelhouse; fi
if [ -d tsv2py.egg-info ]; then rm -rf tsv2py.egg-info; fi
/usr/bin/python3 -m build --outdir wheelhouse
cibuildwheel --platform linux
abi3audit wheelhouse/*-cp3*.whl
