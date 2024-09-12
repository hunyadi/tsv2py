set -e

PYTHON=python3

$PYTHON -m mypy tsv
$PYTHON -m flake8 tsv
$PYTHON -m mypy tests
$PYTHON -m flake8 tests

docker build --build-arg PYTHON_VERSION=3.8 .
docker build --build-arg PYTHON_VERSION=3.9 .
docker build --build-arg PYTHON_VERSION=3.10 .
docker build --build-arg PYTHON_VERSION=3.11 .
docker build --build-arg PYTHON_VERSION=3.12 .
