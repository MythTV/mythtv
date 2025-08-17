/*
 *  Class TestThemeVersion
 *
 *  Copyright (c) David Hampton 2025
 *
 *  See the file LICENSE_FSF for licensing information.
 */
#ifndef LIBMYTHBASE_TEST_THEME_VERSION_H
#define LIBMYTHBASE_TEST_THEME_VERSION_H

#include <QTest>
#include "mythversion.h"

class TestThemeVersion : public QObject
{
    Q_OBJECT

private slots:
    static void test_version_data(void);
    static void test_version(void);
};

#endif // LIBMYTHBASE_TEST_THEME_VERSION_H
