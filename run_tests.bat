@echo off
setlocal

rmdir /s /q evaluate
set TSV_AVX2=0
python setup.py build --build-lib evaluate || exit /b
xcopy /e /i tests evaluate\tests\
pushd evaluate
python -m unittest discover tests || exit /b
popd

rmdir /s /q evaluate
set TSV_AVX2=1
python setup.py build --build-lib evaluate || exit /b
xcopy /e /i tests evaluate\tests\
pushd evaluate
python -m unittest discover tests || exit /b
popd

rmdir /s /q evaluate
set TSV_AVX2=1
set TSV_LIMITED_API=0
python setup.py build --build-lib evaluate || exit /b
xcopy /e /i tests evaluate\tests\
pushd evaluate
python -m unittest discover tests || exit /b
popd

rmdir /s /q evaluate
:quit
