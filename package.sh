set -e
if [ -d build ]; then rm -rf build; fi
if [ -d dist ]; then rm -rf dist; fi
if [ -d tsv.egg-info ]; then rm -rf tsv.egg-info; fi

python3 -m build

IMAGE_TAG=tsv_python_wheel
docker build -t ${IMAGE_TAG} .
docker run -d ${IMAGE_TAG}
CONTAINER_ID=$(docker ps -alq)
docker cp ${CONTAINER_ID}:/package/wheelhouse dist/
mv dist/wheelhouse/* dist/
rmdir dist/wheelhouse
docker stop ${CONTAINER_ID}
