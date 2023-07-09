set -e

# compile wheels
for PYBIN in /opt/python/*/bin; do
    "${PYBIN}/python" -m venv ${PYBIN}
    source ${PYBIN}/bin/activate
    PIP_DISABLE_PIP_VERSION_CHECK=1 python -m pip install build
    python -m build --wheel --outdir wheelhouse
    deactivate
done
