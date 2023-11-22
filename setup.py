# If you have cloned the source code repository, use editable install to link the package catalog to the repository directory:
# $ pip install -e . --config-settings editable_mode=compat

import os
import sys
from typing import List, Optional, Tuple

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

define_macros: List[Tuple[str, Optional[str]]]
if os.getenv("TSV_LIMITED_API", "1") == "1":
    print("compiling with limited C API")
    define_macros = [("Py_LIMITED_API", "0x03080000")]
    limited_api = True
else:
    print("compiling with regular C API")
    define_macros = []
    limited_api = False

extension_modules = [
    Extension(
        "tsv.parser",
        ["lib/tsv_parser.c"],
        extra_compile_args=compile_args,
        extra_link_args=[],
        include_dirs=["lib"],
        define_macros=define_macros,
        language="c",
        py_limited_api=limited_api,
    )
]

if __name__ == "__main__":
    setup(
        ext_modules=extension_modules,
        cmdclass={"bdist_wheel": bdist_wheel_abi3},
    )
