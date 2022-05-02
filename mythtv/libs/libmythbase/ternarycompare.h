#ifndef TERNARY_COMPARE_H_
#define TERNARY_COMPARE_H_

#include <type_traits>

#include <QChar>

/**
\brief balanced ternary (three way) comparison
\param a any integral type
\param b any integral type
\return -1, 0, or +1 if a is <, =, or > b, respectively
This function is equivalent to C++20's operator <=> for integral types,
returning an integer equivalent of std::strong_ordering.

Balance ternary comparison is useful for creating an ordering function for
composite types, removing a double comparison of equality and a separate
comparison for less than.  Hopefully, the compiler will superoptimize this into
a single, branchless comparison.
*/
template<typename T>
std::enable_if_t<std::is_integral_v<T>, int>
inline ternary_compare(const T a, const T b)
{
    if (a < b) return -1;
    if (a > b) return +1;
    return 0; // a == b
}
// QChar should be uint16_t
inline int ternary_compare(const QChar a, const QChar b)
{
    if (a < b) return -1;
    if (a > b) return +1;
    return 0; // a == b
}

#endif // TERNARY_COMPARE_H_
