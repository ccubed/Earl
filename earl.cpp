/*
 * Earl, the fanciest ETF Packer and Unpacker for Python.
 * Written as a C++ Extension for Maximum Effort and Speed.
 *
 * Copyright 2016 Charles Click under the MIT License.
 */

// Includes
#include <Python.h>
#include <string>
#include <vector>
#include <stdio.h>
#include <algorithm>

// External Term Format Defines
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

#if _MSC_VER // MSVC doesn't support keywords
#define or ||
#define and &&
#endif


// Function Prototypes
// Packing Functions
std::string etfp_small_int(long value);
std::string etfp_big_int(long value);
std::string etfp_string(PyObject* value);
std::string etfp_float(double value);
std::string etfp_tuple(PyObject* tuple);
std::string etfp_set(PyObject* set);
std::string etfp_list(PyObject* list);
std::string etfp_dict(PyObject* dict);
std::string etf_pack_item(PyObject* object);
// Unpacking Functions
PyObject* etfup_bytes(PyObject* item);
PyObject* etfup_item(char *buffer, int &pos);
PyObject* etfup_small_int(char *buffer, int &pos);
PyObject* etfup_int(char *buffer, int &pos);
PyObject* etfup_float_old(char *buffer, int &pos);
PyObject* etfup_float_new(char *buffer, int &pos);
PyObject* etfup_tuple(char *buffer, int &pos);
PyObject* etfup_list(char *buffer, int &pos);
PyObject* etfup_map(char *buffer, int &pos);
// Extern C Functions
extern "C" {
  static PyObject* earl_pack(PyObject* self, PyObject* args);
  static PyObject* earl_unpack(PyObject* self, PyObject* args);
}
// End Function Prototypes

std::string etfp_small_int(long value){

  std::string buffer(1, SMALL_INTEGER_EXT);
  buffer += char(value);
  return buffer;

}

std::string etfp_big_int(long value){

  int temp = int(value);

  std::string buffer(1, INTEGER_EXT);
  buffer += ((temp >> 24) & 0xFF);
  buffer += ((temp >> 16) & 0xFF);
  buffer += ((temp >> 8) & 0xFF);
  buffer += (temp & 0xFF);

  return buffer;

}

std::string etfp_string(PyObject *value){

  int len = PyUnicode_GET_LENGTH(value);
  int kind = PyUnicode_KIND(value);

  if (!kind){

      if ( !PyErr_Occurred() ){

          PyErr_SetString(PyExc_RuntimeError, "Failed to process the python string.");

      }

  } else {

    std::string buffer(1, STRING_EXT);
    buffer += (len >> 8) & 0xFF;
    buffer += len & 0xFF;

    if( len > 0 ){

      for( int i={0}; i < len; i++ ){

          buffer += char(PyUnicode_READ(kind, PyUnicode_DATA(value), i));

      }

    } else {

      buffer += '\0';

    }

    return buffer;

  }

}

std::string etfp_float(double value){

  std::string buffer(1, FLOAT_IEEE_EXT);

  char *raw = (char*)(&value);
  std::reverse(&raw, &raw + sizeof(double));

  for (int i={0}; i < sizeof(raw); i++){

    buffer.append(raw[i], 1);

  }

  return buffer;

}

std::string etfp_tuple(PyObject* tuple){

  Py_ssize_t len = PyTuple_Size(tuple);
  std::string buffer(1, (len > 256 ? LARGE_TUPLE_EXT : SMALL_TUPLE_EXT));

  if( len > 256 ){

    buffer += ((len >> 24) & 0xFF);
    buffer += ((len >> 16) & 0xFF);
    buffer += ((len >> 8) & 0xFF);
    buffer += (len & 0xFF);

  } else {

    buffer += (len & 0xFF);

  }

  for( int ii={0}; ii < len; ii++ ){

      PyObject* temp = PyTuple_GetItem(tuple, ii);

      buffer += etf_pack_item(temp);

  }

  if( !PyErr_Occurred() ){

    return buffer;

  } else {

    return NULL;

  }

}

std::string etfp_set(PyObject* set){

  Py_ssize_t len = PySet_Size(set);
  std::string buffer(1, LIST_EXT);

  buffer += ((len >> 24) & 0xFF);
  buffer += ((len >> 16) & 0xFF);
  buffer += ((len >> 8) & 0xFF);
  buffer += (len & 0xFF);

  for( int ii={0}; ii < len; ii++ ){

    PyObject* temp = PySet_Pop(set);
    buffer += etf_pack_item(temp);

  }

  if( !PyErr_Occurred() ){

    buffer += NIL_EXT;
    return buffer;

  } else {

    return NULL;

  }

}

std::string etfp_list(PyObject* list){

  Py_ssize_t len = PyList_Size(list);
  std::string buffer(1, LIST_EXT);

  buffer += ((len >> 24) & 0xFF);
  buffer += ((len >> 16) & 0xFF);
  buffer += ((len >> 8) & 0xFF);
  buffer += (len & 0xFF);

  for( int ii={0}; ii < len; ii++ ){

    PyObject* temp = PyList_GetItem(list, ii);
    buffer += etf_pack_item(temp);

  }

  if( !PyErr_Occurred() ){

    buffer += NIL_EXT;
    return buffer;

  } else {

    return NULL;

  }

}

std::string etfp_dict(PyObject* dict){

  Py_ssize_t len = PyDict_Size(dict);
  std::string buffer(1, MAP_EXT);

  buffer += ((len >> 24) & 0xFF);
  buffer += ((len >> 16) & 0xFF);
  buffer += ((len >> 8) & 0xFF);
  buffer += (len & 0xFF);

  len = 0;
  PyObject *k, *v;

  while( PyDict_Next(dict, &len, &k, &v) ){

    buffer += etf_pack_item(k);
    buffer += etf_pack_item(v);

  }

  if( !PyErr_Occurred() ){

    return buffer;

  } else {

    return NULL;

  }

}

std::string etf_pack_item(PyObject* temp){

  std::string buffer;

  if( PyLong_Check(temp) ){

    long temp_int = PyLong_AsLong(temp);

    if ( temp_int > 0 and temp_int < 255 ){

      buffer += etfp_small_int(temp_int);

    } else if( temp_int > -2147483647 and temp_int < 2147483647 ){

      buffer += etfp_big_int(temp_int);

    } else {

      if( !PyErr_Occurred() ){

        PyErr_SetString(PyExc_RuntimeError, "Number too large to pack.");

      }

    }

  } else if( PyUnicode_Check(temp) ) {

    if( PyUnicode_READY(temp) != 0 ){

      if( !PyErr_Occurred() ){

        PyErr_SetString(PyExc_RuntimeError, "Earl wasn't able to migrate the Python Unicode data to memory.");

      }

    }else{

      buffer += etfp_string(temp);

    }

  }else if( PyFloat_Check(temp) ){

    buffer += etfp_float(PyFloat_AsDouble(temp));

  }else if( PyTuple_Check(temp) ){

    buffer += etfp_tuple(temp);

  }else if( PySet_Check(temp) ){

    buffer += etfp_set(temp);

  }else if( PyList_Check(temp) ){

    buffer += etfp_list(temp);

  }else if( PyDict_Check(temp) ){

    buffer += etfp_dict(temp);

  }else{

    if ( !PyErr_Occurred() ){

      PyErr_SetString(PyExc_TypeError, "Earl can't pack one of the types you gave it!");

    }

  }

  return buffer;

}


PyObject* etfup_bytes(PyObject* item, Py_ssize_t len){

    char *buffer;
    Py_ssize_t pbl;

    if( PyBytes_AsStringAndSize(item, &buffer, &pbl) == -1 ){

      PyErr_SetString(PyExc_RuntimeError, "Unable to convert the bytes object to a char array.");
      return NULL;

    }

    std::vector<PyObject*> objects;
    int pos = 0;

    for( pos; pos < len; pos++ ){

        if( buffer[pos] == INTEGER_EXT or buffer[pos] == SMALL_INTEGER_EXT ){

          if( buffer[pos] == INTEGER_EXT ){

            objects.push_back(etfup_int(buffer, pos));

          } else {

            objects.push_back(etfup_small_int(buffer, pos));

          }

        } else if( buffer[pos] == FLOAT_IEEE_EXT or buffer[pos] == FLOAT_EXT ){

          if( buffer[pos] == FLOAT_IEEE_EXT ){

            objects.push_back(etfup_float_new(buffer, pos));

          } else {

            objects.push_back(etfup_float_old(buffer, pos));

          }

        } else if( buffer[pos] == LIST_EXT ){

          objects.push_back(etfup_list(buffer, pos));

        } else if( buffer[pos] == MAP_EXT ){

          objects.push_back(etfup_map(buffer, pos));

        } else if( buffer[pos] == STRING_EXT ){

          int length = 0;
          length = (length << 8) + buffer[pos+1];
          length = (length << 8) + buffer[pos+2];
          pos += 3;

          std::string strbuf;

          if( length ){

            for( unsigned nb = 0; nb < length; nb++ ){

              strbuf.append(1, buffer[pos+nb]);

            }

            strbuf.append(1, '\0');
            pos += length+1;

          } else {

            strbuf.append(1, '\0');
            pos++;

          }

          PyObject* held_return = PyUnicode_FromString(strbuf.c_str());
          return held_return;

        } else if( buffer[pos] == SMALL_TUPLE_EXT or buffer[pos] == LARGE_TUPLE_EXT ){

          objects.push_back(etfup_tuple(buffer, pos));

        }

    }

  if( objects.size() > 1 ){

    PyObject* tlist = PyList_New(objects.size());

    for( int ii={0}; ii < objects.size(); ii++ ){

      if( PyList_SetItem(tlist, ii, objects[ii]) != 0 ){

        if( !PyErr_Occurred() ){

          PyErr_SetString(PyExc_RuntimeError, "Earl encountered an error building the python objects.");

        }
        return NULL;

      }

    }

    return tlist;

  } else {

    return objects[0];

  }

}

PyObject* etfup_item(char *buffer, int &pos){

  if( buffer[pos] == SMALL_INTEGER_EXT ){

    return etfup_small_int(buffer, pos);

  } else if( buffer[pos] == INTEGER_EXT ){

    return etfup_int(buffer, pos);

  } else if( buffer[pos] == FLOAT_EXT ){

    return etfup_float_old(buffer, pos);

  } else if( buffer[pos] == FLOAT_IEEE_EXT ){

    return etfup_float_new(buffer, pos);

  } else if( buffer[pos] == LIST_EXT ){

    return etfup_list(buffer, pos);

  } else if( buffer[pos] == MAP_EXT ){

    return etfup_map(buffer, pos);

  } else if( buffer[pos] == STRING_EXT ){

    int length = 0;
    length = (length << 8) + buffer[pos+1];
    length = (length << 8) + buffer[pos+2];
    pos += 3;

    std::string strbuf;

    for( unsigned nb = 0; nb < length; nb++ ){

      strbuf.append(1, buffer[pos+nb]);

    }

    PyObject* held_return = PyUnicode_FromString(strbuf.c_str());
    pos += length+1;
    return held_return;

  } else if( buffer[pos] == SMALL_TUPLE_EXT or buffer[pos] == LARGE_TUPLE_EXT ){

    return etfup_tuple(buffer, pos);

  } else {

    if( !PyErr_Occurred() ){

      PyErr_SetString(PyExc_RuntimeError, "Couldn't unpack the given types.");
      return NULL;

    }

  }

  return NULL;

}

PyObject* etfup_small_int(char *buffer, int &pos){

  pos++;
  long upd = int(buffer[pos]);

  if ( upd < 0 ){

    upd = 127 + (128 - (upd*-1)) + 1;

  }

  pos++;

  return PyLong_FromLong(upd);

}

PyObject* etfup_int(char *buffer, int &pos){

  pos += 1;
  int upd = 0;

  for( unsigned nb = 0; nb < sizeof(upd); nb++ ){

    upd = (upd << 8) + static_cast<unsigned char>(buffer[pos+nb]);

  }

  pos += 4;

  return PyLong_FromLong(long(upd));

}

PyObject* etfup_float_new(char *buffer, int &pos){

  pos += 1;
  double upd;

  memcpy(&upd, &pos, sizeof(double));

  pos += 8;

  return PyFloat_FromDouble(upd);

}

PyObject* etfup_float_old(char *buffer, int &pos){

  double upd = 0.0;
  pos += 1;
  char substr[31];

  for( unsigned nb = 0; nb < 31; nb ++){

    substr[nb] = buffer[pos+nb];

  }

  sscanf(substr, "%lf", &upd);
  pos += 31;
  return PyFloat_FromDouble(upd);

}

PyObject* etfup_tuple(char *buffer, int &pos){

  int len = 0;
  if( buffer[pos] == SMALL_TUPLE_EXT ){

    len = (len << 8) + buffer[pos+1];
    pos += 2;

  } else {

    for( int ii={1}; ii < 5; ii++){

      len = (len << 8) + buffer[pos+ii];

    }
    pos += 5;

  }

  PyObject* tuple = PyTuple_New(len);

  for( int ii={0}; ii < len; ii++ ){

    if( PyTuple_SetItem(tuple, ii, etfup_item(buffer, pos)) != 0 ){

      if( !PyErr_Occurred() ){

        PyErr_SetString(PyExc_RuntimeError, "Error populating Tuple object.");

      }

    }

  }

  return tuple;

}

PyObject* etfup_list(char *buffer, int &pos){

  int len = 0;
  len = (len << 8) + buffer[pos+1];
  len = (len << 8) + buffer[pos+2];
  len = (len << 8) + buffer[pos+3];
  len = (len << 8) + buffer[pos+4];
  pos += 5;

  PyObject* list = PyList_New(len);

  for( int ii={0}; ii < len; ii++ ){

    if( buffer[pos] == NIL_EXT ){

      pos++;

    } else {

      if( PyList_SetItem(list, ii, etfup_item(buffer, pos)) != 0 ){

        if( !PyErr_Occurred() ){

          PyErr_SetString(PyExc_RuntimeError, "Unable to build Python List.");

        }

      }

    }

  }

  if( buffer[pos+1] == NIL_EXT ){

    pos += 2;

  }

  return list;

}

PyObject* etfup_map(char *buffer, int &pos){

  int len = 0;
  len = (len << 8) + buffer[pos+1];
  len = (len << 8) + buffer[pos+2];
  len = (len << 8) + buffer[pos+3];
  len = (len << 8) + buffer[pos+4];
  pos += 5;

  PyObject* dict = PyDict_New();

  std::vector<PyObject*> keys, values;

  for( int ii={0}; ii < (len*2); ii++ ){

    if( ii % 2 ){

      values.push_back(etfup_item(buffer, pos));

    } else {

      keys.push_back(etfup_item(buffer,pos));

    }

  }

  if (keys.size() != values.size()){

    if( !PyErr_Occurred() ){

      PyErr_SetString(PyExc_RuntimeError, "Dictionary has an uneven amount of keys and values.");
      return NULL;

    }

  }

  for( int ii={0}; ii < keys.size(); ii++ ){

    if( PyDict_SetItem(dict, keys[ii], values[ii]) != 0 ){

      if( !PyErr_Occurred() ){

        PyErr_SetString(PyExc_RuntimeError, "Error building the Python Dictionary.");
        return NULL;

      }

    }

  }

  return dict;

}

static PyObject* earl_pack(PyObject* self, PyObject *args){

  std::string package;
  Py_ssize_t len = PyTuple_Size(args);

  if( !len ){

    if( !PyErr_Occurred() ){

      PyErr_SetString(PyExc_SyntaxError, "You must supply at least one argument.");

    }

    return NULL;

  }

  package = "\x83";

  if( len > 1 ){

    package += LIST_EXT;
    package += (len & 0xFF);

  }

  for( int i={0}; i < len; i++ ){

    PyObject* temp = PyTuple_GetItem(args, i);

    if( temp == NULL ){

      if( !PyErr_Occurred() ){

        PyErr_SetString(PyExc_TypeError, "Earl wasn't able to unpack all your arguments for some reason.");

      }

      return NULL;

    }

    package += etf_pack_item(temp);

  }

  if( len > 1 and !PyErr_Occurred() ){

    package += NIL_EXT;

  }

  if( !PyErr_Occurred() ){

    return Py_BuildValue("y#", package.c_str(), package.length());

  } else {

    return NULL;

  }

}

static PyObject* earl_unpack(PyObject* self, PyObject *args){

    Py_ssize_t len = PyTuple_Size(args);

    if ( len > 1 ){

      if( !PyErr_Occurred() ){

        PyErr_SetString(PyExc_RuntimeError, "Earl can only unpack one bytes object at a time.");

      }

    } else {

        PyObject* temp = PyTuple_GetItem(args, 0);

        if( PyBytes_Check(temp) ){

            return Py_BuildValue("O", etfup_bytes(temp, PyBytes_Size(temp)));

        } else {

            if( !PyErr_Occurred() ){

                PyErr_SetString(PyExc_TypeError, "Earl can only unpack Bytes objects.");
                return NULL;

            }

        }

    }

    return NULL;

}

static char earl_pack_docs[] = "pack(values): Pack a bunch of things into an External Term Format. Multiple items or types of items are packed into a list format.";
static char earl_unpack_docs[] = "unpack(data): Unpack ETF data. Data is unpacked according to standards and supported types.";

static PyMethodDef earlmethods[] = {

  {"pack", earl_pack, METH_VARARGS, earl_pack_docs},
  {"unpack", earl_unpack, METH_VARARGS, earl_unpack_docs},
  {NULL, NULL, 0, NULL}

};

static struct PyModuleDef earl = {

  PyModuleDef_HEAD_INIT,
  "earl",
  "Earl is the fanciest External Term Format library for Python.",
  -1,
  earlmethods

};

PyMODINIT_FUNC PyInit_earl(void){

  return PyModule_Create(&earl);

}
