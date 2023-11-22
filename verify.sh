set -e
docker build --build-arg PYTHON_VERSION=3.8 .
docker build --build-arg PYTHON_VERSION=3.9 .
docker build --build-arg PYTHON_VERSION=3.10 .
docker build --build-arg PYTHON_VERSION=3.11 .
docker build --build-arg PYTHON_VERSION=3.12 .
