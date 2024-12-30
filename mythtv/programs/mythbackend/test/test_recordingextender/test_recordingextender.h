/*
 *  Class TestRecordingExtender
 *
 *  Copyright (c) David Hampton 2021
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

#include <QTest>
#include <iostream>
#include "recordingextender.h"

class TestRecordingExtender : public RecordingExtender
{
    Q_OBJECT

  public:
    TestRecordingExtender();
    QDateTime getNow() {return m_nowForTest; }

  private:
    RecExtDataSource* createDataSource(AutoExtendType type) override;

    QDateTime m_nowForTest;

  private slots:
    // Before/after all test cases
    static void initTestCase(void);
    static void cleanupTestCase(void);

    // Before/after each test cases
    static void init(void);
    static void cleanup(void);

    // Test cases
    static void test_findKnownSport_data(void);
    void test_findKnownSport(void);
    static void test_nameCleanup_data(void);
    static void test_nameCleanup(void);
    static void test_parseProgramInfo_data(void);
    static void test_parseProgramInfo(void);
    void test_parseJson(void);
    static void test_parseEspn_data(void);
    void test_parseEspn(void);
    static void test_parseMlb_data(void);
    void test_parseMlb(void);
    static void test_processNewRecordings_data(void);
    void test_processNewRecordings(void);
};

//////////////////////////////////////////////////

class TestRecExtEspnDataSource : public RecExtEspnDataSource
{
  public:
    TestRecExtEspnDataSource(QObject *parent) :
      RecExtEspnDataSource(parent) {};
    RecExtDataPage* newPage(const QJsonDocument& doc) override;
    QUrl makeInfoUrl(const SportInfo& info, const QDateTime& dt) override;
    QUrl makeGameUrl(const ActiveGame& game, const QString& str) override;
    QDateTime getNow()
        {return static_cast<TestRecordingExtender*>(parent())->getNow(); }
};

class TestRecExtEspnDataPage : public RecExtEspnDataPage
{
  public:
    TestRecExtEspnDataPage(RecExtDataSource* parent, QJsonDocument doc) :
        RecExtEspnDataPage(parent, std::move(doc)) {}
    QDateTime getNow() override
        {return static_cast<TestRecExtEspnDataSource*>(parent())->getNow(); }
};

//////////////////////////////////////////////////

class TestRecExtMlbDataSource : public RecExtMlbDataSource
{
  public:
    TestRecExtMlbDataSource(QObject *parent) :
      RecExtMlbDataSource(parent) {};
    RecExtDataPage* newPage(const QJsonDocument& doc) override;
    QDateTime getNow()
        {return static_cast<TestRecordingExtender*>(parent())->getNow(); }
};

class TestRecExtMlbDataPage : public RecExtMlbDataPage
{
  public:
    TestRecExtMlbDataPage(RecExtDataSource* parent, QJsonDocument doc) :
        RecExtMlbDataPage(parent, std::move(doc)) {}
    QDateTime getNow() override
        {return static_cast<TestRecExtMlbDataSource*>(parent())->getNow(); }
};
