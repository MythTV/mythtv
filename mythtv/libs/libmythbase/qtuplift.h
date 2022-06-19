#ifndef QTUPLIFT_H
#define QTUPLIFT_H

#include <QtGlobal>

#if QT_VERSION < QT_VERSION_CHECK(5,13,0)

#define Q_DISABLE_COPY_MOVE(Class) \
    Q_DISABLE_COPY(Class) \
    Class(Class &&) = delete; \
    Class &operator=(Class &&) = delete;

#endif

#endif // QTUPLIFT_H
