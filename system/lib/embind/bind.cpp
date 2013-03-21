#include <emscripten/bind.h>
#include <climits>

using namespace emscripten;

namespace emscripten {
    namespace internal {
        void registerStandardTypes() {
            static bool first = true;
            if (first) {
                first = false;

                _embind_register_void(getTypeID<void>(), "void");

                _embind_register_bool(getTypeID<bool>(), "bool", true, false);

                _embind_register_integer(getTypeID<char>(), "char", CHAR_MIN, CHAR_MAX);
                _embind_register_integer(getTypeID<signed char>(), "signed char", SCHAR_MIN, SCHAR_MAX);
                _embind_register_integer(getTypeID<unsigned char>(), "unsigned char", 0, UCHAR_MAX);
                _embind_register_integer(getTypeID<signed short>(), "short", SHRT_MIN, SHRT_MAX);
                _embind_register_integer(getTypeID<unsigned short>(), "unsigned short", 0, USHRT_MAX);
                _embind_register_integer(getTypeID<signed int>(), "int", INT_MIN, INT_MAX);
                _embind_register_integer(getTypeID<unsigned int>(), "unsigned int", 0, UINT_MAX);
                _embind_register_integer(getTypeID<signed long>(), "long", LONG_MIN, LONG_MAX);
                _embind_register_integer(getTypeID<unsigned long>(), "unsigned long", 0, ULONG_MAX);

                _embind_register_float(getTypeID<float>(), "float");
                _embind_register_float(getTypeID<double>(), "double");

                _embind_register_cstring(getTypeID<std::string>(), "std::string");
                _embind_register_emval(getTypeID<val>(), "emscripten::val");
            }
        }
    }
}
