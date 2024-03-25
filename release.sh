set -e
if [ -d build ]; then rm -rf build; fi
if [ -d wheelhouse ]; then rm -rf wheelhouse; fi
if [ -d *.egg-info ]; then rm -rf *.egg-info; fi
MACOSX_DEPLOYMENT_TARGET=10.9 /usr/bin/python3 -m build --outdir wheelhouse
cibuildwheel --platform linux
abi3audit wheelhouse/*-cp3*.whl
