/*
 *  Class TestThemeVersion
 *
 *  Copyright (c) David Hampton 2025
 *
 *  See the file LICENSE_FSF for licensing information.
 */

#include <QTest>
#include "mythversion.h"

class TestThemeVersion : public QObject
{
    Q_OBJECT

private slots:
    static void test_version_data(void);
    static void test_version(void);
};
