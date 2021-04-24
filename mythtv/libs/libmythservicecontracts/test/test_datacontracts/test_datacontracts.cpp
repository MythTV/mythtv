/*
 *  Copyright (C) David Hampton 2017
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

#include <QStringList>
#include "mythversion.h"
#include "mythdate.h"
#include "test_datacontracts.h"
#include "datacontracts/programAndChannel.h"
#include "datacontracts/programList.h"
#include "datacontracts/recRule.h"
#include "datacontracts/recording.h"

void TestDataContracts::initTestCase(void)
{
}

void TestDataContracts::cleanupTestCase(void)
{
}

void TestDataContracts::test_channelinfo(void)
{
    auto *pChan = new DTC::ChannelInfo();
    pChan->setChanId(1119);
    pChan->setChanNum("119");
    pChan->setCallSign("WEATH");
    pChan->setChannelName("The Weather Channel");

    // Convert to QVariant
    QVariant v = QVariant::fromValue(pChan);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QVariant::Type known = QVariant::nameToType("DTC::ChannelInfo*");
    QVariant::Type qtype = v.type();
#else
    QMetaType known = QMetaType::fromName("DTC::ChannelInfo*");
    QMetaType qtype = v.metaType();
#endif
    QCOMPARE(known, qtype);

    // Convert back to DTC::Program
    auto *pChan2 = v.value<DTC::ChannelInfo*>();
    QCOMPARE(pChan->ChanId(), pChan2->ChanId());
    QCOMPARE(pChan->ChanNum(), pChan2->ChanNum());
    QCOMPARE(pChan->CallSign(), pChan2->CallSign());
    QCOMPARE(pChan->ChannelName(), pChan2->ChannelName());
}

void TestDataContracts::test_program(void)
{
    // Create DTC::Program
    auto *pProgram = new DTC::Program();
    pProgram->setTitle("Big Buck Bunny");
    pProgram->setDescription("Follow a day of the life of Big Buck Bunny "
                             "when he meets three bullying rodents...");
    pProgram->setInetref("10378");
    pProgram->setCategory("movie");

    // Convert to QVariant
    QVariant v = QVariant::fromValue(pProgram);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QVariant::Type known = QVariant::nameToType("DTC::Program*");
    QVariant::Type qtype = v.type();
#else
    QMetaType known = QMetaType::fromName("DTC::Program*");
    QMetaType qtype = v.metaType();
#endif
    QCOMPARE(known, qtype);

    // Convert back to DTC::Program
    auto *pProgram2 = v.value<DTC::Program*>();
    QCOMPARE(pProgram->Title(), pProgram2->Title());
    QCOMPARE(pProgram->Description(), pProgram2->Description());
    QCOMPARE(pProgram->Inetref(), pProgram2->Inetref());
    QCOMPARE(pProgram->Category(), pProgram2->Category());
}

void TestDataContracts::test_programlist(void)
{
    // Create ProgramList with one Program
    auto *pPrograms = new DTC::ProgramList();
    DTC::Program *pProgram = pPrograms->AddNewProgram();
    pProgram->setTitle("Big Buck Bunny");
    pProgram->setDescription("Follow a day of the life of Big Buck Bunny "
                             "when he meets three bullying rodents...");
    pProgram->setInetref    ("10378");
    pProgram->setCategory   ("movie");
    pPrograms->setStartIndex    ( 1 );
    pPrograms->setCount         ( 2 );
    pPrograms->setTotalAvailable( 3 );
    pPrograms->setAsOf          ( MythDate::current() );
    pPrograms->setVersion       ( MYTH_BINARY_VERSION );
    pPrograms->setProtoVer      ( MYTH_PROTO_VERSION  );

    // Convert to QVariant
    QVariant v = QVariant::fromValue(pPrograms);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QVariant::Type known = QVariant::nameToType("DTC::ProgramList*");
    QVariant::Type qtype = v.type();
#else
    QMetaType known = QMetaType::fromName("DTC::ProgramList*");
    QMetaType qtype = v.metaType();
#endif
    QCOMPARE(known, qtype);

    // Convert back to DTC::ProgramList
    auto *pPrograms2 = v.value<DTC::ProgramList*>();
    QCOMPARE(pPrograms->StartIndex(), pPrograms2->StartIndex());
    QCOMPARE(pPrograms->Count(), pPrograms2->Count());
    QCOMPARE(pPrograms->TotalAvailable(), pPrograms2->TotalAvailable());
    QCOMPARE(pPrograms->Version(), pPrograms2->Version());
    QCOMPARE(pPrograms->ProtoVer(), pPrograms2->ProtoVer());
    QEXPECT_FAIL("", "Bogus count", Continue);
    QCOMPARE(123, pPrograms2->Count());

    // Check the program
    QVariant v2 = pPrograms2->Programs()[0];
    auto *pProgram2 = v2.value<DTC::Program*>();
    QCOMPARE(pProgram->Title(), pProgram2->Title());
    QCOMPARE(pProgram->Description(), pProgram2->Description());
    QCOMPARE(pProgram->Inetref(), pProgram2->Inetref());
    QCOMPARE(pProgram->Category(), pProgram2->Category());
}

void TestDataContracts::test_recrule(void)
{
    // Create DTC::RecRule
    auto *pRule = new DTC::RecRule();
    pRule->setId(12345);
    pRule->setTitle("Big Buck Bunny");
    pRule->setSubTitle("Bunny");
    pRule->setDescription("Follow a day of the life of Big Buck Bunny "
                          "when he meets three bullying rodents...");
    pRule->setInetref("10378");
    pRule->setRecPriority(3);

    // Convert to QVariant
    QVariant v = QVariant::fromValue(pRule);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QVariant::Type known = QVariant::nameToType("DTC::RecRule*");
    QVariant::Type qtype = v.type();
#else
    QMetaType known = QMetaType::fromName("DTC::RecRule*");
    QMetaType qtype = v.metaType();
#endif
    QCOMPARE(known, qtype);

    // Convert back to DTC::RecRule
    auto *pRule2 = v.value<DTC::RecRule*>();
    QCOMPARE(pRule->Id(), pRule2->Id());
    QCOMPARE(pRule->Title(), pRule2->Title());
    QCOMPARE(pRule->SubTitle(), pRule2->SubTitle());
    QCOMPARE(pRule->Description(), pRule2->Description());
    QCOMPARE(pRule->Inetref(), pRule2->Inetref());
    QCOMPARE(pRule->RecPriority(), pRule2->RecPriority());
}

void TestDataContracts::test_recordinginfo(void)
{
    QDateTime now = QDateTime::currentDateTime();

    // Create DTC::RecordingInfo
    auto *pRecording = new DTC::RecordingInfo();
    pRecording->setRecordedId(12345);
    pRecording->setStatus(RecStatus::Recorded);
    pRecording->setStartTs(now);
    pRecording->setEndTs(now.addSecs(3600));

    // Convert to QVariant
    QVariant v = QVariant::fromValue(pRecording);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QVariant::Type known = QVariant::nameToType("DTC::RecordingInfo*");
    QVariant::Type qtype = v.type();
#else
    QMetaType known = QMetaType::fromName("DTC::RecordingInfo*");
    QMetaType qtype = v.metaType();
#endif
    QCOMPARE(known, qtype);

    // Convert back to DTC::RecordingInfo
    auto *pRecording2 = v.value<DTC::RecordingInfo*>();
    QCOMPARE(pRecording->RecordedId(), pRecording2->RecordedId());
    QCOMPARE(pRecording->Status(), pRecording2->Status());
    QCOMPARE(pRecording->StartTs(), pRecording2->StartTs());
    QCOMPARE(pRecording->EndTs(), pRecording2->EndTs());
}



QTEST_APPLESS_MAIN(TestDataContracts)
