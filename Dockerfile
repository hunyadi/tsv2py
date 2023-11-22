ARG PYTHON_VERSION=3.8
FROM python:${PYTHON_VERSION}-alpine
RUN python3 -m pip install --upgrade pip
COPY wheelhouse/*.whl wheelhouse/
RUN python3 -m pip install --disable-pip-version-check --no-index --find-links=wheelhouse tsv2py
COPY tests/*.py tests/
RUN python3 -m unittest discover
