/*
 * Earl, the fanciest ETF Packer and Unpacker for Python.
 * Written as a C++ Extension for Maximum Effort and Speed.
 *
 * Copyright 2016 Charles Click under the MIT License.
 */

// Includes
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <string>
#include <vector>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <algorithm>
#include <stdexcept>
#include <iostream>

#if defined(_MSC_VER) && _MSC_VER
#include <iso646.h>
#endif

// External Term Format Defines
const char FORMAT_VERSION = '\x83';
const char FLOAT_IEEE_EXT = 'F';
const char BIT_BINARY_EXT = 'M';
const char SMALL_INTEGER_EXT = 'a';
const char INTEGER_EXT = 'b';
const char FLOAT_EXT = 'c';
const char SMALL_TUPLE_EXT = 'h';
const char LARGE_TUPLE_EXT = 'i';
const char NIL_EXT = 'j';
const char STRING_EXT = 'k';
const char LIST_EXT = 'l';
const char BINARY_EXT = 'm';
const char SMALL_BIG_EXT = 'n';
const char LARGE_BIG_EXT = 'o';
const char MAP_EXT = 't';
const char ATOM_EXT = 'd';
const char SMALL_ATOM_EXT = 's';
const char ATOM_UTF_EXT = 'v';
const char ATOM_UTF_SMALL_EXT = 'w';
const char COMPRESSED_TERM = 'P';

extern "C" {
static PyObject* earl_pack(PyObject* self, PyObject* args, PyObject* kwargs);
static PyObject* earl_unpack(PyObject* self, PyObject* args, PyObject* kwargs);
// our custom exception types
PyObject* earl_DecodeError;
PyObject* earl_EncodeError;
}

enum encode_type {
    str = 0,
    bytes = 1,
    atom = 2
};

template<typename T>
static T from_big_endian(const char* bytes) {
    const unsigned char* r = reinterpret_cast<const unsigned char*>(bytes);
    Py_ssize_t i = sizeof(T);
    T length = 0;
    do { length = length << 8 | *r++; } while(--i > 0);
    return length;
}

static void as_big_endian32(unsigned char* buffer, uint32_t integer) {
    buffer[0] = integer >> 24;
    buffer[1] = integer >> 16;
    buffer[2] = integer >> 8;
    buffer[3] = integer >> 0;
}

static void as_big_endian16(unsigned char* buffer, uint16_t integer) {
    buffer[0] = integer >> 8;
    buffer[1] = integer >> 0;
}

static void as_big_endian64(unsigned char* buffer, uint64_t integer) {
    buffer[0] = integer >> 56;
    buffer[1] = integer >> 48;
    buffer[2] = integer >> 40;
    buffer[3] = integer >> 32;
    buffer[4] = integer >> 24;
    buffer[5] = integer >> 16;
    buffer[6] = integer >> 8;
    buffer[7] = integer >> 0;
}

struct packer {
    packer(const char* encoding, int encode_mode):
        encoding(encoding), encode_mode(encode_mode) {}

    PyObject* pack(PyObject* obj) {
        buffer.reserve(1024 * 1024);
        append_version();
        if(pack_object(obj)) {
            // error happened
            if(PyErr_Occurred()) {
                return NULL;
            }
            else {
                PyErr_SetString(earl_EncodeError, "An unknown error occurred while packing.");
                return NULL;
            }
        }
        return PyBytes_FromStringAndSize(&buffer[0], buffer.size());
    }
private:
    std::string buffer;
    const char* encoding;
    int encode_mode;

    void append_version() {
        buffer.push_back(FORMAT_VERSION);
    }

    void append_nil() {
        buffer.push_back(SMALL_ATOM_EXT);
        buffer.push_back(3);
        buffer.push_back('n');
        buffer.push_back('i');
        buffer.push_back('l');
    }

    void append_true() {
        buffer.push_back(SMALL_ATOM_EXT);
        buffer.push_back(4);
        buffer.push_back('t');
        buffer.push_back('r');
        buffer.push_back('u');
        buffer.push_back('e');
    }

    void append_false() {
        buffer.push_back(SMALL_ATOM_EXT);
        buffer.push_back(5);
        buffer.push_back('f');
        buffer.push_back('a');
        buffer.push_back('l');
        buffer.push_back('s');
        buffer.push_back('e');
    }

    void append_small_integer(uint8_t integer) {
        buffer.push_back(SMALL_INTEGER_EXT);
        buffer.push_back(integer);
    }

    void append_integer(int32_t integer) {
        unsigned char bytes[5]; // 1 + sizeof(integer)
        bytes[0] = INTEGER_EXT;
        as_big_endian32(bytes + 1, integer);
        buffer.append(reinterpret_cast<const char*>(bytes), sizeof(bytes));
    }

    void append_uint64_t(uint64_t integer) {
        unsigned char bytes[11]; // 3 + sizeof(integer)
        pack_long_long(bytes, integer);
        bytes[2] = 0;
        buffer.append(reinterpret_cast<const char*>(bytes), 3 + bytes[1]);
    }

    void append_int64_t(int64_t integer) {
        unsigned char bytes[11];
        uint64_t value;
        if(integer < 0) {
            bytes[2] = 1;
            value = -integer;
        }
        else {
            bytes[2] = 0;
            value = integer;
        }
        pack_long_long(bytes, value);
        buffer.append(reinterpret_cast<const char*>(bytes), 3 + bytes[1]);
    }

    void append_double(double f) {
        unsigned char bytes[9]; // 1 + sizeof(double)
        bytes[0] = FLOAT_IEEE_EXT;
        uint64_t as_number;
        memcpy(&as_number, &f, sizeof(as_number));
        as_big_endian64(bytes + 1, as_number);
        buffer.append(reinterpret_cast<const char*>(bytes), sizeof(bytes));
    }

    void append_atom(const char* bytes, uint16_t size) {
        if(size < 255) {
            buffer.push_back(SMALL_ATOM_EXT);
            buffer.push_back(static_cast<unsigned char>(size));
            buffer.append(bytes, size);
            return;
        }

        unsigned char buf[3];
        buf[0] = ATOM_EXT;
        as_big_endian16(buf + 1, size);
        buffer.append(reinterpret_cast<const char*>(buf), sizeof(buf));
        buffer.append(bytes, size);
    }

    void append_binary(const char* bytes, uint32_t size) {
        unsigned char buf[5];
        buf[0] = BINARY_EXT;
        as_big_endian32(buf + 1, size);
        buffer.append(reinterpret_cast<const char*>(buf), sizeof(buf));
        buffer.append(bytes, size);
    }

    void append_string(const char* bytes, uint16_t size) {
        unsigned char buf[3];
        buf[0] = STRING_EXT;
        as_big_endian16(buf + 1, size);
        buffer.append(reinterpret_cast<const char*>(buf), sizeof(buf));
        buffer.append(bytes, size);
    }

    void append_nil_ext() {
        buffer.push_back(NIL_EXT);
    }

    void append_list_header(uint32_t size) {
        unsigned char bytes[5];
        bytes[0] = LIST_EXT;
        as_big_endian32(bytes + 1, size);
        buffer.append(reinterpret_cast<const char*>(bytes), sizeof(bytes));
    }

    void append_map_header(uint32_t size) {
        unsigned char bytes[5];
        bytes[0] = MAP_EXT;
        as_big_endian32(bytes + 1, size);
        buffer.append(reinterpret_cast<const char*>(bytes), sizeof(bytes));
    }

    void append_tuple_header(uint32_t size) {
        if(size < 256) {
            buffer.push_back(SMALL_TUPLE_EXT);
            buffer.push_back(size);
        }
        else {
            unsigned char bytes[5];
            bytes[0] = LARGE_TUPLE_EXT;
            as_big_endian32(bytes + 1, size);
            buffer.append(reinterpret_cast<const char*>(bytes), sizeof(bytes));
        }
    }

    void pack_long_long(unsigned char* bytes, uint64_t value) {
        uint8_t bytes_encoded = 0;
        bytes[0] = SMALL_BIG_EXT;
        while(value > 0) {
            bytes[3 + bytes_encoded] = value & 0xFF;
            value >>= 8;
            ++bytes_encoded;
        }
        bytes[1] = bytes_encoded;
    }

    int unicode_as_atom(PyObject* str) {
        PyObject* bytes = PyUnicode_AsUTF8String(str);
        if(bytes == NULL || PyErr_Occurred()) {
            Py_XDECREF(bytes);
            return 1;
        }

        Py_ssize_t len = PyBytes_GET_SIZE(bytes);
        if(len > UINT16_MAX) {
            PyErr_SetString(earl_EncodeError, "string too big to encoded as ATOM_EXT");
            Py_DECREF(bytes);
            return 1;
        }

        append_atom(PyBytes_AS_STRING(bytes), len);
        return 0;
    }

    int pack_object(PyObject* obj) {
        if(obj == Py_None) {
            append_nil();
            return 0;
        }
        else if(obj == Py_False) {
            append_false();
            return 0;
        }
        else if(obj == Py_True) {
            append_true();
            return 0;
        }
        else if(PyLong_Check(obj)) {
            int overflow;
            long long ret = PyLong_AsLongLongAndOverflow(obj, &overflow);
            if(ret == -1 && PyErr_Occurred()) {
                return 1;
            }

            if(overflow == 0) {
                // no overflow, fits perfectly in a long long so..
                if(ret >= 0 && ret <= UINT8_MAX) {
                    append_small_integer(ret);
                }
                else if(ret >= INT32_MIN && ret <= INT32_MAX) {
                    append_integer(ret);
                }
                else {
                    append_int64_t(ret);
                }
                return 0;
            }

            if(overflow == -1) {
                // too small for a long long means it's too small for us
                PyErr_SetString(earl_EncodeError, "Integer value to pack is too small.");
                return 1;
            }

            // if overflow is 1 then it could *potentially* fit in an unsigned long long
            unsigned long long other = PyLong_AsUnsignedLongLong(obj);
            if(PyErr_Occurred()) {
                return 1;
            }
            append_uint64_t(other);
            return 0;
        }
        else if(PyFloat_Check(obj)) {
            append_double(PyFloat_AS_DOUBLE(obj));
            return 0;
        }
        else if(PyUnicode_Check(obj)) {
            if(encode_mode == encode_type::atom) {
                return unicode_as_atom(obj);
            }

            PyObject* bytes_obj = PyUnicode_AsEncodedString(obj, encoding, NULL);
            if(PyErr_Occurred()) {
                Py_XDECREF(bytes_obj);
                return 1;
            }
            Py_ssize_t byte_size = PyBytes_GET_SIZE(bytes_obj);
            if(encode_mode == encode_type::str) {
                if(byte_size > UINT16_MAX) {
                    PyErr_SetString(earl_EncodeError, "str is too big to be encoded as STRING_EXT");
                    Py_DECREF(bytes_obj);
                    return 1;
                }
                append_string(PyBytes_AS_STRING(bytes_obj), byte_size);
            }
            else {
                if(byte_size > INT32_MAX) {
                    PyErr_SetString(earl_EncodeError, "str is too big to be encoded as BINARY_EXT");
                    Py_DECREF(bytes_obj);
                    return 1;
                }
                append_binary(PyBytes_AS_STRING(bytes_obj), byte_size);
            }
            Py_DECREF(bytes_obj); // we don't need you any longer
            return 0;
        }
        else if(PyTuple_Check(obj)) {
            Py_ssize_t tuple_size = PyTuple_GET_SIZE(obj);
            if(tuple_size > INT32_MAX) {
                PyErr_SetString(earl_EncodeError, "tuple has too many elements");
                return 1;
            }
            append_tuple_header(tuple_size);
            for(Py_ssize_t index = 0; index < tuple_size; ++index) {
                if(pack_object(PyTuple_GET_ITEM(obj, index))) {
                    return 1;
                }
            }
            return 0;
        }
        else if(PyList_Check(obj)) {
            Py_ssize_t list_size = PyList_GET_SIZE(obj);
            if(list_size > INT32_MAX) {
                PyErr_SetString(earl_EncodeError, "list has too many elements");
                return 1;
            }
            if(list_size == 0) {
                append_nil_ext();
                return 0;
            }

            append_list_header(list_size);
            for(Py_ssize_t index = 0; index < list_size; ++index) {
                if(pack_object(PyList_GET_ITEM(obj, index))) {
                    return 1;
                }
            }
            append_nil_ext();
            return 0;
        }
        else if(PyDict_Check(obj)) {
            Py_ssize_t dict_size = PyDict_Size(obj);
            if(dict_size > INT32_MAX) {
                PyErr_SetString(earl_EncodeError, "dict has too many elements");
                return 1;
            }

            append_map_header(dict_size);
            PyObject* key;
            PyObject* value;
            Py_ssize_t pos = 0;
            while(PyDict_Next(obj, &pos, &key, &value)) {
                if(pack_object(key) || pack_object(value)) {
                    return 1;
                }
            }
            return 0;
        }
        else if(PyBytes_Check(obj)) {
            append_binary(PyBytes_AS_STRING(obj), PyBytes_GET_SIZE(obj));
            return 0;
        }
        else if(PyByteArray_Check(obj)) {
            append_binary(PyByteArray_AS_STRING(obj), PyByteArray_GET_SIZE(obj));
            return 0;
        }
        else {
            PyErr_SetString(earl_EncodeError, "unable to encode object");
            return 1;
        }
    }
};

// this is an unrolled version of unpacker::get() seen below
#define EARL_GET_UNROLLED(name) \
    if(offset > buf.len) { \
        return PyErr_Format(earl_DecodeError, "Unexpected end of byte string found (offset: %zd, size: %zd)", offset, buf.len); \
    } \
    uint8_t name = bytes[offset++]

// a macro that gives a uint32_t length for use of the unpacker
// also handles the return NULL case.
#define EARL_GET_LENGTH \
    const char* len = range(4); \
    if(len == NULL) { \
        return NULL; \
    } \
    uint32_t length = from_big_endian<uint32_t>(len);

struct unpacker {
    unpacker(Py_buffer buf, const char* encoding, bool encode_binary_ext):
        buf(buf), bytes(reinterpret_cast<const char*>(buf.buf)),
        encoding(encoding), offset(0), encode_binary_ext(encode_binary_ext) {}

    PyObject* unpack() {
        char version = bytes[offset++];
        if(version != FORMAT_VERSION) {
            return PyErr_Format(earl_DecodeError, "Bad version. Expected '\\x%x', found '\\x%x' instead", FORMAT_VERSION & 0xFF, version);
        }

        return decode();
    }

    ~unpacker() {
        PyBuffer_Release(&buf);
    }
private:
    Py_buffer buf;
    const char* bytes;
    const char* encoding;
    Py_ssize_t offset;
    bool encode_binary_ext;

    const char* get() {
        if(offset > buf.len) {
            PyErr_Format(earl_DecodeError, "Unexpected end of byte string found (offset: %zd, size: %zd)", offset, buf.len);
            return NULL;
        }
        return &bytes[offset++];
    }

    const char* range(Py_ssize_t count) {
        if(offset + count > buf.len) {
            PyErr_Format(earl_DecodeError, "Unexpected end of byte string found (offset: %zd, size: %zd, count: %zd)", offset, buf.len, count);
            return NULL;
        }
        const char* copy = bytes + offset;
        offset += count;
        return copy;
    }

    PyObject* decode() {
        EARL_GET_UNROLLED(op);
        switch(op) {
        case SMALL_INTEGER_EXT:
            return small_int_ext();
        case INTEGER_EXT:
            return integer_ext();
        case FLOAT_IEEE_EXT:
            return float_ieee();
        case SMALL_BIG_EXT:
            return small_big_int();
        case ATOM_EXT:
            return atom_ext();
        case SMALL_ATOM_EXT:
            return small_atom_ext();
        case NIL_EXT:
            return nil_ext();
        case SMALL_TUPLE_EXT:
            return small_tuple_ext();
        case LARGE_TUPLE_EXT:
            return large_tuple_ext();
        case LIST_EXT:
            return list_ext();
        case STRING_EXT:
            return string_ext();
        case BINARY_EXT:
            return binary_ext();
        case MAP_EXT:
            return map_ext();
        case COMPRESSED_TERM:
            return compressed();
        default:
            return PyErr_Format(earl_DecodeError, "Unexpected opcode: '\\x%x'", op);
        }
    }

    PyObject* small_int_ext() {
        const char* byte = get();
        return byte ? PyLong_FromUnsignedLong(static_cast<unsigned char>(*byte)) : NULL;
    }

    PyObject* integer_ext() {
        const char* ret = range(4);
        if(ret == NULL) {
            return NULL;
        }
        int32_t x = 0;
#if PY_BIG_ENDIAN
        memcpy(&x, value, length);
#else
        char buf[4]; // enough to fill our bytes
        char* end = &buf[3];
        for(Py_ssize_t i = 0; i < 4; ++i) {
            *end-- = *ret++;
        }
        memcpy(&x, buf, 4);
#endif // endian check
        return PyLong_FromLong(x);
    }

    PyObject* small_big_int() {
        EARL_GET_UNROLLED(length);
        const char* e = range(length + 1);
        if(e == NULL) {
            return NULL;
        }

        if(length > 8) {
            return PyErr_Format(earl_DecodeError, "big integer too big to unpack, expected up to 8 bytes"
                                                  " but received %d bytes instead", length);
        }

        return convert_big_integer(e, length);
    }

    PyObject* convert_big_integer(const char* value, Py_ssize_t length) {
        // length does not contain the sign bit but value does
        uint8_t sign = *value++;

        // the easy ULL case
        if(sign == 0) {
            uint64_t ret = 0;
#if PY_BIG_ENDIAN
            char buf[8]; // enough to fill our bytes
            char* end = &buf[length - 1];
            for(Py_ssize_t i = 0; i < length; ++i) {
                *end-- = *value++;
            }
            memcpy(&ret, buf, length);
#else
            memcpy(&ret, value, length);
#endif // endian check
            return PyLong_FromUnsignedLongLong(ret);
        }

        uint64_t val = 0;
        uint64_t b = 1;
        for(Py_ssize_t i = 0; i < length; ++i) {
            val += static_cast<unsigned char>(*value) * b;
            b <<= 8;
            ++value;
        }

        PyObject* ret = PyLong_FromUnsignedLongLong(val);

        // negate if necessary
        if(sign == 0) {
            return ret;
        }
        else {
            PyObject* val = PyNumber_Negative(ret);
            Py_DECREF(ret);
            if(val == NULL) {
                return NULL;
            }
            return val;
        }
    }

    PyObject* float_ieee() {
        const char* ret = range(8);
        if(ret == NULL) {
            return NULL;
        }

        double x;
#if PY_BIG_ENDIAN
        memcpy(&x, ret, 8);
#else
        char buf[8];
        char* end = &buf[7];
        for(unsigned i = 0; i < 8; ++i) {
            *end-- = *ret++;
        }
        memcpy(&x, buf, 8);
#endif // endian check

        return PyFloat_FromDouble(x);
    }

    PyObject* atom_ext() {
        const char* len_bytes = range(2);
        if(len_bytes == NULL) {
            return NULL;
        }
        uint16_t length = from_big_endian<uint16_t>(len_bytes);
        return convert_atom(length);
    }

    PyObject* small_atom_ext() {
        EARL_GET_UNROLLED(length);
        return convert_atom(length);
    }

    PyObject* convert_atom(Py_ssize_t length) {
        const char* atom = range(length);
        if(atom == NULL) {
            return NULL;
        }

        if(strncmp(atom, "nil", 3) == 0) {
            Py_RETURN_NONE;
        }
        else if(strncmp(atom, "true", 4) == 0) {
            Py_RETURN_TRUE;
        }
        else if(strncmp(atom, "false", 5) == 0) {
            Py_RETURN_FALSE;
        }

        // we return atoms as UTF-8 encoded unicode strings
        return PyUnicode_DecodeUTF8(atom, length, NULL);
    }

    PyObject* nil_ext() {
        return PyList_New(0); // empty list
    }

    PyObject* small_tuple_ext() {
        EARL_GET_UNROLLED(length);
        return create_tuple(length);
    }

    PyObject* large_tuple_ext() {
        EARL_GET_LENGTH
        return create_tuple(length);
    }

    PyObject* create_tuple(Py_ssize_t length) {
        PyObject* tuple = PyTuple_New(length);
        if(tuple == NULL) {
            return NULL;
        }

        for(Py_ssize_t i = 0; i < length; ++i) {
            PyObject* element = decode();
            if(element == NULL) {
                Py_DECREF(tuple);
                return NULL;
            }
            PyTuple_SET_ITEM(tuple, i, element);
        }
        return tuple;
    }

    PyObject* list_ext() {
        EARL_GET_LENGTH
        PyObject* list = PyList_New(length);
        if(list == NULL) {
            return NULL;
        }

        for(Py_ssize_t i = 0; i < length; ++i) {
            PyObject* element = decode();
            if(element == NULL) {
                Py_DECREF(list);
                return NULL;
            }
            PyList_SET_ITEM(list, i, element);
        }

        EARL_GET_UNROLLED(op);
        if(op != NIL_EXT) {
            PyErr_SetString(earl_DecodeError, "Expected NIL_EXT after list but did not receive one");
            Py_DECREF(list);
            return NULL;
        }
        return list;
    }

    PyObject* string_ext() {
        const char* len = range(2);
        if(len == NULL) {
            return NULL;
        }
        uint16_t length = from_big_endian<uint16_t>(len);
        const char* string_bytes = range(length);
        if(string_bytes == NULL) {
            return NULL;
        }

        if(encoding == NULL) {
            // no encoding, so just return a bytes object
            return PyBytes_FromStringAndSize(string_bytes, length);
        }

        return PyUnicode_Decode(string_bytes, length, encoding, "strict");
    }

    PyObject* binary_ext() {
        EARL_GET_LENGTH
        const char* string_bytes = range(length);
        if(string_bytes == NULL) {
            return NULL;
        }
        if(!encode_binary_ext || encoding == NULL) {
            return PyBytes_FromStringAndSize(string_bytes, length);
        }
        return PyUnicode_Decode(string_bytes, length, encoding, "strict");
    }

    PyObject* map_ext() {
        EARL_GET_LENGTH
        PyObject* dict = PyDict_New();
        if(dict == NULL) {
            return NULL;
        }

        for(Py_ssize_t i = 0; i < length; ++i) {
            PyObject* key = decode();
            if(key == NULL) {
                PyDict_Clear(dict);
                Py_DECREF(dict);
                return NULL;
            }

            PyObject* value = decode();
            if(value == NULL) {
                Py_DECREF(key);
                PyDict_Clear(dict);
                Py_DECREF(dict);
                return NULL;
            }

            int ret = PyDict_SetItem(dict, key, value);
            Py_DECREF(key);
            Py_DECREF(value);

            if(ret < 0) {
                PyDict_Clear(dict);
                Py_DECREF(dict);
                return NULL;
            }
        }

        return dict;
    }

    PyObject* compressed() {
        EARL_GET_LENGTH
        PyObject* zlib = PyImport_ImportModule("zlib");
        if(zlib == NULL) {
            return NULL;
        }

        PyObject* decompress = PyObject_GetAttrString(zlib, "decompress");
        if(decompress == NULL) {
            Py_DECREF(zlib);
            return NULL;
        }

        PyObject* bytes_obj = PyBytes_FromStringAndSize(&bytes[offset], buf.len - offset);
        if(bytes_obj == NULL) {
            Py_DECREF(decompress);
            Py_DECREF(zlib);
            return NULL;
        }

        PyObject* wbits = PyLong_FromLong(15L);
        PyObject* py_length = PyLong_FromUnsignedLong(length);
        PyObject* args = Py_BuildValue("(OOO)", bytes_obj, wbits, py_length);
        Py_DECREF(bytes_obj);
        if(args == NULL) {
            Py_XDECREF(py_length);
            Py_DECREF(wbits);
            Py_DECREF(decompress);
            Py_DECREF(zlib);
            return NULL;
        }

        PyObject* new_bytes = PyEval_CallObject(decompress, args);
        Py_DECREF(wbits);
        Py_DECREF(py_length);
        Py_DECREF(args);
        if(new_bytes == NULL) {
            return NULL;
        }

        Py_buffer buffer;
        if(PyObject_GetBuffer(new_bytes, &buffer, PyBUF_C_CONTIGUOUS) < 0) {
            Py_DECREF(new_bytes);
            Py_DECREF(decompress);
            Py_DECREF(zlib);
            return NULL;
        }

        // replace ourselves with the new buffer
        this->~unpacker();
        new(this) unpacker(buffer, encoding, encode_binary_ext);
        PyObject* ret = decode();
        Py_DECREF(new_bytes);
        Py_DECREF(decompress);
        Py_DECREF(zlib);
        return ret;
    }
};

#undef EARL_GET_UNROLLED
#undef EARL_GET_LENGTH

static PyObject* earl_pack(PyObject* self, PyObject* args, PyObject* kwargs) {
    PyObject* to_pack;
    int encode_mode = encode_type::bytes;
    const char* encoding = "utf-8";
    size_t len;

    static const char* kwlist[] = { "obj", "encoding", "encode_mode", NULL };

    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O|$s#i:pack", const_cast<char**>(kwlist),
                                   &to_pack, &encoding, &len, &encode_mode)) {
        return NULL;
    }

    packer p(encoding, encode_mode);
    PyObject* ret = p.pack(to_pack);
    return ret;
}

static PyObject* earl_unpack(PyObject* self, PyObject* args, PyObject* kwargs) {
    static const char* kwlist[] = { "data", "encoding", "encode_binary_ext", NULL };
    const char* encoding = NULL;
    size_t len;
    int encode_binary_ext = 0;
    Py_buffer buf;

    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "y*|$s#i", const_cast<char**>(kwlist),
                                   &buf, &encoding, &len, &encode_binary_ext)) {
        return NULL;
    }

    unpacker p(buf, encoding, encode_binary_ext);
    PyObject* unpacked = p.unpack();
    return unpacked;
}

static char earl_pack_docs[] = "pack(value, *, encoding=None, encode_mode=ENCODE_AS_BYTES)\n"
                              "Packs a value to External Term Format.\n"
                              "The encode_mode parameter is used to set how to encode unicode\n"
                              "strings to ETF. Depending on the mode, the effect changes as follows:\n\n"
                              "- ENCODE_AS_STR: Encodes the string with STRING_EXT\n"
                              "- ENCODE_AS_BYTES: Encodes the string with BINARY_EXT\n"
                              "- ENCODE_AS_ATOM: Encodes the string with ATOM_EXT (or SMALL_ATOM_EXT)\n\n"
                              "When using ENCODE_AS_ATOM the string will be encoded into UTF-8.\n\n"
                              "The encoding parameter denotes how to encode the unicode strings.\n"
                              "By default, it encodes them into UTF-8.";
static char earl_unpack_docs[] = "unpack(data, *, encoding=None, encode_binary_ext=False): Unpack ETF data.\n"
                                "The encoding parameter specifies how to decode STRING_EXT data\n"
                                "if encountered. If no encoding is passed, then STRING_EXT is encoded\n"
                                "as a bytes object.\n\n If the encode_binary_ext parameter is set to True, "
                                "then BINARY_EXT is also encoded into the encoding given.";

static PyMethodDef earlmethods[] = {
    {"pack", (PyCFunction)earl_pack, METH_VARARGS | METH_KEYWORDS, earl_pack_docs},
    {"unpack", (PyCFunction)earl_unpack, METH_VARARGS | METH_KEYWORDS, earl_unpack_docs},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef earl = {
    PyModuleDef_HEAD_INIT,
    "earl",
    "Earl is the fanciest External Term Format library for Python.",
    -1,
    earlmethods
};

PyMODINIT_FUNC PyInit_earl() {
    PyObject* mod = PyModule_Create(&earl);
    if(mod == NULL) {
        return NULL;
    }

    earl_EncodeError = PyErr_NewException("earl.EncodeError", PyExc_Exception, NULL);
    if(earl_EncodeError == NULL) {
        goto error;
    }

    earl_DecodeError = PyErr_NewException("earl.DecodeError", PyExc_Exception, NULL);
    if(earl_DecodeError == NULL) {
        goto error;
    }

    if(PyModule_AddObject(mod, "EncodeError", earl_EncodeError)) {
        goto error;
    }

    if(PyModule_AddObject(mod, "DecodeError", earl_DecodeError)) {
        goto error;
    }

    if(PyModule_AddIntConstant(mod, "ENCODE_AS_STR", encode_type::str)) {
        goto error;
    }

    if(PyModule_AddIntConstant(mod, "ENCODE_AS_BYTES", encode_type::bytes)) {
        goto error;
    }

    if(PyModule_AddIntConstant(mod, "ENCODE_AS_ATOM", encode_type::atom)) {
        goto error;
    }
error:
    if(PyErr_Occurred()) {
        PyErr_SetString(PyExc_ImportError, "init failed");
        Py_DECREF(mod);
        mod = NULL;
    }
    return mod;
}
