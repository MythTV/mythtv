#ifndef DECODEENCODE_H_
#define DECODEENCODE_H_

#include <algorithm>
using namespace std;

#include <QStringList>

// This is necessary for GCC 3.3, which has llabs(long long)
// // but not abs(long long) or std::llabs(long long)
MPUBLIC inline long long absLongLong(long long  n) { return n >= 0 ? n : -n; }

MPUBLIC void encodeLongLong(QStringList &list, long long num);
MPUBLIC long long decodeLongLong(QStringList &list, uint offset);
MPUBLIC long long decodeLongLong(QStringList &list, QStringList::const_iterator &it);

#endif // DECODEENCODE_H_
