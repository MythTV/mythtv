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

#include <cstdio>
#include <iostream>

#include "test_eitfixups.h"

#include "libmythbase/programinfo.h"
#include "libmythtv/channelutil.h"
#include "libmythtv/eitfixup.h"
#include "libmythtv/mpeg/dishdescriptors.h"


// Make this non-zero to enable dumping event details to stdout
#define DUMP_EVENTS 0 // NOLINT(cppcoreguidelines-macro-usage)

#if DUMP_EVENTS
    #define PRINT_EVENT(a) printEvent(a)
#else
    #define PRINT_EVENT(a)
#endif
#define TEST_AND_ADD(t,m,s) do{if ((t) & (m)) {(s) += " | "#m;(t) &= ~(m);}}while(false)

void TestEITFixups::initTestCase()
{
    ChannelUtil::s_channelDefaultAuthority_runInit = false;
    ChannelUtil::s_channelDefaultAuthorityMap[11382] = "magic";
}

void TestEITFixups::cleanupTestCase()
{
}

QString TestEITFixups::getSubtitleType(unsigned char type)
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

QString TestEITFixups::getAudioProps(unsigned char props)
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

QString TestEITFixups::getVideoProps(unsigned char props)
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

void TestEITFixups::printEvent(const DBEventEIT& event)
{
    printf("\n------------Event - %s------------\n", QTest::currentDataTag());
    printf("Title           %s\n",  event.m_title.toLocal8Bit().constData());
    printf("Subtitle        %s\n",  event.m_subtitle.toLocal8Bit().constData());
    printf("Description     %s\n",  event.m_description.toLocal8Bit().constData());
    printf("Airdate         %u\n",  event.m_airdate);
    printf("Season          %u\n",  event.m_season);
    printf("Episode         %u\n",  event.m_episode);
    printf("Total episodes  %u\n",  event.m_totalepisodes);
    printf("Part number     %u\n",  event.m_partnumber);
    printf("Part total      %u\n",  event.m_parttotal);
    printf("Series ID       %s\n",  qPrintable(event.m_seriesId));
    printf("Program ID      %s\n",  qPrintable(event.m_programId));
    printf("Category        %s\n",  qPrintable(event.m_category));
    printf("Category Type   %u\n",  event.m_categoryType);
    printf("SubtitleType    %s\n",  getSubtitleType(event.m_subtitleType).toLocal8Bit().constData());
    printf("Audio props     %s\n",  getAudioProps(event.m_audioProps).toLocal8Bit().constData());
    printf("Video props     %s\n",  getVideoProps(event.m_videoProps).toLocal8Bit().constData());
    printf("Previous shown  %s\n",  event.m_previouslyshown ? "true" : "false");
    if (event.m_credits && !event.m_credits->empty())
    {
        printf("Credits        %3zu\n", event.m_credits->size());
        for (const auto& person : *event.m_credits)
            printf("               %s: %s\n", qPrintable(person.GetRole()), qPrintable(person.m_name));
    }
    if (!event.m_items.isEmpty())
    {
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        printf("Items        %3d\n", event.m_items.count());
#else
        printf("Items        %3lld\n", event.m_items.count());
#endif
    }
    printf("Category       %3u\n", event.m_categoryType);
    printf("\n");
}

void TestEITFixups::checkCast(const DBEventEIT& event,
                              const QStringList& e_actors,
                              const QStringList& e_directors,
                              const QStringList& e_hosts,
                              const QStringList& e_presenters,
                              const QStringList& e_commentators,
                              const QStringList& e_producers,
                              const QStringList& e_writers)
{
    QStringList allCast = e_actors + e_directors + e_hosts + e_presenters;
    allCast.removeAll("");
    QCOMPARE(event.HasCredits(), !allCast.isEmpty());
    if (!allCast.isEmpty())
    {
        for (const auto& person : *event.m_credits)
        {
            switch (person.m_role) {
              case DBPerson::kActor:
                QVERIFY(e_actors.contains(person.m_name));
                allCast.removeOne(person.m_name);
                break;
              case DBPerson::kDirector:
                QVERIFY(e_directors.contains(person.m_name));
                allCast.removeOne(person.m_name);
                break;
              case DBPerson::kHost:
                QVERIFY(e_hosts.contains(person.m_name));
                allCast.removeOne(person.m_name);
                break;
              case DBPerson::kPresenter:
                QVERIFY(e_presenters.contains(person.m_name));
                allCast.removeOne(person.m_name);
                break;
              case DBPerson::kCommentator:
                QVERIFY(e_commentators.contains(person.m_name));
                allCast.removeOne(person.m_name);
                break;
              case DBPerson::kProducer:
                QVERIFY(e_producers.contains(person.m_name));
                allCast.removeOne(person.m_name);
                break;
              case DBPerson::kWriter:
                QVERIFY(e_writers.contains(person.m_name));
                allCast.removeOne(person.m_name);
                break;
              default:
                QFAIL(qPrintable(QString("Unknown person '%1'of type %2")
                                 .arg(person.m_name, person.GetRole())));
                allCast.removeOne(person.m_name);
                break;
            }
        }
        QVERIFY2(allCast.isEmpty(),
                 qPrintable(QString("Names not parsed: %1").arg(allCast.join(", "))));
    }
}

void TestEITFixups::checkRating(const DBEventEIT& event, const QString& e_system, const QString& e_rating)
{
    for (const auto& rating : std::as_const(event.m_ratings))
    {
        QCOMPARE(rating.m_system, e_system);
        QCOMPARE(rating.m_rating, e_rating);
    }
}

void TestEITFixups::testParseRoman()
{
    QCOMPARE(EITFixUp::parseRoman(""),            0);
    QCOMPARE(EITFixUp::parseRoman("I"),           1);
    QCOMPARE(EITFixUp::parseRoman("ΙΙ"),          2); // Greek Ι
    QCOMPARE(EITFixUp::parseRoman("IV"),          4);
    QCOMPARE(EITFixUp::parseRoman("IX"),          9);
    QCOMPARE(EITFixUp::parseRoman("XLIX"),       49);
    QCOMPARE(EITFixUp::parseRoman("XCVIII"),     98);
    QCOMPARE(EITFixUp::parseRoman("MCDXCVII"), 1497);
    QCOMPARE(EITFixUp::parseRoman("MCMXCIX"),  1999);
}

void TestEITFixups::testUKFixups1()
{
    DBEventEIT event1(11381,
                      "Book of the Week",
                      "Girl in the Dark: Anna Lyndsey's account of finding light in the darkness after illness changed her life. 3/5. A Descent into Darkness: The disquieting persistence of the light.",
                      QDateTime::fromString("2015-03-05T00:30:00Z", Qt::ISODate),
                      QDateTime::fromString("2015-03-05T00:48:00Z", Qt::ISODate),
                      EITFixUp::kFixGenericDVB | EITFixUp::kFixUK,
                      SUB_UNKNOWN,
                      AUD_STEREO,
                      VID_UNKNOWN);

    EITFixUp::Fix(event1);

    PRINT_EVENT(event1);
    QCOMPARE(event1.m_episode,       3U);
    QCOMPARE(event1.m_totalepisodes, 5U);
}

void TestEITFixups::testUKFixups2()
{
    DBEventEIT event2(54275,
                      "Hoarders",
                      "Fascinating series chronicling the lives of serial hoarders. Often facing loss of their children, career, or divorce, can people with this disorder be helped? S3, Ep1",
                      QDateTime::fromString("2015-02-28T17:00:00Z", Qt::ISODate),
                      QDateTime::fromString("2015-02-28T18:00:00Z", Qt::ISODate),
                      EITFixUp::kFixGenericDVB | EITFixUp::kFixUK,
                      SUB_UNKNOWN,
                      AUD_STEREO,
                      VID_UNKNOWN);

    EITFixUp::Fix(event2);
    PRINT_EVENT(event2);
    QCOMPARE(event2.m_season,  3U);
    QCOMPARE(event2.m_episode, 1U);
}

void TestEITFixups::testUKFixups3()
{
    DBEventEIT event3(54340,
                      "Yu-Gi-Oh! ZEXAL",
                      "It's a duelling disaster for Yuma when Astral, a mysterious visitor from another galaxy, suddenly appears, putting his duel with Shark in serious jeopardy! S01 Ep02 (Part 2 of 2)",
                      QDateTime::fromString("2015-02-28T17:30:00Z", Qt::ISODate),
                      QDateTime::fromString("2015-02-28T18:00:00Z", Qt::ISODate),
                      EITFixUp::kFixGenericDVB | EITFixUp::kFixUK,
                      SUB_UNKNOWN,
                      AUD_STEREO,
                      VID_UNKNOWN);

    EITFixUp::Fix(event3);
    PRINT_EVENT(event3);
    QCOMPARE(event3.m_season,     1U);
    QCOMPARE(event3.m_episode,    2U);
    QCOMPARE(event3.m_partnumber, (uint16_t)2U);
    QCOMPARE(event3.m_parttotal,  (uint16_t)2U);
}

void TestEITFixups::testUKFixups4()
{
    DBEventEIT event4(54345,
                      "Ella The Elephant",
                      "Ella borrows her Dad's camera and sets out to take some exciting pictures for her newspaper. S01 Ep39",
                      QDateTime::fromString("2015-02-28T17:45:00Z", Qt::ISODate),
                      QDateTime::fromString("2015-02-28T18:00:00Z", Qt::ISODate),
                      EITFixUp::kFixGenericDVB | EITFixUp::kFixUK,
                      SUB_UNKNOWN,
                      AUD_STEREO,
                      VID_UNKNOWN);

    EITFixUp::Fix(event4);
    PRINT_EVENT(event4);
    QCOMPARE(event4.m_season,  1U);
    QCOMPARE(event4.m_episode, 39U);
}

void TestEITFixups::testUKFixups5()
{
    DBEventEIT event5(7940,
                      "The World at War",
                      "12/26. Whirlwind: Acclaimed documentary series about World War II. This episode focuses on the Allied bombing campaign which inflicted grievous damage upon Germany, both day and night. [S]",
                      QDateTime::fromString("2015-03-03T13:50:00Z", Qt::ISODate),
                      QDateTime::fromString("2015-03-03T14:45:00Z", Qt::ISODate),
                      EITFixUp::kFixGenericDVB | EITFixUp::kFixUK,
                      SUB_UNKNOWN,
                      AUD_STEREO,
                      VID_HDTV | VID_WIDESCREEN | VID_AVC);

    EITFixUp::Fix(event5);
    PRINT_EVENT(event5);
    QCOMPARE(event5.m_episode,       12U);
    QCOMPARE(event5.m_totalepisodes, 26U);
    QCOMPARE(event5.m_subtitleType,  (unsigned char)SUB_NORMAL);
    QCOMPARE(event5.m_subtitle,      QString("Whirlwind"));
    QCOMPARE(event5.m_description,   QString("Acclaimed documentary series about World War II. This episode focuses on the Allied bombing campaign which inflicted grievous damage upon Germany, both day and night."));
}

void TestEITFixups::testUKFixups6()
{
    DBEventEIT event6(11260,
                      "A Touch of Frost",
                      "The Things We Do for Love: When a beautiful woman is found dead in a car park, the list of suspects leads Jack Frost (David Jason) into the heart of a religious community. [SL] S4 Ep3",
                      QDateTime::fromString("2015-03-01T00:10:00Z", Qt::ISODate),
                      QDateTime::fromString("2015-03-01T02:10:00Z", Qt::ISODate),
                      EITFixUp::kFixGenericDVB | EITFixUp::kFixUK,
                      SUB_UNKNOWN,
                      AUD_STEREO,
                      VID_UNKNOWN);

    EITFixUp::Fix(event6);
    PRINT_EVENT(event6);
    QCOMPARE(event6.m_season,       4U);
    QCOMPARE(event6.m_episode,      3U);
    QCOMPARE(event6.m_subtitleType, (unsigned char)SUB_SIGNED);
    QCOMPARE(event6.m_subtitle,     QString("The Things We Do for Love"));
    QCOMPARE(event6.m_description,  QString("When a beautiful woman is found dead in a car park, the list of suspects leads Jack Frost (David Jason) into the heart of a religious community. S4 Ep3"));
}

void TestEITFixups::testUKFixups7()
{
    DBEventEIT event7(7940,
                      "Suffragettes Forever! The Story of...",
                      "...Women and Power. 2/3. Documentary series presented by Amanda Vickery. During Victoria's reign extraordinary women gradually changed the lives and opportunities of their sex. [HD] [AD,S]",
                      QDateTime::fromString("2015-03-04T20:00:00Z", Qt::ISODate),
                      QDateTime::fromString("2015-03-04T21:00:00Z", Qt::ISODate),
                      EITFixUp::kFixGenericDVB | EITFixUp::kFixUK,
                      SUB_UNKNOWN,
                      AUD_STEREO,
                      VID_HDTV | VID_WIDESCREEN | VID_AVC);

    EITFixUp::Fix(event7);
    PRINT_EVENT(event7);
    QCOMPARE(event7.m_episode,       2U);
    QCOMPARE(event7.m_totalepisodes, 3U);
    QCOMPARE(event7.m_subtitleType,  (unsigned char)SUB_NORMAL);
    QCOMPARE(event7.m_audioProps,    (unsigned char)(AUD_STEREO | AUD_VISUALIMPAIR));
    QCOMPARE(event7.m_title,         QString("Suffragettes Forever!"));
    QCOMPARE(event7.m_subtitle,      QString("The Story of Women and Power"));
    QCOMPARE(event7.m_description,   QString("2/3. Documentary series presented by Amanda Vickery. During Victoria's reign extraordinary women gradually changed the lives and opportunities of their sex."));
}

void TestEITFixups::testUKFixups8()
{
    DBEventEIT event8(7302,
                      "Brooklyn's Finest",
                      "Three unconnected Brooklyn cops wind up at the same deadly location. Contains very strong language, sexual content and some violence.  Also in HD. [2009] [AD,S]",
                      QDateTime::fromString("2015-02-28T23:55:00Z", Qt::ISODate),
                      QDateTime::fromString("2015-03-01T02:00:00Z", Qt::ISODate),
                      EITFixUp::kFixGenericDVB | EITFixUp::kFixUK,
                      SUB_UNKNOWN,
                      AUD_STEREO,
                      VID_UNKNOWN);

    EITFixUp::Fix(event8);
    PRINT_EVENT(event8);
    QCOMPARE(event8.m_subtitleType, (unsigned char)SUB_NORMAL);
    QCOMPARE(event8.m_audioProps,   (unsigned char)(AUD_STEREO | AUD_VISUALIMPAIR));
    QCOMPARE(event8.m_description,  QString("Three unconnected Brooklyn cops wind up at the same deadly location. Contains very strong language, sexual content and some violence."));
    QCOMPARE(event8.m_airdate,      (uint16_t)2009U);
}

void TestEITFixups::testUKFixups9()
{
    // Make sure numbers don't get misinterpreted as episode number or similar.
    DBEventEIT event9(9311,
                      "Channel 4 News",
                      "Includes sport and weather.",
                      QDateTime::fromString("2015-02-28T19:40:00Z", Qt::ISODate),
                      QDateTime::fromString("2015-02-28T20:00:00Z", Qt::ISODate),
                      EITFixUp::kFixGenericDVB | EITFixUp::kFixUK,
                      SUB_UNKNOWN,
                      AUD_STEREO,
                      VID_UNKNOWN);

    EITFixUp::Fix(event9);
    PRINT_EVENT(event9);
    QCOMPARE(event9.m_title,       QString("Channel 4 News"));
    QCOMPARE(event9.m_description, QString("Includes sport and weather"));
}

void TestEITFixups::testUKLawAndOrder()
{
    DBEventEIT *event = SimpleDBEventEIT (EITFixUp::kFixUK,
                                         "Law & Order: Special Victims Unit",
                                         "",
                                         "Crime drama series. Detective Cassidy is accused of raping ...");

    PRINT_EVENT(*event);
    EITFixUp::Fix(*event);
    PRINT_EVENT(*event);
    QCOMPARE(event->m_title,    QString("Law & Order: Special Victims Unit"));
    QCOMPARE(event->m_subtitle, QString(""));

    delete event;

    DBEventEIT *event2 = SimpleDBEventEIT (EITFixUp::kFixUK,
                                         "Law & Order: Special Victims Unit",
                                         "",
                                         "Sugar: New. Police drama series about an elite sex crime  ...");

    PRINT_EVENT(*event2);
    EITFixUp::Fix(*event2);
    PRINT_EVENT(*event2);
    QCOMPARE(event2->m_title,    QString("Law & Order: Special Victims Unit"));
    QCOMPARE(event2->m_subtitle, QString("Sugar"));
    QCOMPARE(event2->m_description, QString("Police drama series about an elite sex crime ..."));

    delete event2;
}

void TestEITFixups::testUKMarvel()
{
    DBEventEIT *event = SimpleDBEventEIT (EITFixUp::kFixUK,
                                         "Marvel's Agents of S.H.I.E.L.D.",
                                         "Maveth: <description> (S3 Ep10/22)  [AD,S]",
                                         "");

    PRINT_EVENT(*event);
    EITFixUp::Fix(*event);
    PRINT_EVENT(*event);
    QCOMPARE(event->m_title,    QString("Marvel's Agents of S.H.I.E.L.D."));
    QCOMPARE(event->m_subtitle, QString("Maveth"));

    delete event;


    DBEventEIT *event2 = SimpleDBEventEIT (EITFixUp::kFixUK,
                                          "New: Marvel's Agents of...",
                                          "...S.H.I.E.L.D. Brand new series - Bouncing Back: <description> (S3 Ep11/22)  [AD,S]",
                                          "");

    PRINT_EVENT(*event2);
    EITFixUp::Fix(*event2);
    PRINT_EVENT(*event2);
    QCOMPARE(event2->m_title,    QString("Marvel's Agents of S.H.I.E.L.D."));
    QCOMPARE(event2->m_subtitle, QString("Bouncing Back"));

    delete event2;
}

DBEventEIT *TestEITFixups::SimpleDBEventEIT (FixupValue fixup, const QString& title, const QString& subtitle, const QString& description)
{
    auto *event = new DBEventEIT (1, // channel id
                                       title, // title
                                       subtitle, // subtitle
                                       description, // description
                                       "", // category
                                       ProgramInfo::kCategoryNone, // category_type
                                       QDateTime::fromString("2015-02-28T19:40:00Z", Qt::ISODate),
                                       QDateTime::fromString("2015-02-28T20:00:00Z", Qt::ISODate),
                                       EITFixUp::kFixGenericDVB | fixup,
                                       SUB_UNKNOWN,
                                       AUD_STEREO,
                                       VID_UNKNOWN,
                                       0.0F, // star rating
                                       "", // series id
                                       "", // program id
                                       0, // season
                                       0, // episode
                                       0); //episode total

    return event;
}

void TestEITFixups::testUKXFiles()
{
    // Make sure numbers don't get misinterpreted as episode number or similar.
    DBEventEIT event(1005,
                      "New: The X-Files",
                      "Hit sci-fi drama series returns. Mulder and Scully are reunited after the collapse of their relationship when a TV host contacts them, believing he has uncovered a significant conspiracy. (Ep 1)[AD,S]",
                      QDateTime::fromString("2016-02-08T22:00:00Z", Qt::ISODate),
                      QDateTime::fromString("2016-02-08T23:00:00Z", Qt::ISODate),
                      EITFixUp::kFixGenericDVB | EITFixUp::kFixUK,
                      SUB_UNKNOWN,
                      AUD_STEREO,
                      VID_UNKNOWN);

    EITFixUp::Fix(event);
    PRINT_EVENT(event);
    QCOMPARE(event.m_title,       QString("The X-Files"));
    QCOMPARE(event.m_description, QString("Hit sci-fi drama series returns. Mulder and Scully are reunited after the collapse of their relationship when a TV host contacts them, believing he has uncovered a significant conspiracy. (Ep 1)"));
}

void TestEITFixups::testDEPro7Sat1()
{
    DBEventEIT *event = SimpleDBEventEIT (EITFixUp::kFixP7S1,
                                         "The Big Bang Theory",
                                         "Eine Nacht pro Woche Sitcom, USA 2015 Altersfreigabe: Ohne Altersbeschränkung (WH vom Montag, 18.11.2024, 15:35 Uhr)",
                                         "Beschreibung");

    PRINT_EVENT(*event);
    EITFixUp::Fix(*event);
    PRINT_EVENT(*event);
    QCOMPARE(event->m_title, QString("The Big Bang Theory"));
    QCOMPARE(event->m_subtitle, QString("Eine Nacht pro Woche"));
    QCOMPARE(event->m_description, QString("Beschreibung (USA 2015)"));
    QCOMPARE(event->m_category, QString("Sitcom"));
    QCOMPARE(event->m_airdate, (unsigned short) 2015);
    QCOMPARE(event->m_originalairdate, QDate(2015, 1, 1));
    QCOMPARE(event->m_previouslyshown, true);
    checkRating(*event, "DE", "0");

    delete event;

    DBEventEIT *event2 = SimpleDBEventEIT (EITFixUp::kFixP7S1,
                                           "taff",
                                           "taff Infotainment, D 2024",
                                           "Beschreibung");
    PRINT_EVENT(*event2);
    EITFixUp::Fix(*event2);
    PRINT_EVENT(*event2);
    QCOMPARE(event2->m_title, QString("taff"));
    QCOMPARE(event2->m_subtitle, QString(""));
    QCOMPARE(event2->m_description, QString("Beschreibung (D 2024)"));
    QCOMPARE(event2->m_category, QString("Infotainment"));
    QCOMPARE(event2->m_airdate, (unsigned short) 2024);
    QCOMPARE(event2->m_originalairdate, QDate(2024, 1, 1));
    QCOMPARE(event2->m_previouslyshown, false);

    delete event2;

    DBEventEIT *event3 = SimpleDBEventEIT (EITFixUp::kFixP7S1,
                                           "Titel",
                                           "Folgentitel",
                                           "Beschreibung");
    PRINT_EVENT(*event3);
    EITFixUp::Fix(*event3);
    PRINT_EVENT(*event3);
    QCOMPARE(event3->m_title, QString("Titel"));
    QCOMPARE(event3->m_subtitle, QString("Folgentitel"));
    QCOMPARE(event3->m_description, QString("Beschreibung"));
    QCOMPARE(event3->m_airdate, (unsigned short) 0);
    QCOMPARE(event3->m_previouslyshown, false);

    delete event3;


    DBEventEIT *event4 = SimpleDBEventEIT (EITFixUp::kFixP7S1,
                                           "The Taste",
                                           "The Taste Kochshow, D 2024 Altersfreigabe: ab 6 Mit Gebärdensprache (Online-Stream)",
                                           "Beschreibung");
    PRINT_EVENT(*event4);
    EITFixUp::Fix(*event4);
    PRINT_EVENT(*event4);
    QCOMPARE(event4->m_title, QString("The Taste"));
    QCOMPARE(event4->m_subtitle, QString(""));
    QCOMPARE(event4->m_description, QString("Beschreibung (D 2024)"));
    QCOMPARE(event4->m_category, QString("Kochshow"));
    QCOMPARE(event4->m_airdate, (unsigned short) 2024);
    QCOMPARE(event4->m_originalairdate, QDate(2024, 1, 1));
    QCOMPARE(event4->m_previouslyshown, false);
    checkRating(*event4, "DE", "6");

    delete event4;

    DBEventEIT *event5 = SimpleDBEventEIT (EITFixUp::kFixP7S1,
                                           "Titel",
                                           "Event Horizon - Am Rande des Universums Science-Fiction, USA/GB 1997 Altersfreigabe: ab 16",
                                           "Beschreibung");
    PRINT_EVENT(*event5);
    EITFixUp::Fix(*event5);
    PRINT_EVENT(*event5);
    QCOMPARE(event5->m_title, QString("Titel"));
    QCOMPARE(event5->m_subtitle, QString("Event Horizon - Am Rande des Universums"));
    QCOMPARE(event5->m_description, QString("Beschreibung (USA/GB 1997)"));
    QCOMPARE(event5->m_airdate, (unsigned short) 1997);
    QCOMPARE(event5->m_originalairdate, QDate(1997, 1, 1));
    QCOMPARE(event5->m_category, QString("Science-Fiction"));
    QCOMPARE(event5->m_previouslyshown, false);
    checkRating(*event5, "DE", "16");

    delete event5;

    DBEventEIT *event6 = SimpleDBEventEIT (EITFixUp::kFixP7S1,
                                           "Titel",
                                           "Doku-Reihe, USA 2016 Altersfreigabe: ab 12",
                                           "Beschreibung");
    PRINT_EVENT(*event6);
    EITFixUp::Fix(*event6);
    PRINT_EVENT(*event6);
    QCOMPARE(event6->m_title, QString("Titel"));
    QCOMPARE(event6->m_subtitle, QString(""));
    QCOMPARE(event6->m_description, QString("Beschreibung (USA 2016)"));
    QCOMPARE(event6->m_category, QString("Doku-Reihe"));
    QCOMPARE(event6->m_airdate, (unsigned short) 2016);
    QCOMPARE(event6->m_originalairdate, QDate(2016, 1, 1));
    QCOMPARE(event6->m_previouslyshown, false);
    checkRating(*event6, "DE", "12");

    delete event6;
}

void TestEITFixups::testHTMLFixup()
{
    // Make sure we correctly strip HTML tags from EIT data
    DBEventEIT event(9311,
                      "<EM>CSI: Crime Scene Investigation</EM>",
                      "Double-Cross: Las Vegas-based forensic drama. The team investigates when two nuns find a woman crucified in the rafters of their church - and clues implicate the priest. (S7 Ep 5)",
                      QDateTime::fromString("2015-02-28T19:40:00Z", Qt::ISODate),
                      QDateTime::fromString("2015-02-28T20:00:00Z", Qt::ISODate),
                      EITFixUp::kFixHTML | EITFixUp::kFixUK,
                      SUB_UNKNOWN,
                      AUD_STEREO,
                      VID_UNKNOWN);

    EITFixUp::Fix(event);
    PRINT_EVENT(event);
    QCOMPARE(event.m_title,       QString("CSI: Crime Scene Investigation"));
    QCOMPARE(event.m_subtitle,    QString("Double-Cross"));
// FIXME: Need to fix the capturing of (S7 Ep 5) for this to properly validate.
//    QCOMPARE(event.m_description, QString("Las Vegas-based forensic drama. The team investigates when two nuns find a woman crucified in the rafters of their church - and clues implicate the priest."));

    DBEventEIT event2(9311,
                      "<EM>New: Redneck Island</EM>",
                      "Twelve rednecks are stranded on a tropical island with 'Stone Cold' Steve Austin, but this is no holiday, they're here to compete for $100,000. S4, Ep4",
                      QDateTime::fromString("2015-02-28T19:40:00Z", Qt::ISODate),
                      QDateTime::fromString("2015-02-28T20:00:00Z", Qt::ISODate),
                      EITFixUp::kFixHTML | EITFixUp::kFixUK,
                      SUB_UNKNOWN,
                      AUD_STEREO,
                      VID_UNKNOWN);

    EITFixUp::Fix(event2);
    PRINT_EVENT(event2);
    QCOMPARE(event2.m_title,       QString("Redneck Island"));

    DBEventEIT event3(14101,
                      "New: Jericho",
                      "Drama set in 1870s Yorkshire. In her desperation to protect her son, Annie unwittingly opens the door for Bamford the railway detective, who has returned to Jericho. [AD,S]",
                      QDateTime::fromString("2015-02-28T19:40:00Z", Qt::ISODate),
                      QDateTime::fromString("2015-02-28T20:00:00Z", Qt::ISODate),
                      EITFixUp::kFixHTML | EITFixUp::kFixUK,
                      SUB_UNKNOWN,
                      AUD_STEREO,
                      VID_UNKNOWN);

    EITFixUp::Fix(event3);
    PRINT_EVENT(event3);
    QCOMPARE(event3.m_title,       QString("Jericho"));
    QCOMPARE(event3.m_description, QString("Drama set in 1870s Yorkshire. In her desperation to protect her son, Annie unwittingly opens the door for Bamford the railway detective, who has returned to Jericho."));
}

void TestEITFixups::testSkyEpisodes()
{
    DBEventEIT *event = SimpleDBEventEIT (EITFixUp::kFixPremiere,
                                         "Titel",
                                         "Subtitle",
                                         "4. Staffel, Folge 16: Viele Mitglieder einer christlichen Gemeinde erkranken nach einem Giftanschlag tödlich. Doch die fanatisch Gläubigen lassen weder polizeiliche, noch ärztliche Hilfe zu. Don (Rob Morrow) und Charlie (David Krumholtz) gelingt es jedoch durch einen Nebeneingang ins Gebäude zu kommen. Bei ihren Ermittlungen finden sie heraus, dass der Anführer der Sekte ein Betrüger war. Auch sein Sohn wusste von den Machenschaften des Vaters. War der Giftanschlag ein Racheakt? 50 Min. USA 2008. Von Leslie Libman, mit Rob Morrow, David Krumholtz, Judd Hirsch. Ab 12 Jahren");

    PRINT_EVENT(*event);
    EITFixUp::Fix(*event);
    PRINT_EVENT(*event);
    QCOMPARE(event->m_description, QString("Viele Mitglieder einer christlichen Gemeinde erkranken nach einem Giftanschlag tödlich. Doch die fanatisch Gläubigen lassen weder polizeiliche, noch ärztliche Hilfe zu. Don (Rob Morrow) und Charlie (David Krumholtz) gelingt es jedoch durch einen Nebeneingang ins Gebäude zu kommen. Bei ihren Ermittlungen finden sie heraus, dass der Anführer der Sekte ein Betrüger war. Auch sein Sohn wusste von den Machenschaften des Vaters. War der Giftanschlag ein Racheakt? Ab 12 Jahren"));
    QCOMPARE(event->m_season,   4U);
    QCOMPARE(event->m_episode, 16U);
    /* FixPremiere should scrape the credits, too! */
    QVERIFY(event->HasCredits());

    delete event;

    DBEventEIT *event2 = SimpleDBEventEIT (EITFixUp::kFixPremiere,
                                         "Titel",
                                         "Subtitle",
                                         "Washington, 1971: Vor dem Obersten Gerichtshof wird über die Kriegsdienstverweigerung von Box-Ikone Cassius Clay aka Muhammad Ali verhandelt. Während draußen Tausende gegen den Vietnamkrieg protestieren, verteidigen acht weiße, alte Bundesrichter unter dem Vorsitzenden Warren Burger (Frank Langella) die harte Linie der Regierung Nixon. Doch Kevin Connolly (Benjamin Walker), ein idealistischer junger Mitarbeiter von Richter Harlan (Christopher Plummer), gibt nicht auf. - Muhammad Alis Kiegsdienst-Verweigerungsprozess, als Mix aus Kammerspiel und Archivaufnahmen starbesetzt verfilmt. 94 Min. USA 2012. Von Stephen Frears, mit Danny Glover, Barry Levinson, Bob Balaban. Ab 12 Jahren");

    PRINT_EVENT(*event2);
    EITFixUp::Fix(*event2);
    PRINT_EVENT(*event2);
    QCOMPARE(event2->m_description, QString("Washington, 1971: Vor dem Obersten Gerichtshof wird über die Kriegsdienstverweigerung von Box-Ikone Cassius Clay aka Muhammad Ali verhandelt. Während draußen Tausende gegen den Vietnamkrieg protestieren, verteidigen acht weiße, alte Bundesrichter unter dem Vorsitzenden Warren Burger (Frank Langella) die harte Linie der Regierung Nixon. Doch Kevin Connolly (Benjamin Walker), ein idealistischer junger Mitarbeiter von Richter Harlan (Christopher Plummer), gibt nicht auf. - Muhammad Alis Kiegsdienst-Verweigerungsprozess, als Mix aus Kammerspiel und Archivaufnahmen starbesetzt verfilmt. Ab 12 Jahren"));
    QCOMPARE(event2->m_season,  0U);
    QCOMPARE(event2->m_episode, 0U);

    delete event2;

    DBEventEIT *event3 = SimpleDBEventEIT (EITFixUp::kFixPremiere,
                                         "Titel",
                                         "Subtitle",
                                         "50 Min. USA 2008. Von Leslie Libman, mit Rob Morrow, David Krumholtz, Judd Hirsch. Ab 12 Jahren");

    PRINT_EVENT(*event3);
    EITFixUp::Fix(*event3);
    PRINT_EVENT(*event3);
    QCOMPARE(event3->m_description, QString("Ab 12 Jahren"));
    QCOMPARE(event3->m_season,  0U);
    QCOMPARE(event3->m_episode, 0U);

    delete event3;

    DBEventEIT *event4 = SimpleDBEventEIT (EITFixUp::kFixPremiere,
                                         "Schwerter des Königs - Zwei Welten",
                                         "Subtitle",
                                         "Ex-Marine und Kampfsportlehrer Granger (Dolph Lundgren) ... Star Dolph Lundgren. 92 Min.\u000AD/CDN 2011. Von Uwe Boll, mit Dolph Lundgren, Natassia Malthe, Lochlyn Munro.\u000AAb 16 Jahren");

    EITFixUp::Fix(*event4);
    PRINT_EVENT(*event4);
    QCOMPARE(event4->m_description, QString("Ex-Marine und Kampfsportlehrer Granger (Dolph Lundgren) ... Star Dolph Lundgren. Ab 16 Jahren"));
    QCOMPARE(event4->m_season,  0U);
    QCOMPARE(event4->m_episode, 0U);
    QCOMPARE(event4->m_airdate,  (unsigned short) 2011);
    QVERIFY(event4->HasCredits());

    delete event4;

    DBEventEIT *event5 = SimpleDBEventEIT (EITFixUp::kFixPremiere,
                                         "Die wilden 70ern",
                                            "Laurie zieht aus",
                                            "2. Staffel, Folge 11: Lauries Auszug setzt Red zu, denn er hat ... ist.\u000AUSA 1999. 25 Min. Von David Trainer, mit Topher Grace, Mila Kunis, Ashton Kutcher.");

    EITFixUp::Fix(*event5);
    PRINT_EVENT(*event5);
    QCOMPARE(event5->m_description, QString("Lauries Auszug setzt Red zu, denn er hat ... ist."));
    QCOMPARE(event5->m_season,  2U);
    QCOMPARE(event5->m_episode, 11U);
    QCOMPARE(event5->m_airdate,  (unsigned short) 1999);
    QVERIFY(event5->HasCredits());

    delete event5;

    // Test kDePremiereOTitle regex
    DBEventEIT *event6 = SimpleDBEventEIT (EITFixUp::kFixPremiere,
                                           "Die wilden 70ern (Extra data here)",
                                           "Laurie zieht aus",
                                           "2. Staffel, Folge 11: Lauries Auszug setzt Red zu, denn er hat ... ist.\u000AUSA 1999. 25 Min. Von David Trainer, mit Topher Grace, Mila Kunis, Ashton Kutcher.");

    EITFixUp::Fix(*event6);
    PRINT_EVENT(*event6);
    QCOMPARE(event6->m_title, QString("Die wilden 70ern"));
    QCOMPARE(event6->m_subtitle, QString("Extra data here, USA"));
    QCOMPARE(event6->m_description, QString("Lauries Auszug setzt Red zu, denn er hat ... ist."));
    QCOMPARE(event6->m_season,  2U);
    QCOMPARE(event6->m_episode, 11U);
    QCOMPARE(event6->m_airdate,  (unsigned short) 1999);
    QVERIFY(event6->HasCredits());

    delete event6;
}

void TestEITFixups::testRTLEpisodes()
{
    DBEventEIT *event = SimpleDBEventEIT (EITFixUp::kFixRTL,
                                         "Titel",
                                         "Folge 9: 'Leckerlis auf Eis / Rettung aus höchster Not'",
                                         "Description")

    PRINT_EVENT(*event);
    EITFixUp::Fix(*event);
    PRINT_EVENT(*event);
    QCOMPARE(event->m_subtitle, QString("Leckerlis auf Eis / Rettung aus höchster Not"));
    QCOMPARE(event->m_season,  0U);
    QCOMPARE(event->m_episode, 9U);

    delete event;
}

void TestEITFixups::testUnitymedia()
{
    DBEventEIT *event = SimpleDBEventEIT (EITFixUp::kFixUnitymedia,
                                         "Titel",
                                         "Beschreib",
                                         "Beschreibung ... IMDb Rating: 8.9 /10");
    QMultiMap<QString,QString> cast;
    cast.insert ("Role Player", "Great Actor");
    cast.insert ("Role Player", "Other Actor");
    cast.insert ("Director", "Great Director");
    cast.insert ("Commentary or Commentator", "Great Commentator");
    cast.insert ("Presenter", "Great Presenter");
    cast.insert ("Producer", "Great Producer");
    cast.insert ("Scriptwriter", "Great Writer");
    cast.insert ("Unhandled", "lets fix it up");
    event->m_items = cast;

    QVERIFY(!event->HasCredits());
    QCOMPARE(event->m_items.count(), 8);

    PRINT_EVENT(*event);
    EITFixUp::Fix(*event);
    PRINT_EVENT(*event);

    QVERIFY(event->HasCredits());
    QCOMPARE(event->m_credits->size(), (size_t)7);
    checkCast(*event, {"Great Actor", "Other Actor"}, {"Great Director"}, {},
              {"Great Presenter"}, {"Great Commentator"}, {"Great Producer"}, {"Great Writer"});
    QVERIFY(event->m_subtitle.isEmpty());
    QCOMPARE(event->m_description, QString("Beschreibung ..."));
    QCOMPARE(event->m_stars, 0.89F);
    QCOMPARE(event->m_items.count(), 1);

    delete event;

    /* test star rating without space */
    event = SimpleDBEventEIT (EITFixUp::kFixUnitymedia,
                              "Titel",
                              "",
                              "Beschreibung ... IMDb Rating: 8.9/10");

    PRINT_EVENT(*event);
    EITFixUp::Fix(*event);
    PRINT_EVENT(*event);

    QCOMPARE(event->m_stars, 0.89F);

    delete event;
}

void TestEITFixups::testDeDisneyChannel()
{
    DBEventEIT *event = SimpleDBEventEIT (EITFixUp::kFixDisneyChannel,
                                         "Meine Schwester Charlie",
                                         "Das Ablenkungsmanöver Familien-Serie, USA 2011",
                                         "...");

    PRINT_EVENT(*event);
    EITFixUp::Fix(*event);
    PRINT_EVENT(*event);
    QCOMPARE(event->m_title,    QString("Meine Schwester Charlie"));
    QCOMPARE(event->m_subtitle, QString("Das Ablenkungsmanöver"));
    QCOMPARE(event->m_category, QString("Familien-Serie"));
    QCOMPARE(event->m_categoryType, ProgramInfo::kCategorySeries);

    delete event;

    DBEventEIT *event2 = SimpleDBEventEIT (EITFixUp::kFixDisneyChannel,
                                         "Phineas und Ferb",
                                         "Das Achterbahn - Musical Zeichentrick-Serie, USA 2011",
                                         "...");

    PRINT_EVENT(*event2);
    EITFixUp::Fix(*event2);
    PRINT_EVENT(*event2);
    QCOMPARE(event2->m_title,    QString("Phineas und Ferb"));
    QCOMPARE(event2->m_subtitle, QString("Das Achterbahn - Musical"));

    delete event2;
}

void TestEITFixups::testATV()
{
    DBEventEIT *event = SimpleDBEventEIT (EITFixUp::kFixATV,
                                         "Gilmore Girls",
                                         "Eine Hochzeit und ein Todesfall, Folge 17",
                                         "Lorelai und Rory helfen Luke in seinem Café aus, der mit den Vorbereitungen für das ...");

    PRINT_EVENT(*event);
    EITFixUp::Fix(*event);
    PRINT_EVENT(*event);
    QCOMPARE(event->m_title,    QString("Gilmore Girls"));
    QCOMPARE(event->m_subtitle, QString("Eine Hochzeit und ein Todesfall"));

    delete event;
}

void TestEITFixups::test64BitEnum(void)
{
    QVERIFY(EITFixUp::kFixUnitymedia != EITFixUp::kFixNone);
    QVERIFY(EITFixUp::kFixATV != EITFixUp::kFixNone);
    QVERIFY(EITFixUp::kFixDisneyChannel != EITFixUp::kFixNone);
#if 0
    QCOMPARE(QString::number(EITFixUp::kFixUnitymedia), QString::number(EITFixUp::kFixNone));
#endif

#if 0
    // needs some casting around
    FixupValue theFixup = EITFixUp::kFixUnitymedia;
    QCOMPARE(EITFixUp::kFixUnitymedia, theFixup);
#endif

    // this is likely what happens somewhere in the fixup pipeline
    QCOMPARE((FixupValue)((uint32_t)EITFixUp::kFixUnitymedia), (FixupValue)EITFixUp::kFixNone);

    FixupMap   fixes;
    fixes[0xFFFFULL<<32] = EITFixUp::kFixDisneyChannel;
    fixes[0xFFFFULL<<32] |= EITFixUp::kFixATV;
    FixupValue fix = EITFixUp::kFixGenericDVB;
    fix |= fixes.value(0xFFFFULL<<32);
    QCOMPARE(fix, EITFixUp::kFixGenericDVB | EITFixUp::kFixDisneyChannel | EITFixUp::kFixATV);
    QVERIFY(EITFixUp::kFixATV & fix);

    // did kFixGreekCategories = 1<<31 cause issues?
#if 0
    QCOMPARE(QString::number(1<<31, 16), QString::number(1U<<31, 16));
#endif
    // two different flags, fixed version
    QVERIFY(!(1ULL<<31 & 1ULL<<32));
    // oops, this is true, so setting the old kFixGreekCategories also set all flags following it
    QVERIFY(1<<31 & 1ULL<<32);
}

void TestEITFixups::testDvbEitAuthority_data()
{
    QTest::addColumn<quint32>("chanid");
    QTest::addColumn<QString>("seriesID");
    QTest::addColumn<QString>("programID");
    QTest::addColumn<QString>("e_seriesID");
    QTest::addColumn<QString>("e_programID");

    QTest::newRow("generic") << 11381U
                             << "SER1234" << "EP123405"
                             << "ser1234" << "ep123405";
    QTest::newRow("w/crid") << 11381U
                            << "SER1234" << "crid://AuthEP123405"
                            << "ser1234" << "authep123405";
    QTest::newRow("noDefAuth") << 11381U
                               << "SER1234" << "crid:///whocares"
                               << "ser1234" << "";
    QTest::newRow("defAuth") << 11382U
                             << "SER1234" << "crid:///something"
                             << "ser1234" << "magic/something";
}

void TestEITFixups::testDvbEitAuthority()
{
    QFETCH(quint32, chanid);
    QFETCH(QString, seriesID);
    QFETCH(QString, programID);
    QFETCH(QString, e_seriesID);
    QFETCH(QString, e_programID);

    DBEventEIT event(chanid,
                     "Book of the Week", "Book of the Week",
                     "",
                     "", ProgramInfo::kCategoryNone,
                     QDateTime::fromString("2020-03-05T00:30:00Z", Qt::ISODate),
                     QDateTime::fromString("2020-03-05T00:48:00Z", Qt::ISODate),
                     EITFixUp::kFixGenericDVB | EITFixUp::kFixUK | EITFixUp::kFixHDTV,
                     SUB_UNKNOWN, AUD_STEREO, VID_UNKNOWN, 0.0F,
                     seriesID, programID,
                     0, 0, 0);

    EITFixUp::Fix(event);

    PRINT_EVENT(event);
    QCOMPARE(event.m_subtitle, QString(""));
    QCOMPARE(event.m_videoProps & VID_HDTV, (int)VID_HDTV);
    QCOMPARE(event.m_seriesId, e_seriesID);
    QCOMPARE(event.m_programId, e_programID);
}

void TestEITFixups::testGenericTitle_data()
{
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("subtitle");
    QTest::addColumn<QString>("description");
    QTest::addColumn<int>("fixuptype");
    QTest::addColumn<QString>("e_title");
    QTest::addColumn<QString>("e_subtitle");
    QTest::addColumn<QString>("e_description");
    QTest::addColumn<ProgramInfo::CategoryType>("e_cattype");

    QTest::newRow("simple") << "The title" << ""
                            << "This is a description."
                            << (int)EITFixUp::kFixHTML
                            // results
                            << "The title" << ""
                            << "This is a description."
                            << ProgramInfo::kCategoryNone;
    QTest::newRow("duptitle") << "The title" << "The title"
                              << "This is a description."
                              << (int)EITFixUp::kFixHTML
                              // results
                              << "The title" << ""
                              << "This is a description."
                              << ProgramInfo::kCategoryNone;
    QTest::newRow("html") << "The <em>title</em>" << ""
                          << "This is a description."
                          << (int)EITFixUp::kFixHTML
                          << "The title" << ""
                          << "This is a description."
                          << ProgramInfo::kCategoryNone;
}

void TestEITFixups::testGenericTitle()
{
    QFETCH(QString, title);
    QFETCH(QString, subtitle);
    QFETCH(QString, description);
    QFETCH(int, fixuptype);
    QFETCH(QString, e_title);
    QFETCH(QString, e_subtitle);
    QFETCH(QString, e_description);
    QFETCH(ProgramInfo::CategoryType, e_cattype);

    DBEventEIT event_st(7302, title, description,
                      QDateTime::fromString("2020-02-28T23:55:00Z", Qt::ISODate),
                      QDateTime::fromString("2020-03-01T02:00:00Z", Qt::ISODate),
                      fixuptype,
                      SUB_UNKNOWN,
                      AUD_STEREO,
                      VID_UNKNOWN);
    event_st.m_subtitle = subtitle;

    EITFixUp::Fix(event_st);
    PRINT_EVENT(event_st);
    QCOMPARE(event_st.m_title,        e_title);
    QCOMPARE(event_st.m_subtitle,     e_subtitle);
    QCOMPARE(event_st.m_description,  e_description);
    QCOMPARE(event_st.m_categoryType, e_cattype);
}

void TestEITFixups::testUKTitleDescriptionFixups_data()
{
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("description");
    QTest::addColumn<QString>("category");
    QTest::addColumn<QString>("e_title");
    QTest::addColumn<QString>("e_subtitle");
    QTest::addColumn<QString>("e_description");

    QTest::newRow("follow60") << "The title"
                              << "This is a description Followed by 60 Seconds."
                              << ""
                              // results
                              << "The title" << ""
                              << "This is a description";
    QTest::newRow("then60")   << "The title"
                              << "This is a description then 60 Seconds."
                              << ""
                              // results
                              << "The title" << ""
                              << "This is a description";
    QTest::newRow("new1") << "New: The title"
                          << "'Subtitle.' brand new episode: This is a description."
                          << ""
                          // results
                          << "The title" << "Subtitle"
                          << "This is a description.";
    QTest::newRow("new2") << "Brand New The title"
                          << "new series: This is a description."
                              << ""
                          // results
                          << "The title" << ""
                          << "This is a description";
    QTest::newRow("cbbc1") << "t4: The title"
                           << "CBBC. This is a description."
                           << ""
                           << "The title" << ""
                           << "This is a description";
    QTest::newRow("cbbc2") << "T4:\t The title"
                           << "CBeebies\t.  This is a description."
                           << ""
                           << "The title" << ""
                           << "This is a description";
    QTest::newRow("cbbc3") << "Schools\t \r\n: The title"
                           << "Class TV: This is a description."
                           << ""
                           << "The title" << ""
                           << "This is a description";
    QTest::newRow("cbbc4") << "Schools\t : The title"
                           << "BBC Switch. BBC THREE on BBC TWO. This is a description."
                           << ""
                           << "The title" << ""
                           << "This is a description";
    QTest::newRow("bbc347a") << "The title"
                             << "BBC FOUR on BBC ONE. This is a description. [Rpt of blah blah 01.00am]."
                           << ""
                             << "The title" << ""
                             << "This is a description";
    QTest::newRow("bbc347b") << "The title"
                             << "bbc three on bbc two. This is a description. [Rptd blah blah 11.00pm]."
                             << ""
                             << "The title" << ""
                             << "This is a description";
    QTest::newRow("new_also") << "The title"
                              << "All New To 4Music! \t This is a description. \n\r\t Also in HD."
                              << ""
                              << "The title" << ""
                              << "This is a description";
    QTest::newRow("dbldot")  << "The title! This is.."
                             << "..a run-on description that just keeps going."
                             << ""
                             << "The title!" << ""
                             << "This is a run-on description that just keeps going";
    QTest::newRow("dbldot2")  << "The title munged with.."
                              << "..a run-on description that just keeps going."
                              << ""
                              << "The title munged with" << ""
                              << "a run-on description that just keeps going";
    QTest::newRow("dbldot3")  << "The title munged with.."
                              << "..a run-on! The description that just keeps going."
                              << "Movie"
                              << "The title munged with a run-on!" << ""
                              << "The description that just keeps going.";
    QTest::newRow("dbldot4")  << "The title munged with.."
                              << "..a run-on.. The description that just keeps going."
                              << "Movie"
                              << "The title munged with a run-on." << ""
                              << "The description that just keeps going.";
    QTest::newRow("uk24")     << "The title."
                              << "1:00am to 3:00am: This is a description."
                              << ""
                              << "The title." << "1:00am to 3:00am"
                              << "This is a description.";
    QTest::newRow("subtitle1") << "The title:The subtitle"
                               << "This is a description."
                               << ""
                               << "The title" << "The subtitle"
                               << "This is a description.";
    QTest::newRow("subtitle2") << "The title:.."
                               << "This is a description."
                               << ""
                               << "The title" << ""
                               << "..This is a description";
    QTest::newRow("titletime") << "The title"
                               << "Subtitle :. This is a description. 24:00"
                               << ""
                               << "The title" << "Subtitle"
                               << ". This is a description. 24:00";
    QTest::newRow("titledash") << "The title - Subtitle"
                               << ""
                               << ""
                               << "The title" << ""
                               << "Subtitle"; // Huh?
}

void TestEITFixups::testUKTitleDescriptionFixups()
{
    QFETCH(QString, title);
    QFETCH(QString, description);
    QFETCH(QString, category);
    QFETCH(QString, e_title);
    QFETCH(QString, e_subtitle);
    QFETCH(QString, e_description);

    DBEventEIT event_td(7302, title, description,
                      QDateTime::fromString("2020-02-28T23:55:00Z", Qt::ISODate),
                      QDateTime::fromString("2020-03-01T02:00:00Z", Qt::ISODate),
                      EITFixUp::kFixUK,
                      SUB_UNKNOWN,
                      AUD_STEREO,
                      VID_UNKNOWN);
    if (category != "")
        event_td.m_category = category;

    EITFixUp::Fix(event_td);
    PRINT_EVENT(event_td);
    QCOMPARE(event_td.m_title,        e_title);
    QCOMPARE(event_td.m_subtitle,     e_subtitle);
    QCOMPARE(event_td.m_description,  e_description);
}

void TestEITFixups::testUKTitlePropsFixups()
{
    DBEventEIT event_prop(7302, "Title", "This is a [W]description. [AD] [HD,S] [SL]",
                          QDateTime::fromString("2020-02-28T23:55:00Z", Qt::ISODate),
                          QDateTime::fromString("2020-03-01T02:00:00Z", Qt::ISODate),
                          EITFixUp::kFixUK,
                          SUB_UNKNOWN,
                          AUD_STEREO,
                          VID_UNKNOWN);

    EITFixUp::Fix(event_prop);
    PRINT_EVENT(event_prop);
    QCOMPARE(event_prop.m_title,        QString("Title"));
    QCOMPARE(event_prop.m_description,  QString("This is a description"));
    QCOMPARE(event_prop.m_audioProps,   (uint8_t)(AUD_STEREO|AUD_VISUALIMPAIR));
    QCOMPARE(event_prop.m_videoProps,   (uint8_t)(VID_HDTV|VID_WIDESCREEN));
    QCOMPARE(event_prop.m_subtitleType, (uint8_t)(SUB_NORMAL|SUB_SIGNED));
}

void TestEITFixups::testUKSubtitleFixups_data()
{
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("subtitle");
    QTest::addColumn<QString>("description");
    QTest::addColumn<QString>("category");
    QTest::addColumn<QString>("e_title");
    QTest::addColumn<QString>("e_subtitle");
    QTest::addColumn<QString>("e_description");
    QTest::addColumn<ProgramInfo::CategoryType>("e_cattype");

    QTest::newRow("subnodesc") << "The title" << "This is a description."
                               << ""
                               << ""
                               // results
                               << "The title" << ""
                               << "This is a description"
                               << ProgramInfo::kCategoryNone;
    // Movies don't have subtitles.
    QTest::newRow("subtitle quotes")  << "The title" << ""
                            << "'Im a subtitle.' This is a description."
                            << ""
                            // results
                            << "The title" << "Im a subtitle"
                            << "This is a description."
                            << ProgramInfo::kCategoryNone;
    QTest::newRow("movie no subtitle")  << "The title" << ""
                            << "'Im a subtitle.' This is a description."
                            << "movie"
                            // results
                            << "The title" << ""
                            << "'Im a subtitle.' This is a description."
                            << ProgramInfo::kCategoryMovie;
    QTest::newRow("question")  << "The title" << ""
                            << "This is a ? description"
                            << ""
                            // results
                            << "The title" << "This is a ?"
                            << "description"
                            << ProgramInfo::kCategoryNone;
    QTest::newRow("exclamation")  << "The title" << ""
                            << "This is a description!"
                            << ""
                            // results
                            << "The title" << ""
                            << "This is a description!"
                            << ProgramInfo::kCategoryNone;
    QTest::newRow("w/colon1") << "The title" << ""
                              << "Subtitle.. : This is a description!"
                              << ""
                              // results
                              << "The title" << "Subtitle.."
                              << "This is a description!"
                              << ProgramInfo::kCategoryNone;
    QTest::newRow("w/colon2") << "The title" << ""
                              << "Subtitle. : This is a description!"
                              << ""
                              // results
                              << "The title" << "Subtitle."
                              << "This is a description!"
                              << ProgramInfo::kCategoryNone;
}

void TestEITFixups::testUKSubtitleFixups()
{
    QFETCH(QString, title);
    QFETCH(QString, subtitle);
    QFETCH(QString, description);
    QFETCH(QString, category);
    QFETCH(QString, e_title);
    QFETCH(QString, e_subtitle);
    QFETCH(QString, e_description);
    QFETCH(ProgramInfo::CategoryType, e_cattype);

    DBEventEIT event_st(7302, title, description,
                      QDateTime::fromString("2020-02-28T23:55:00Z", Qt::ISODate),
                      QDateTime::fromString("2020-03-01T02:00:00Z", Qt::ISODate),
                      EITFixUp::kFixUK,
                      SUB_UNKNOWN,
                      AUD_STEREO,
                      VID_UNKNOWN);
    event_st.m_subtitle = subtitle;
    event_st.m_category = category;

    EITFixUp::Fix(event_st);
    PRINT_EVENT(event_st);
    QCOMPARE(event_st.m_title,        e_title);
    QCOMPARE(event_st.m_subtitle,     e_subtitle);
    QCOMPARE(event_st.m_description,  e_description);
    QCOMPARE(event_st.m_categoryType, e_cattype);
}

void TestEITFixups::testUKSeriesFixups_data()
{
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("subtitle");
    QTest::addColumn<QString>("description");
    QTest::addColumn<QString>("category");
    QTest::addColumn<QString>("e_title");
    QTest::addColumn<QString>("e_subtitle");
    QTest::addColumn<QString>("e_description");
    QTest::addColumn<int>("e_season");
    QTest::addColumn<int>("e_episode");
    QTest::addColumn<int>("e_totalepisodes");

    QTest::newRow("descr")  << "The title" << ""
                            << "Season 2 , Episode 3. This is a description."
                            << ""
                            // results
                            << "The title" << ""
                            << "This is a description"
                            << 2 << 3 << 0;
    QTest::newRow("season") << "The title Season 2 , Episode 3" << ""
                            << "This is a description."
                            << ""
                            // results
                            << "The title" << ""
                            << "This is a description"
                            << 2 << 3 << 0;
    QTest::newRow("series") << "The title Series 2, Episode 3 of 12." << ""
                            << "This is a description."
                            << ""
                            // results
                            << "The title" << ""
                            << "This is a description"
                            << 2 << 3 << 12;
    QTest::newRow("s")      << "The title (S2,Ep3/6)" << ""
                            << "This is a description."
                            << ""
                            // results
                            << "The title" << ""
                            << "This is a description"
                            << 2 << 3 << 6;
    QTest::newRow("s2")     << "The title Ep3/6" << ""
                            << "This is a description."
                            << ""
                            // results
                            << "The title" << ""
                            << "This is a description"
                            << 0 << 3 << 6;
    QTest::newRow("short1") << "The title. 3/6" << ""
                            << "This is a description."
                            << ""
                            // results
                            << "The title." << ""
                            << "This is a description"
                            << 0 << 3 << 6;
    QTest::newRow("short2") << "The title. (3 of 6)." << ""
                            << "This is a description."
                            << ""
                            // results
                            << "The title." << ""
                            << "This is a description"
                            << 0 << 3 << 6;
    QTest::newRow("shortx") << "The title 24/7" << ""
                            << "This is a description."
                            << ""
                            // results
                            << "The title 24/7" << ""
                            << "This is a description"
                            << 0 << 0 << 0;
}

void TestEITFixups::testUKSeriesFixups()
{
    QFETCH(QString, title);
    QFETCH(QString, subtitle);
    QFETCH(QString, description);
    QFETCH(QString, category);
    QFETCH(QString, e_title);
    QFETCH(QString, e_subtitle);
    QFETCH(QString, e_description);
    QFETCH(int, e_season);
    QFETCH(int, e_episode);
    QFETCH(int, e_totalepisodes);

    DBEventEIT event_se(7302, title, description,
                      QDateTime::fromString("2020-02-28T23:55:00Z", Qt::ISODate),
                      QDateTime::fromString("2020-03-01T02:00:00Z", Qt::ISODate),
                      EITFixUp::kFixUK,
                      SUB_UNKNOWN,
                      AUD_STEREO,
                      VID_UNKNOWN);
    event_se.m_subtitle = subtitle;
    event_se.m_category = category;

    EITFixUp::Fix(event_se);
    PRINT_EVENT(event_se);
    QCOMPARE(event_se.m_title,         e_title);
    QCOMPARE(event_se.m_subtitle,      e_subtitle);
    QCOMPARE(event_se.m_description,   e_description);
    QCOMPARE(event_se.m_season,        (uint)e_season);
    QCOMPARE(event_se.m_episode,       (uint)e_episode);
    QCOMPARE(event_se.m_totalepisodes, (uint)e_totalepisodes);
}

void TestEITFixups::testUKPartFixups_data()
{
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("subtitle");
    QTest::addColumn<QString>("description");
    QTest::addColumn<QString>("e_title");
    QTest::addColumn<QString>("e_subtitle");
    QTest::addColumn<QString>("e_description");
    QTest::addColumn<int>("e_partnumber");
    QTest::addColumn<int>("e_parttotal");

    QTest::newRow("descr")  << "The title" << ""
                            << "(Part 1 of 2) This is a description."
                            // results
                            << "The title" << ""
                            << "This is a description"
                            << 1 << 2;
    QTest::newRow("part")   << "The title (Part 1 of 2)" << ""
                            << "This is a description."
                            // results
                            << "The title" << ""
                            << "This is a description"
                            << 1 << 2;
    QTest::newRow("pt")     << "The title -Pt 1/2-" << ""
                            << "This is a description."
                            // results
                            << "The title" << ""
                            << "This is a description"
                            << 1 << 2;
    QTest::newRow("misc")   << "The title :Pt 1 of 2," << ""
                            << "This is a description."
                            // results
                            << "The title" << ""
                            << "This is a description"
                            << 1 << 2;
}

void TestEITFixups::testUKPartFixups()
{
    QFETCH(QString, title);
    QFETCH(QString, subtitle);
    QFETCH(QString, description);
    QFETCH(QString, e_title);
    QFETCH(QString, e_subtitle);
    QFETCH(QString, e_description);
    QFETCH(int, e_partnumber);
    QFETCH(int, e_parttotal);

    DBEventEIT event_pt(7302, title, description,
                      QDateTime::fromString("2020-02-28T23:55:00Z", Qt::ISODate),
                      QDateTime::fromString("2020-03-01T02:00:00Z", Qt::ISODate),
                      EITFixUp::kFixUK,
                      SUB_UNKNOWN,
                      AUD_STEREO,
                      VID_UNKNOWN);
    event_pt.m_subtitle = subtitle;

    EITFixUp::Fix(event_pt);
    PRINT_EVENT(event_pt);
    QCOMPARE(event_pt.m_title,       e_title);
    QCOMPARE(event_pt.m_subtitle,    e_subtitle);
    QCOMPARE(event_pt.m_description, e_description);
    QCOMPARE(event_pt.m_partnumber,  (uint16_t)e_partnumber);
    QCOMPARE(event_pt.m_parttotal,   (uint16_t)e_parttotal);
}

void TestEITFixups::testUKStarringFixups_data()
{
    QTest::addColumn<QString>("description");
    QTest::addColumn<QString>("e_person1");
    QTest::addColumn<QString>("e_person2");
    QTest::addColumn<int>("e_year");

    QTest::newRow("year")     << "Western starring Bud Abbott and Lou Costello, 1955."
                              << "Bud Abbott" << "Lou Costello" << 1955;
    QTest::newRow("noyear")   << "Western starring Bud Abbott and Lou Costello, sometime."
                              << "Bud Abbott" << "Lou Costello" << -1;
    QTest::newRow("noyear2")  << "Western starring Lou Costello and Bud Abbott."
                              << "Bud Abbott" << "Lou Costello" << -1;
    QTest::newRow("descyear") << "Description not a western [1987]"
                              << "" << "" << 1987;
}

void TestEITFixups::testUKStarringFixups()
{
    QFETCH(QString, description);
    QFETCH(QString, e_person1);
    QFETCH(QString, e_person2);
    QFETCH(int, e_year);

    DBEventEIT event_st(7302, "Title", description,
                      QDateTime::fromString("2020-02-28T23:55:00Z", Qt::ISODate),
                      QDateTime::fromString("2020-03-01T02:00:00Z", Qt::ISODate),
                      EITFixUp::kFixUK,
                      SUB_UNKNOWN,
                      AUD_STEREO,
                      VID_UNKNOWN);

    EITFixUp::Fix(event_st);
    PRINT_EVENT(event_st);
    if (e_year != -1)
    {
      QCOMPARE(event_st.m_airdate,         (uint16_t)e_year);
      QCOMPARE(event_st.m_originalairdate, QDate(e_year,1,1));
    }
    if (event_st.m_credits)
    {
        for (const auto& person : *event_st.m_credits)
        {
            QVERIFY((person.m_name == e_person1) ||
                    (person.m_name == e_person2));
        }
    }
}

void TestEITFixups::testBellExpress_data()
{
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("description");
    QTest::addColumn<QString>("category");
    QTest::addColumn<int>("cattype");
    QTest::addColumn<QString>("e_title");
    QTest::addColumn<QString>("e_subtitle");
    QTest::addColumn<QString>("e_description");
    QTest::addColumn<QString>("e_category");
    QTest::addColumn<int>("e_year");
    QTest::addColumn<int>("e_videoProps");
    QTest::addColumn<int>("e_audioProps");
    QTest::addColumn<int>("e_subtitleType");
    QTest::addColumn<bool>("e_previouslyShown");

    QTest::newRow("one")      << "Title" << "Subtitle\r\nDescription."
                              << "" << (int)DishThemeType::kThemeNone
                              << "Title" << "Subtitle" << "Description." << "Unknown" << -1
                              << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN << false;
    QTest::newRow("cat")      << "Title" << "Subtitle\r\nCategory. Description."
                              << "" << (int)DishThemeType::kThemeNone
                              << "Title" << "Subtitle" << "Description." << "Category" << -1
                              << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN << false;
    QTest::newRow("catbad")   << "Title (HD)" << "Subtitle\r\nCat(bad). Description."
                              << "" << (int)DishThemeType::kThemeNone
                              << "Title" << "Subtitle" << "Description." << "Unknown" << -1
                              << (int)VID_HDTV << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN << false;
    QTest::newRow("nocat")    << "Title HD" << "Subtitle\r\nToo long for category. Description."
                              << "" << (int)DishThemeType::kThemeNone
                              << "Title" << "Subtitle" << "Too long for category. Description." << "Unknown" << -1
                              << (int)VID_HDTV << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN << false;
    QTest::newRow("catbad")   << "Title" << "Subtitle\r\nCat(bad).Description. (HD)"
                              << "" << (int)DishThemeType::kThemeNone
                              << "Title" << "Subtitle" << "Cat(bad).Description." << "Unknown" << -1
                              << (int)VID_HDTV << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN << false;
    QTest::newRow("offair")   << "Title" << "Subtitle\r\n-. Description."
                              << "" << (int)DishThemeType::kThemeNone
                              << "Title" << "Subtitle" << "Description." << "OffAir" << -1
                              << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN << false;
    QTest::newRow("movie")    << "Title:" << "Subtitle\r\nMovie. (2012) Description."
                              << "dummy" << (int)DishThemeType::kThemeMovie
                              << "Title" << "Subtitle" << "Description." << "dummy"<< 2012
                              << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN << true;
    QTest::newRow("cc")       << "HD - Title" << "Subtitle\r\nMovie. (2012) Description. (CC) (DD)"
                              << "dummy" << (int)DishThemeType::kThemeMovie
                              << "Title" << "Subtitle" << "Description." << "dummy"<< 2012
                              << (int)VID_HDTV << (int)(AUD_DOLBY|AUD_STEREO) << (int)SUB_HARDHEAR << true;
    QTest::newRow("stereo1")  << "Title (All Day, HD)" << "Subtitle\r\nDescription. Stereo (SAP)"
                              << "" << (int)DishThemeType::kThemeNone
                              << "Title" << "Subtitle" << "Description." << "Unknown" << -1
                              << (int)VID_HDTV << (int)AUD_STEREO << (int)SUB_HARDHEAR << false;
    QTest::newRow("stereo2")  << "Title (All Day)" << "Subtitle\r\nDescription. (Stereo) "
                              << "" << (int)DishThemeType::kThemeNone
                              << "Title" << "Subtitle" << "Description." << "Unknown" << -1
                              << (int)VID_UNKNOWN << (int)AUD_STEREO << (int)SUB_UNKNOWN << false;
    QTest::newRow("tagnew")   << "Title" << "Subtitle\r\nMovie. (2012) Description. New."
                              << "dummy" << (int)DishThemeType::kThemeMovie
                              << "Title" << "Subtitle" << "Description." << "dummy"<< 2012
                              << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN << false;
    QTest::newRow("tagfinale1") << "Title" << "Subtitle\r\nMovie. (2012) Description. Finale."
                                << "dummy" << (int)DishThemeType::kThemeMovie
                                << "Title" << "Subtitle" << "Description." << "dummy"<< 2012
                                << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN << false;
    QTest::newRow("tagfinale2") << "Title" << "Subtitle\r\nMovie. (2012) Description. Season Finale."
                                << "dummy" << (int)DishThemeType::kThemeMovie
                                << "Title" << "Subtitle" << "Description." << "dummy"<< 2012
                                << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN << false;
    QTest::newRow("tagfinale3") << "Title" << "Subtitle\r\nMovie. (2012) Description. Series Finale."
                                << "dummy" << (int)DishThemeType::kThemeMovie
                                << "Title" << "Subtitle" << "Description." << "dummy"<< 2012
                                << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN << false;
    QTest::newRow("tagpremier1") << "Title" << "Subtitle\r\nMovie. (2012) Description. Series Premiere."
                                << "dummy" << (int)DishThemeType::kThemeMovie
                                << "Title" << "Subtitle" << "Description." << "dummy"<< 2012
                                << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN << false;
    QTest::newRow("tagpremier2") << "Title" << "Subtitle\r\nMovie. (2012) Description. Season Premier."
                                << "dummy" << (int)DishThemeType::kThemeMovie
                                << "Title" << "Subtitle" << "Description." << "dummy"<< 2012
                                << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN << false;
    QTest::newRow("tagpremier3") << "Title" << "Subtitle\r\nMovie. (2012) Description. Premier."
                                << "dummy" << (int)DishThemeType::kThemeMovie
                                << "Title" << "Subtitle" << "Description." << "dummy"<< 2012
                                << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN << false;
    QTest::newRow("tagpremier4") << "Title" << "Subtitle\r\nMovie. (2012) Description. Premiere."
                                << "dummy" << (int)DishThemeType::kThemeMovie
                                << "Title" << "Subtitle" << "Description." << "dummy"<< 2012
                                << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN << false;
    QTest::newRow("ppv1")     << "Title" << "Subtitle\r\nDescription. (A2c4E)"
                              << "" << (int)DishThemeType::kThemeNone
                              << "Title" << "Subtitle" << "Description." << "Unknown" << -1
                              << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN << false;
    QTest::newRow("ppv2")     << "Title" << "Subtitle\r\nDescription. (A2C4)" // Must be 5
                              << "" << (int)DishThemeType::kThemeNone
                              << "Title" << "Subtitle" << "Description. (A2C4)" << "Unknown" << -1
                              << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN << false;
    QTest::newRow("ppv3")     << "Title" << "Subtitle\r\nDescription. (A2C4E6)" // Must be 5
                              << "" << (int)DishThemeType::kThemeNone
                              << "Title" << "Subtitle" << "Description. (A2C4E6)" << "Unknown" << -1
                              << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN << false;
    QTest::newRow("eastern1") << "Title" << "All Day (Blah Blah Eastern)\r\nDescription."
                              << "" << (int)DishThemeType::kThemeNone
                              << "Title" << "" << "Description." << "Unknown" << -1
                              << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN << false;
    QTest::newRow("eastern2") << "Title" << "Subtitle\r\n(Blah Blah Eastern) Description."
                              << "" << (int)DishThemeType::kThemeNone
                              << "Title" << "Subtitle" << "Description." << "Unknown" << -1
                              << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN << false;
    QTest::newRow("eastern3") << "Title" << "Subtitle\r\n(0xxam-9xxam ET) Description."
                              << "" << (int)DishThemeType::kThemeNone
                              << "Title" << "Subtitle" << "Description." << "Unknown" << -1
                              << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN << false;
}

void TestEITFixups::testBellExpress()
{
    QFETCH(QString, title);
    QFETCH(QString, description);
    QFETCH(QString, category);
    QFETCH(int, cattype);
    QFETCH(QString, e_title);
    QFETCH(QString, e_subtitle);
    QFETCH(QString, e_description);
    QFETCH(QString, e_category);
    QFETCH(int, e_year);
    QFETCH(int, e_videoProps);
    QFETCH(int, e_audioProps);
    QFETCH(int, e_subtitleType);
    QFETCH(bool, e_previouslyShown);

    DBEventEIT event(7302, title, description,
                     QDateTime::fromString("2020-02-28T23:55:00Z", Qt::ISODate),
                     QDateTime::fromString("2020-03-01T02:00:00Z", Qt::ISODate),
                     EITFixUp::kFixBell,
                     (int)SUB_UNKNOWN,
                     AUD_UNKNOWN,
                     VID_UNKNOWN);
    event.m_category = category;
    event.m_categoryType = (ProgramInfo::CategoryType)cattype;

    EITFixUp::Fix(event);
    PRINT_EVENT(event);
    QCOMPARE(event.m_title,       e_title);
    QCOMPARE(event.m_subtitle,    e_subtitle);
    QCOMPARE(event.m_description, e_description);
    QCOMPARE(event.m_category,    e_category);
    if (e_year != -1)
        QCOMPARE(event.m_originalairdate, QDate(e_year,1,1));
    QCOMPARE(event.m_videoProps,      (uint8_t)e_videoProps);
    QCOMPARE(event.m_audioProps,      (uint8_t)e_audioProps);
    QCOMPARE(event.m_subtitleType,    (uint8_t)e_subtitleType);
    QCOMPARE(event.m_previouslyshown, e_previouslyShown);
}

void TestEITFixups::testBellExpressActors()
{
    DBEventEIT event(7302, "Title",
                     "Subtitle\r\nMovie. Actor 1, Actor 2 et Actor 3---(2012) Description.",
                     QDateTime::fromString("2020-02-28T23:55:00Z", Qt::ISODate),
                     QDateTime::fromString("2020-03-01T02:00:00Z", Qt::ISODate),
                     EITFixUp::kFixBell,
                     SUB_UNKNOWN,
                     AUD_STEREO,
                     VID_UNKNOWN);
    event.m_category = "dummy";
    event.m_categoryType = (ProgramInfo::CategoryType)DishThemeType::kThemeMovie;

    EITFixUp::Fix(event);
    PRINT_EVENT(event);
    QCOMPARE(event.m_title,          QString("Title"));
    QCOMPARE(event.m_subtitle,       QString("Subtitle"));
    QCOMPARE(event.m_description,    QString("Description."));
    QCOMPARE(event.m_category,       QString("dummy"));
    QCOMPARE(event.m_originalairdate, QDate(2012,1,1));
    if (event.m_credits)
    {
        for (const auto& person : *event.m_credits)
        {
            QVERIFY((person.m_name == "Actor 1") ||
                    (person.m_name == "Actor 2") ||
                    (person.m_name == "Actor 3"));
        }
    }
}

void TestEITFixups::testPBS()
{
    DBEventEIT event_prop(7302, "Title", "Subtitle : Description.",
                          QDateTime::fromString("2020-02-28T23:55:00Z", Qt::ISODate),
                          QDateTime::fromString("2020-03-01T02:00:00Z", Qt::ISODate),
                          EITFixUp::kFixPBS,
                          SUB_UNKNOWN,
                          AUD_STEREO,
                          VID_UNKNOWN);

    EITFixUp::Fix(event_prop);
    PRINT_EVENT(event_prop);
    QCOMPARE(event_prop.m_title,        QString("Title"));
    QCOMPARE(event_prop.m_subtitle,     QString("Subtitle"));
    QCOMPARE(event_prop.m_description,  QString("Description."));
}

//
// ComHem DVB-C service in Sweden.
// Test most title and subtitle parsing fixups.
//
void TestEITFixups::testComHem_data()
{
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("subtitle");
    QTest::addColumn<QString>("description");
    QTest::addColumn<QString>("e_title");
    QTest::addColumn<QString>("e_subtitle");
    QTest::addColumn<QString>("e_description");
    QTest::addColumn<int>("e_partNumber");
    QTest::addColumn<int>("e_partTotal");
    QTest::addColumn<int>("e_year");
    QTest::addColumn<ProgramInfo::CategoryType>("e_category");
    QTest::addColumn<bool>("e_subtitles");

    QTest::newRow("revert")  << "Title" << "Subtitle" << ""
                             << "Title" << "" << "" << 0 << 0 << 0
                             << ProgramInfo::kCategoryNone << false;

    // Episode info in description
    QTest::newRow("series1-slashkeep") << "Title" << "Subtitle" << "Del 12/345. Description."
                                       << "Title" << "Del 12/345" << "Description." << 12 << 345 << 0
                                       << ProgramInfo::kCategorySeries << false;
    QTest::newRow("series1-slash")     << "Title" << "Subtitle" << "Description. Del 12/345."
                                       << "Title" << "Del 12 av 345" << "Description." << 12 << 345 << 0
                                       << ProgramInfo::kCategorySeries << false;
    QTest::newRow("series1-colon")     << "Title" << "Subtitle" << "Description. del 12 : 345."
                                       << "Title" << "Del 12 av 345" << "Description." << 12 << 345 << 0
                                       << ProgramInfo::kCategorySeries << false;
    QTest::newRow("series1-av")        << "Title" << "Subtitle" << "Description. episode 12 av 345."
                                       << "Title" << "Del 12 av 345" << "Description." << 12 << 345 << 0
                                       << ProgramInfo::kCategorySeries << false;
    QTest::newRow("series1-eponly")    << "Title" << "Subtitle" << "Description. episode 12."
                                       << "Title" << "Del 12" << "Description." << 12 << 0 << 0
                                       << ProgramInfo::kCategorySeries << false;
    // Episode info in title
    QTest::newRow("series2-nodash")     << "Title Del 123" << "Subtitle" << "Description"
                                        << "Title" << "Del 123" << "Description" << 123 << 0 << 0
                                        << ProgramInfo::kCategoryNone << false;
    QTest::newRow("series2-spacenone")  << "Title-Del 123" << "Subtitle" << "Description"
                                        << "Title" << "Del 123" << "Description" << 123 << 0 << 0
                                        << ProgramInfo::kCategoryNone << false;
    QTest::newRow("series2-spacepre")   << "Title -del 123" << "Subtitle" << "Description"
                                        << "Title" << "Del 123" << "Description" << 123 << 0 << 0
                                        << ProgramInfo::kCategoryNone << false;
    QTest::newRow("series2-space-post") << "Title- del 123" << "Subtitle" << "Description text-tv"
                                        << "Title" << "Del 123" << "Description text-tv" << 123 << 0 << 0
                                        << ProgramInfo::kCategoryNone << true;
    QTest::newRow("series2-spaceboth")  << "Title - Del 123" << "Subtitle" << "Description Text-TV"
                                        << "Title" << "Del 123" << "Description Text-TV" << 123 << 0 << 0
                                        << ProgramInfo::kCategoryNone << true;

    // Move subtitle out of title field
    QTest::newRow("subtitle") << "Title  -  Blah de blah blah 123" << "Subtitle" << "Description"
                              << "Title" << "Blah de blah blah 123" << "Description" << 0 << 0 << 0
                              << ProgramInfo::kCategoryNone << false;
}

void TestEITFixups::testComHem()
{
    QFETCH(QString, title);
    QFETCH(QString, subtitle);
    QFETCH(QString, description);
    QFETCH(QString, e_title);
    QFETCH(QString, e_subtitle);
    QFETCH(QString, e_description);
    QFETCH(int, e_partNumber);
    QFETCH(int, e_partTotal);
    QFETCH(int, e_year);
    QFETCH(ProgramInfo::CategoryType, e_category);
    QFETCH(bool, e_subtitles);

    DBEventEIT event_prop(7302, title, subtitle, description,
                          "", ProgramInfo::kCategoryNone,
                          QDateTime::fromString("2020-02-28T23:55:00Z", Qt::ISODate),
                          QDateTime::fromString("2020-03-01T02:00:00Z", Qt::ISODate),
                          EITFixUp::kFixComHem | EITFixUp::kFixSubtitle,
                          SUB_UNKNOWN, AUD_STEREO, VID_UNKNOWN, 0.0F,
                          "", "", 0, 0, 0);

    EITFixUp::Fix(event_prop);
    PRINT_EVENT(event_prop);
    QCOMPARE(event_prop.m_title,        e_title);
    QCOMPARE(event_prop.m_subtitle,     e_subtitle);
    QCOMPARE(event_prop.m_description,  e_description);
    QCOMPARE(event_prop.m_partnumber,   (uint16_t)e_partNumber);
    QCOMPARE(event_prop.m_parttotal,    (uint16_t)e_partTotal);
    QCOMPARE(event_prop.m_airdate,      (uint16_t)e_year);
    QCOMPARE(event_prop.m_categoryType, e_category);
    QCOMPARE(((event_prop.m_subtitleType & SUB_NORMAL) != 0), e_subtitles);
}

//
// ComHem DVB-C service in Sweden.
// Test the name parsing from descriptions .
//
void TestEITFixups::testComHem2_data()
{
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("subtitle");
    QTest::addColumn<QString>("description");
    QTest::addColumn<QString>("e_title");
    QTest::addColumn<QString>("e_subtitle");
    QTest::addColumn<QString>("e_description");
    QTest::addColumn<int>("e_partNumber");
    QTest::addColumn<int>("e_partTotal");
    QTest::addColumn<int>("e_year");
    QTest::addColumn<ProgramInfo::CategoryType>("e_category");
    QTest::addColumn<QStringList>("e_actors");
    QTest::addColumn<QStringList>("e_directors");
    QTest::addColumn<QStringList>("e_hosts");

    // Parse description
    // (Title) Country Category Year Actors Description
    // Title:    in parentheses (optional)
    // Country:  terminated by a space
    // Category: Anything but digit or period or space
    // Year:     'från' followed by four digits
    // Actors:   'med' followed by anything but a period (optional)
    // Period
    QTest::newRow("desc-year")     << "Title" << "Subtitle" << "(Title)USA blah från 1984. Description"
                                   << "Title" << "(Title) USA blah" << "Description" << 0 << 0 << 1984
                                   << ProgramInfo::kCategoryNone
                                   << QStringList() << QStringList() << QStringList(); //oops?
    QTest::newRow("desc-cast")     << "Title" << "Subtitle" << "(Title) USA serie från 1984 med Bud Abbott, Lou Costello och Sting. Description"
                                   << "Title" << "(Title) USA serie" << "Description" << 0 << 0 << 1984
                                   << ProgramInfo::kCategorySeries
                                   << QStringList({"Lou Costello", "Sting", "Bud Abbott"}) << QStringList() << QStringList();
    QTest::newRow("desc-director") << "Title" << "Subtitle" << "Description regi: Bud Abbott."
                                   << "Title" << "" << "Description" << 0 << 0 << 0
                                   << ProgramInfo::kCategoryNone
                                   << QStringList() << QStringList("Bud Abbott") << QStringList();
    QTest::newRow("desc-actor1")   << "Title" << "Subtitle" << "Description skådespelare: Bud Abbott."
                                   << "Title" << "" << "Description" << 0 << 0 << 0
                                   << ProgramInfo::kCategoryNone
                                   << QStringList("Bud Abbott") << QStringList() << QStringList();
    QTest::newRow("desc-actor2")   << "Title" << "Subtitle" << "Description i rollerna: Bud Abbott."
                                   << "Title" << "" << "Description" << 0 << 0 << 0
                                   << ProgramInfo::kCategoryNone
                                   << QStringList("Bud Abbott") << QStringList() << QStringList();
    QTest::newRow("desc-host")     << "Title" << "Subtitle" << "Description programledare: Bud Abbott."
                                   << "Title" << "" << "Description" << 0 << 0 << 0
                                   << ProgramInfo::kCategoryNone
                                   << QStringList() << QStringList() << QStringList("Bud Abbott");
    QTest::newRow("desc-multi")    << "Title" << "Subtitle" << "Description Regi: Bud Abbott. I rollerna: Bugs Bunny. Skådespelare: Lou Costello. Programledare: Sting."
                                   << "Title" << "" << "Description" << 0 << 0 << 0
                                   << ProgramInfo::kCategoryNone
                                   << QStringList({"Lou Costello", "Bugs Bunny"}) << QStringList("Bud Abbott") << QStringList("Sting");
}

void TestEITFixups::testComHem2()
{
    QFETCH(QString, title);
    QFETCH(QString, subtitle);
    QFETCH(QString, description);
    QFETCH(QString, e_title);
    QFETCH(QString, e_subtitle);
    QFETCH(QString, e_description);
    QFETCH(int, e_partNumber);
    QFETCH(int, e_partTotal);
    QFETCH(int, e_year);
    QFETCH(ProgramInfo::CategoryType, e_category);
    QFETCH(QStringList, e_actors);
    QFETCH(QStringList, e_directors);
    QFETCH(QStringList, e_hosts);

    DBEventEIT event_prop(7302, title, subtitle, description,
                          "", ProgramInfo::kCategoryNone,
                          QDateTime::fromString("2020-02-28T23:55:00Z", Qt::ISODate),
                          QDateTime::fromString("2020-03-01T02:00:00Z", Qt::ISODate),
                          EITFixUp::kFixComHem | EITFixUp::kFixSubtitle,
                          SUB_UNKNOWN, AUD_STEREO, VID_UNKNOWN, 0.0F,
                          "", "", 0, 0, 0);

    EITFixUp::Fix(event_prop);
    PRINT_EVENT(event_prop);
    QCOMPARE(event_prop.m_title,        e_title);
    QCOMPARE(event_prop.m_subtitle,     e_subtitle);
    QCOMPARE(event_prop.m_description,  e_description);
    QCOMPARE(event_prop.m_partnumber,   (uint16_t)e_partNumber);
    QCOMPARE(event_prop.m_parttotal,    (uint16_t)e_partTotal);
    QCOMPARE(event_prop.m_airdate,      (uint16_t)e_year);
    QCOMPARE(event_prop.m_categoryType, e_category);
    checkCast(event_prop, e_actors, e_directors, e_hosts);
}

//
// ComHem DVB-C service in Sweden.
// Test the rerun fixups. Dates are in the form "dd/mm - yyyy".
//
void TestEITFixups::testComHem3_data()
{
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("subtitle");
    QTest::addColumn<QString>("description");
    QTest::addColumn<QString>("e_title");
    QTest::addColumn<QString>("e_subtitle");
    QTest::addColumn<QString>("e_description");
    QTest::addColumn<QDate>("e_airdate");

    QTest::newRow("rerun-today")     << "Title" << "Subtitle" << "Subtitle2. Description. Repris från i dag."
                                     << "Title" << "Subtitle2" << "Description. Repris från i dag."
                                     << QDate(2020,2,14);
    QTest::newRow("rerun-yesterday") << "Title" << "Subtitle" << "Subtitle2. Description. Repris från eftermiddagen."
                                     << "Title" << "Subtitle2" << "Description. Repris från eftermiddagen."
                                     << QDate(2020,2,13);
    QTest::newRow("rerun-date")      << "Title" << "Subtitle" << "Subtitle2. Description. Repris från 1/2."
                                     << "Title" << "Subtitle2" << "Description. Repris från 1/2."
                                     << QDate(2020,2,1);
    QTest::newRow("rerun-date2")     << "Title" << "Subtitle" << "Subtitle2. Description. Repris från 2/12."
                                     << "Title" << "Subtitle2" << "Description. Repris från 2/12."
                                     << QDate(2019,12,2);
    QTest::newRow("rerun-date2")     << "Title" << "Subtitle" << "Subtitle2. Description. Repris från 2/12 - 2011."
                                     << "Title" << "Subtitle2" << "Description. Repris från 2/12 - 2011."
                                     << QDate(2019,12,2); // not implemented
}

void TestEITFixups::testComHem3()
{
    QFETCH(QString, title);
    QFETCH(QString, subtitle);
    QFETCH(QString, description);
    QFETCH(QString, e_title);
    QFETCH(QString, e_subtitle);
    QFETCH(QString, e_description);
    QFETCH(QDate,   e_airdate);

    DBEventEIT event_prop(7302, title, subtitle, description,
                          "", ProgramInfo::kCategoryNone,
                          QDateTime::fromString("2020-02-14T23:55:00Z", Qt::ISODate),
                          QDateTime::fromString("2020-03-01T02:00:00Z", Qt::ISODate),
                          EITFixUp::kFixComHem | EITFixUp::kFixSubtitle,
                          SUB_UNKNOWN, AUD_STEREO, VID_UNKNOWN, 0.0F,
                          "", "", 0, 0, 0);

    EITFixUp::Fix(event_prop);
    PRINT_EVENT(event_prop);
    QCOMPARE(event_prop.m_title,        e_title);
    QCOMPARE(event_prop.m_subtitle,     e_subtitle);
    QCOMPARE(event_prop.m_description,  e_description);
    QCOMPARE(event_prop.m_originalairdate, e_airdate);
}

void TestEITFixups::testAUStar_data()
{
    QTest::addColumn<QString>("subtitle");
    QTest::addColumn<QString>("description");
    QTest::addColumn<QString>("e_subtitle");
    QTest::addColumn<QString>("e_description");
    QTest::addColumn<QString>("e_category");

    QTest::newRow("nosplit") << "Category" << "Subtitle - Description"
                             << "Category" << "Subtitle - Description" << "Category";
    QTest::newRow("split")   << "Category" << "Subtitle : Description"
                             << "Subtitle" << "Description" << "Category";
}

void TestEITFixups::testAUStar()
{
    QFETCH(QString, subtitle);
    QFETCH(QString, description);
    QFETCH(QString, e_subtitle);
    QFETCH(QString, e_description);
    QFETCH(QString, e_category);

    DBEventEIT event_prop(7302, "Title", subtitle, description,
                          "", ProgramInfo::kCategoryNone,
                          QDateTime::fromString("2020-02-28T23:55:00Z", Qt::ISODate),
                          QDateTime::fromString("2020-03-01T02:00:00Z", Qt::ISODate),
                          EITFixUp::kFixAUStar,
                          SUB_UNKNOWN, AUD_STEREO, VID_UNKNOWN, 0.0F,
                          "", "", 0, 0, 0);

    EITFixUp::Fix(event_prop);
    PRINT_EVENT(event_prop);
    QCOMPARE(event_prop.m_subtitle,     e_subtitle);
    QCOMPARE(event_prop.m_description,  e_description);
    QCOMPARE(event_prop.m_category,     e_category);
}

void TestEITFixups::testAUDescription_data()
{
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("subtitle");
    QTest::addColumn<QString>("description");
    QTest::addColumn<QString>("e_title");
    QTest::addColumn<QString>("e_subtitle");
    QTest::addColumn<QString>("e_description");

    QTest::newRow("pgmdata")  << "Title" << "Real description" << "[Program data blah blah"
                              << "Title" << "" << "Real description";
    QTest::newRow("pgminfo")  << "Title" << "Real description" << "[Program info blah blah"
                              << "Title" << "" << "Real description";
    QTest::newRow("west")     << "Title" << "" << "Description blah blah (blah blah2 Copyright West TV Ltd. 2011)"
                              << "Title" << "" << "Description blah blah";
    QTest::newRow("duptitle") << "Title" << "Subtitle" << "Title - Description"
                              << "Title" << "Subtitle" << "Description";
    QTest::newRow("live")     << "Live: Title" << "Subtitle" << "Description"
                              << "Title" << "Subtitle" << "(Live) Description";
}

void TestEITFixups::testAUDescription()
{
    QFETCH(QString, title);
    QFETCH(QString, subtitle);
    QFETCH(QString, description);
    QFETCH(QString, e_title);
    QFETCH(QString, e_subtitle);
    QFETCH(QString, e_description);

    DBEventEIT event_prop(7302, title, subtitle, description,
                          "", ProgramInfo::kCategoryNone,
                          QDateTime::fromString("2020-02-28T23:55:00Z", Qt::ISODate),
                          QDateTime::fromString("2020-03-01T02:00:00Z", Qt::ISODate),
                          EITFixUp::kFixAUDescription,
                          SUB_UNKNOWN, AUD_STEREO, VID_UNKNOWN, 0.0F,
                          "", "", 0, 0, 0);

    EITFixUp::Fix(event_prop);
    PRINT_EVENT(event_prop);
    QCOMPARE(event_prop.m_title,        e_title);
    QCOMPARE(event_prop.m_subtitle,     e_subtitle);
    QCOMPARE(event_prop.m_description,  e_description);
}

void TestEITFixups::testAUNine_data()
{
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("subtitle");
    QTest::addColumn<QString>("description");
    QTest::addColumn<QString>("e_title");
    QTest::addColumn<QString>("e_subtitle");
    QTest::addColumn<QString>("e_description");
    QTest::addColumn<int>("e_videoProps");
    QTest::addColumn<int>("e_subtitleType");
    QTest::addColumn<ProgramInfo::CategoryType>("e_cattype");
    QTest::addColumn<QString>("e_rating");

    QTest::newRow("hd g") << "Title" << "Subtitle" << "(G) [HD] Description"
                          << "Title" << "Subtitle" << "Description"
                          << (int)VID_HDTV << (int)SUB_UNKNOWN
                          << ProgramInfo::kCategoryNone << "G";
    QTest::newRow("cc movie ma") << "Title" << "Movie" << "(MA) [CC] Title Description"
                          << "Title" << "" << "Description"
                          << (int)VID_UNKNOWN << (int)SUB_NORMAL
                          << ProgramInfo::kCategoryMovie << "MA";
}

void TestEITFixups::testAUNine()
{
    QFETCH(QString, title);
    QFETCH(QString, subtitle);
    QFETCH(QString, description);
    QFETCH(QString, e_title);
    QFETCH(QString, e_subtitle);
    QFETCH(QString, e_description);
    QFETCH(int, e_videoProps);
    QFETCH(int, e_subtitleType);
    QFETCH(ProgramInfo::CategoryType, e_cattype);
    QFETCH(QString, e_rating);

    DBEventEIT event(7302, title, subtitle, description,
                          "", ProgramInfo::kCategoryNone,
                          QDateTime::fromString("2020-02-28T23:55:00Z", Qt::ISODate),
                          QDateTime::fromString("2020-03-01T02:00:00Z", Qt::ISODate),
                          EITFixUp::kFixAUNine,
                          SUB_UNKNOWN, AUD_UNKNOWN, VID_UNKNOWN, 0.0F,
                          "", "", 0, 0, 0);

    EITFixUp::Fix(event);
    PRINT_EVENT(event);
    QCOMPARE(event.m_title,        e_title);
    QCOMPARE(event.m_subtitle,     e_subtitle);
    QCOMPARE(event.m_description,  e_description);
    QCOMPARE(event.m_videoProps,   (uint8_t)e_videoProps);
    QCOMPARE(event.m_subtitleType, (uint8_t)e_subtitleType);
    QCOMPARE(event.m_categoryType, e_cattype);
    QCOMPARE(event.m_ratings.isEmpty(), e_rating.isEmpty());
    checkRating(event, "AU", e_rating);
}

void TestEITFixups::testAUSeven_data()
{
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("subtitle");
    QTest::addColumn<QString>("description");
    QTest::addColumn<QString>("e_title");
    QTest::addColumn<QString>("e_subtitle");
    QTest::addColumn<QString>("e_description");
    QTest::addColumn<int>("e_subtitleType");
    QTest::addColumn<int>("e_year");
    QTest::addColumn<QString>("e_rating");

    QTest::newRow("cc rpt g")   << "Title" << "Subtitle" << "Description G CC 1984 Rpt"
                                << "Title" << "Subtitle" << "Description"
                                << (int)SUB_NORMAL << 1984 << "G";
    QTest::newRow("ma advice")  << "Title" << "Subtitle" << "Description MA (ADVICE,HERE)"
                                << "Title" << "Subtitle" << "Description"
                                << (int)SUB_UNKNOWN << 0 << "MA (ADVICE,HERE)";
    QTest::newRow("bad advice") << "Title" << "Subtitle" << "Description MA (Bad,advice,capitalization)"
                                << "Title" << "Subtitle" << "Description MA (Bad,advice,capitalization)"
                                << (int)SUB_UNKNOWN << 0 << "";
}

void TestEITFixups::testAUSeven()
{
    QFETCH(QString, title);
    QFETCH(QString, subtitle);
    QFETCH(QString, description);
    QFETCH(QString, e_title);
    QFETCH(QString, e_subtitle);
    QFETCH(QString, e_description);
    QFETCH(int, e_subtitleType);
    QFETCH(int, e_year);
    QFETCH(QString, e_rating);

    DBEventEIT event(7302, title, subtitle, description,
                          "", ProgramInfo::kCategoryNone,
                          QDateTime::fromString("2020-02-28T23:55:00Z", Qt::ISODate),
                          QDateTime::fromString("2020-03-01T02:00:00Z", Qt::ISODate),
                          EITFixUp::kFixAUSeven,
                          SUB_UNKNOWN, AUD_UNKNOWN, VID_UNKNOWN, 0.0F,
                          "", "", 0, 0, 0);

    EITFixUp::Fix(event);
    PRINT_EVENT(event);
    QCOMPARE(event.m_title,           e_title);
    QCOMPARE(event.m_subtitle,        e_subtitle);
    QCOMPARE(event.m_description,     e_description);
    QCOMPARE(event.m_subtitleType,    (uint8_t)e_subtitleType);
    QCOMPARE(event.m_airdate,         (uint16_t)e_year);
    QCOMPARE(event.m_ratings.isEmpty(), e_rating.isEmpty());
    checkRating(event, "AU", e_rating);
}

void TestEITFixups::testAUFreeview_data()
{
    QTest::addColumn<QString>("subtitle");
    QTest::addColumn<QString>("description");
    QTest::addColumn<QString>("e_subtitle");
    QTest::addColumn<QString>("e_description");
    QTest::addColumn<int>("e_year");
    QTest::addColumn<bool>("e_hascast");

    QTest::newRow("bail") << "Subtitle" << "Description.."
                          << "Subtitle" << "Description.." << 0 << false;
    QTest::newRow("sy1")  << "Subtitle1" << "Description blah blah (Subtitle2) (1984)"
                          << "Subtitle1" << "Description blah blah" << 1984 << false;
    QTest::newRow("sy2")  << "" << "Description blah blah (Subtitle2) (1984)"
                          << "Subtitle2" << "Description blah blah" << 1984 << false;
    QTest::newRow("y")    << "" << "Description blah blah (1984)"
                          << "" << "Description blah blah" << 1984 << false;
    QTest::newRow("syc1") << "Subtitle1" << "Description blah blah (Subtitle2) (1984) (Bud Abbott/Lou Costello)"
                          << "Subtitle1" << "Description blah blah" << 1984 << true;
    QTest::newRow("syc2") << "" << "Description blah blah (Subtitle2) (1984) (Bud Abbott/Lou Costello)"
                          << "Subtitle2" << "Description blah blah" << 1984 << true;
    QTest::newRow("syc")  << "Subtitle1" << "Description blah blah (1984) (Bud Abbott/Lou Costello)"
                          << "Subtitle1" << "Description blah blah" << 1984 << true;
}

void TestEITFixups::testAUFreeview()
{
    QFETCH(QString, subtitle);
    QFETCH(QString, description);
    QFETCH(QString, e_subtitle);
    QFETCH(QString, e_description);
    QFETCH(int, e_year);
    QFETCH(bool, e_hascast);

    DBEventEIT event(7302, "Title", subtitle, description,
                          "", ProgramInfo::kCategoryNone,
                          QDateTime::fromString("2020-02-28T23:55:00Z", Qt::ISODate),
                          QDateTime::fromString("2020-03-01T02:00:00Z", Qt::ISODate),
                          EITFixUp::kFixAUFreeview,
                          SUB_UNKNOWN, AUD_UNKNOWN, VID_UNKNOWN, 0.0F,
                          "", "", 0, 0, 0);

    EITFixUp::Fix(event);
    PRINT_EVENT(event);
    QCOMPARE(event.m_subtitle,           e_subtitle);
    QCOMPARE(event.m_description,        e_description);
    QCOMPARE(event.m_airdate,            (uint16_t)e_year);
    QCOMPARE(event.m_credits != nullptr, e_hascast);
    if (event.m_credits && e_hascast)
    {
        for (const auto& person : *event.m_credits)
        {
            QVERIFY((person.m_name == "Bud Abbott") ||
                    (person.m_name == "Lou Costello"));
        }
    }
}

//
// MultiChoice Africa DVB-S guide
// Handle most of the data in the description.
//
void TestEITFixups::testMCA_data()
{
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("subtitle");
    QTest::addColumn<QString>("description");
    QTest::addColumn<QString>("e_title");
    QTest::addColumn<QString>("e_subtitle");
    QTest::addColumn<QString>("e_description");
    QTest::addColumn<int>("e_season");
    QTest::addColumn<int>("e_episode");
    QTest::addColumn<int>("e_audioProps");
    QTest::addColumn<int>("e_subtitleType");

    QString toolong("'This looks like it should be a subtitle but the string is way to long to fit "
                    "into the 128 bytes that is marked as the upper limit for a subtitle'. This "
                    "description is also very long so that it is the size check and not the "
                    "percentage check that is being triggered in the code.");
    QString badpercentage("This looks like a subtitle but it takes up more than 60% of the "
                          "description'. So it gets ignored.");

    QTest::newRow("nodesc")   << "Title" << "Subtitle" << ""
                              << "Title" << "" << "Subtitle"
                              << 0 << 0 << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN;
    QTest::newRow("title1")   << "Title..." << "Subtitle" << "Title is really long. This is the description."
                              << "Title is really long" << "" << "This is the description."
                              << 0 << 0 << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN;
    QTest::newRow("title2")   << "Title...." << "Subtitle" << "Title is really long? This is the description."
                              << "Title is really long" << "" << "This is the description."
                              << 0 << 0 << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN;
    QTest::newRow("subtitle") << "Title" << "Subtitle" << "'Real subtitle'. This is the description."
                              << "Title" << "Real subtitle" << "This is the description."
                              << 0 << 0 << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN;
    QTest::newRow("episode")  << "Title" << "Subtitle" << "'S01/E02 - Real subtitle'. This is the description."
                              << "Title" << "Real subtitle" << "This is the description."
                              << 1 << 2 << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN;
    QTest::newRow("toolong")  << "Title" << "Subtitle" << toolong
                              << "Title" << "" << toolong
                              << 0 << 0 << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN;
    QTest::newRow("percent")  << "Title" << "Subtitle" << badpercentage
                              << "Title" << "" << badpercentage
                              << 0 << 0 << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN;
    QTest::newRow("dolby")    << "Title" << "Subtitle" << "This is the description. DD"
                              << "Title" << "" << "This is the description."
                              << 0 << 0 << (int)AUD_DOLBY << (int)SUB_UNKNOWN;
    QTest::newRow("cc1")      << "Title" << "Subtitle" << "This is the description. English Subtitles"
                              << "Title" << "" << "This is the description."
                              << 0 << 0 << (int)AUD_UNKNOWN << (int)SUB_HARDHEAR;
    QTest::newRow("cc1")      << "Title" << "Subtitle" << "This is the description, HI Subtitles"
                              << "Title" << "" << "This is the description"
                              << 0 << 0 << (int)AUD_UNKNOWN << (int)SUB_HARDHEAR;
    QTest::newRow("boquet")   << "Title" << "Subtitle" << "This is the description. Only available on qwerty bouquet."
                              << "Title" << "" << "This is the description."
                              << 0 << 0 << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN;
    QTest::newRow("notrsa")   << "Title" << "Subtitle" << "This is the description. Not available in RSA qwerty."
                              << "Title" << "" << "This is the description."
                              << 0 << 0 << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN;
    QTest::newRow("mix")      << "Title" << "Subtitle" << "'01/02 - Real subtitle'. This is the description. Not available in RSA abcdefg. HI Subtitles, DD."
                              << "Title" << "Real subtitle" << "This is the description."
                              << 1 << 2 << (int)AUD_DOLBY << (int)SUB_HARDHEAR;
}

void TestEITFixups::testMCA()
{
    QFETCH(QString, title);
    QFETCH(QString, subtitle);
    QFETCH(QString, description);
    QFETCH(QString, e_title);
    QFETCH(QString, e_subtitle);
    QFETCH(QString, e_description);
    QFETCH(int, e_season);
    QFETCH(int, e_episode);
    QFETCH(int, e_audioProps);
    QFETCH(int, e_subtitleType);

    DBEventEIT event(7302, title, subtitle, description,
                          "", ProgramInfo::kCategoryNone,
                          QDateTime::fromString("2020-02-28T23:55:00Z", Qt::ISODate),
                          QDateTime::fromString("2020-03-01T02:00:00Z", Qt::ISODate),
                          EITFixUp::kFixMCA,
                          SUB_UNKNOWN, AUD_UNKNOWN, VID_UNKNOWN, 0.0F,
                          "", "", 0, 0, 0);

    EITFixUp::Fix(event);
    PRINT_EVENT(event);
    QCOMPARE(event.m_title,           e_title);
    QCOMPARE(event.m_subtitle,        e_subtitle);
    QCOMPARE(event.m_description,     e_description);
    QCOMPARE(event.m_season,          (uint)e_season);
    QCOMPARE(event.m_episode,         (uint)e_episode);
    QCOMPARE(event.m_audioProps,      (uint8_t)e_audioProps);
    QCOMPARE(event.m_subtitleType,    (uint8_t)e_subtitleType);
}

//
// MultiChoice Africa DVB-S guide
// Handle the year and peoples names in the description.
//
void TestEITFixups::testMCA2_data()
{
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("subtitle");
    QTest::addColumn<QString>("description");
    QTest::addColumn<QString>("e_title");
    QTest::addColumn<QString>("e_subtitle");
    QTest::addColumn<QString>("e_description");
    QTest::addColumn<int>("e_year");
    QTest::addColumn<QStringList>("e_directors");
    QTest::addColumn<QStringList>("e_actors");

    QTest::newRow("year")     << "Title" << "Subtitle" << "This is the description. (1984)"
                              << "Title" << "" << "This is the description."
                              << 1984 << QStringList() << QStringList();
    QTest::newRow("director") << "Title" << "Subtitle" << "This is the description. (1983) Barry Levinson"
                              << "Title" << "" << "This is the description."
                              << 1983 << QStringList("Barry Levinson") << QStringList();
    QTest::newRow("actors")   << "Title" << "Subtitle" << "This is the description. Bud Abbott,   Lou Costello. (1983) Barry Levinson"
                              << "Title" << "" << "This is the description."
                              << 1983 << QStringList("Barry Levinson") << QStringList({"Lou Costello", "Bud Abbott"});
}

void TestEITFixups::testMCA2()
{
    QFETCH(QString, title);
    QFETCH(QString, subtitle);
    QFETCH(QString, description);
    QFETCH(QString, e_title);
    QFETCH(QString, e_subtitle);
    QFETCH(QString, e_description);
    QFETCH(int, e_year);
    QFETCH(QStringList, e_directors);
    QFETCH(QStringList, e_actors);

    DBEventEIT event(7302, title, subtitle, description,
                          "", ProgramInfo::kCategoryNone,
                          QDateTime::fromString("2020-02-28T23:55:00Z", Qt::ISODate),
                          QDateTime::fromString("2020-03-01T02:00:00Z", Qt::ISODate),
                          EITFixUp::kFixMCA,
                          SUB_UNKNOWN, AUD_UNKNOWN, VID_UNKNOWN, 0.0F,
                          "", "", 0, 0, 0);

    EITFixUp::Fix(event);
    PRINT_EVENT(event);
    QCOMPARE(event.m_title,           e_title);
    QCOMPARE(event.m_subtitle,        e_subtitle);
    QCOMPARE(event.m_description,     e_description);
    QCOMPARE(event.m_airdate,         (uint16_t)e_year);
    checkCast(event, e_actors, e_directors);
}

//
// Germany RTLgroup
// Handle the data in the description field.
//
void TestEITFixups::testRTL_data()
{
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("subtitle");
    QTest::addColumn<QString>("description");
    QTest::addColumn<QString>("e_title");
    QTest::addColumn<QString>("e_subtitle");
    QTest::addColumn<QString>("e_description");
    QTest::addColumn<QString>("e_syndicatedepisode");
    QTest::addColumn<ProgramInfo::CategoryType>("e_cattype");

    const QString ignoreme {"If there is a subtitle then anything in the description is ignored."};
    QTest::newRow("subtitle") << "Title" << "Subtitle" << ignoreme
                              << "Title" << "Subtitle" << ignoreme
                              << "" << ProgramInfo::kCategoryNone;

    QTest::newRow("repeat1")  << "Title" << "" << "Description. Wiederholung vom 25.12.2018"
                              << "Title" << "" << "Description."
                              << "" << ProgramInfo::kCategoryNone;
    QTest::newRow("repeat2")  << "Title" << "" << "Description. Wiederholungxxvon 25.12.2018"
                              << "Title" << "" << "Description."
                              << "" << ProgramInfo::kCategoryNone;
    QTest::newRow("repeat3")  << "Title" << "" << "Description. Wiederholung vom 10.30 Uhr"
                              << "Title" << "" << "Description."
                              << "" << ProgramInfo::kCategoryNone;
    QTest::newRow("repeat4")  << "Title" << "" << "Description. Wiederholungxxvon 10:30 Uhr"
                              << "Title" << "" << "Description."
                              << "" << ProgramInfo::kCategoryNone;
    QTest::newRow("repeat5")  << "Title" << "" << "Wiederholung von 10:30 Uhr. Description."
                              << "Title" << "" << "Description."
                              << "" << ProgramInfo::kCategoryNone;

    QTest::newRow("st1folge0")  << "Title" << "" << "Folge 1: 'The subtitle'"
                                << "Title" << "The subtitle" << ""
                                << "1" << ProgramInfo::kCategorySeries;
    QTest::newRow("st1folge1")  << "Title" << "" << "Folge 1: 'The subtitle' Description"
                                << "Title" << "The subtitle" << "Description"
                                << "1" << ProgramInfo::kCategorySeries;
    QTest::newRow("st1folge2")  << "Title" << "" << "Folge 1: 'The subtitle' Description Wiederholung vom 25.12.2018"
                                << "Title" << "The subtitle" << "Description"
                                << "1" << ProgramInfo::kCategorySeries;
    QTest::newRow("st1folge4")  << "Title" << "" << "Folge 1234  :  'The subtitle'. Description"
                                << "Title" << "The subtitle" << "Description"
                                << "1234" << ProgramInfo::kCategorySeries;

    QTest::newRow("st2folge?")  << "Title" << "" << "Folge 1 abcde fghijklm? Description"
                                << "Title" << "abcde fghijklm" << "Description"
                                << "1" << ProgramInfo::kCategorySeries;
    QTest::newRow("st2folge!")  << "Title" << "" << "Folge 1234 abcdefgh ijklm!  Description"
                                << "Title" << "abcdefgh ijklm" << "Description"
                                << "1234" << ProgramInfo::kCategorySeries;
    QTest::newRow("st2folge.")  << "Title" << "" << "Folge 1234 abcdefghijklm.  Description"
                                << "Title" << "abcdefghijklm" << "Description"
                                << "1234" << ProgramInfo::kCategorySeries;

    QTest::newRow("st3folge?")  << "Title" << "" << "Folge 1/I abcde fghijklm? Description"
                                << "Title" << "abcde fghijklm" << "Description"
                                << "1/I" << ProgramInfo::kCategorySeries;
    QTest::newRow("st3folge!")  << "Title" << "" << "Folge 1234/XIV abcdefgh ijklm!  Description"
                                << "Title" << "abcdefgh ijklm" << "Description"
                                << "1234/XIV" << ProgramInfo::kCategorySeries;
    QTest::newRow("st3folge.")  << "Title" << "" << "Folge 1234/XXVII abcdefghijklm.  Description"
                                << "Title" << "abcdefghijklm" << "Description"
                                << "1234/XXVII" << ProgramInfo::kCategorySeries;

    QTest::newRow("st4thema1")  << "Title" << "" << "Thema: The subtitle. Description"
                                << "Title" << "The subtitle" << "Description"
                                << "" << ProgramInfo::kCategorySeries;
    QTest::newRow("st4thema2")  << "Title" << "" << "Themaaa: The subtitle.  Description"
                                << "Title" << "The subtitle" << "Description"
                                << "" << ProgramInfo::kCategorySeries;
    QTest::newRow("st4thema3")  << "Title" << "" << "Thema12345: The subtitle.     Description"
                                << "Title" << "The subtitle" << "Description"
                                << "" << ProgramInfo::kCategorySeries;

    QTest::newRow("st5")        << "Title" << "" << "'The subtitle'. Description"
                                << "Title" << "The subtitle" << "Description"
                                << "" << ProgramInfo::kCategorySeries;

    // Mistake in regexp m_rtlEpisodeNo1. There is no second capturing group.
    QTest::newRow("episode1a")  << "Title" << "" << "Folge 1 Description"
                                << "Title" << "Folge 1" << "Description"
                                << "" << ProgramInfo::kCategorySeries;
    QTest::newRow("episode1b")  << "Title" << "" << "Folge 1234 Desc"
                                << "Title" << "Folge 1234" << "Desc"
                                << "" << ProgramInfo::kCategorySeries;
    QTest::newRow("episode1c")  << "Title" << "" << "Folge 12345 Desc"
                                << "Title" << "Folge 1234" << "5 Desc"
                                << "" << ProgramInfo::kCategorySeries; // oops?

    // Mistake in regexp m_rtlEpisodeNo2. There is no second capturing group.
    QTest::newRow("episode2a")  << "Title" << "" << "1/VII Desc"
                                << "Title" << "1/VII" << "Desc"
                                << "" << ProgramInfo::kCategorySeries;
    QTest::newRow("episode2b")  << "Title" << "" << "23/XXVII Desc"
                                << "Title" << "23/XXVII" << "Desc"
                                << "" << ProgramInfo::kCategorySeries;
    QTest::newRow("episode2c")  << "Title" << "" << "23/XXVIIabc Desc"
                                << "Title" << "23/XXVII" << "abc Desc"
                                << "" << ProgramInfo::kCategorySeries; // oops?

    QString toolong("This looks like it should be a subtitle but the string is way to long to fit "
                    "into the 50 bytes that is marked as the upper limit for a subtitle. This "
                    "description is also very long so that it is the size check and not the "
                    "percentage check that is being triggered in the code.");
    QString badpercentage("This looks like a subtitle but it takes up more than 35% of the "
                          "description'. So it gets ignored.");

    QTest::newRow("guess1")     << "Title" << "" << "Abcdef. Description"
                                << "Title" << "Abcdef" << "Description"
                                << "" << ProgramInfo::kCategoryNone;
    QTest::newRow("guess2")     << "Title" << "" << toolong
                                << "Title" << "" << toolong
                                << "" << ProgramInfo::kCategoryNone;

    QTest::newRow("guess3")     << "Title" << "" << badpercentage
                                << "Title" << "" << badpercentage
                                << "" << ProgramInfo::kCategoryNone;

}

void TestEITFixups::testRTL()
{
    QFETCH(QString, title);
    QFETCH(QString, subtitle);
    QFETCH(QString, description);
    QFETCH(QString, e_title);
    QFETCH(QString, e_subtitle);
    QFETCH(QString, e_description);
    QFETCH(QString, e_syndicatedepisode);
    QFETCH(ProgramInfo::CategoryType, e_cattype);

    DBEventEIT event(7302, title, subtitle, description,
                     "", ProgramInfo::kCategoryNone,
                     QDateTime::fromString("2020-02-28T23:55:00Z", Qt::ISODate),
                     QDateTime::fromString("2020-03-01T02:00:00Z", Qt::ISODate),
                     EITFixUp::kFixRTL, SUB_UNKNOWN, AUD_UNKNOWN, VID_UNKNOWN, 0.0F,
                     "", "", 0, 0, 0);

    EITFixUp::Fix(event);
    PRINT_EVENT(event);
    QCOMPARE(event.m_title,           e_title);
    QCOMPARE(event.m_subtitle,        e_subtitle);
    QCOMPARE(event.m_description,     e_description);
    QCOMPARE(event.m_syndicatedepisodenumber, e_syndicatedepisode);
    QCOMPARE(event.m_categoryType, e_cattype);
}

//
// Finland
// Handle the data in the title and description fields.
//
void TestEITFixups::testFI_data()
{
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("subtitle");
    QTest::addColumn<QString>("description");
    QTest::addColumn<QString>("e_title");
    QTest::addColumn<QString>("e_subtitle");
    QTest::addColumn<QString>("e_description");
    QTest::addColumn<bool>("e_previouslyShown");
    QTest::addColumn<QString>("e_category");
    QTest::addColumn<ProgramInfo::CategoryType>("e_cattype");
    QTest::addColumn<int>("e_audioProps");
    QTest::addColumn<bool>("e_hasRatings"); // Because can store empty rating.
    QTest::addColumn<QString>("e_rating");

    QTest::newRow("base")    << "Title" << "Subtitle" << "Description."
                             << "Title" << "Subtitle" << "Description."
                             << false << "" << ProgramInfo::kCategoryNone
                             << (int)AUD_UNKNOWN << false << "";
    QTest::newRow("rerun1a") << "Title" << "Subtitle" << "Description. Uusinta"
                             << "Title" << "Subtitle" << "Description."
                             << true << "" << ProgramInfo::kCategoryNone
                             << (int)AUD_UNKNOWN << false << "";
    QTest::newRow("rerun1b") << "Title" << "Subtitle" << "Description. Uusintaqwe rty."
                             << "Title" << "Subtitle" << "Description."
                             << true << "" << ProgramInfo::kCategoryNone
                             << (int)AUD_UNKNOWN << false << "";
    QTest::newRow("rerun2a") << "Title" << "Subtitle" << "Description. (U)"
                             << "Title" << "Subtitle" << "Description."
                             << true << "" << ProgramInfo::kCategoryNone
                             << (int)AUD_UNKNOWN << false << "";
    QTest::newRow("rerun2b") << "Title" << "Subtitle" << "Description. (u)"
                             << "Title" << "Subtitle" << "Description."
                             << true << "" << ProgramInfo::kCategoryNone
                             << (int)AUD_UNKNOWN << false << "";

    QTest::newRow("stereo1") << "Title" << "Subtitle" << "Description. (Stereo)"
                             << "Title" << "Subtitle" << "Description."
                             << false << "" << ProgramInfo::kCategoryNone
                             << (int)AUD_STEREO << false << "";
    QTest::newRow("stereo2") << "Title" << "Subtitle" << "Description. Stereo"
                             << "Title" << "Subtitle" << "Description."
                             << false << "" << ProgramInfo::kCategoryNone
                             << (int)AUD_STEREO << false << "";
    QTest::newRow("stereo3") << "Title" << "Subtitle" << "Description. (Stereo) (U)"
                             << "Title" << "Subtitle" << "Description."
                             << true << "" << ProgramInfo::kCategoryNone
                             << (int)AUD_STEREO << false << "";

    QTest::newRow("rating1") << "Title (S)" << "Subtitle" << "Description. (u)"
                             << "Title" << "Subtitle" << "Description."
                             << true << "" << ProgramInfo::kCategoryNone
                             << (int)AUD_UNKNOWN << true << "S";
    QTest::newRow("rating2") << "Title (T)" << "Subtitle" << "Description. (u)"
                             << "Title" << "Subtitle" << "Description."
                             << true << "" << ProgramInfo::kCategoryNone
                             << (int)AUD_UNKNOWN << true << "T";
    QTest::newRow("rating3") << "Title (8)" << "Subtitle" << "Description. (u)"
                             << "Title" << "Subtitle" << "Description."
                             << true << "" << ProgramInfo::kCategoryNone
                             << (int)AUD_UNKNOWN << true << "8";
    QTest::newRow("rating4") << "Title (17)" << "Subtitle" << "Description. (u)"
                             << "Title" << "Subtitle" << "Description."
                             << true << "" << ProgramInfo::kCategoryNone
                             << (int)AUD_UNKNOWN << true << "17";
    QTest::newRow("rating5") << "Title (17)" << "Subtitle" << "Description. (u) Stereo"
                             << "Title" << "Subtitle" << "Description."
                             << true << "" << ProgramInfo::kCategoryNone
                             << (int)AUD_STEREO << true << "17";

    QTest::newRow("film1")   << "Film: Title" << "Subtitle" << "Description."
                             << "Title" << "Subtitle" << "Description."
                             << false << "Film" << ProgramInfo::kCategoryMovie
                             << (int)AUD_UNKNOWN << false << "";
    QTest::newRow("film2")   << "Elokuva: Title" << "Subtitle" << "Description."
                             << "Title" << "Subtitle" << "Description."
                             << false << "Film" << ProgramInfo::kCategoryMovie
                             << (int)AUD_UNKNOWN << false << "";
}

void TestEITFixups::testFI()
{
    QFETCH(QString, title);
    QFETCH(QString, subtitle);
    QFETCH(QString, description);
    QFETCH(QString, e_title);
    QFETCH(QString, e_subtitle);
    QFETCH(QString, e_description);
    QFETCH(bool, e_previouslyShown);
    QFETCH(QString, e_category);
    QFETCH(ProgramInfo::CategoryType, e_cattype);
    QFETCH(int, e_audioProps);
    QFETCH(bool, e_hasRatings);
    QFETCH(QString, e_rating);

    DBEventEIT event(7302, title, subtitle, description,
                     "", ProgramInfo::kCategoryNone,
                     QDateTime::fromString("2020-02-28T23:55:00Z", Qt::ISODate),
                     QDateTime::fromString("2020-03-01T02:00:00Z", Qt::ISODate),
                     EITFixUp::kFixFI, SUB_UNKNOWN, AUD_UNKNOWN, VID_UNKNOWN, 0.0F,
                     "", "", 0, 0, 0);

    EITFixUp::Fix(event);
    PRINT_EVENT(event);
    QCOMPARE(event.m_title,           e_title);
    QCOMPARE(event.m_subtitle,        e_subtitle);
    QCOMPARE(event.m_description,     e_description);
    QCOMPARE(event.m_previouslyshown, e_previouslyShown);
    QCOMPARE(event.m_category,        e_category);
    QCOMPARE(event.m_categoryType,    e_cattype);
    QCOMPARE(event.m_audioProps,      (uint8_t)e_audioProps);
    QCOMPARE(event.m_ratings.isEmpty(), !e_hasRatings);
    checkRating(event, "FI", e_rating);
}

//
// @Home DVB-C guide in the Netherlands
// Handle the data in the title, subtitle, and description fields.
//
// The first thing the fixup does is concatenate the subtitle and
// description. No need to pass separate fields here.
//
void TestEITFixups::testNL_data()
{
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("description");
    QTest::addColumn<QString>("category");
    QTest::addColumn<QString>("e_title");
    QTest::addColumn<QString>("e_subtitle");
    QTest::addColumn<QString>("e_description");
    QTest::addColumn<QString>("e_category");
    QTest::addColumn<ProgramInfo::CategoryType>("e_cattype");
    QTest::addColumn<int>("e_videoProps");
    QTest::addColumn<int>("e_audioProps");
    QTest::addColumn<int>("e_subtitleType");
    QTest::addColumn<QDate>("e_originalAirdate");
    QTest::addColumn<QStringList>("e_actors");
    QTest::addColumn<QStringList>("e_directors");
    QTest::addColumn<QStringList>("e_presenters");

    QTest::newRow("category1")  << "Title" << "Description." << "Documentary"
                                << "Title" << "" << "Description."
                                << "Documentaire" << ProgramInfo::kCategoryNone
                                << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN
                                << QDate() << QStringList() << QStringList() << QStringList();
    QTest::newRow("category2")  << "Title" << "Description." << "Sports"
                                << "Title" << "" << "Description."
                                << "Sport" << ProgramInfo::kCategorySports
                                << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN
                                << QDate() << QStringList() << QStringList() << QStringList();
    QTest::newRow("category3")  << "Title" << "Description." << "Film - ABCXYZ"
                                << "Title" << "" << "Description."
                                << "Film - ABCXYZ" << ProgramInfo::kCategorySeries
                                << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN
                                << QDate() << QStringList() << QStringList() << QStringList();
    QTest::newRow("stereo1")    << "Title" << "Description. (Stereo)" << "Documentary"
                                << "Title" << "" << "Description."
                                << "Documentaire" << ProgramInfo::kCategoryNone
                                << (int)VID_UNKNOWN << (int)AUD_STEREO << (int)SUB_UNKNOWN
                                << QDate() << QStringList() << QStringList() << QStringList();
    QTest::newRow("stereo2")    << "Title" << "Description. stereo" << "Documentary"
                                << "Title" << "" << "Description."
                                << "Documentaire" << ProgramInfo::kCategoryNone
                                << (int)VID_UNKNOWN << (int)AUD_STEREO << (int)SUB_UNKNOWN
                                << QDate() << QStringList() << QStringList() << QStringList();
    QTest::newRow("stripme")    << "Title" << "Description. Alpha beta breedbeeld gamma herh. Delta." << "Documentary"
                                << "Title" << "" << "Description. Alpha beta . gamma . Delta."
                                << "Documentaire" << ProgramInfo::kCategoryNone
                                << (int)VID_WIDESCREEN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN
                                << QDate() << QStringList() << QStringList() << QStringList();
    QTest::newRow("teletext")   << "Title" << "Description. Alpha txt Beta" << "Documentary"
                                << "Title" << "" << "Description. Alpha . Beta"
                                << "Documentaire" << ProgramInfo::kCategoryNone
                                << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_NORMAL
                                << QDate() << QStringList() << QStringList() << QStringList();
    QTest::newRow("hd")         << "Title HD" << "Description." << "Documentary"
                                << "Title" << "" << "Description."
                                << "Documentaire" << ProgramInfo::kCategoryNone
                                << (int)VID_HDTV << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN
                                << QDate() << QStringList() << QStringList() << QStringList();
    QTest::newRow("subtitle1")  << "Title" << "Description. Afl.: Subtitle words here. More Description." << "Documentary"
                                << "Title" << "Subtitle words here" << "Description. More Description."
                                << "Documentaire" << ProgramInfo::kCategoryNone
                                << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN
                                << QDate() << QStringList() << QStringList() << QStringList();
    QTest::newRow("subtitle2")  << "Title" << "Description. \"Subtitle words here.\" More Description." << "Documentary"
                                << "Title" << "Subtitle words here." << "Description. More Description."
                                << "Documentaire" << ProgramInfo::kCategoryNone
                                << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN
                                << QDate() << QStringList() << QStringList() << QStringList();
    QTest::newRow("subtitle3")  << "Title:Subtitle" << "Description. More Description." << "Documentary"
                                << "Title" << "Subtitle" << "Description. More Description."
                                << "Documentaire" << ProgramInfo::kCategoryNone
                                << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN
                                << QDate() << QStringList() << QStringList() << QStringList();
    // Negative test
    QTest::newRow("subtitle4")  << "Title:subtitle" << "Description. More Description." << "Documentary"
                                << "Title:subtitle" << "" << "Description. More Description."
                                << "Documentaire" << ProgramInfo::kCategoryNone
                                << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN
                                << QDate() << QStringList() << QStringList() << QStringList();
    // First subtitle found wins.
    QTest::newRow("subtitle5")  << "Title:Subtitle" << "Description. \"Subtitle words here.\" More Description." << "Documentary"
                                << "Title:Subtitle" << "Subtitle words here." << "Description. More Description."
                                << "Documentaire" << ProgramInfo::kCategoryNone
                                << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN
                                << QDate() << QStringList() << QStringList() << QStringList();
    QTest::newRow("actors")     << "Title:Subtitle"
                                << "Description. Met: Larry, Moe en Curly e.a. Presentatie: Stephen Colbert, Jimmy Fallon en Jimmy Kimmell. More Description."
                                << "Documentary"
                                << "Title" << "Subtitle" << "Description. More Description."
                                << "Documentaire" << ProgramInfo::kCategoryNone
                                << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN
                                << QDate() << QStringList({"Larry", "Moe", "Curly"})
                                << QStringList()
                                << QStringList({"Stephen Colbert", "Jimmy Fallon", "Jimmy Kimmell"});
    // Doesn't strip director name from text.
    QTest::newRow("director")   << "Title:Subtitle"
                                << "Description. Blah blah van Levinson blah."
                                << "Documentary"
                                << "Title" << "Subtitle" << "Description. Blah blah van Levinson blah."
                                << "Documentaire" << ProgramInfo::kCategoryNone
                                << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN
                                << QDate() << QStringList() << QStringList("Levinson") << QStringList();
    // Needs to be fixed to match anything 19xx or 20xx
    QTest::newRow("year1")      << "Title:Subtitle" << "Description. uit 2021" << "Documentary"
                                << "Title" << "Subtitle" << "Description. uit 2021"
                                << "Documentaire" << ProgramInfo::kCategoryNone
                                << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN
                                << QDate(2021,1,1) << QStringList() << QStringList() << QStringList();
    QTest::newRow("year2a")     << "Title:Subtitle" << "Description. (2021)" << "Documentary"
                                << "Title" << "Subtitle" << "Description. (2021)"
                                << "Documentaire" << ProgramInfo::kCategoryNone
                                << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN
                                << QDate(2021,1,1) << QStringList() << QStringList() << QStringList();
    QTest::newRow("year2b")     << "Title:Subtitle" << "Description. (FEB/2021)" << "Documentary"
                                << "Title" << "Subtitle" << "Description. (FEB/2021)"
                                << "Documentaire" << ProgramInfo::kCategoryNone
                                << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN
                                << QDate(2021,1,1) << QStringList() << QStringList() << QStringList();
    QTest::newRow("year2c")     << "Title:Subtitle" << "Description. (Feb/2021)" << "Documentary"
                                << "Title" << "Subtitle" << "Description. (Feb/2021)"
                                << "Documentaire" << ProgramInfo::kCategoryNone
                                << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN
                                << QDate(2021,1,1) << QStringList() << QStringList() << QStringList();
    QTest::newRow("rub")        << "Title" << "Description. (@@) foo" << "Documentary"
                                << "Title" << "" << "Description.foo"
                                << "Documentaire" << ProgramInfo::kCategoryNone
                                << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN
                                << QDate() << QStringList() << QStringList() << QStringList();
    QTest::newRow("category1") << "Title" << "Amusement. Description." << "Documentary"
                                << "Title" << "" << "Description."
                                << "Documentaire" << ProgramInfo::kCategoryNone
                                << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN
                                << QDate() << QStringList() << QStringList() << QStringList();
    QTest::newRow("category2") << "Title" << "Nieuws/actualiteiten. Description." << "Documentary"
                                << "Title" << "" << "Description."
                                << "Documentaire" << ProgramInfo::kCategoryNone
                                << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN
                                << QDate() << QStringList() << QStringList() << QStringList();
    QTest::newRow("omreop1")     << "Title (AAA)" << "Description." << "Documentary"
                                << "Title" << "" << "Description."
                                << "Documentaire" << ProgramInfo::kCategoryNone
                                << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN
                                << QDate() << QStringList() << QStringList() << QStringList();
    QTest::newRow("omreop2")     << "Title (AAA/BB/)" << "Description." << "Documentary"
                                << "Title" << "" << "Description."
                                << "Documentaire" << ProgramInfo::kCategoryNone
                                << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN
                                << QDate() << QStringList() << QStringList() << QStringList();
}

void TestEITFixups::testNL()
{
    QFETCH(QString, title);
    QFETCH(QString, category);
    QFETCH(QString, description);
    QFETCH(QString, e_title);
    QFETCH(QString, e_subtitle);
    QFETCH(QString, e_description);
    QFETCH(QString, e_category);
    QFETCH(ProgramInfo::CategoryType, e_cattype);
    QFETCH(int, e_videoProps);
    QFETCH(int, e_audioProps);
    QFETCH(int, e_subtitleType);
    QFETCH(QDate, e_originalAirdate);
    QFETCH(QStringList, e_actors);
    QFETCH(QStringList, e_directors);
    QFETCH(QStringList, e_presenters);

    DBEventEIT event(7302, title, "", description,
                     category, ProgramInfo::kCategoryNone,
                     QDateTime::fromString("2020-02-28T23:55:00Z", Qt::ISODate),
                     QDateTime::fromString("2020-03-01T02:00:00Z", Qt::ISODate),
                     EITFixUp::kFixNL, SUB_UNKNOWN, AUD_UNKNOWN, VID_UNKNOWN, 0.0F,
                     "", "", 0, 0, 0);

    PRINT_EVENT(event);
    EITFixUp::Fix(event);
    PRINT_EVENT(event);
    QCOMPARE(event.m_title,           e_title);
    QCOMPARE(event.m_subtitle,        e_subtitle);
    QCOMPARE(event.m_description,     e_description);

    QCOMPARE(event.m_category,        e_category);
    QCOMPARE(event.m_categoryType,    e_cattype);

    QCOMPARE(event.m_videoProps,      (uint8_t)e_videoProps);
    QCOMPARE(event.m_audioProps,      (uint8_t)e_audioProps);
    QCOMPARE(event.m_subtitleType,    (uint8_t)e_subtitleType);
    QCOMPARE(event.m_originalairdate, e_originalAirdate);
    checkCast(event, e_actors, e_directors, QStringList(), e_presenters);
}

//
// Category Fixup
//
void TestEITFixups::testCategory_data()
{
    QTest::addColumn<QString>("startTime");
    QTest::addColumn<QString>("endTime");
    QTest::addColumn<ProgramInfo::CategoryType>("e_cattype");

    QTest::newRow("short") << "2020-02-28T12:00:00Z" << "2020-02-28T13:00:00Z"
                           << ProgramInfo::kCategoryTVShow;
    QTest::newRow("valid") << "2020-02-28T12:00:00Z" << "2020-02-28T14:00:00Z"
                           << ProgramInfo::kCategoryMovie;
}

void TestEITFixups::testCategory()
{
    QFETCH(QString, startTime);
    QFETCH(QString, endTime);
    QFETCH(ProgramInfo::CategoryType, e_cattype);

    DBEventEIT event(7302, "Title", "Subtitle", "Description",
                     "", ProgramInfo::kCategoryMovie,
                     QDateTime::fromString(startTime, Qt::ISODate),
                     QDateTime::fromString(endTime, Qt::ISODate),
                     EITFixUp::kFixCategory, SUB_UNKNOWN, AUD_UNKNOWN, VID_UNKNOWN, 0.0F,
                     "", "", 0, 0, 0);

    EITFixUp::Fix(event);
    PRINT_EVENT(event);
    QCOMPARE(event.m_categoryType,    e_cattype);
}

//
// Norway DVB-S guide
// Handle the data in the title, subtitle, and description fields.
//
void TestEITFixups::testNO_data()
{
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("subtitle");
    QTest::addColumn<QString>("description");
    QTest::addColumn<QString>("e_title");
    QTest::addColumn<QString>("e_subtitle");
    QTest::addColumn<QString>("e_description");
    QTest::addColumn<bool>("e_previouslyShown");
    QTest::addColumn<int>("e_videoProps");

    QTest::newRow("rerun")  << "Title (R)" << "Subtitle" << "Description."
                            << "Title" << "Subtitle" << "Description."
                            << true << (int)VID_UNKNOWN;
    QTest::newRow("hd1")    << "Title" << "Subtitle (HD)" << "Description."
                            << "Title" << "Subtitle" << "Description."
                            << false << (int)VID_HDTV;
    QTest::newRow("hd2")    << "Title" << "Subtitle" << "Description. (HD)"
                            << "Title" << "Subtitle" << "Description."
                            << false << (int)VID_HDTV;
}

void TestEITFixups::testNO()
{
    QFETCH(QString, title);
    QFETCH(QString, subtitle);
    QFETCH(QString, description);
    QFETCH(QString, e_title);
    QFETCH(QString, e_subtitle);
    QFETCH(QString, e_description);
    QFETCH(bool, e_previouslyShown);
    QFETCH(int, e_videoProps);

    DBEventEIT event(7302, title, subtitle, description,
                     "", ProgramInfo::kCategoryNone,
                     QDateTime::fromString("2020-02-28T23:55:00Z", Qt::ISODate),
                     QDateTime::fromString("2020-03-01T02:00:00Z", Qt::ISODate),
                     EITFixUp::kFixNO, SUB_UNKNOWN, AUD_UNKNOWN, VID_UNKNOWN, 0.0F,
                     "", "", 0, 0, 0);

    EITFixUp::Fix(event);
    PRINT_EVENT(event);
    QCOMPARE(event.m_title,           e_title);
    QCOMPARE(event.m_subtitle,        e_subtitle);
    QCOMPARE(event.m_description,     e_description);
    QCOMPARE(event.m_previouslyshown, e_previouslyShown);
    QCOMPARE(event.m_videoProps,      (uint8_t)e_videoProps);
}

//
// Norway DVB-T NRK
// Handle the data in the title, subtitle, and description fields.
//
void TestEITFixups::testNRK_data()
{
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("subtitle");
    QTest::addColumn<QString>("description");
    QTest::addColumn<QString>("e_title");
    QTest::addColumn<QString>("e_subtitle");
    QTest::addColumn<QString>("e_description");
    QTest::addColumn<bool>("e_previouslyShown");

    QTest::newRow("rerun1")    << "Title (R)" << "Subtitle" << "Description."
                               << "Title" << "Subtitle" << "Description."
                               << true;
    QTest::newRow("rerun2")    << "Title" << "Subtitle" << "Description. (R)"
                               << "Title" << "Subtitle" << "Description. (R)"
                               << true;
    QTest::newRow("category" ) << "Detektimen: Title" << "Subtitle" << "Description."
                               << "Title" << "Subtitle" << "(Detektimen) Description."
                               << false;
    QTest::newRow("premiere1") << "Title - Sesongpremiere" << "Subtitle" << "Description."
                               << "Title" << "Subtitle" << "Description."
                               << false;
    QTest::newRow("premiere2") << "Title - Premiere!" << "Subtitle" << "Description."
                               << "Title" << "Subtitle" << "Description."
                               << false;
    QTest::newRow("premiere3") << "Title - premiere!" << "Subtitle" << "Description."
                               << "Title" << "Subtitle" << "Description."
                               << false;
    QTest::newRow("subtitle1") << "CSI: New York" << "Subtitle" << "Description."
                               << "CSI: New York" << "Subtitle" << "Description."
                               << false;
    QTest::newRow("subtitle2") << "Distriktsnyheter: fra New York" << "Subtitle" << "Description."
                               << "Distriktsnyheter: fra New York" << "Subtitle" << "Description."
                               << false;
    QTest::newRow("subtitle3") << "Title: Subtitle" << "" << "Description."
                               << "Title" << "Subtitle" << "Description."
                               << false;
    QTest::newRow("subtitle4") << "Title: Subtitle" << "Subtitle" << "Description."
                               << "Title" << "Subtitle" << "Description."
                               << false;
    QTest::newRow("subtitle4") << "Title: Subtitle2" << "Subtitle" << "Description."
                               << "Title: Subtitle2" << "Subtitle" << "Description."
                               << false;
}

void TestEITFixups::testNRK()
{
    QFETCH(QString, title);
    QFETCH(QString, subtitle);
    QFETCH(QString, description);
    QFETCH(QString, e_title);
    QFETCH(QString, e_subtitle);
    QFETCH(QString, e_description);
    QFETCH(bool, e_previouslyShown);

    DBEventEIT event(7302, title, subtitle, description,
                     "", ProgramInfo::kCategoryNone,
                     QDateTime::fromString("2020-02-28T23:55:00Z", Qt::ISODate),
                     QDateTime::fromString("2020-03-01T02:00:00Z", Qt::ISODate),
                     EITFixUp::kFixNRK_DVBT, SUB_UNKNOWN, AUD_UNKNOWN, VID_UNKNOWN, 0.0F,
                     "", "", 0, 0, 0);

    EITFixUp::Fix(event);
    PRINT_EVENT(event);
    QCOMPARE(event.m_title,           e_title);
    QCOMPARE(event.m_subtitle,        e_subtitle);
    QCOMPARE(event.m_description,     e_description);
    QCOMPARE(event.m_previouslyshown, e_previouslyShown);
}

//
// YouSee's DVB-C guide in Denmark
//
void TestEITFixups::testDK_data()
{
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("subtitle");
    QTest::addColumn<QString>("description");
    QTest::addColumn<QString>("e_title");
    QTest::addColumn<QString>("e_subtitle");
    QTest::addColumn<QString>("e_description");

    QTest::addColumn<int>("e_season");
    QTest::addColumn<int>("e_episode");
    QTest::addColumn<int>("e_partnumber");
    QTest::addColumn<int>("e_parttotal");
    QTest::addColumn<bool>("e_previouslyShown");
    QTest::addColumn<QDate>("e_originalAirdate");

    QTest::addColumn<int>("e_videoProps");
    QTest::addColumn<int>("e_audioProps");
    QTest::addColumn<int>("e_subtitleType");

    QTest::newRow("episode")   << "Title (42)" << "Subtitle" << "Description. fra 1984 ."
                               << "Title" << "Subtitle (42)" << "Description. fra 1984 ."
                               << 0 << 42 << 42 << 0 << false << QDate(1984,1,1)
                               << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN;
    QTest::newRow("part")      << "Title (42:49)" << "Subtitle" << "Description."
                               << "Title" << "Subtitle (42:49)" << "Description."
                               << 0 << 42 << 42 << 49 << false << QDate()
                               << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN;
    QTest::newRow("subtitle1") << "Title : Subtitle" << "Overwritten" << "Description."
                               << "Title" << "Subtitle" << "Description."
                               << 0 << 0 << 0 << 0 << false << QDate()
                               << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN;
    QTest::newRow("subtitle2") << "Title - Subtitle" << "Overwritten" << "Description."
                               << "Title" << "Subtitle" << "Description."
                               << 0 << 0 << 0 << 0 << false << QDate()
                               << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN;
    QTest::newRow("subtitle3") << "Title - Subtitle (42:49)" << "Overwritten" << "Description."
                               << "Title" << "Subtitle (42:49)" << "Description."
                               << 0 << 42 << 42 << 49 << false << QDate()
                               << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN;
    QTest::newRow("season1")   << "Title (42)" << "Subtitle" << "Description. Sæson 2."
                               << "Title" << "Subtitle (42 Sæson 2)" << "Description. Sæson 2."
                               << 2 << 42 << 42 << 0 << false << QDate()
                               << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN;
    QTest::newRow("season2")   << "Title (42)" << "Subtitle" << "Description. - år 2 : blah"
                               << "Title" << "Subtitle (42 Sæson 2)" << "Description. - år 2 : blah"
                               << 2 << 42 << 42 << 0 << false << QDate()
                               << (int)VID_UNKNOWN << (int)AUD_UNKNOWN << (int)SUB_UNKNOWN;
    QTest::newRow("features1") << "Title" << "Subtitle" << "Description. Features: 16:9 5:1 (G)"
                               << "Title" << "Subtitle" << "Description."
                               << 0 << 0 << 0 << 0 << true << QDate()
                               << (int)VID_WIDESCREEN << (int)AUD_DOLBY << (int)SUB_UNKNOWN;
    QTest::newRow("features2") << "Title" << "Subtitle" << "Description. Features: HD ((S)) TTV"
                               << "Title" << "Subtitle" << "Description."
                               << 0 << 0 << 0 << 0 << false << QDate()
                               << (int)VID_HDTV << (int)AUD_SURROUND << (int)SUB_NORMAL;
    QTest::newRow("features3") << "Title" << "Subtitle" << "Description. Features: S"
                               << "Title" << "Subtitle" << "Description."
                               << 0 << 0 << 0 << 0 << false << QDate()
                               << (int)VID_UNKNOWN << (int)AUD_STEREO << (int)SUB_UNKNOWN;
}

void TestEITFixups::testDK()
{
    QFETCH(QString, title);
    QFETCH(QString, subtitle);
    QFETCH(QString, description);
    QFETCH(QString, e_title);
    QFETCH(QString, e_subtitle);
    QFETCH(QString, e_description);
    QFETCH(int, e_season);
    QFETCH(int, e_episode);
    QFETCH(int, e_partnumber);
    QFETCH(int, e_parttotal);
    QFETCH(bool, e_previouslyShown);
    QFETCH(int, e_videoProps);
    QFETCH(int, e_audioProps);
    QFETCH(int, e_subtitleType);
    QFETCH(QDate, e_originalAirdate);

    DBEventEIT event(7302, title, subtitle, description,
                     "", ProgramInfo::kCategoryNone,
                     QDateTime::fromString("2020-02-28T23:55:00Z", Qt::ISODate),
                     QDateTime::fromString("2020-03-01T02:00:00Z", Qt::ISODate),
                     EITFixUp::kFixDK, SUB_UNKNOWN, AUD_UNKNOWN, VID_UNKNOWN, 0.0F,
                     "", "", 0, 0, 0);

    EITFixUp::Fix(event);
    PRINT_EVENT(event);
    QCOMPARE(event.m_title,           e_title);
    QCOMPARE(event.m_subtitle,        e_subtitle);
    QCOMPARE(event.m_description,     e_description);

    QCOMPARE(event.m_season,          (uint)e_season);
    QCOMPARE(event.m_episode,         (uint)e_episode);
    QCOMPARE(event.m_partnumber,      (uint16_t)e_partnumber);
    QCOMPARE(event.m_parttotal,       (uint16_t)e_parttotal);

    QCOMPARE(event.m_previouslyshown, e_previouslyShown);
    QCOMPARE(event.m_videoProps,      (uint8_t)e_videoProps);
    QCOMPARE(event.m_audioProps,      (uint8_t)e_audioProps);
    QCOMPARE(event.m_subtitleType,    (uint8_t)e_subtitleType);
    QCOMPARE(event.m_originalairdate, e_originalAirdate);
}

void TestEITFixups::testDK2_data()
{
    QTest::addColumn<QString>("description");
    QTest::addColumn<QString>("seriesID");
    QTest::addColumn<QString>("programID");

    QTest::addColumn<QString>("e_description");
    QTest::addColumn<QString>("e_seriesID");
    QTest::addColumn<QString>("e_programID");
    QTest::addColumn<QStringList>("e_actors");
    QTest::addColumn<QStringList>("e_directors");

    QTest::newRow("series0")   << "Description." << "123456789" << "123"
                               << "Description." << "123456789" << "123"
                               << QStringList() << QStringList();
    QTest::newRow("series1")   << "Description." << "/123456789" << "/123"
                               << "Description." << "730223456789" << "_123"
                               << QStringList() << QStringList();
    QTest::newRow("series2")   << "Description." << "/223456789" << ""
                               << "Description." << "23456789" << ""
                               << QStringList() << QStringList();
    QTest::newRow("director1") << "Description. Instrx: Barry Levinson, Steven Spielberg" << "" << ""
                               << "Description. Instrx: Barry Levinson, Steven Spielberg" << "" << ""
                               << QStringList() << QStringList({"Barry Levinson", "Steven Spielberg"});
    QTest::newRow("director2") << "Description. Instruktor: Barry Levinson og Steven Spielberg" << "" << ""
                               << "Description. Instruktor: Barry Levinson og Steven Spielberg" << "" << ""
                               << QStringList() << QStringList({"Barry Levinson", "Steven Spielberg"});
    QTest::newRow("actor1")    << "Description. Medvirkende: Larry, Moe og Curly" << "" << ""
                               << "Description. Medvirkende: Larry, Moe og Curly" << "" << ""
                               << QStringList({"Larry", "Moe", "Curly"}) << QStringList();
    QTest::newRow("actor2")    << "Description. Medv.: Larry, Moe og Curly" << "" << ""
                               << "Description. Medv.: Larry, Moe og Curly" << "" << ""
                               << QStringList({"Larry", "Moe", "Curly"}) << QStringList();
    QTest::newRow("actor3")    << "Description. Medv.: Larry, Moe, Curly, Barry Levinson, Steven Spielberg. Instruktor: Barry Levinson, Steven Spielberg" << "" << ""
                               << "Description. Medv.: Larry, Moe, Curly, Barry Levinson, Steven Spielberg. Instruktor: Barry Levinson, Steven Spielberg" << "" << ""
                               << QStringList({"Larry", "Moe", "Curly"}) << QStringList({"Barry Levinson", "Steven Spielberg"});
}

void TestEITFixups::testDK2()
{
    QFETCH(QString, description);
    QFETCH(QString, seriesID);
    QFETCH(QString, programID);
    QFETCH(QString, e_description);
    QFETCH(QString, e_seriesID);
    QFETCH(QString, e_programID);
    QFETCH(QStringList, e_actors);
    QFETCH(QStringList, e_directors);

    DBEventEIT event(7302, "Title", "Subtitle", description,
                     "", ProgramInfo::kCategoryNone,
                     QDateTime::fromString("2020-02-28T23:55:00Z", Qt::ISODate),
                     QDateTime::fromString("2020-03-01T02:00:00Z", Qt::ISODate),
                     EITFixUp::kFixDK, SUB_UNKNOWN, AUD_UNKNOWN, VID_UNKNOWN, 0.0F,
                     seriesID, programID, 0, 0, 0);

    EITFixUp::Fix(event);
    PRINT_EVENT(event);
    QCOMPARE(event.m_description,     e_description);
    QCOMPARE(event.m_seriesId,        e_seriesID);
    QCOMPARE(event.m_programId,       e_programID);
    checkCast(event, e_actors, e_directors);
}

//
// Greek Subtitle
// Handle the data in the title, subtitle, and description fields.
//
void TestEITFixups::testGreekSubtitle_data()
{
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("subtitle");
    QTest::addColumn<QString>("description");
    QTest::addColumn<QString>("e_title");
    QTest::addColumn<QString>("e_subtitle");
    QTest::addColumn<QString>("e_description");

    QTest::newRow("base")      << "Title" << "Subtitle" << "Description."
                               << "Title" << "" << "Subtitle. Description.";
    QTest::newRow("dupdescr")  << "Title" << "Subtitle." << "Title"
                               << "Title" << "" << "Subtitle..";
}

void TestEITFixups::testGreekSubtitle()
{
    QFETCH(QString, title);
    QFETCH(QString, subtitle);
    QFETCH(QString, description);
    QFETCH(QString, e_title);
    QFETCH(QString, e_subtitle);
    QFETCH(QString, e_description);

    DBEventEIT event(7302, title, subtitle, description,
                     "", ProgramInfo::kCategoryNone,
                     QDateTime::fromString("2020-02-28T23:55:00Z", Qt::ISODate),
                     QDateTime::fromString("2020-03-01T02:00:00Z", Qt::ISODate),
                     EITFixUp::kFixGreekSubtitle, SUB_UNKNOWN, AUD_UNKNOWN, VID_UNKNOWN, 0.0F,
                     "", "", 0, 0, 0);

    EITFixUp::Fix(event);
    PRINT_EVENT(event);
    QCOMPARE(event.m_title,           e_title);
    QCOMPARE(event.m_subtitle,        e_subtitle);
    QCOMPARE(event.m_description,     e_description);
}

//
// Greek
// Handle the data in the title, subtitle, and description fields.
//
void TestEITFixups::testGreek_data()
{
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("subtitle");
    QTest::addColumn<QString>("description");
    QTest::addColumn<bool>("previouslyShown");
    QTest::addColumn<QString>("e_title");
    QTest::addColumn<QString>("e_subtitle");
    QTest::addColumn<QString>("e_description");
    QTest::addColumn<bool>("e_previouslyShown");
    QTest::addColumn<QString>("e_rating");
    QTest::addColumn<int>("e_videoProps");
    QTest::addColumn<QStringList>("e_actors");
    QTest::addColumn<QStringList>("e_directors");
    QTest::addColumn<QStringList>("e_presenters");

    QTest::newRow("base")      << "Title" << "Subtitle" << "Description" << false
                               << "Title" << "Subtitle" << "Description" << false
                               << "" << (int)VID_UNKNOWN
                               << QStringList() << QStringList() << QStringList();
    QTest::newRow("live")      << "Title (Ζ)" << "Subtitle" << "Description" << false
                               << "Title" << "Subtitle" << "Ζωντανή Μετάδοση. Description" << false
                               << "" << (int)VID_UNKNOWN
                               << QStringList() << QStringList() << QStringList();
    QTest::newRow("rating1")   << "Title [k]" << "Subtitle" << "Description" << false
                               << "Title" << "Subtitle" << "Description" << false
                               << "k" << (int)VID_UNKNOWN
                               << QStringList() << QStringList() << QStringList();
    QTest::newRow("rating2")   << "Title [Κ]" << "Subtitle" << "Description" << false
                               << "Title" << "Subtitle" << "Description" << false
                               << "Κ" << (int)VID_UNKNOWN
                               << QStringList() << QStringList() << QStringList();
    QTest::newRow("rating3")   << "Title [18]" << "Subtitle" << "Description" << false
                               << "Title" << "Subtitle" << "Description" << false
                               << "18" << (int)VID_UNKNOWN
                               << QStringList() << QStringList() << QStringList();
    QTest::newRow("notprev1")  << "Title - Αη τηλεοπτικη μεταδοση" << "Subtitle" << "Description" << true
                               << "Title" << "Subtitle" << "Description" << false
                               << "" << (int)VID_UNKNOWN
                               << QStringList() << QStringList() << QStringList();
    QTest::newRow("notprev2")  << "Title - 1η τηλεοπτική μετάδοση" << "Subtitle" << "Description" << true
                               << "Title" << "Subtitle" << "Description" << false
                               << "" << (int)VID_UNKNOWN
                               << QStringList() << QStringList() << QStringList();
    QTest::newRow("notprev3")  << "Title - αη τηλεοπτική προβολη" << "Subtitle" << "Description" << true
                               << "Title" << "Subtitle" << "Description" << false
                               << "" << (int)VID_UNKNOWN
                               << QStringList() << QStringList() << QStringList();
    QTest::newRow("notprev4")  << "Title - 1η τηλεοπτική προβολή" << "Subtitle" << "Description" << true
                               << "Title" << "Subtitle" << "Description" << false
                               << "" << (int)VID_UNKNOWN
                               << QStringList() << QStringList() << QStringList();
    QTest::newRow("replay1")   << "Title (Ε)" << "Subtitle" << "Description" << false
                               << "Title" << "Subtitle" << "Description" << true
                               << "" << (int)VID_UNKNOWN
                               << QStringList() << QStringList() << QStringList();
    QTest::newRow("replay2")   << "Title (E)" << "Subtitle" << "Description" << false
                               << "Title" << "Subtitle" << "Description" << true
                               << "" << (int)VID_UNKNOWN
                               << QStringList() << QStringList() << QStringList();
    QTest::newRow("hd")        << "Title" << "Subtitle" << "Description (HD)" << false
                               << "Title" << "Subtitle" << "Description" << false
                               << "" << (int)VID_HDTV
                               << QStringList() << QStringList() << QStringList();
    QTest::newRow("fullhd")    << "Title" << "Subtitle" << "Description (Full HD)" << false
                               << "Title" << "Subtitle" << "Description" << false
                               << "" << (int)VID_HDTV
                               << QStringList() << QStringList() << QStringList();
    QTest::newRow("Actors1")   << "Title" << "Subtitle" << "Description Παίζουν: Larry" << false
                               << "Title" << "Subtitle" << "Description." << false
                               << "" << (int)VID_UNKNOWN
                               << QStringList("Larry") << QStringList() << QStringList();
    QTest::newRow("Actors2")   << "Title" << "Subtitle" << "Description Πρωταγωνιστουν: Larry, Moe-  Curly" << false
                               << "Title" << "Subtitle" << "Description." << false
                               << "" << (int)VID_UNKNOWN
                               << QStringList({"Larry", "Moe", "Curly"}) << QStringList() << QStringList();
    QTest::newRow("Director1") << "Title" << "Subtitle" << "Description Σκηνοθεσία: Barry Levinson" << false
                               << "Title" << "Subtitle" << "Description." << false
                               << "" << (int)VID_UNKNOWN
                               << QStringList() << QStringList("Barry Levinson") << QStringList();
    QTest::newRow("Director2") << "Title" << "Subtitle" << "Description Σκηνοθέτης: Barry Levinson." << false
                               << "Title" << "Subtitle" << "Description." << false
                               << "" << (int)VID_UNKNOWN
                               << QStringList() << QStringList({"Barry Levinson"}) << QStringList();
    QTest::newRow("Director3") << "Title" << "Subtitle" << "Description Σκηνοθέτης - Επιμέλεια: Barry Levinson" << false
                               << "Title" << "Subtitle" << "Description." << false
                               << "" << (int)VID_UNKNOWN
                               << QStringList() << QStringList("Barry Levinson") << QStringList();
    QTest::newRow("Presenter1") << "Title" << "Subtitle" << "Description. Παρουσιαση: Stephen Colbert." << false
                                << "Title" << "Subtitle" << "Description." << false
                                << "" << (int)VID_UNKNOWN
                                << QStringList() << QStringList() << QStringList("Stephen Colbert");
    QTest::newRow("Presenter2") << "Title" << "Subtitle" << "Description. Παρουσιαζουν: Stephen Colbert." << false
                                << "Title" << "Subtitle" << "Description." << false
                                << "" << (int)VID_UNKNOWN
                                << QStringList() << QStringList() << QStringList("Stephen Colbert");
    QTest::newRow("Presenter3") << "Title" << "Subtitle" << "Description. Παρουσιάζει ο Stephen Colbert" << false
                                << "Title" << "Subtitle" << "Description." << false
                                << "" << (int)VID_UNKNOWN
                                << QStringList() << QStringList() << QStringList("Stephen Colbert");
    QTest::newRow("Presenter4") << "Title" << "Subtitle" << "Description. Παρουσιάζει η Stephen Colbert" << false
                                << "Title" << "Subtitle" << "Description." << false
                                << "" << (int)VID_UNKNOWN
                                << QStringList() << QStringList() << QStringList("Stephen Colbert");
    QTest::newRow("Presenter5") << "Title" << "Subtitle" << "Description. Παρουσιαστης: Stephen Colbert" << false
                                << "Title" << "Subtitle" << "Description." << false
                                << "" << (int)VID_UNKNOWN
                                << QStringList() << QStringList() << QStringList("Stephen Colbert");
    QTest::newRow("Presenter6") << "Title" << "Subtitle" << "Description. Παρουσιάστρια ο Stephen Colbert" << false
                                << "Title" << "Subtitle" << "Description." << false
                                << "" << (int)VID_UNKNOWN
                                << QStringList() << QStringList() << QStringList("Stephen Colbert");
    QTest::newRow("Presenter7") << "Title" << "Subtitle" << "Description. Παρουσιαστριες η Stephen Colbert" << false
                                << "Title" << "Subtitle" << "Description." << false
                                << "" << (int)VID_UNKNOWN
                                << QStringList() << QStringList() << QStringList("Stephen Colbert");
    QTest::newRow("Presenter8") << "Title" << "Subtitle" << "Description. Παρουσιάστές: Stephen Colbert" << false
                                << "Title" << "Subtitle" << "Description." << false
                                << "" << (int)VID_UNKNOWN
                                << QStringList() << QStringList() << QStringList("Stephen Colbert");
    QTest::newRow("Presenter9") << "Title" << "Subtitle" << "Description. Με τον : Larry, Moe, Curly" << false
                                << "Title" << "Subtitle" << "Description." << false
                                << "" << (int)VID_UNKNOWN
                                << QStringList() << QStringList() << QStringList({"Larry", "Moe", "Curly"});
    QTest::newRow("Presenter10") << "Title" << "Subtitle" << "Description. Με την : Stephen Colbert, Sting" << false
                                 << "Title" << "Subtitle" << "Description." << false
                                 << "" << (int)VID_UNKNOWN
                                 << QStringList() << QStringList() << QStringList({"Stephen Colbert", "Sting"});
}

void TestEITFixups::testGreek()
{
    QFETCH(QString, title);
    QFETCH(QString, subtitle);
    QFETCH(QString, description);
    QFETCH(bool,    previouslyShown);
    QFETCH(QString, e_title);
    QFETCH(QString, e_subtitle);
    QFETCH(QString, e_description);
    QFETCH(QString, e_rating);
    QFETCH(bool,    e_previouslyShown);
    QFETCH(int,     e_videoProps);
    QFETCH(QStringList, e_actors);
    QFETCH(QStringList, e_directors);
    QFETCH(QStringList, e_presenters);

    DBEventEIT event(7302, title, subtitle, description,
                     "", ProgramInfo::kCategoryNone,
                     QDateTime::fromString("2020-02-28T23:55:00Z", Qt::ISODate),
                     QDateTime::fromString("2020-03-01T02:00:00Z", Qt::ISODate),
                     EITFixUp::kFixGreekEIT, SUB_UNKNOWN, AUD_UNKNOWN, VID_UNKNOWN, 0.0F,
                     "", "", 0, 0, 0);
    event.m_previouslyshown = previouslyShown;

    EITFixUp::Fix(event);
    PRINT_EVENT(event);
    QCOMPARE(event.m_title,           e_title);
    QCOMPARE(event.m_subtitle,        e_subtitle);
    QCOMPARE(event.m_description,     e_description);
    QCOMPARE(event.m_previouslyshown, e_previouslyShown);
    QCOMPARE(event.m_videoProps,      (uint8_t)e_videoProps);
    checkRating(event, "GR", e_rating);
    checkCast(event, e_actors, e_directors, QStringList(), e_presenters);
}

//
// Greek
// Handle the data in the title, subtitle, and description fields.
//
void TestEITFixups::testGreek2_data()
{
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("subtitle");
    QTest::addColumn<QString>("description");
    QTest::addColumn<QString>("e_title");
    QTest::addColumn<QString>("e_subtitle");
    QTest::addColumn<QString>("e_description");
    QTest::addColumn<int>("e_season");
    QTest::addColumn<int>("e_episode");
    QTest::addColumn<QDate>("e_airdate");

    QTest::newRow("date1")     << "Title" << "Subtitle" << "Description. παραγωγης 1999"
                               << "Title" << "Subtitle" << "Description"
                               << 0 << 0 << QDate(1999,1,1);
    QTest::newRow("date2")     << "Title" << "Subtitle" << "Description. - 2021"
                               << "Title" << "Subtitle" << "Description"
                               << 0 << 0 << QDate(2021,1,1);
    QTest::newRow("date3")     << "Title" << "Subtitle" << "Description, 2021"
                               << "Title" << "Subtitle" << "Description"
                               << 0 << 0 << QDate(2021,1,1);
    QTest::newRow("date4")     << "Title" << "Subtitle" << "Description. Παραγωγής 1966-1998"
                               << "Title" << "Subtitle" << "Description"
                               << 0 << 0 << QDate(1966,1,1);
    QTest::newRow("country1")  << "Title" << "Subtitle" << "Description. ελληνικης"
                               << "Title" << "Subtitle" << "Description."
                               << 0 << 0 << QDate();
    QTest::newRow("country2")  << "Title" << "Subtitle" << "Description. βρεττανικησ"
                               << "Title" << "Subtitle" << "Description."
                               << 0 << 0 << QDate();
    QTest::newRow("country3")  << "Title" << "Subtitle" << "Description. ελβετικής"
                               << "Title" << "Subtitle" << "Description."
                               << 0 << 0 << QDate();
    QTest::newRow("country4")  << "Title" << "Subtitle" << "Description. βραζιλιάνικήσ"
                               << "Title" << "Subtitle" << "Description."
                               << 0 << 0 << QDate();
    QTest::newRow("season1t")  << "ΣΤ Κυκλου επεισοδιο: 1 Title" << "Subtitle" << "Description."
                               << "Title" << "Subtitle" << "Description."
                               << 6 << 1 << QDate();
    QTest::newRow("season2t")  << "Title - 12 κυκλοσ, επεισόδιο 2" << "Subtitle" << "Description."
                               << "Title," << "Subtitle" << "Description."
                               << 12 << 2 << QDate();
    QTest::newRow("season3t")  << "Title - Γ Kύκλος, Επε 3" << "Subtitle" << "Description."
                               << "Title," << "Subtitle" << "Description."
                               << 3 << 3 << QDate();
    QTest::newRow("season4t")  << "Title - M Kύκλος" << "Subtitle" << "Description."
                               << "Title" << "Subtitle" << "Description."
                               << 13 << 0 << QDate();
    QTest::newRow("season1d")  << "Title" << "Subtitle" << "ΣΤ Κυκλου επεισοδιο: 1 Description"
                               << "Title" << "Subtitle" << "Description"
                               << 6 << 1 << QDate();
    QTest::newRow("season2d")  << "Title" << "Subtitle" << "Description. - 12 κυκλοσ, επεισόδιο 2"
                               << "Title" << "Subtitle" << "Description,"
                               << 12 << 2 << QDate();
    QTest::newRow("season3d")  << "Title" << "Subtitle" << "Description - Γ Kύκλος, Επε 3"
                               << "Title" << "Subtitle" << "Description,"
                               << 3 << 3 << QDate();
    // No english letters checking in description.
    //QTest::newRow("season4d")  << "Title" << "Subtitle" << "Description - M Kύκλος"
    //                           << "Title" << "Subtitle" << "Description"
    //                           << 13 << 0 << QDate();
    QTest::newRow("season5d")  << "Title" << "Subtitle" << "επεισοδιο: 1 Description"
                               << "Title" << "Subtitle" << "Description"
                               << 1 << 1 << QDate();

    // Double comma tickles a cleanup clause.
    // VII: English VI, greek Ι
    QTest::newRow("season1r")  << "Title,, VIΙ" << "Subtitle" << "Description."
                               << "Title" << "Subtitle" << "Description."
                               << 7 << 0 << QDate();
    QTest::newRow("season2r")  << "Title" << "Subtitle" << "Description,, XIX"
                               << "Title" << "Subtitle" << "Description"
                               << 19 << 0 << QDate();

    QTest::newRow("comment1")  << "Title (εισαγωγη αρχειων - 1)" << "Subtitle" << "Description"
                               << "Title" << "Subtitle" << "Description"
                               << 0 << 0 << QDate();
    // Pattern doesn't handle greek letters with diacritical marks
    //QTest::newRow("comment2")  << "Title (εισαγωγή αρχείων - 1)" << "Subtitle" << "Description"
    //                           << "Title" << "Subtitle" << "Description"
    //                           << 0 << 0 << QDate();

    QTest::newRow("realTitleT")  << "Title (Real Title)" << "Subtitle" << "Description"
                                 << "Real Title" << "Subtitle" << "(Title). Description"
                                 << 0 << 0 << QDate();
    QTest::newRow("realTitleD")  << "Title" << "Subtitle" << "(Real Title) Description"
                                 << "Real Title" << "Subtitle" << "(Title). Description"
                                 << 0 << 0 << QDate();
    QTest::newRow("eposideInSt") << "Title" << "Subtitle" << "Επεισόδιο: Lion in the cage. Description"
                                 << "Title" << "Lion in the cage" << "Description"
                                 << 0 << 0 << QDate();
}

void TestEITFixups::testGreek2()
{
    QFETCH(QString, title);
    QFETCH(QString, subtitle);
    QFETCH(QString, description);
    QFETCH(QString, e_title);
    QFETCH(QString, e_subtitle);
    QFETCH(QString, e_description);
    QFETCH(int, e_season);
    QFETCH(int, e_episode);
    QFETCH(QDate,   e_airdate);

    DBEventEIT event(7302, title, subtitle, description,
                     "", ProgramInfo::kCategoryNone,
                     QDateTime::fromString("2020-02-28T23:55:00Z", Qt::ISODate),
                     QDateTime::fromString("2020-03-01T02:00:00Z", Qt::ISODate),
                     EITFixUp::kFixGreekEIT, SUB_UNKNOWN, AUD_UNKNOWN, VID_UNKNOWN, 0.0F,
                     "", "", 0, 0, 0);

    EITFixUp::Fix(event);
    PRINT_EVENT(event);
    QCOMPARE(event.m_title,           e_title);
    QCOMPARE(event.m_subtitle,        e_subtitle);
    QCOMPARE(event.m_description,     e_description);
    QCOMPARE(event.m_season,          (uint)e_season);
    QCOMPARE(event.m_episode,         (uint)e_episode);
    QCOMPARE(event.m_originalairdate, e_airdate);
}

void TestEITFixups::testGreek3()
{
    DBEventEIT event(7302, "Title", "Subtitle", "Description ταινια",
                     "", ProgramInfo::kCategoryNone,
                     QDateTime::fromString("2020-02-28T23:55:00Z", Qt::ISODate),
                     QDateTime::fromString("2020-03-01T02:00:00Z", Qt::ISODate),
                     EITFixUp::kFixGreekEIT, SUB_UNKNOWN, AUD_UNKNOWN, VID_UNKNOWN, 0.0F,
                     "", "", 0, 0, 0);

    EITFixUp::Fix(event);
    PRINT_EVENT(event);
    QCOMPARE(event.m_description,  QString("Description ταινια"));
    QCOMPARE(event.m_categoryType, ProgramInfo::kCategoryMovie);
}

void TestEITFixups::testGreekCategories_data()
{
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("description");
    QTest::addColumn<QString>("e_category");

    // Trailing non-word character is required for a match.
    QTest::newRow("comedy1")     << "Title" << "Description. κωμικη "              << "Κωμωδία";
    QTest::newRow("comedy2")     << "Title" << "Description. κωμικη εκπομπη "      << "Κωμωδία";
    QTest::newRow("comedy3")     << "Title" << "Description. κωμικό εκπομπή "      << "Κωμωδία";
    QTest::newRow("comedy4")     << "Title" << "Description. χιουμοριστικη "       << "Κωμωδία";
    QTest::newRow("comedy5")     << "Title" << "Description. χιουμοριστική σειρα " << "Κωμωδία";
    QTest::newRow("comedy6")     << "Title" << "Description. χιουμοριστικό σειρά " << "Κωμωδία";
    QTest::newRow("comedy7")     << "Title" << "Description. κωμωδια ταινία "      << "Κωμωδία";
    QTest::newRow("telemag1")    << "Title" << "Description. ενημερωτικη "         << "Τηλεπεριοδικό";
    QTest::newRow("telemag2")    << "Title" << "Description. ενημερωτική εκπομπή " << "Τηλεπεριοδικό";
    QTest::newRow("telemag3")    << "Title" << "Description. ενημερωτική σειρα "   << "Τηλεπεριοδικό";
    QTest::newRow("telemag4")    << "Title" << "Description. ψυχαγωγικη "          << "Τηλεπεριοδικό";
    QTest::newRow("telemag5")    << "Title" << "Description. ψυχαγωγική εκπομπη "  << "Τηλεπεριοδικό";
    QTest::newRow("telemag6")    << "Title" << "Description. ψυχαγωγική σειρά "    << "Τηλεπεριοδικό";
    QTest::newRow("telemag7")    << "Title" << "Description. μαγκαζινο ταινία "    << "Τηλεπεριοδικό";

    QTest::newRow("nature1")     << "Title" << "Description. φυση" << "Επιστήμη/Φύση";
    QTest::newRow("nature2")     << "Title" << "Description. φύση" << "Επιστήμη/Φύση";
    QTest::newRow("nature3")     << "Title" << "Description. περιβάλλο" << "Επιστήμη/Φύση";
    QTest::newRow("nature4")     << "Title" << "Description. κατασκευ" << "Επιστήμη/Φύση";
    QTest::newRow("nature5")     << "Title" << "Description. επιστήμ" << "Επιστήμη/Φύση";
    QTest::newRow("nature6")     << "Title" << "Description. επιστήμονικης φαντασιας" << "Φαντασίας"; // Test negative lookahead
    QTest::newRow("health1")     << "Title" << "Description. υγεια" << "Υγεία";
    QTest::newRow("health2")     << "Title" << "Description. υγεία" << "Υγεία";
    QTest::newRow("health3")     << "Title" << "Description. υγειιν" << "Υγεία";
    QTest::newRow("health4")     << "Title" << "Description. ιατρικ" << "Υγεία";
    QTest::newRow("health5")     << "Title" << "Description. διατροφ" << "Υγεία";
    QTest::newRow("reality1")    << "Title" << "Description. ριαλιτι" << "Ριάλιτι";
    QTest::newRow("reality2")    << "Title" << "Description. ριάλιτι)" << "Ριάλιτι";
    QTest::newRow("reality3")    << "Title" << "Description. reality)" << "Ριάλιτι";
    QTest::newRow("drama1")      << "Title" << "Description. κοινωνικη " << "Κοινωνικό";
    QTest::newRow("drama2")      << "Title" << "Description. κοινωνική εκπομπη" << "Κοινωνικό";
    QTest::newRow("drama3")      << "Title" << "Description. κοινωνικό εκπομπή" << "Κοινωνικό";
    QTest::newRow("drama4")      << "Title" << "Description. κοινωνική σειρά" << "Κοινωνικό";
    QTest::newRow("drama5")      << "Title" << "Description. κοινωνικό ταινια" << "Κοινωνικό";
    QTest::newRow("drama6")      << "Title" << "Description. δραματικη " << "Κοινωνικό";
    QTest::newRow("drama7")      << "Title" << "Description. δραματική εκπομπη" << "Κοινωνικό";
    QTest::newRow("drama8")      << "Title" << "Description. δραματικη σειρά" << "Κοινωνικό";
    QTest::newRow("drama9")      << "Title" << "Description. δραματική ταινια" << "Κοινωνικό";
    QTest::newRow("drama10")     << "Title" << "Description. δραμα " << "Κοινωνικό";
    QTest::newRow("drama11")     << "Title" << "Description. δράμα εκπομπή" << "Κοινωνικό";
    QTest::newRow("drama12")     << "Title" << "Description. δραμα σειρα" << "Κοινωνικό";
    QTest::newRow("drama13")     << "Title" << "Description. δράμα ταινία" << "Κοινωνικό";
    QTest::newRow("children1")   << "Title" << "Description. παιδικη " << "Παιδικό";
    QTest::newRow("children2")   << "Title" << "Description. παιδικό " << "Παιδικό";
    QTest::newRow("children3")   << "Title" << "Description. παιδική εκπομπη" << "Παιδικό";
    QTest::newRow("children4")   << "Title" << "Description. παιδικο σειρά" << "Παιδικό";
    QTest::newRow("children5")   << "Title" << "Description. παιδικο ταινία" << "Παιδικό";
    QTest::newRow("children6")   << "Title" << "Description. κινουμενων σχεδιων " << "Παιδικό";
    QTest::newRow("children7")   << "Title" << "Description. κινουμενων σχεδια " << "Παιδικό";
    QTest::newRow("children8")   << "Title" << "Description. κινουμενα σχέδίων " << "Παιδικό";
    QTest::newRow("children9")   << "Title" << "Description. κινουμενα σχέδία " << "Παιδικό";
    QTest::newRow("children10")  << "Title" << "Description. κινουμενα σχέδία εκπομπη" << "Παιδικό";
    QTest::newRow("children11")  << "Title" << "Description. κινουμενα σχέδία σειρα" << "Παιδικό";
    QTest::newRow("children12")  << "Title" << "Description. κινουμενα σχέδία ταινία" << "Παιδικό";
    QTest::newRow("scifi1")      << "Title" << "Description. επιστο φαντασιας" << "Επιστ.Φαντασίας";
    QTest::newRow("scifi2")      << "Title" << "Description. επιστημονικης φαντασιας" << "Επιστ.Φαντασίας";
    QTest::newRow("scifi3")      << "Title" << "Description. επιστημονικής φαντασίας" << "Επιστ.Φαντασίας";
    QTest::newRow("mystery1")    << "Title" << "Description. μυστηριου" << "Μυστηρίου";
    QTest::newRow("mystery2")    << "Title" << "Description. πομπη μυστηρίου" << "Μυστηρίου";
    QTest::newRow("mystery3")    << "Title" << "Description. σειρα μυστηρίου" << "Μυστηρίου";
    QTest::newRow("mystery4")    << "Title" << "Description. ταινία  μυστηρίου" << "Μυστηρίου";
    QTest::newRow("fantasy1")    << "Title" << "Description. φαντασίας" << "Φαντασίας";
    QTest::newRow("fantasy2")    << "Title" << "Description. πομπή φαντασιας" << "Φαντασίας";
    QTest::newRow("fantasy3")    << "Title" << "Description. σειρά φαντασίας" << "Φαντασίας";
    QTest::newRow("fantasy4")    << "Title" << "Description. ταινια φαντασίας" << "Φαντασίας";
    QTest::newRow("myst/fant")   << "Title" << "Description. μυστηρίου φαντασίας" << "Φαντασίας/Μυστηρίου";
    QTest::newRow("history1")    << "Title" << "Description. ιστορικη" << "Ιστορικό";
    QTest::newRow("history2")    << "Title" << "Description. ιστορική" << "Ιστορικό";
    QTest::newRow("history3")    << "Title" << "Description. ιστορικο εκπομπη " << "Ιστορικό";
    QTest::newRow("history4")    << "Title" << "Description. ιστορικό σειρα " << "Ιστορικό";
    QTest::newRow("history5")    << "Title" << "Description. ιστορικό ταινία " << "Ιστορικό";
    QTest::newRow("food1")       << "Title" << "Description. Γαστρονομια" << "Γαστρονομία";
    QTest::newRow("food2")       << "Title" << "Description. Γαστρονομίασ" << "Γαστρονομία";
    QTest::newRow("food3")       << "Title" << "Description. μαγειρικη" << "Γαστρονομία";
    QTest::newRow("food4")       << "Title" << "Description. μαγειρικής" << "Γαστρονομία";
    QTest::newRow("food5")       << "Title" << "Description. chef" << "Γαστρονομία";
    QTest::newRow("food6")       << "Title" << "Description. συνταγή" << "Γαστρονομία";
    // QTest::newRow("food7")       << "Title" << "Description. διατροφ" << "Γαστρονομία"; // Matches health first
    QTest::newRow("food8")       << "Title" << "Description. wine" << "Γαστρονομία";
    QTest::newRow("food9")       << "Title" << "Description. μαγειρα" << "Γαστρονομία";
    QTest::newRow("food10")      << "Title" << "Description. μάγειρασ" << "Γαστρονομία";
    QTest::newRow("biography1")  << "Title" << "Description. βιογραφια" << "Βιογραφία";
    QTest::newRow("biography2")  << "Title" << "Description. βιογραφικο)" << "Βιογραφία";
    QTest::newRow("biography3")  << "Title" << "Description. βιογραφικόσ)" << "Βιογραφία";
    QTest::newRow("biography4")  << "Title" << "Description. βιογραφικός)" << "Βιογραφία";
    QTest::newRow("news1")       << "Title δελτιο" << "Description." << "Ειδήσεις";
    QTest::newRow("news2")       << "Title ειδησεισ" << "Description." << "Ειδήσεις";
    QTest::newRow("news3")       << "Title ειδησεις" << "Description." << "Ειδήσεις";
    QTest::newRow("news4")       << "Title ειδήσεων" << "Description." << "Ειδήσεις";
    QTest::newRow("sports1")     << "Title" << "Description. champion" << "Αθλητικά";
    QTest::newRow("sports2")     << "Title" << "Description. αθλητικα" << "Αθλητικά";
    QTest::newRow("sports3")     << "Title" << "Description. πρωτάθλημα" << "Αθλητικά";
    QTest::newRow("sports4")     << "Title" << "Description. ποδοσφαιρο" << "Αθλητικά";
    QTest::newRow("sports5")     << "Title" << "Description. ποδόσφαιρου" << "Αθλητικά";
    QTest::newRow("sports6")     << "Title" << "Description. κολυμβηση" << "Αθλητικά";
    QTest::newRow("sports7")     << "Title" << "Description. πατινάζ" << "Αθλητικά";
    QTest::newRow("sports8")     << "Title" << "Description. formula" << "Αθλητικά";
    QTest::newRow("sports9")     << "Title" << "Description. μπάσκετ" << "Αθλητικά";
    QTest::newRow("sports10")    << "Title" << "Description. βολει" << "Αθλητικά";
    QTest::newRow("sports11")    << "Title" << "Description. βόλεϊ" << "Αθλητικά";
    QTest::newRow("documentary1") << "Title" << "Description. ντοκιμαντερ" << "Ντοκιμαντέρ";
    QTest::newRow("documentary2") << "Title" << "Description. ντοκυμαντέρ" << "Ντοκιμαντέρ";
    QTest::newRow("religion")    << "Title" << "Description. θρησκεια" << "Θρησκεία";
    QTest::newRow("religion")    << "Title" << "Description. θρησκευτικ" << "Θρησκεία";
    QTest::newRow("religion")    << "Title" << "Description. ναο" << "Θρησκεία";
    QTest::newRow("religion")    << "Title" << "Description. ναός" << "Θρησκεία";
    QTest::newRow("religion")    << "Title" << "Description. θεια λειτουργια)" << "Θρησκεία";
    QTest::newRow("religion")    << "Title" << "Description. θεία λειτουργία)" << "Θρησκεία";
    QTest::newRow("culture")     << "Title" << "Description. τεχνη" << "Τέχνες/Πολιτισμός";
    QTest::newRow("culture")     << "Title" << "Description. τέχνες" << "Τέχνες/Πολιτισμός";
    QTest::newRow("culture")     << "Title" << "Description. πολιτισμ)" << "Τέχνες/Πολιτισμός";
    QTest::newRow("special")     << "Title" << "Description. αφιερωμα)" << "Αφιέρωμα";

    // test title too
    QTest::newRow("teleshop1d")  << "Title" << "Description. οδηγο αγορων" << "Τηλεπωλήσεις";
    QTest::newRow("teleshop2d")  << "Title" << "Description. οδηγός αγορών" << "Τηλεπωλήσεις";
    QTest::newRow("teleshop3d")  << "Title" << "Description. τηλεπωλησ" << "Τηλεπωλήσεις";
    QTest::newRow("teleshop4d")  << "Title" << "Description. τηλεπώλήσ" << "Τηλεπωλήσεις";
    QTest::newRow("teleshop5d")  << "Title" << "Description. τηλεαγορ" << "Τηλεπωλήσεις";
    QTest::newRow("teleshop6d")  << "Title" << "Description. τηλεμαρκετ" << "Τηλεπωλήσεις";
    QTest::newRow("teleshop7d")  << "Title" << "Description. telemarket" << "Τηλεπωλήσεις";
    QTest::newRow("teleshop8d")  << "Title" << "Description. telemarket εκπομπη" << "Τηλεπωλήσεις";
    QTest::newRow("teleshop9d")  << "Title" << "Description. telemarket σειρα" << "Τηλεπωλήσεις";
    QTest::newRow("teleshop10d") << "Title telemarket ταινία" << "Description." << "Τηλεπωλήσεις";
    QTest::newRow("teleshop1t")  << "Title οδηγο αγορων" << "Description." << "Τηλεπωλήσεις";
    QTest::newRow("teleshop2t")  << "Title οδηγός αγορών" << "Description." << "Τηλεπωλήσεις";
    QTest::newRow("teleshop3t")  << "Title τηλεπωλησ" << "Description." << "Τηλεπωλήσεις";
    QTest::newRow("teleshop4t")  << "Title τηλεπώλήσ" << "Description." << "Τηλεπωλήσεις";
    QTest::newRow("teleshop5t")  << "Title τηλεαγορ" << "Description." << "Τηλεπωλήσεις";
    QTest::newRow("teleshop6t")  << "Title τηλεμαρκετ" << "Description." << "Τηλεπωλήσεις";
    QTest::newRow("teleshop7t")  << "Title telemarket" << "Description." << "Τηλεπωλήσεις";
    QTest::newRow("teleshop8t")  << "Title telemarket εκπομπη" << "Description." << "Τηλεπωλήσεις";
    QTest::newRow("teleshop9t")  << "Title telemarket σειρα" << "Description." << "Τηλεπωλήσεις";
    QTest::newRow("teleshop10t") << "Title telemarket ταινία" << "Description." << "Τηλεπωλήσεις";
    QTest::newRow("gameshow1d")  << "Title" << "Description. τηλεπαιχνιδι" << "Τηλεπαιχνίδι";
    QTest::newRow("gameshow2d")  << "Title" << "Description. quiz" << "Τηλεπαιχνίδι";
    QTest::newRow("gameshow1t")  << "Title τηλεπαιχνιδι" << "Description." << "Τηλεπαιχνίδι";
    QTest::newRow("gameshow2t")  << "Title quiz" << "Description." << "Τηλεπαιχνίδι";
    QTest::newRow("music1d")     << "Title" << "Description. μουσικο" << "Μουσική";
    QTest::newRow("music2d")     << "Title" << "Description. eurovision" << "Μουσική";
    QTest::newRow("music3d")     << "Title" << "Description. τραγούδι" << "Μουσική";
    QTest::newRow("music1t")     << "Title μουσικο" << "Description." << "Μουσική";
    QTest::newRow("music2t")     << "Title eurovision" << "Description." << "Μουσική";
    QTest::newRow("music3t")     << "Title τραγούδι" << "Description." << "Μουσική";
}

void TestEITFixups::testGreekCategories()
{
    QFETCH(QString, title);
    QFETCH(QString, description);
    QFETCH(QString, e_category);

    DBEventEIT event(7302, title, "Subtitle", description,
                     "", ProgramInfo::kCategoryNone,
                     QDateTime::fromString("2020-02-28T23:55:00Z", Qt::ISODate),
                     QDateTime::fromString("2020-03-01T02:00:00Z", Qt::ISODate),
                     EITFixUp::kFixGreekCategories, SUB_UNKNOWN, AUD_UNKNOWN, VID_UNKNOWN, 0.0F,
                     "", "", 0, 0, 0);

    EITFixUp::Fix(event);
    PRINT_EVENT(event);
    QCOMPARE(event.m_category, e_category);
}


QTEST_APPLESS_MAIN(TestEITFixups)
