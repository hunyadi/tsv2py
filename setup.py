# If you have cloned the source code repository, use editable install to link the package catalog to the repository directory:
# $ pip install -e . --config-settings editable_mode=compat

import os
import sys
from typing import Tuple

from setuptools import Extension, setup
from wheel.bdist_wheel import bdist_wheel


class bdist_wheel_abi3(bdist_wheel):
    def get_tag(self) -> Tuple[str, str, str]:
        python, abi, plat = super().get_tag()

        if python.startswith("cp"):
            # on CPython, our wheels are abi3 and compatible back to 3.8
            return "cp38", "abi3", plat

        return python, abi, plat


if sys.platform.startswith("win"):
    compile_args = []
else:
    compile_args = ["-fvisibility=hidden"]

if os.getenv("TSV_AVX2", "1") == "1":
    print("compiling with AVX2")
    if sys.platform.startswith("win"):
        compile_args.append("/arch:AVX2")
    else:
        compile_args.append("-mavx2")
else:
    print("compiling without AVX2")

setup_args = dict(
    ext_modules=[
        Extension(
            "tsv.parser",
            ["lib/tsv_parser.c"],
            extra_compile_args=compile_args,
            include_dirs=["lib"],
            define_macros=[("Py_LIMITED_API", "0x03080000")],
            language="c",
            py_limited_api=True,
        )
    ],
    cmdclass={"bdist_wheel": bdist_wheel_abi3},
)

if __name__ == "__main__":
    setup(**setup_args)
