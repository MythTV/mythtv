/*
 *  Class TestEITFixups
 *
 *  Copyright (C) Richard Hulme 2015
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

#include <stdio.h>
#include "test_eitfixups.h"
#include "eitfixup.h"
#include "programdata.h"
#include "programinfo.h"

void printEvent( const DBEventEIT& event, const QString& name );
QString getSubtitleType(unsigned char type);
QString getAudioProps(unsigned char props);
QString getVideoProps(unsigned char props);


// Make this non-zero to enable dumping event details to stdout
#define DUMP_EVENTS 0

#if DUMP_EVENTS
    #define PRINT_EVENT(a) printEvent(a, #a)
#else
    #define PRINT_EVENT(a)
#endif
#define TEST_AND_ADD(t,m,s) do{if (t & m) {s += " | "#m;t &= ~m;}}while(0)

QString getSubtitleType(unsigned char type)
{
    QString ret;

    if (type == SUB_UNKNOWN)
    {
        ret = "SUB_UNKNOWN";
    }
    else
    {
        TEST_AND_ADD(type, SUB_HARDHEAR, ret);
        TEST_AND_ADD(type, SUB_NORMAL, ret);
        TEST_AND_ADD(type, SUB_ONSCREEN, ret);
        TEST_AND_ADD(type, SUB_SIGNED, ret);

        if (type != 0)
        {
            // Any other bits are shown numerically
            ret += QString(" | %1").arg(type);
        }

        // Remove initial ' | '
        ret = ret.remove(0,3);
    }

    return ret;
}

QString getAudioProps(unsigned char props)
{
    QString ret;

    if (props == AUD_UNKNOWN)
    {
        ret = "AUD_UNKNOWN";
    }
    else
    {
        TEST_AND_ADD(props, AUD_STEREO,       ret);
        TEST_AND_ADD(props, AUD_MONO,         ret);
        TEST_AND_ADD(props, AUD_SURROUND,     ret);
        TEST_AND_ADD(props, AUD_DOLBY,        ret);
        TEST_AND_ADD(props, AUD_HARDHEAR,     ret);
        TEST_AND_ADD(props, AUD_VISUALIMPAIR, ret);

        if (props != 0)
        {
            // Any other bits are shown numerically
            ret += QString(" | %1").arg(props);
        }

        // Remove initial ' | '
        ret = ret.remove(0,3);
    }

    return ret;
}

QString getVideoProps(unsigned char props)
{
    QString ret;

    if (props == VID_UNKNOWN)
    {
        ret = "VID_UNKNOWN";
    }
    else
    {
        TEST_AND_ADD(props, VID_HDTV,       ret);
        TEST_AND_ADD(props, VID_WIDESCREEN, ret);
        TEST_AND_ADD(props, VID_AVC,        ret);
        TEST_AND_ADD(props, VID_720,        ret);
        TEST_AND_ADD(props, VID_1080,       ret);
        TEST_AND_ADD(props, VID_DAMAGED,    ret);
        TEST_AND_ADD(props, VID_3DTV,       ret);

        if (props != 0)
        {
            // Any other bits are shown numerically
            ret += QString(" | %1").arg(props);
        }

        // Remove initial ' | '
        ret = ret.remove(0,3);
    }

    return ret;
}

void printEvent(const DBEventEIT& event, const QString& name)
{
    printf("\n------------Event - %s------------\n", name.toLocal8Bit().constData());
    printf("Title          %s\n",  event.title.toLocal8Bit().constData());
    printf("Subtitle       %s\n",  event.subtitle.toLocal8Bit().constData());
    printf("Description    %s\n",  event.description.toLocal8Bit().constData());
    printf("Season         %3u\n", event.season);
    printf("Episode        %3u\n", event.episode);
    printf("Total episodes %3u\n", event.totalepisodes);
    printf("Part number    %3u\n", event.partnumber);
    printf("Part total     %3u\n", event.parttotal);
    printf("SubtitleType   %s\n",  getSubtitleType(event.subtitleType).toLocal8Bit().constData());
    printf("Audio props    %s\n",  getAudioProps(event.audioProps).toLocal8Bit().constData());
    printf("Video props    %s\n",  getVideoProps(event.videoProps).toLocal8Bit().constData());
    printf("\n");
}

void TestEITFixups::testUKFixups1()
{
    EITFixUp fixup;

    DBEventEIT event1(11381,
                      "Book of the Week",
                      "Girl in the Dark: Anna Lyndsey's account of finding light in the darkness after illness changed her life. 3/5. A Descent into Darkness: The disquieting persistence of the light.",
                      QDateTime::fromString("2015-03-05T00:30:00Z", Qt::ISODate),
                      QDateTime::fromString("2015-03-05T00:48:00Z", Qt::ISODate),
                      EITFixUp::kFixGenericDVB | EITFixUp::kFixUK,
                      SUB_UNKNOWN,
                      AUD_STEREO,
                      VID_UNKNOWN);

    fixup.Fix(event1);

    PRINT_EVENT(event1);
    QCOMPARE(event1.episode,       3u);
    QCOMPARE(event1.totalepisodes, 5u);
}

void TestEITFixups::testUKFixups2()
{
    EITFixUp fixup;

    DBEventEIT event2(54275,
                      "Hoarders",
                      "Fascinating series chronicling the lives of serial hoarders. Often facing loss of their children, career, or divorce, can people with this disorder be helped? S3, Ep1",
                      QDateTime::fromString("2015-02-28T17:00:00Z", Qt::ISODate),
                      QDateTime::fromString("2015-02-28T18:00:00Z", Qt::ISODate),
                      EITFixUp::kFixGenericDVB | EITFixUp::kFixUK,
                      SUB_UNKNOWN,
                      AUD_STEREO,
                      VID_UNKNOWN);

    fixup.Fix(event2);
    PRINT_EVENT(event2);
    QCOMPARE(event2.season,  3u);
    QCOMPARE(event2.episode, 1u);
}

void TestEITFixups::testUKFixups3()
{
    EITFixUp fixup;

    DBEventEIT event3(54340,
                      "Yu-Gi-Oh! ZEXAL",
                      "It's a duelling disaster for Yuma when Astral, a mysterious visitor from another galaxy, suddenly appears, putting his duel with Shark in serious jeopardy! S01 Ep02 (Part 2 of 2)",
                      QDateTime::fromString("2015-02-28T17:30:00Z", Qt::ISODate),
                      QDateTime::fromString("2015-02-28T18:00:00Z", Qt::ISODate),
                      EITFixUp::kFixGenericDVB | EITFixUp::kFixUK,
                      SUB_UNKNOWN,
                      AUD_STEREO,
                      VID_UNKNOWN);

    fixup.Fix(event3);
    PRINT_EVENT(event3);
    QCOMPARE(event3.season,     1u);
    QCOMPARE(event3.episode,    2u);
    QCOMPARE(event3.partnumber, (uint16_t)2u);
    QCOMPARE(event3.parttotal,  (uint16_t)2u);
}

void TestEITFixups::testUKFixups4()
{
    EITFixUp fixup;

    DBEventEIT event4(54345,
                      "Ella The Elephant",
                      "Ella borrows her Dad's camera and sets out to take some exciting pictures for her newspaper. S01 Ep39",
                      QDateTime::fromString("2015-02-28T17:45:00Z", Qt::ISODate),
                      QDateTime::fromString("2015-02-28T18:00:00Z", Qt::ISODate),
                      EITFixUp::kFixGenericDVB | EITFixUp::kFixUK,
                      SUB_UNKNOWN,
                      AUD_STEREO,
                      VID_UNKNOWN);

    fixup.Fix(event4);
    PRINT_EVENT(event4);
    QCOMPARE(event4.season,  1u);
    QCOMPARE(event4.episode, 39u);
}

void TestEITFixups::testUKFixups5()
{
    EITFixUp fixup;

    DBEventEIT event5(7940,
                      "The World at War",
                      "12/26. Whirlwind: Acclaimed documentary series about World War II. This episode focuses on the Allied bombing campaign which inflicted grievous damage upon Germany, both day and night. [S]",
                      QDateTime::fromString("2015-03-03T13:50:00Z", Qt::ISODate),
                      QDateTime::fromString("2015-03-03T14:45:00Z", Qt::ISODate),
                      EITFixUp::kFixGenericDVB | EITFixUp::kFixUK,
                      SUB_UNKNOWN,
                      AUD_STEREO,
                      VID_HDTV | VID_WIDESCREEN | VID_AVC);

    fixup.Fix(event5);
    PRINT_EVENT(event5);
    QCOMPARE(event5.episode,       12u);
    QCOMPARE(event5.totalepisodes, 26u);
    QCOMPARE(event5.subtitleType,  (unsigned char)SUB_NORMAL);
    QCOMPARE(event5.subtitle,      QString("Whirlwind"));
    QCOMPARE(event5.description,   QString("Acclaimed documentary series about World War II. This episode focuses on the Allied bombing campaign which inflicted grievous damage upon Germany, both day and night."));
}

void TestEITFixups::testUKFixups6()
{
    EITFixUp fixup;

    DBEventEIT event6(11260,
                      "A Touch of Frost",
                      "The Things We Do for Love: When a beautiful woman is found dead in a car park, the list of suspects leads Jack Frost (David Jason) into the heart of a religious community. [SL] S4 Ep3",
                      QDateTime::fromString("2015-03-01T00:10:00Z", Qt::ISODate),
                      QDateTime::fromString("2015-03-01T02:10:00Z", Qt::ISODate),
                      EITFixUp::kFixGenericDVB | EITFixUp::kFixUK,
                      SUB_UNKNOWN,
                      AUD_STEREO,
                      VID_UNKNOWN);

    fixup.Fix(event6);
    PRINT_EVENT(event6);
    QCOMPARE(event6.season,       4u);
    QCOMPARE(event6.episode,      3u);
    QCOMPARE(event6.subtitleType, (unsigned char)SUB_SIGNED);
    QCOMPARE(event6.subtitle,     QString("The Things We Do for Love"));
    QCOMPARE(event6.description,  QString("When a beautiful woman is found dead in a car park, the list of suspects leads Jack Frost (David Jason) into the heart of a religious community.  S4 Ep3"));
}

void TestEITFixups::testUKFixups7()
{
    EITFixUp fixup;

    DBEventEIT event7(7940,
                      "Suffragettes Forever! The Story of...",
                      "...Women and Power. 2/3. Documentary series presented by Amanda Vickery. During Victoria's reign extraordinary women gradually changed the lives and opportunities of their sex. [HD] [AD,S]",
                      QDateTime::fromString("2015-03-04T20:00:00Z", Qt::ISODate),
                      QDateTime::fromString("2015-03-04T21:00:00Z", Qt::ISODate),
                      EITFixUp::kFixGenericDVB | EITFixUp::kFixUK,
                      SUB_UNKNOWN,
                      AUD_STEREO,
                      VID_HDTV | VID_WIDESCREEN | VID_AVC);

    fixup.Fix(event7);
    PRINT_EVENT(event7);
    QCOMPARE(event7.episode,       2u);
    QCOMPARE(event7.totalepisodes, 3u);
    QCOMPARE(event7.subtitleType,  (unsigned char)SUB_NORMAL);
    QCOMPARE(event7.audioProps,    (unsigned char)(AUD_STEREO | AUD_VISUALIMPAIR));
    QCOMPARE(event7.title,         QString("Suffragettes Forever!"));
    QCOMPARE(event7.subtitle,      QString("The Story of Women and Power"));
    QCOMPARE(event7.description,   QString("2/3. Documentary series presented by Amanda Vickery. During Victoria's reign extraordinary women gradually changed the lives and opportunities of their sex."));
}

void TestEITFixups::testUKFixups8()
{
    EITFixUp fixup;

    DBEventEIT event8(7302,
                      "Brooklyn's Finest",
                      "Three unconnected Brooklyn cops wind up at the same deadly location. Contains very strong language, sexual content and some violence.  Also in HD. [2009] [AD,S]",
                      QDateTime::fromString("2015-02-28T23:55:00Z", Qt::ISODate),
                      QDateTime::fromString("2015-03-01T02:00:00Z", Qt::ISODate),
                      EITFixUp::kFixGenericDVB | EITFixUp::kFixUK,
                      SUB_UNKNOWN,
                      AUD_STEREO,
                      VID_UNKNOWN);

    fixup.Fix(event8);
    PRINT_EVENT(event8);
    QCOMPARE(event8.subtitleType, (unsigned char)SUB_NORMAL);
    QCOMPARE(event8.audioProps,   (unsigned char)(AUD_STEREO | AUD_VISUALIMPAIR));
    QCOMPARE(event8.description,  QString("Three unconnected Brooklyn cops wind up at the same deadly location. Contains very strong language, sexual content and some violence."));
    QCOMPARE(event8.airdate,      (uint16_t)2009u);
}

void TestEITFixups::testUKFixups9()
{
    // Make sure numbers don't get misinterpreted as episode number or similar.
    EITFixUp fixup;

    DBEventEIT event9(9311,
                      "Channel 4 News",
                      "Includes sport and weather.",
                      QDateTime::fromString("2015-02-28T19:40:00Z", Qt::ISODate),
                      QDateTime::fromString("2015-02-28T20:00:00Z", Qt::ISODate),
                      EITFixUp::kFixGenericDVB | EITFixUp::kFixUK,
                      SUB_UNKNOWN,
                      AUD_STEREO,
                      VID_UNKNOWN);

    fixup.Fix(event9);
    PRINT_EVENT(event9);
    QCOMPARE(event9.title,       QString("Channel 4 News"));
    QCOMPARE(event9.description, QString("Includes sport and weather"));
}

QTEST_APPLESS_MAIN(TestEITFixups)
