#include <emscripten/bind.h>
#include <../lib/libcxxabi/include/cxxabi.h>
#include <list>
#include <vector>
#include <typeinfo>
#include <algorithm>
#include <emscripten/emscripten.h>
#include <climits>

using namespace emscripten;

extern "C" {
    const char* EMSCRIPTEN_KEEPALIVE __getTypeName(const std::type_info* ti) {
        int stat;
        char* demangled = abi::__cxa_demangle(ti->name(), NULL, NULL, &stat);
        if (stat == 0 && demangled) {
            return demangled;
        }

        switch (stat) {
            case -1:
                return strdup("<allocation failure>");
            case -2:
                return strdup("<invalid C++ symbol>");
            case -3:
                return strdup("<invalid argument>");
            default:
                return strdup("<unknown error>");
        }
    }
}

namespace emscripten {
    namespace internal {
        JSInterface* create_js_interface(EM_VAL e) {
            return new JSInterface(e);
        }
    }
}

EMSCRIPTEN_BINDINGS(native_and_builtin_types) {
    using namespace emscripten::internal;

    _embind_register_void(TypeID<void>::get(), "void");
    
    _embind_register_bool(TypeID<bool>::get(), "bool", true, false);

    _embind_register_integer(TypeID<char>::get(), "char", CHAR_MIN, CHAR_MAX);
    _embind_register_integer(TypeID<signed char>::get(), "signed char", SCHAR_MIN, SCHAR_MAX);
    _embind_register_integer(TypeID<unsigned char>::get(), "unsigned char", 0, UCHAR_MAX);
    _embind_register_integer(TypeID<signed short>::get(), "short", SHRT_MIN, SHRT_MAX);
    _embind_register_integer(TypeID<unsigned short>::get(), "unsigned short", 0, USHRT_MAX);
    _embind_register_integer(TypeID<signed int>::get(), "int", INT_MIN, INT_MAX);
    _embind_register_integer(TypeID<unsigned int>::get(), "unsigned int", 0, UINT_MAX);
    _embind_register_integer(TypeID<signed long>::get(), "long", LONG_MIN, LONG_MAX);
    _embind_register_integer(TypeID<unsigned long>::get(), "unsigned long", 0, ULONG_MAX);
    
    _embind_register_float(TypeID<float>::get(), "float");
    _embind_register_float(TypeID<double>::get(), "double");
    
    _embind_register_cstring(TypeID<std::string>::get(), "std::string");
    _embind_register_emval(TypeID<val>::get(), "emscripten::val");
}
