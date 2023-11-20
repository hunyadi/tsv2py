#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <datetime.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#if !defined(PY_MAJOR_VERSION) || !defined(PY_MINOR_VERSION) || PY_MAJOR_VERSION < 3 || (PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION < 8)
#error This extension requires Python 3.8 or later.
#endif

#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#endif

#if defined(__AVX2__)
#include <immintrin.h>

#ifdef _DEBUG
static void
debug_print_128(__m128i value)
{
    uint8_t str[16];
    _mm_storeu_si128((__m128i*)str, value);

    for (int i = 0; i < 16; i++)
    {
        printf("0x%.2x ", str[i]);
    }
    printf("\n");
}

static void
debug_print_256(__m256i value)
{
    uint8_t str[32];
    _mm256_storeu_si256((__m256i*)str, value);

    for (int i = 0; i < 32; i++)
    {
        printf("0x%.2x ", str[i]);
    }
    printf("\n");
}
#endif
#endif

#if defined(__GNUC__)
#define tzcnt_32(x) __builtin_ctz(x)
#elif defined(_WIN32)
#define tzcnt_32(x) _tzcnt_u32(x)
#endif

#if defined(Py_LIMITED_API)
#define PyTuple_SET_ITEM(tpl, index, value) PyTuple_SetItem(tpl, index, value)
#define PyTuple_GET_ITEM(tpl, index) PyTuple_GetItem(tpl, index)
#endif

inline bool
is_escaped(const char* data, Py_ssize_t size)
{
#if defined(__AVX2__)
    if (size >= 16)
    {
        __m128i tocmp = _mm_set1_epi8('\\');
        for (; size >= 16; size -= 16)
        {
            __m128i chunk = _mm_loadu_si128((__m128i const*)data);
            __m128i results = _mm_cmpeq_epi8(chunk, tocmp);
            if (_mm_movemask_epi8(results))
            {
                return true;
            }
            data += 16;
        }
    }
#endif

    for (; size > 0; --size)
    {
        if (*data == '\\')
        {
            return true;
        }
        data++;
    }
    return false;
}

static bool
unescape(const char* source, Py_ssize_t source_len, char** target, Py_ssize_t* target_len)
{
    char* output = PyMem_Malloc(source_len);

    const char* s = source;
    char* t = output;

    Py_ssize_t index = 0;
    Py_ssize_t output_len = 0;

    while (index < source_len)
    {
        if (*s == '\\')
        {
            ++s;
            ++index;

            switch (*s)
            {
            case '\\': // ASCII 92
                *t = '\\';
                break;
            case '0': // ASCII 48
                *t = '\0';
                break;
            case 'b': // ASCII 98
                *t = '\b';
                break;
            case 'f': // ASCII 102
                *t = '\f';
                break;
            case 'n': // ASCII 110
                *t = '\n';
                break;
            case 'r': // ASCII 114
                *t = '\r';
                break;
            case 't': // ASCII 116
                *t = '\t';
                break;
            case 'v': // ASCII 118
                *t = '\v';
                break;
            default:
                PyMem_Free(output);
                return false;
            }
            ++output_len;
        }
        else
        {
            *t = *s;
            ++output_len;
        }

        ++s;
        ++t;
        ++index;
    }

    *target = output;
    *target_len = output_len;
    return true;
}

#if defined(Py_LIMITED_API)
static PyObject* datetime_module;
static PyObject* date_constructor;
static PyObject* time_constructor;
static PyObject* datetime_constructor;
#endif

/**
 * Scale multipliers from micro-seconds to deci-seconds.
 * Trailing zeros are included to handle special cases with no fractional digits.
 */
static int fractional_second_scales[] = { 1, 10, 100, 1000, 10000, 100000, 0, 0 };

inline PyObject*
python_date(int year, int month, int day)
{
#if defined(Py_LIMITED_API)
    return PyObject_CallFunction(date_constructor, "iii", year, month, day);
#else
    return PyDate_FromDate(year, month, day);
#endif
}

typedef struct {
    int year;
    int month;
    int day;
} date_struct;

inline bool
parse_date(const char* input_string, date_struct* ds)
{
    char* ym_sep_ptr;
    ds->year = strtol(input_string, &ym_sep_ptr, 10);
    char* md_sep_ptr;
    ds->month = strtol(input_string + 5, &md_sep_ptr, 10);
    char* end_ptr;
    ds->day = strtol(input_string + 8, &end_ptr, 10);

    if (ym_sep_ptr != input_string + 4 || md_sep_ptr != input_string + 7 || end_ptr != input_string + 10)
    {
        return false;
    }

    return true;
}

static PyObject*
create_date(const char* input_string, Py_ssize_t input_size)
{
    if (input_size != 10 || input_string[4] != '-' || input_string[7] != '-')
    {
        PyErr_Format(PyExc_ValueError, "expected: a date field of the format `YYYY-MM-DD`; got: %.32s (len = %zd)", input_string, input_size);
        return NULL;
    }

    date_struct ds;
    if (!parse_date(input_string, &ds))
    {
        PyErr_Format(PyExc_ValueError, "expected: a date field of the format `YYYY-MM-DD`; got: %s", input_string);
        return NULL;
    }

    return python_date(ds.year, ds.month, ds.day);
}

inline PyObject*
python_time(int hour, int minute, int second, int microsecond)
{
#if defined(Py_LIMITED_API)
    return PyObject_CallFunction(time_constructor, "iiii", hour, minute, second, microsecond);
#else
    return PyTime_FromTime(hour, minute, second, microsecond);
#endif
}

inline bool
is_valid_time(const char* input_string, Py_ssize_t input_size)
{
    if (input_size < 9 || input_string[2] != ':' || input_string[5] != ':' || input_string[input_size - 1] != 'Z')
    {
        return false;
    }

    if (input_size > 9)
    {
        // e.g. `hh:mm:ss.fffZ` or `hh:mm:ss.ffffffZ`
        if (input_size < 11 || input_size > 16 || input_string[8] != '.')
        {
            return false;
        }
    }

    return true;
}

typedef struct {
    int hour;
    int minute;
    int second;
    int microsecond;
} time_struct;

inline bool
parse_time(const char* input_string, Py_ssize_t input_size, time_struct* ts)
{
    char* hm_sep_ptr;
    ts->hour = strtol(input_string, &hm_sep_ptr, 10);
    char* ms_sep_ptr;
    ts->minute = strtol(input_string + 3, &ms_sep_ptr, 10);
    char* sf_sep_ptr;
    ts->second = strtol(input_string + 6, &sf_sep_ptr, 10);
    if (hm_sep_ptr != input_string + 2 || ms_sep_ptr != input_string + 5 || sf_sep_ptr != input_string + 8)
    {
        return false;
    }

    if (input_size > 9)
    {
        char* end_ptr;
        int fractional = strtol(input_string + 9, &end_ptr, 10);
        if (end_ptr != input_string + input_size - 1)
        {
            return false;
        }

        // minimum (len = 11): hh:mm:ss.fZ
        // maximum (len = 16): hh:mm:ss.ffffffZ
        ts->microsecond = fractional * fractional_second_scales[16 - input_size];
    }
    else
    {
        ts->microsecond = 0;
    }

    return true;
}

static PyObject*
create_time(const char* input_string, Py_ssize_t input_size)
{
    if (!is_valid_time(input_string, input_size))
    {
        PyErr_Format(PyExc_ValueError, "expected: a time field of the format `hh:mm:ssZ`, or `hh:mm:ss.fZ` to `hh:mm:ss.ffffffZ`; got: %.32s (len = %zd)", input_string, input_size);
        return NULL;
    }

    time_struct ts;
    if (!parse_time(input_string, input_size, &ts))
    {
        PyErr_Format(PyExc_ValueError, "expected: a time field of the format `hh:mm:ssZ`, or `hh:mm:ss.fZ` to `hh:mm:ss.ffffffZ`; got: %s", input_string);
        return NULL;
    }

    return python_time(ts.hour, ts.minute, ts.second, ts.microsecond);
}

inline PyObject*
python_datetime(int year, int month, int day, int hour, int minute, int second, int microsecond)
{
#if defined(Py_LIMITED_API)
    return PyObject_CallFunction(datetime_constructor, "iiiiiii", year, month, day, hour, minute, second, microsecond);
#else
    return PyDateTime_FromDateAndTime(year, month, day, hour, minute, second, microsecond);
#endif
}

#if defined(__AVX2__)
/**
 * Validates a 16-byte partial date-time string `YYYY-MM-DDThh:mm`.
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
static PyObject*
create_datetime(const char* input_string, Py_ssize_t input_size)
{
    if (input_size < 20 || input_size > 27)
    {
        PyErr_Format(PyExc_ValueError, "expected: a datetime field of the format `YYYY-MM-DDThh:mm:ssZ`; got: %.32s (len = %zd)", input_string, input_size);
        return NULL;
    }

    const __m128i characters = _mm_loadu_si128((__m128i*)input_string);

    if (!is_valid_date_hour_minute(characters) || input_string[16] != ':' || input_string[input_size - 1] != 'Z')
    {
        goto error;
    }

    // convert ASCII characters into digit value (offset from character `0`)
    const __m128i ascii_digit_mask = _mm_setr_epi8(15, 15, 15, 15, 0, 15, 15, 0, 15, 15, 0, 15, 15, 0, 15, 15); // 15 = 0x0F
    const __m128i spread_integers = _mm_and_si128(characters, ascii_digit_mask);

    // group spread digits `YYYY-MM-DD hh:mm:ss` into packed digits `YYYYMMDDhhmmss--`
    const __m128i mask = _mm_set_epi8(-1, -1, 18, 17, 15, 14, 12, 11, 9, 8, 6, 5, 3, 2, 1, 0);
    const __m128i packed_integers = _mm_shuffle_epi8(spread_integers, mask);

    // fuse neighboring digits into a single value
    const __m128i weights = _mm_setr_epi8(10, 1, 10, 1, 10, 1, 10, 1, 10, 1, 10, 1, 10, 1, 0, 0);
    const __m128i values = _mm_maddubs_epi16(packed_integers, weights);

    // extract values
    char result[16];
    _mm_storeu_si128((__m128i*)result, values);

    int year = (result[0] * 100) + result[2];
    int month = result[4];
    int day = result[6];
    int hour = result[8];
    int minute = result[10];

    char* sf_sep_ptr;
    int second = strtol(input_string + 17, &sf_sep_ptr, 10);
    if (sf_sep_ptr != input_string + 19)
    {
        goto error;
    }

    int fractional = 0;
    if (input_size > 20)
    {
        char* end_ptr;
        fractional = strtol(input_string + 20, &end_ptr, 10);
        if (end_ptr != input_string + input_size - 1)
        {
            goto error;
        }
    }

    // minimum (len = 20): YYYY-MM-DDThh:mm:ssZ
    // maximum (len = 27): YYYY-MM-DDThh:mm:ss.ffffffZ
    return python_datetime(year, month, day, hour, minute, second, fractional * fractional_second_scales[27 - input_size]);

error:
    PyErr_Format(PyExc_ValueError, "expected: a datetime field of the format `YYYY-MM-DDThh:mm:ssZ`; got: %s", input_string);
    return NULL;
}
#else
static PyObject*
create_datetime(const char* input_string, Py_ssize_t input_size)
{
    const int DATE_LEN = 11;

    if (input_size < 20 || input_string[4] != '-' || input_string[7] != '-' || (input_string[10] != 'T' && input_string[10] != ' ') || !is_valid_time(input_string + DATE_LEN, input_size - DATE_LEN))
    {
        PyErr_Format(PyExc_ValueError, "expected: a datetime field of the format `YYYY-MM-DDThh:mm:ssZ`; got: %.32s (len = %zd)", input_string, input_size);
        return NULL;
    }

    date_struct ds;
    time_struct ts;
    if (!parse_date(input_string, &ds) || !parse_time(input_string + DATE_LEN, input_size - DATE_LEN, &ts))
    {
        PyErr_Format(PyExc_ValueError, "expected: a datetime field of the format `YYYY-MM-DDThh:mm:ssZ`; got: %s", input_string);
        return NULL;
    }

    return python_datetime(ds.year, ds.month, ds.day, ts.hour, ts.minute, ts.second, ts.microsecond);
}
#endif

static PyObject*
create_float(const char* input_string, Py_ssize_t input_size)
{
    char* str = PyMem_Malloc(input_size + 1);
    memcpy(str, input_string, input_size);
    str[input_size] = '\0'; // include terminating NUL byte

    char* p;
    double value = PyOS_string_to_double(str, &p, NULL);
    Py_ssize_t len = p - str;
    PyMem_Free(str);

    if (len != input_size)
    {
        PyErr_Format(PyExc_ValueError, "expected: a field with a floating-point number; got: %.32s (len = %zd)", input_string, input_size);
        return NULL;
    }

    return PyFloat_FromDouble(value);
}

inline bool
parse_integer(const char* input_string, Py_ssize_t input_size, long* value)
{
    char* end_ptr;
    *value = strtol(input_string, &end_ptr, 10);
    return end_ptr == input_string + input_size;
}

static PyObject*
create_integer(const char* input_string, Py_ssize_t input_size)
{
    if (sizeof(long) >= 8)
    {
        if (input_size < 19)
        {
            long value;
            if (!parse_integer(input_string, input_size, &value))
            {
                goto error;
            }
            return PyLong_FromLong(value);
        }
    }
    else if (sizeof(long) >= 4)
    {
        if (input_size < 10)
        {
            long value;
            if (!parse_integer(input_string, input_size, &value))
            {
                goto error;
            }
            return PyLong_FromLong(value);
        }
    }

    char* str = PyMem_Malloc(input_size + 1);
    memcpy(str, input_string, input_size);
    str[input_size] = '\0'; // include terminating NUL byte

    char* p;
    PyObject* result = PyLong_FromString(str, &p, 10);
    Py_ssize_t len = p - str;
    PyMem_Free(str);

    if (len != input_size)
    {
        goto error;
    }

    return result;

error:
    PyErr_Format(PyExc_ValueError, "expected: an integer field consisting of an optional sign and decimal digits; got: %.32s (len = %zd)", input_string, input_size);
    return NULL;
}

static PyObject*
create_bytes(const char* input_string, Py_ssize_t input_size)
{
    if (!is_escaped(input_string, input_size))
    {
        return PyBytes_FromStringAndSize(input_string, input_size);
    }

    char* output_string;
    Py_ssize_t output_size;

    if (!unescape(input_string, input_size, &output_string, &output_size))
    {
        PyErr_SetString(PyExc_ValueError, "invalid character escape sequence, only \\0, \\b, \\f, \\n, \\r, \\t and \\v are allowed");
        return NULL;
    }

    PyObject* result = PyBytes_FromStringAndSize(output_string, output_size);
    PyMem_Free(output_string);
    return result;
}

static PyObject*
create_string(const char* input_string, Py_ssize_t input_size)
{
    if (!is_escaped(input_string, input_size))
    {
        return PyUnicode_FromStringAndSize(input_string, input_size);
    }

    char* output_string;
    Py_ssize_t output_size;

    if (!unescape(input_string, input_size, &output_string, &output_size))
    {
        PyErr_SetString(PyExc_ValueError, "invalid character escape sequence, only \\0, \\b, \\f, \\n, \\r, \\t and \\v are allowed");
        return NULL;
    }

    PyObject* result = PyUnicode_FromStringAndSize(output_string, output_size);
    PyMem_Free(output_string);
    return result;
}

static PyObject*
create_boolean(const char* input_string, Py_ssize_t input_size)
{
    if (input_size == 4 && !memcmp(input_string, "true", 4))
    {
        Py_RETURN_TRUE;
    }
    else if (input_size == 5 && !memcmp(input_string, "false", 5))
    {
        Py_RETURN_FALSE;
    }
    else
    {
        PyErr_Format(PyExc_ValueError, "expected: a boolean field with a value of either `true` or `false`; got: %.32s (len = %zd)", input_string, input_size);
        return NULL;
    }
}

static PyObject* decimal_module;
static PyObject* decimal_constructor;

static PyObject*
create_decimal(const char* input_string, Py_ssize_t input_size)
{
    return PyObject_CallFunction(decimal_constructor, "s#", input_string, input_size);
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

inline bool
parse_uuid_compact(const char* str, uuid_t id)
{
    const __m256i x = _mm256_loadu_si256((__m256i*)str);
    _mm_storeu_si128((__m128i*)id, parse_uuid(x));
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
inline bool
parse_uuid_rfc_4122(const char* str, uuid_t id)
{
    // Remove dashes and pack hexadecimal ASCII bytes in a 256-bit integer
    const __m256i dash_shuffle = _mm256_set_epi32(0x80808080, 0x0f0e0d0c, 0x0b0a0908, 0x06050403, 0x80800f0e, 0x0c0b0a09, 0x07060504, 0x03020100);

    __m256i x = _mm256_loadu_si256((__m256i*)str);
    x = _mm256_shuffle_epi8(x, dash_shuffle);
    x = _mm256_insert_epi16(x, *(uint16_t*)(str + 16), 7);
    x = _mm256_insert_epi32(x, *(uint32_t*)(str + 32), 7);

    _mm_storeu_si128((__m128i*)id, parse_uuid(x));
    return true;
}
#else
inline bool
parse_uuid_compact(const char* str, uuid_t id)
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

inline bool
parse_uuid_rfc_4122(const char* str, uuid_t id)
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

static PyObject* uuid_module;
static PyObject* uuid_constructor;

static PyObject*
create_uuid(const char* input_string, Py_ssize_t input_size)
{
    uuid_t id;

    switch (input_size)
    {
    case 32:
        if (!parse_uuid_compact(input_string, id))
        {
            PyErr_Format(PyExc_ValueError, "expected: a UUID string of 32 hexadecimal digits; got: %.32s (len = %zd)", input_string, input_size);
            return NULL;
        }
        break;
    case 36:
        if (!parse_uuid_rfc_4122(input_string, id))
        {
            PyErr_Format(PyExc_ValueError, "expected: a UUID string in the 8-4-4-4-12 format, e.g. `f81d4fae-7dec-11d0-a765-00a0c91e6bf6`; got: %.32s (len = %zd)", input_string, input_size);
            return NULL;
        }
        break;
    default:
        PyErr_Format(PyExc_ValueError, "expected: a UUID string of 32 hexadecimal digits, or a UUID in the 8-4-4-4-12 format; got: %.32s (len = %zd)", input_string, input_size);
        return NULL;
    }

    /* Python signature: uuid.UUID(hex=None, bytes=None, ...) */
    return PyObject_CallFunction(uuid_constructor, "sy#", NULL, id, (Py_ssize_t)sizeof(uuid_t));
}

static PyObject* ipaddress_module;
static PyObject* ipv4addr_constructor;
static PyObject* ipv6addr_constructor;

/** Parses an IPv4 address string into an IPv4 address object. */
static PyObject*
create_ipv4addr(const char* input_string, Py_ssize_t input_size)
{
    if (input_size >= INET_ADDRSTRLEN)
    {
        PyErr_Format(PyExc_ValueError, "expected: IPv4 address; got: %.32s (len = %zd)", input_string, input_size);
        return NULL;
    }

    char addr_str[INET_ADDRSTRLEN];
    memcpy(addr_str, input_string, input_size);
    addr_str[input_size] = 0;

    unsigned char addr_net[sizeof(struct in_addr)];

    int status = inet_pton(AF_INET, addr_str, addr_net);
    if (status <= 0)
    {
        PyErr_Format(PyExc_ValueError, "expected: IPv4 address; got: %s", addr_str);
        return NULL;
    }

    return PyObject_CallFunction(ipv4addr_constructor, "y#", addr_net, sizeof(struct in_addr));
}

/** Parses an IPv6 address string into an IPv6 address object. */
static PyObject*
create_ipv6addr(const char* input_string, Py_ssize_t input_size)
{
    if (input_size >= INET6_ADDRSTRLEN)
    {
        PyErr_Format(PyExc_ValueError, "expected: IPv6 address; got: %.32s (len = %zd)", input_string, input_size);
        return NULL;
    }

    char addr_str[INET6_ADDRSTRLEN];
    memcpy(addr_str, input_string, input_size);
    addr_str[input_size] = 0;

    unsigned char addr_net[sizeof(struct in6_addr)];

    int status = inet_pton(AF_INET6, addr_str, addr_net);
    if (status <= 0)
    {
        PyErr_Format(PyExc_ValueError, "expected: IPv6 address; got: %s", addr_str);
        return NULL;
    }

    return PyObject_CallFunction(ipv6addr_constructor, "y#", addr_net, sizeof(struct in6_addr));
}

/** Parses an IPv4 or IPv6 address string into an IPv4 or IPv6 address object. */
static PyObject*
create_ipaddr(const char* input_string, Py_ssize_t input_size)
{
    if (input_size >= INET6_ADDRSTRLEN)
    {
        PyErr_Format(PyExc_ValueError, "expected: IPv4 or IPv6 address; got: %.32s (len = %zd)", input_string, input_size);
        return NULL;
    }

    char addr_str[INET6_ADDRSTRLEN];
    memcpy(addr_str, input_string, input_size);
    addr_str[input_size] = 0;

    unsigned char addr_net[sizeof(struct in6_addr)];
    int status;

    status = inet_pton(AF_INET, addr_str, addr_net);
    if (status > 0)
    {
        return PyObject_CallFunction(ipv4addr_constructor, "y#", addr_net, sizeof(struct in_addr));
    }

    status = inet_pton(AF_INET6, addr_str, addr_net);
    if (status > 0)
    {
        return PyObject_CallFunction(ipv6addr_constructor, "y#", addr_net, sizeof(struct in6_addr));
    }

    PyErr_Format(PyExc_ValueError, "expected: IPv4 or IPv6 address; got: %s", addr_str);
    return NULL;
}

static PyObject* json_module;
static PyObject* json_decoder_object;
static PyObject* json_decode_function;
static const char* json_format;

static PyObject*
create_json(const char* input_string, Py_ssize_t input_size)
{
    if (!is_escaped(input_string, input_size))
    {
        return PyObject_CallFunction(json_decode_function, json_format, input_string, input_size);
    }

    char* output_string;
    Py_ssize_t output_size;

    if (!unescape(input_string, input_size, &output_string, &output_size))
    {
        PyErr_SetString(PyExc_ValueError, "invalid character escape sequence, only \\0, \\b, \\f, \\n, \\r, \\t and \\v are allowed");
        return NULL;
    }

    PyObject* result = PyObject_CallFunction(json_decode_function, json_format, output_string, output_size);
    PyMem_Free(output_string);
    return result;
}

static PyObject*
create_any(char field_type, const char* input_string, Py_ssize_t input_size)
{
    switch (field_type)
    {
    case 'b':
        return create_bytes(input_string, input_size);

    case 'd':
        return create_date(input_string, input_size);

    case 't':
        return create_time(input_string, input_size);

    case 'T':
        return create_datetime(input_string, input_size);

    case 'f':
        return create_float(input_string, input_size);

    case 'i':
        return create_integer(input_string, input_size);

    case 's':
        return create_string(input_string, input_size);

    case 'z':
        return create_boolean(input_string, input_size);

    case '.':
        return create_decimal(input_string, input_size);

    case 'u':
        return create_uuid(input_string, input_size);

    case '4':
        return create_ipv4addr(input_string, input_size);

    case '6':
        return create_ipv6addr(input_string, input_size);

    case 'n':
        return create_ipaddr(input_string, input_size);

    case 'j':
        return create_json(input_string, input_size);

    case '_':
        // skip field without parsing
        Py_RETURN_NONE;

    default:
        PyErr_SetString(PyExc_TypeError, "expected: a field type string consisting of specifiers "
            "`b` (`bytes`), "
            "`d` (`datetime.date`), "
            "`t` (`datetime.time`), "
            "`T` (`datetime.datetime`), "
            "`f` (`float`), "
            "`i` (`int`), "
            "`s` (`str`), "
            "`z` (`bool`), "
            "`.` (`decimal.Decimal`), "
            "`u` (`uuid.UUID`), "
            "`4` (`ipaddress.IPv6Address`), "
            "`6` (`ipaddress.IPv6Address`), "
            "`n` (IPv4 or IPv6 address), "
            "`j` (serialized JSON) or "
            "`_` (skip field)"
        );
        return NULL;
    }
}

static PyObject*
create_optional_any(char field_type, const char* input_string, Py_ssize_t input_size)
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

static PyObject*
create_optional_any_range(char field_type, const char* field_start, const char* field_end)
{
    return create_optional_any(field_type, field_start, field_end - field_start);
}

static PyObject*
tsv_parse_record(PyObject* self, PyObject* args)
{
    const char* field_types;
    Py_ssize_t field_count;
    PyObject* tsv_record;
    PyObject* py_record;

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
        PyObject* tsv_field = PyTuple_GET_ITEM(tsv_record, k);
        char* input_string;
        Py_ssize_t input_size;

        if (!PyBytes_Check(tsv_field))
        {
            PyErr_SetString(PyExc_TypeError, "expected: field value as a `bytes` object");
            Py_DECREF(py_record);
            return NULL;
        }

        if (PyBytes_AsStringAndSize(tsv_field, &input_string, &input_size) < 0)
        {
            Py_DECREF(py_record);
            return NULL;
        }

        PyObject* py_field = create_optional_any(field_types[k], input_string, input_size);
        if (!py_field)
        {
            Py_DECREF(py_record);
            return NULL;
        }

        PyTuple_SET_ITEM(py_record, k, py_field);
    }

    return py_record;
}

static PyObject*
parse_line(const char* field_types, Py_ssize_t field_count, const char* line_string, Py_ssize_t line_size)
{
    const char* field_start = line_string;
    const char* field_end;
    Py_ssize_t field_index = 0;

    const char* scan_start = line_string;
    Py_ssize_t chars_remain = line_size;

    PyObject* py_record = PyTuple_New(field_count);

#if defined(__AVX2__)
    __m256i tab = _mm256_set1_epi8('\t');
    while (chars_remain >= 32)
    {
        __m256i chunk = _mm256_loadu_si256((__m256i*)scan_start);
        __m256i results = _mm256_cmpeq_epi8(chunk, tab);
        unsigned int mask = _mm256_movemask_epi8(results);

        while (mask)
        {
            unsigned int offset = tzcnt_32(mask);
            mask &= ~(1 << offset);

            field_end = scan_start + offset;

            PyObject* py_field = create_optional_any_range(field_types[field_index], field_start, field_end);
            if (!py_field)
            {
                Py_DECREF(py_record);
                return NULL;
            }
            PyTuple_SET_ITEM(py_record, field_index, py_field);

            ++field_index;
            if (field_index >= field_count)
            {
                PyErr_SetString(PyExc_ValueError, "too many fields in record input");
                Py_DECREF(py_record);
                return NULL;
            }

            field_start = field_end + 1;
        }

        scan_start += 32;
        chars_remain -= 32;
    }
#endif

    while ((field_end = memchr(scan_start, '\t', chars_remain)) != NULL)
    {
        PyObject* py_field = create_optional_any_range(field_types[field_index], field_start, field_end);
        if (!py_field)
        {
            Py_DECREF(py_record);
            return NULL;
        }
        PyTuple_SET_ITEM(py_record, field_index, py_field);

        ++field_index;
        if (field_index >= field_count)
        {
            PyErr_SetString(PyExc_ValueError, "too many fields in record input");
            Py_DECREF(py_record);
            return NULL;
        }

        field_start = field_end + 1;
        scan_start = field_start;
        chars_remain = line_size - (field_start - line_string);
    }

    if (field_index != field_count - 1)
    {
        PyErr_SetString(PyExc_ValueError, "premature end of input when parsing record");
        return NULL;
    }

    field_end = line_string + line_size;

    PyObject* py_field = create_optional_any_range(field_types[field_index], field_start, field_end);
    if (!py_field)
    {
        Py_DECREF(py_record);
        return NULL;
    }

    PyTuple_SET_ITEM(py_record, field_index, py_field);
    return py_record;
}

static PyObject*
tsv_parse_line(PyObject* self, PyObject* args)
{
    const char* field_types;
    Py_ssize_t field_count;
    const char* line_string;
    Py_ssize_t line_size;

    if (!PyArg_ParseTuple(args, "s#y#", &field_types, &field_count, &line_string, &line_size))
    {
        return NULL;
    }

    return parse_line(field_types, field_count, line_string, line_size);
}

static PyObject*
tsv_parse_file(PyObject* self, PyObject* args)
{
    const char* field_types;
    Py_ssize_t field_count;
    PyObject* obj;

    if (!PyArg_ParseTuple(args, "s#O", &field_types, &field_count, &obj))
    {
        return NULL;
    }

    /* get the `read` method of the passed object */
    PyObject* read_method;
    if ((read_method = PyObject_GetAttrString(obj, "read")) == NULL)
    {
        return NULL;
    }

    char cache_data[8192];
    Py_ssize_t cache_size = 0;

    PyObject* result = PyList_New(0);
    while (true)
    {
        /* call `read()` */
        PyObject* data;
        if ((data = PyObject_CallFunction(read_method, "i", 8192)) == NULL)
        {
            Py_DECREF(result);
            Py_DECREF(read_method);
            return NULL;
        }

        /* check for EOF */
        if (PySequence_Length(data) == 0)
        {
            Py_DECREF(data);
            break;
        }

        if (!PyBytes_Check(data))
        {
            Py_DECREF(data);
            Py_DECREF(result);
            Py_DECREF(read_method);
            PyErr_SetString(PyExc_IOError, "file must be opened in binary mode");
            return NULL;
        }

        /* extract underlying buffer data */
        char* buf;
        Py_ssize_t len;
        PyBytes_AsStringAndSize(data, &buf, &len);

        Py_ssize_t offset = 0;
        const char* buf_beg = buf;
        const char* buf_end;
        while ((buf_end = memchr(buf_beg, '\n', len - offset)) != NULL)
        {
            Py_ssize_t chunk_size = buf_end - buf_beg;
            const char* line_string;
            Py_ssize_t line_size;

            if (cache_size > 0)
            {
                if (cache_size + chunk_size >= (Py_ssize_t)sizeof(cache_data))
                {
                    PyErr_SetString(PyExc_RuntimeError, "insufficient cache size to load record");
                    return NULL;
                }
                memcpy(cache_data + cache_size, buf_beg, chunk_size);
                cache_size += chunk_size;
                cache_data[cache_size] = 0;

                line_string = cache_data;
                line_size = cache_size;
            }
            else
            {
                line_string = buf_beg;
                line_size = buf_end - buf_beg;
            }

            PyObject* item = parse_line(field_types, field_count, line_string, line_size);
            if (!item)
            {
                Py_DECREF(data);
                Py_DECREF(result);
                Py_DECREF(read_method);
                return NULL;
            }

            PyList_Append(result, item);
            Py_DECREF(item);

            cache_size = 0;

            offset += buf_end - buf_beg + 1;
            buf_beg = buf_end + 1;
        }

        memcpy(cache_data + cache_size, buf_beg, len - offset);
        cache_size += len - offset;

        /* cleanup */
        Py_DECREF(data);
    }

    /* cleanup */
    Py_DECREF(read_method);

    if (cache_size > 0)
    {
        PyObject* item = parse_line(field_types, field_count, cache_data, cache_size);
        if (!item)
        {
            Py_DECREF(result);
            return NULL;
        }

        PyList_Append(result, item);
        Py_DECREF(item);
    }

    return result;
}

static PyMethodDef TsvParserMethods[] = {
    {"parse_record", tsv_parse_record, METH_VARARGS, "Parses a tuple of byte arrays representing a TSV record into a tuple of Python objects."},
    {"parse_line", tsv_parse_line, METH_VARARGS, "Parses a line representing a TSV record into a tuple of Python objects."},
    {"parse_file", tsv_parse_file, METH_VARARGS, "Parses a TSV file into a list consisting of tuples of Python objects."},
    {NULL, NULL, 0, NULL} };

static struct PyModuleDef TsvParserModule = {
    PyModuleDef_HEAD_INIT,
    /* name of module */
    "parser",
    /* module documentation, may be NULL */
    "Parses TSV fields into a tuple of Python objects.",
    /* size of per-interpreter state of the module, or -1 if the module keeps state in global variables. */
    -1,
    TsvParserMethods
};

#if defined(__GNUC__)
__attribute__((visibility("default")))
#endif
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
    date_constructor = PyObject_GetAttrString(datetime_module, "date");
    if (!date_constructor)
    {
        return NULL;
    }
    time_constructor = PyObject_GetAttrString(datetime_module, "time");
    if (!time_constructor)
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

    /* import module `decimal` */
    decimal_module = PyImport_ImportModule("decimal");
    if (!decimal_module)
    {
        return NULL;
    }
    decimal_constructor = PyObject_GetAttrString(decimal_module, "Decimal");
    if (!decimal_constructor)
    {
        return NULL;
    }

    /* import module `uuid` */
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

    /* import module `ipaddress` */
    ipaddress_module = PyImport_ImportModule("ipaddress");
    if (!ipaddress_module)
    {
        return NULL;
    }
    ipv4addr_constructor = PyObject_GetAttrString(ipaddress_module, "IPv4Address");
    if (!ipv4addr_constructor)
    {
        return NULL;
    }
    ipv6addr_constructor = PyObject_GetAttrString(ipaddress_module, "IPv6Address");
    if (!ipv6addr_constructor)
    {
        return NULL;
    }

    /* import module `orjson` */
    json_module = PyImport_ImportModule("orjson");
    if (json_module)
    {
        json_decode_function = PyObject_GetAttrString(json_module, "loads");
        json_format = "y#";
        return PyModule_Create(&TsvParserModule);
    }
    else
    {
        PyErr_Clear();
    }

    /* import module `json` */
    json_module = PyImport_ImportModule("json");
    if (!json_module)
    {
        return NULL;
    }
    PyObject* json_decoder_constructor = PyObject_GetAttrString(json_module, "JSONDecoder");
    if (!json_decoder_constructor)
    {
        return NULL;
    }
    json_decoder_object = PyObject_CallFunction(json_decoder_constructor, NULL);
    Py_DECREF(json_decoder_constructor);
    if (!json_decoder_object)
    {
        return NULL;
    }
    json_decode_function = PyObject_GetAttrString(json_decoder_object, "decode");
    json_format = "s#";

    return PyModule_Create(&TsvParserModule);
}
