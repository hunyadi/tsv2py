#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <datetime.h>
#include <stdbool.h>

#if defined(__AVX2__)
#include <immintrin.h>

#ifdef _DEBUG
static void
debug_print(__m128i value)
{
    uint8_t str[16];
    _mm_storeu_si128((__m128i *)str, value);

    for (int i = 0; i < 16; i++)
    {
        printf("0x%.2x ", str[i]);
    }
    printf("\n");
}
#endif
#endif

#define TSV_NOT_LEN ~((Py_ssize_t)0)

static Py_ssize_t
unescape(const char *source, Py_ssize_t source_len, char **target)
{
    char *output = PyMem_Malloc(source_len);

    const char *s = source;
    char *t = output;

    Py_ssize_t index = 0;
    Py_ssize_t target_len = 0;

    while (index < source_len)
    {
        if (*s == '\\')
        {
            ++s;
            ++index;

            switch (*s)
            {
            case '\\':
                *t = '\\';
                break;
            case '0':
                *t = '\0';
                break;
            case 'b':
                *t = '\b';
                break;
            case 'f':
                *t = '\f';
                break;
            case 'n':
                *t = '\n';
                break;
            case 'r':
                *t = '\r';
                break;
            case 't':
                *t = '\t';
                break;
            case 'v':
                *t = '\v';
                break;
            default:
                PyMem_Free(output);
                return TSV_NOT_LEN;
            }
            ++target_len;
        }
        else
        {
            *t = *s;
            ++target_len;
        }

        ++s;
        ++t;
        ++index;
    }

    *target = output;
    return target_len;
}

#if defined(Py_LIMITED_API)
static PyObject *datetime_module;
static PyObject *datetime_constructor;
#endif

inline PyObject *
python_datetime(int year, int month, int day, int hour, int minute, int second)
{
#if defined(Py_LIMITED_API)
    return PyObject_CallFunction(datetime_constructor, "iiiiii", year, month, day, hour, minute, second);
#else
    return PyDateTime_FromDateAndTime(year, month, day, hour, minute, second, 0);
#endif
}

#if defined(__AVX2__)
/**
 * Validates a 16-byte partial date-time string `YYYY-MM-DDTHH:MM`.
 */
inline bool
is_valid_date_hour_minute(__m128i characters)
{
    const __m128i lower_bound = _mm_setr_epi8(
        48, 48, 48, 48, // year; 48 = ASCII '0'
        45,             // ASCII '-'
        48, 48,         // month
        45,             // ASCII '-'
        48, 48,         // day
        84,             // ASCII 'T'
        48, 48,         // hour
        58,             // ASCII ':'
        48, 48          // minute
    );
    const __m128i upper_bound = _mm_setr_epi8(
        57, 57, 57, 57, // year; 57 = ASCII '9'
        45,             // ASCII '-'
        49, 57,         // month
        45,             // ASCII '-'
        51, 57,         // day
        84,             // ASCII 'T'
        50, 57,         // hour
        58,             // ASCII ':'
        53, 57          // minute
    );

    const __m128i all_ones = _mm_set1_epi8(-1); // 128 bits of 1s

    const __m128i too_low = _mm_cmpgt_epi8(lower_bound, characters);
    const __m128i too_high = _mm_cmpgt_epi8(characters, upper_bound);
    const __m128i out_of_bounds = _mm_or_si128(too_low, too_high);
    const int within_range = _mm_test_all_zeros(out_of_bounds, all_ones);

    return within_range;
}

/**
 * Parses an RFC 3339 date-time string with SIMD instructions.
 *
 * @see https://movermeyer.com/2023-01-04-rfc-3339-simd/
 */
static PyObject *
create_datetime(const char *input_string, Py_ssize_t input_size)
{
    const __m128i characters = _mm_loadu_si128((__m128i *)input_string);

    if (!is_valid_date_hour_minute(characters) || input_string[16] != ':' || input_string[19] != 'Z')
    {
        PyErr_SetString(PyExc_ValueError, "expected: a datetime field of the format `YYYY-MM-DDTHH:MM:SSZ` or `YYYY-MM-DD HH:MM:SSZ`");
        return NULL;
    }

    // convert ASCII characters into digit value (offset from character `0`)
    const __m128i ascii_digit_mask = _mm_setr_epi8(15, 15, 15, 15, 0, 15, 15, 0, 15, 15, 0, 15, 15, 0, 15, 15); // 15 = 0x0F
    const __m128i spread_integers = _mm_and_si128(characters, ascii_digit_mask);

    // group spread digits `YYYY-MM-DD HH:MM` into packed digits `YYYYMMDDHHMM----`
    const __m128i mask = _mm_set_epi8(-1, -1, -1, -1, 15, 14, 12, 11, 9, 8, 6, 5, 3, 2, 1, 0);
    const __m128i packed_integers = _mm_shuffle_epi8(spread_integers, mask);

    // fuse neighboring digits into a single value
    const __m128i weights = _mm_setr_epi8(10, 1, 10, 1, 10, 1, 10, 1, 10, 1, 10, 1, 0, 0, 0, 0);
    const __m128i values = _mm_maddubs_epi16(packed_integers, weights);

    // extract values
    char result[16];
    _mm_storeu_si128((__m128i *)result, values);

    int year = (result[0] * 100) + result[2];
    int month = result[4];
    int day = result[6];
    int hour = result[8];
    int minute = result[10];

    int second = atoi(input_string + 17);
    return python_datetime(year, month, day, hour, minute, second);
}
#else
static PyObject *
create_datetime(const char *input_string, Py_ssize_t input_size)
{
    if (input_size != 20 || input_string[4] != '-' || input_string[7] != '-' || (input_string[10] != 'T' && input_string[10] != ' ') || input_string[13] != ':' || input_string[16] != ':' || input_string[19] != 'Z')
    {
        PyErr_SetString(PyExc_ValueError, "expected: a datetime field of the format `YYYY-MM-DDTHH:MM:SSZ` or `YYYY-MM-DD HH:MM:SSZ`");
        return NULL;
    }

    int year = atoi(input_string);
    int month = atoi(input_string + 5);
    int day = atoi(input_string + 8);
    int hour = atoi(input_string + 11);
    int minute = atoi(input_string + 14);
    int second = atoi(input_string + 17);
    return python_datetime(year, month, day, hour, minute, second);
}
#endif

static PyObject *
create_float(const char *input_string, Py_ssize_t input_size)
{
    char *p;
    double value = PyOS_string_to_double(input_string, &p, NULL);

    if (p != input_string + input_size)
    {
        PyErr_SetString(PyExc_ValueError, "expected: a field with a floating-point number");
        return NULL;
    }

    return PyFloat_FromDouble(value);
}

static PyObject *
create_integer(const char *input_string, Py_ssize_t input_size)
{
    char *str = PyMem_Malloc(input_size + 1);
    memcpy(str, input_string, input_size);
    str[input_size] = '\0'; // include terminating NUL byte

    char *p;
    PyObject *result = PyLong_FromString(str, &p, 10);
    Py_ssize_t len = p - str;
    PyMem_Free(str);

    if (len != input_size)
    {
        PyErr_SetString(PyExc_ValueError, "expected: an integer field consisting of an optional sign and decimal digits");
        return NULL;
    }

    return result;
}

static PyObject *
create_string(const char *input_string, Py_ssize_t input_size)
{
    PyObject *result;
    char *output_string;
    Py_ssize_t output_size;

    output_size = unescape(input_string, input_size, &output_string);
    if (output_size == TSV_NOT_LEN)
    {
        PyErr_SetString(PyExc_ValueError, "invalid character escape sequence, only \\0, \\b, \\f, \\n, \\r, \\t and \\v are allowed");
        return NULL;
    }

    result = PyUnicode_FromStringAndSize(output_string, output_size);
    PyMem_Free(output_string);
    return result;
}

static PyObject *
create_boolean(const char *input_string, Py_ssize_t input_size)
{
    if (input_size == 4 && !strcmp(input_string, "true"))
    {
        Py_RETURN_TRUE;
    }
    else if (input_size == 5 && !strcmp(input_string, "false"))
    {
        Py_RETURN_FALSE;
    }
    else
    {
        PyErr_SetString(PyExc_ValueError, "expected: a boolean field with a value of either `true` or `false`");
        return NULL;
    }
}

typedef unsigned char uuid_t[16];

#if defined(__AVX2__)
inline __m128i
parse_uuid(__m256i x)
{
    // Build a mask to apply a different offset to alphas and digits
    const __m256i sub = _mm256_set1_epi8(0x2F);
    const __m256i mask = _mm256_set1_epi8(0x20);
    const __m256i alpha_offset = _mm256_set1_epi8(0x28);
    const __m256i digits_offset = _mm256_set1_epi8(0x01);
    const __m256i unweave = _mm256_set_epi32(0x0f0d0b09, 0x0e0c0a08, 0x07050301, 0x06040200, 0x0f0d0b09, 0x0e0c0a08, 0x07050301, 0x06040200);
    const __m256i shift = _mm256_set_epi32(0x00000000, 0x00000004, 0x00000000, 0x00000004, 0x00000000, 0x00000004, 0x00000000, 0x00000004);

    // Translate ASCII bytes to their value
    // i.e. 0x3132333435363738 -> 0x0102030405060708
    // Shift hi-digits
    // i.e. 0x0102030405060708 -> 0x1002300450067008
    // Horizontal add
    // i.e. 0x1002300450067008 -> 0x12345678
    __m256i a = _mm256_sub_epi8(x, sub);
    __m256i alpha = _mm256_slli_epi64(_mm256_and_si256(a, mask), 2);
    __m256i sub_mask = _mm256_blendv_epi8(digits_offset, alpha_offset, alpha);
    a = _mm256_sub_epi8(a, sub_mask);
    a = _mm256_shuffle_epi8(a, unweave);
    a = _mm256_sllv_epi32(a, shift);
    a = _mm256_hadd_epi32(a, _mm256_setzero_si256());
    a = _mm256_permute4x64_epi64(a, 0b00001000);

    return _mm256_castsi256_si128(a);
}

static bool
parse_uuid_compact(const char *str, uuid_t id)
{
    const __m256i x = _mm256_loadu_si256((__m256i *)str);
    _mm_storeu_si128((__m128i *)id, parse_uuid(x));
    return true;
}

/**
 * Converts an UUIDv4 string representation to a 128-bit unsigned int.
 *
 * UUID string is expected in the 8-4-4-4-12 format, e.g. `f81d4fae-7dec-11d0-a765-00a0c91e6bf6`.
 * Uses SIMD via Intel AVX2 instruction set.
 *
 * @see https://github.com/crashoz/uuid_v4
 */
static bool
parse_uuid_rfc_4122(const char *str, uuid_t id)
{
    // Remove dashes and pack hexadecimal ASCII bytes in a 256-bit integer
    const __m256i dash_shuffle = _mm256_set_epi32(0x80808080, 0x0f0e0d0c, 0x0b0a0908, 0x06050403, 0x80800f0e, 0x0c0b0a09, 0x07060504, 0x03020100);

    __m256i x = _mm256_loadu_si256((__m256i *)str);
    x = _mm256_shuffle_epi8(x, dash_shuffle);
    x = _mm256_insert_epi16(x, *(uint16_t *)(str + 16), 7);
    x = _mm256_insert_epi32(x, *(uint32_t *)(str + 32), 7);

    _mm_storeu_si128((__m128i *)id, parse_uuid(x));
    return true;
}
#else
static bool
parse_uuid_compact(const char *str, uuid_t id)
{
    int n = 0;
    sscanf(str,
           "%2hhx%2hhx%2hhx%2hhx"
           "%2hhx%2hhx"
           "%2hhx%2hhx"
           "%2hhx%2hhx"
           "%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%n",
           &id[0], &id[1], &id[2], &id[3],
           &id[4], &id[5],
           &id[6], &id[7],
           &id[8], &id[9],
           &id[10], &id[11], &id[12], &id[13], &id[14], &id[15], &n);
    return n == 32;
}

static bool
parse_uuid_rfc_4122(const char *str, uuid_t id)
{
    int n = 0;
    sscanf(str,
           "%2hhx%2hhx%2hhx%2hhx-"
           "%2hhx%2hhx-"
           "%2hhx%2hhx-"
           "%2hhx%2hhx-"
           "%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%n",
           &id[0], &id[1], &id[2], &id[3],
           &id[4], &id[5],
           &id[6], &id[7],
           &id[8], &id[9],
           &id[10], &id[11], &id[12], &id[13], &id[14], &id[15], &n);
    return n == 36;
}
#endif

static PyObject *uuid_module;
static PyObject *uuid_constructor;

static PyObject *
create_uuid(const char *input_string, Py_ssize_t input_size)
{
    uuid_t id;

    switch (input_size)
    {
    case 32:
        if (!parse_uuid_compact(input_string, id))
        {
            PyErr_SetString(PyExc_ValueError, "expected: a UUID string of 32 hexadecimal digits");
            return NULL;
        }
        break;
    case 36:
        if (!parse_uuid_rfc_4122(input_string, id))
        {
            PyErr_SetString(PyExc_ValueError, "expected: a UUID string in the 8-4-4-4-12 format, e.g. `f81d4fae-7dec-11d0-a765-00a0c91e6bf6`");
            return NULL;
        }
        break;
    default:
        PyErr_SetString(PyExc_ValueError, "expected: a UUID string of 32 hexadecimal digits, or a UUID in the 8-4-4-4-12 format");
        return NULL;
    }

    /* Python signature: uuid.UUID(hex=None, bytes=None, ...) */
    return PyObject_CallFunction(uuid_constructor, "sy#", NULL, id, (Py_ssize_t)sizeof(uuid_t));
}

static PyObject *
create_any(char field_type, const char *input_string, Py_ssize_t input_size)
{
    switch (field_type)
    {
    case 'b':
        return PyBytes_FromStringAndSize(input_string, input_size);

    case 'd':
        return create_datetime(input_string, input_size);

    case 'f':
        return create_float(input_string, input_size);

    case 'i':
        return create_integer(input_string, input_size);

    case 's':
        return create_string(input_string, input_size);

    case 'z':
        return create_boolean(input_string, input_size);

    case 'u':
        return create_uuid(input_string, input_size);

    default:
        PyErr_SetString(PyExc_TypeError, "expected: a field type string consisting of specifiers `b` (`bytes`), `d` (`datetime.datetime`), `f` (`float`), `i` (`int`), `s` (`str`), `u` (`uuid.UUID`) or `z` (`bool`)");
        return NULL;
    }
}

static PyObject *
create_optional_any(char field_type, const char *input_string, Py_ssize_t input_size)
{
    if (input_size == 2 && input_string[0] == '\\' && input_string[1] == 'N')
    {
        /* return TSV \N as Python None */
        Py_RETURN_NONE;
    }
    else
    {
        /* instantiate Python object based on field value */
        return create_any(field_type, input_string, input_size);
    }
}

static PyObject *
tsv_parse_record(PyObject *self, PyObject *args)
{
    const char *field_types;
    Py_ssize_t field_count;
    PyObject *tsv_record;
    PyObject *py_record;

    if (!PyArg_ParseTuple(args, "s#O", &field_types, &field_count, &tsv_record))
    {
        return NULL;
    }

    if (!PyTuple_Check(tsv_record))
    {
        PyErr_SetString(PyExc_TypeError, "expected: record as a tuple of field values");
        return NULL;
    }

    if (PyTuple_Size(tsv_record) != field_count)
    {
        PyErr_SetString(PyExc_ValueError, "expected: field type string length equal to record tuple size");
        return NULL;
    }

    py_record = PyTuple_New(field_count);
    Py_ssize_t k;
    for (k = 0; k < field_count; ++k)
    {
#if defined(Py_LIMITED_API)
        PyObject *tsv_field = PyTuple_GetItem(tsv_record, k);
#else
        PyObject *tsv_field = PyTuple_GET_ITEM(tsv_record, k);
#endif
        char *input_string;
        Py_ssize_t input_size;

        if (!PyBytes_Check(tsv_field))
        {
            PyErr_SetString(PyExc_TypeError, "expected: field value as a `bytes` object");
            return NULL;
        }

        if (PyBytes_AsStringAndSize(tsv_field, &input_string, &input_size) < 0)
        {
            return NULL;
        }

        PyObject *py_field = create_optional_any(field_types[k], input_string, input_size);
        if (!py_field)
        {
            return NULL;
        }

#if defined(Py_LIMITED_API)
        PyTuple_SetItem(py_record, k, py_field);
#else
        PyTuple_SET_ITEM(py_record, k, py_field);
#endif
    }

    return py_record;
}

static PyObject *
tsv_parse_line(PyObject *self, PyObject *args)
{
    const char *field_types;
    Py_ssize_t field_count;
    const char *line_string;
    Py_ssize_t line_size;

    PyObject *py_record;

    if (!PyArg_ParseTuple(args, "s#y#", &field_types, &field_count, &line_string, &line_size))
    {
        return NULL;
    }

    const char *field_start = line_string;
    const char *field_end;

    py_record = PyTuple_New(field_count);

    // Parse first n-1 fields (each terminated by `\t`)
    Py_ssize_t k;
    for (k = 0; k < field_count - 1; ++k)
    {
        field_end = strchr(field_start, '\t');
        if (field_end == NULL)
        {
            PyErr_SetString(PyExc_ValueError, "premature end of input");
            return NULL;
        }

        const char *input_string = field_start;
        Py_ssize_t input_size = field_end - field_start;

        PyObject *py_field = create_optional_any(field_types[k], input_string, input_size);
        if (!py_field)
        {
            return NULL;
        }

#if defined(Py_LIMITED_API)
        PyTuple_SetItem(py_record, k, py_field);
#else
        PyTuple_SET_ITEM(py_record, k, py_field);
#endif

        field_start = field_end + 1;
    }

    // Parse last field (terminated by end of input)
    if (strchr(field_start, '\t') != NULL)
    {
        PyErr_SetString(PyExc_ValueError, "too many fields in input");
        return NULL;
    }

    field_end = line_string + line_size;
    const char *input_string = field_start;
    Py_ssize_t input_size = field_end - field_start;

    PyObject *py_field = create_optional_any(field_types[field_count - 1], input_string, input_size);
    if (!py_field)
    {
        return NULL;
    }

#if defined(Py_LIMITED_API)
    PyTuple_SetItem(py_record, field_count - 1, py_field);
#else
    PyTuple_SET_ITEM(py_record, field_count - 1, py_field);
#endif

    return py_record;
}

static PyMethodDef TsvParserMethods[] = {
    {"parse_record", tsv_parse_record, METH_VARARGS, "Parses a tuple of byte arrays representing a TSV record into a tuple of Python objects."},
    {"parse_line", tsv_parse_line, METH_VARARGS, "Parses a line representing a TSV record into a tuple of Python objects."},
    {NULL, NULL, 0, NULL}};

static struct PyModuleDef TsvParserModule = {
    PyModuleDef_HEAD_INIT,
    /* name of module */
    "parser",
    /* module documentation, may be NULL */
    "Parses TSV fields into a tuple of Python objects.",
    /* size of per-interpreter state of the module, or -1 if the module keeps state in global variables. */
    -1,
    TsvParserMethods};

PyMODINIT_FUNC
PyInit_parser(void)
{
    /* import module datetime */
#if defined(Py_LIMITED_API)
    datetime_module = PyImport_ImportModule("datetime");
    if (!datetime_module)
    {
        return NULL;
    }
    datetime_constructor = PyObject_GetAttrString(datetime_module, "datetime");
    if (!datetime_constructor)
    {
        return NULL;
    }
#else
    PyDateTime_IMPORT;
#endif

    /* import module uuid */
    uuid_module = PyImport_ImportModule("uuid");
    if (!uuid_module)
    {
        return NULL;
    }
    uuid_constructor = PyObject_GetAttrString(uuid_module, "UUID");
    if (!uuid_constructor)
    {
        return NULL;
    }

    return PyModule_Create(&TsvParserModule);
}
