#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "cpu_features.h"

#if !defined(PY_MAJOR_VERSION) || !defined(PY_MINOR_VERSION) || PY_MAJOR_VERSION < 3 || (PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION < 8)
#error This extension requires Python 3.8 or later.
#endif

static PyObject*
has_avx2(PyObject* self, PyObject* args)
{
    if (supports_avx() && supports_avx2() && check_xcr0_ymm())
    {
        Py_RETURN_TRUE;
    }
    else
    {
        Py_RETURN_FALSE;
    }
}

static PyMethodDef CpuFeatureCheckMethods[] = {
    {"has_avx2", has_avx2, METH_VARARGS, "Checks whether the platform supports AVX2 CPU instructions."},
    {NULL, NULL, 0, NULL} };

static struct PyModuleDef CpuFeatureCheckModule = {
    PyModuleDef_HEAD_INIT,
    /* name of module */
    "cpu_features",
    /* module documentation, may be NULL */
    "Detects CPU features and intrinsics available on the platform.",
    /* size of per-interpreter state of the module, or -1 if the module keeps state in global variables. */
    -1,
    CpuFeatureCheckMethods
};

#if defined(__GNUC__)
__attribute__((visibility("default")))
#endif
PyMODINIT_FUNC
PyInit_cpu_features(void)
{
    return PyModule_Create(&CpuFeatureCheckModule);
}
