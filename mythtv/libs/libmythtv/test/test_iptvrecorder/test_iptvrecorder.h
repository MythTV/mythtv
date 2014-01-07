/*
 *  Class TestIPTVRecorder
 *
 *  Copyright (C) Karl Dietz 2014
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <QtTest/QtTest>

#include "iptvtuningdata.h"

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#define MSKIP(MSG) QSKIP(MSG, SkipSingle)
#else
#define MSKIP(MSG) QSKIP(MSG)
#endif

class TestIPTVRecorder: public QObject
{
    Q_OBJECT

  private slots:
    void TuningData(void)
    {
        IPTVTuningData tuning;

        /* test url from #11949 without port */
        tuning.m_data_url = QUrl (QString("rtsp://mafreebox.freebox.fr/fbxtv_pub/stream?namespace=1&service=203&flavour=sd"));
        QVERIFY (tuning.IsValid());

        /* test url from #11949 with port */
        tuning.m_data_url = QUrl (QString("rtsp://mafreebox.freebox.fr:554/fbxtv_pub/stream?namespace=1&service=203&flavour=sd"));
        QVERIFY (tuning.IsValid());
    }
};
