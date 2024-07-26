# If you have cloned the source code repository, use editable install to link the package catalog to the repository directory:
# $ pip install -e . --config-settings editable_mode=compat

import os
import sys
from typing import List, Optional, Tuple

from setuptools import Extension, setup
from wheel.bdist_wheel import bdist_wheel

if sys.version_info < (3, 8):
    raise RuntimeError("tsv2py requires Python 3.8 or later")


class bdist_wheel_abi3(bdist_wheel):
    def get_tag(self) -> Tuple[str, str, str]:
        python, abi, plat = super().get_tag()

        if python.startswith("cp"):
            # on CPython, our wheels are abi3 and compatible back to 3.8
            return "cp38", "abi3", plat

        return python, abi, plat


compile_args: List[str] = []
if not sys.platform.startswith("win"):
    compile_args.append("-fvisibility=hidden")

avx2_args: List[str] = []
avx2_enabled = False
if os.getenv("TSV_AVX2", "1") == "1":
    if sys.platform.startswith("win"):
        avx2_enabled = True
        avx2_args.append("/arch:AVX2")
    elif "x86_64" in os.uname().machine and "ARM" not in os.uname().version:
        avx2_enabled = True
        avx2_args.append("-mavx2")
if avx2_enabled:
    print("compiling with AVX2")
else:
    print("compiling without AVX2")

define_macros: List[Tuple[str, Optional[str]]] = []
if sys.platform.startswith("win"):
    define_macros.append(("_WIN32_WINNT", "0x0603"))
if os.getenv("TSV_LIMITED_API", "1") == "1":
    print("compiling with limited C API")
    define_macros.append(("Py_LIMITED_API", "0x03080000"))
    limited_api = True
else:
    print("compiling with regular C API")
    limited_api = False

extension_modules = [
    Extension(
        "tsv.cpu_features",
        ["lib/cpu_features.c"],
        extra_compile_args=compile_args,
        extra_link_args=[],
        include_dirs=["lib"],
        define_macros=define_macros,
        language="c",
        py_limited_api=limited_api,
    ),
    Extension(
        "tsv.parser_plain",
        ["lib/tsv_parser.c"],
        extra_compile_args=compile_args,
        extra_link_args=[],
        include_dirs=["lib"],
        define_macros=define_macros + [("TSV_MODULE_FUNC", "PyInit_parser_plain")],
        language="c",
        py_limited_api=limited_api,
    ),
]
if avx2_enabled:
    extension_modules.append(
        Extension(
            "tsv.parser_avx2",
            ["lib/tsv_parser.c"],
            extra_compile_args=compile_args + avx2_args,
            extra_link_args=[],
            include_dirs=["lib"],
            define_macros=define_macros + [("TSV_MODULE_FUNC", "PyInit_parser_avx2")],
            language="c",
            py_limited_api=limited_api,
        )
    )

if __name__ == "__main__":
    setup(
        ext_modules=extension_modules,
        cmdclass={"bdist_wheel": bdist_wheel_abi3},
    )
