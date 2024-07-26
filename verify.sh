set -e

python3 -m mypy tsv
python3 -m flake8 tsv
python3 -m mypy tests
python3 -m flake8 tests

docker build --build-arg PYTHON_VERSION=3.8 .
docker build --build-arg PYTHON_VERSION=3.9 .
docker build --build-arg PYTHON_VERSION=3.10 .
docker build --build-arg PYTHON_VERSION=3.11 .
docker build --build-arg PYTHON_VERSION=3.12 .
