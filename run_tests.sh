set -e

rm -rf evaluate
TSV_AVX2=0 python3 setup.py build --build-lib evaluate
cp -R tests evaluate/
pushd evaluate
python3 -m unittest discover tests
popd > /dev/null

rm -rf evaluate
TSV_AVX2=1 python3 setup.py build --build-lib evaluate
cp -R tests evaluate/
pushd evaluate
python3 -m unittest discover tests
popd > /dev/null

rm -rf evaluate
TSV_AVX2=1 TSV_LIMITED_API=0 python3 setup.py build --build-lib evaluate
cp -R tests evaluate/
pushd evaluate
python3 -m unittest discover tests
popd > /dev/null

rm -rf evaluate
