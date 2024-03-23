#ifndef SIZETLITERAL_H
#define SIZETLITERAL_H

#ifdef __cpp_size_t_suffix
# warning "This code should be converted to use the c++23 Z/UZ literals."
#endif

// From https://en.cppreference.com:
//
// Literal suffixes for size_t and its signed version
//
// 1) z or Z  =>  the signed version of std::size_t (since C++23)
//
// 2) both z/Z and u/U  =>  std::size_t (since C++23)

// Analysis:
//
// GCC >= 11.1 and Clang >= 13.0 (*) have builtin support for the 'Z'
// and 'UZ' literals, but they don't set __cpp_size_t_suffix unless
// you compile with -std=c++23. They also issue a warning about each
// usage of Z/UZ, and its impossible to disable those warnings. This
// means its impossible to use the built-in compiler support for the
// Z/UZ literal when compiling c++17 code. Its also impossible to
// create our own user-defined Z/UZ literals without the compiler also
// complaining. Sigh. This means there's no way to implement the Z/UZ
// literals now so that the code will compile under c++17 and c++23
// without change.
//
// * 13.1.6 for Apple Clang.
//
// The fallback is to create _Z/_UZ literals that can be used in c++17, and
// at some point in the future when the code is uplifted to compile with
// -std=c++23 these should be converted to use the compiler supplied Z/UZ.

constexpr ssize_t operator ""  _Z(unsigned long long v)
    { return static_cast<ssize_t>(v); }
constexpr size_t  operator "" _UZ(unsigned long long v)
    { return static_cast<size_t>(v); }

#endif // SIZETLITERAL_H
