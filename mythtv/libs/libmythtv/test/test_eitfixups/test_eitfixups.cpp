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
    if (event.credits && !event.credits->empty())
    {
        printf("Credits      %3zu\n", event.credits->size());
    }
    if (!event.items.isEmpty())
    {
        printf("Items        %3d\n", event.items.count());
    }
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

void TestEITFixups::testUKLawAndOrder()
{
    EITFixUp fixup;

    DBEventEIT *event = SimpleDBEventEIT (EITFixUp::kFixUK,
                                         "Law & Order: Special Victims Unit",
                                         "",
                                         "Crime drama series. Detective Cassidy is accused of raping ...");

    PRINT_EVENT(*event);
    fixup.Fix(*event);
    PRINT_EVENT(*event);
    QCOMPARE(event->title,    QString("Law & Order: Special Victims Unit"));
    QCOMPARE(event->subtitle, QString(""));
    delete event;

    DBEventEIT *event2 = SimpleDBEventEIT (EITFixUp::kFixUK,
                                         "Law & Order: Special Victims Unit",
                                         "",
                                         "Sugar: New. Police drama series about an elite sex crime  ...");

    PRINT_EVENT(*event2);
    fixup.Fix(*event2);
    PRINT_EVENT(*event2);
    QCOMPARE(event2->title,    QString("Law & Order: Special Victims Unit"));
    QCOMPARE(event2->subtitle, QString("Sugar"));
    QCOMPARE(event2->description, QString("Police drama series about an elite sex crime  ..."));
    delete event2;
}

void TestEITFixups::testUKMarvel()
{
    EITFixUp fixup;

    DBEventEIT *event = SimpleDBEventEIT (EITFixUp::kFixUK,
                                         "Marvel's Agents of S.H.I.E.L.D.",
                                         "",
                                         "");

    PRINT_EVENT(*event);
    fixup.Fix(*event);
    PRINT_EVENT(*event);
    QCOMPARE(event->title,    QString("Marvel's Agents of S.H.I.E.L.D."));
    delete event;


    DBEventEIT *event2 = SimpleDBEventEIT (EITFixUp::kFixUK,
                                          "NEW: Marvel's Agents of S.H.I.E.L.D.",
                                          "",
                                          "");

    PRINT_EVENT(*event2);
    fixup.Fix(*event2);
    PRINT_EVENT(*event2);
    QCOMPARE(event2->title,    QString("Marvel's Agents of S.H.I.E.L.D."));
    delete event2;
}

DBEventEIT *TestEITFixups::SimpleDBEventEIT (FixupValue fixup, QString title, QString subtitle, QString description)
{
    DBEventEIT *event = new DBEventEIT (1, // channel id
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
                                       0.0f, // star rating
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
    EITFixUp fixup;

    DBEventEIT event(1005,
                      "New: The X-Files",
                      "Hit sci-fi drama series returns. Mulder and Scully are reunited after the collapse of their relationship when a TV host contacts them, believing he has uncovered a significant conspiracy. (Ep 1)[AD,S]",
                      QDateTime::fromString("2016-02-08T22:00:00Z", Qt::ISODate),
                      QDateTime::fromString("2016-02-08T23:00:00Z", Qt::ISODate),
                      EITFixUp::kFixGenericDVB | EITFixUp::kFixUK,
                      SUB_UNKNOWN,
                      AUD_STEREO,
                      VID_UNKNOWN);

    fixup.Fix(event);
    PRINT_EVENT(event);
    QCOMPARE(event.title,       QString("The X-Files"));
    QCOMPARE(event.description, QString("Hit sci-fi drama series returns. Mulder and Scully are reunited after the collapse of their relationship when a TV host contacts them, believing he has uncovered a significant conspiracy. (Ep 1)"));
}

void TestEITFixups::testDEPro7Sat1()
{
    EITFixUp fixup;

    DBEventEIT *event = SimpleDBEventEIT (EITFixUp::kFixP7S1,
                                         "Titel",
                                         "Folgentitel, Mystery, USA 2011",
                                         "Beschreibung");

    PRINT_EVENT(*event);
    fixup.Fix(*event);
    PRINT_EVENT(*event);
    QCOMPARE(event->title,    QString("Titel"));
    QCOMPARE(event->subtitle, QString("Folgentitel"));
    QCOMPARE(event->airdate,  (unsigned short) 2011);
    delete event;

    DBEventEIT *event2 = SimpleDBEventEIT (EITFixUp::kFixP7S1,
                                           "Titel",
                                           "Kurznachrichten, D 2015",
                                           "Beschreibung");
    PRINT_EVENT(*event2);
    fixup.Fix(*event2);
    PRINT_EVENT(*event2);
    QCOMPARE(event2->subtitle, QString(""));
    QCOMPARE(event2->airdate,  (unsigned short) 2015);
    delete event2;

    DBEventEIT *event3 = SimpleDBEventEIT (EITFixUp::kFixP7S1,
                                           "Titel",
                                           "Folgentitel",
                                           "Beschreibung");
    PRINT_EVENT(*event3);
    fixup.Fix(*event3);
    PRINT_EVENT(*event3);
    QCOMPARE(event3->subtitle, QString("Folgentitel"));
    QCOMPARE(event3->airdate,  (unsigned short) 0);
    delete event3;

    DBEventEIT *event4 = SimpleDBEventEIT (EITFixUp::kFixP7S1,
                                           "Titel",
                                           "\"Lokal\", Ort, Doku-Soap, D 2015",
                                           "Beschreibung");
    PRINT_EVENT(*event4);
    fixup.Fix(*event4);
    PRINT_EVENT(*event4);
    QCOMPARE(event4->subtitle, QString("\"Lokal\", Ort"));
    QCOMPARE(event4->airdate,  (unsigned short) 2015);
    delete event4;

    DBEventEIT *event5 = SimpleDBEventEIT (EITFixUp::kFixP7S1,
                                           "Titel",
                                           "In Morpheus' Armen, Science-Fiction, CDN/USA 2006",
                                           "Beschreibung");
    PRINT_EVENT(*event5);
    fixup.Fix(*event5);
    PRINT_EVENT(*event5);
    QCOMPARE(event5->subtitle, QString("In Morpheus' Armen"));
    QCOMPARE(event5->airdate,  (unsigned short) 2006);
    delete event5;

    DBEventEIT *event6 = SimpleDBEventEIT (EITFixUp::kFixP7S1,
                                           "Titel",
                                           "Drei Kleintiere durchschneiden (1), Zeichentrick, J 2014",
                                           "Beschreibung");
    PRINT_EVENT(*event6);
    fixup.Fix(*event6);
    PRINT_EVENT(*event6);
    QCOMPARE(event6->subtitle, QString("Drei Kleintiere durchschneiden (1)"));
    QCOMPARE(event6->airdate,  (unsigned short) 2014);
    delete event6;

    /* #12151 */
    DBEventEIT *event7 = SimpleDBEventEIT (EITFixUp::kFixP7S1,
                                           "Criminal Minds",
                                           "<episode title>, Crime-Serie, USA 2011",
                                           "<plot summary>\n\n"
                                           "Regie: Frau Regisseur\n"
                                           "Drehbuch: Lieschen Mueller, Frau Meier\n\n"
                                           "Darsteller:\n"
                                           "Herr Schauspieler (in einer (kleinen) Rolle)\n"
                                           "Frau Schauspielerin (in einer Rolle)");
    PRINT_EVENT(*event7);
    fixup.Fix(*event7);
    PRINT_EVENT(*event7);
    QCOMPARE(event7->subtitle, QString("<episode title>"));
    QCOMPARE(event7->airdate,  (unsigned short) 2011);
    QCOMPARE(event7->description, QString("<plot summary>"));
    delete event7;
}

void TestEITFixups::testHTMLFixup()
{
    // Make sure we correctly strip HTML tags from EIT data
    EITFixUp fixup;

    DBEventEIT event(9311,
                      "<EM>CSI: Crime Scene Investigation</EM>",
                      "Double-Cross: Las Vegas-based forensic drama. The team investigates when two nuns find a woman crucified in the rafters of their church - and clues implicate the priest. (S7 Ep 5)",
                      QDateTime::fromString("2015-02-28T19:40:00Z", Qt::ISODate),
                      QDateTime::fromString("2015-02-28T20:00:00Z", Qt::ISODate),
                      EITFixUp::kFixHTML | EITFixUp::kFixUK,
                      SUB_UNKNOWN,
                      AUD_STEREO,
                      VID_UNKNOWN);

    fixup.Fix(event);
    PRINT_EVENT(event);
    QCOMPARE(event.title,       QString("CSI: Crime Scene Investigation"));
    QCOMPARE(event.subtitle,    QString("Double-Cross"));
// FIXME: Need to fix the capturing of (S7 Ep 5) for this to properly validate.
//    QCOMPARE(event.description, QString("Las Vegas-based forensic drama. The team investigates when two nuns find a woman crucified in the rafters of their church - and clues implicate the priest."));

    DBEventEIT event2(9311,
                      "<EM>New: Redneck Island</EM>",
                      "Twelve rednecks are stranded on a tropical island with 'Stone Cold' Steve Austin, but this is no holiday, they're here to compete for $100,000. S4, Ep4",
                      QDateTime::fromString("2015-02-28T19:40:00Z", Qt::ISODate),
                      QDateTime::fromString("2015-02-28T20:00:00Z", Qt::ISODate),
                      EITFixUp::kFixHTML | EITFixUp::kFixUK,
                      SUB_UNKNOWN,
                      AUD_STEREO,
                      VID_UNKNOWN);

    fixup.Fix(event2);
    PRINT_EVENT(event2);
    QCOMPARE(event2.title,       QString("Redneck Island"));

    DBEventEIT event3(14101,
                      "New: Jericho",
                      "Drama set in 1870s Yorkshire. In her desperation to protect her son, Annie unwittingly opens the door for Bamford the railway detective, who has returned to Jericho. [AD,S]",
                      QDateTime::fromString("2015-02-28T19:40:00Z", Qt::ISODate),
                      QDateTime::fromString("2015-02-28T20:00:00Z", Qt::ISODate),
                      EITFixUp::kFixHTML | EITFixUp::kFixUK,
                      SUB_UNKNOWN,
                      AUD_STEREO,
                      VID_UNKNOWN);

    fixup.Fix(event3);
    PRINT_EVENT(event3);
    QCOMPARE(event3.title,       QString("Jericho"));
    QCOMPARE(event3.description, QString("Drama set in 1870s Yorkshire. In her desperation to protect her son, Annie unwittingly opens the door for Bamford the railway detective, who has returned to Jericho."));

}

void TestEITFixups::testSkyEpisodes()
{
    EITFixUp fixup;

    DBEventEIT *event = SimpleDBEventEIT (EITFixUp::kFixPremiere,
                                         "Titel",
                                         "Subtitle",
                                         "4. Staffel, Folge 16: Viele Mitglieder einer christlichen Gemeinde erkranken nach einem Giftanschlag tödlich. Doch die fanatisch Gläubigen lassen weder polizeiliche, noch ärztliche Hilfe zu. Don (Rob Morrow) und Charlie (David Krumholtz) gelingt es jedoch durch einen Nebeneingang ins Gebäude zu kommen. Bei ihren Ermittlungen finden sie heraus, dass der Anführer der Sekte ein Betrüger war. Auch sein Sohn wusste von den Machenschaften des Vaters. War der Giftanschlag ein Racheakt? 50 Min. USA 2008. Von Leslie Libman, mit Rob Morrow, David Krumholtz, Judd Hirsch. Ab 12 Jahren");

    PRINT_EVENT(*event);
    fixup.Fix(*event);
    PRINT_EVENT(*event);
    QCOMPARE(event->description, QString("Viele Mitglieder einer christlichen Gemeinde erkranken nach einem Giftanschlag tödlich. Doch die fanatisch Gläubigen lassen weder polizeiliche, noch ärztliche Hilfe zu. Don (Rob Morrow) und Charlie (David Krumholtz) gelingt es jedoch durch einen Nebeneingang ins Gebäude zu kommen. Bei ihren Ermittlungen finden sie heraus, dass der Anführer der Sekte ein Betrüger war. Auch sein Sohn wusste von den Machenschaften des Vaters. War der Giftanschlag ein Racheakt? Ab 12 Jahren"));
    QCOMPARE(event->season,   4u);
    QCOMPARE(event->episode, 16u);
    /* FixPremiere should scrape the credits, too! */
    QVERIFY(event->HasCredits());
    delete event;

    DBEventEIT *event2 = SimpleDBEventEIT (EITFixUp::kFixPremiere,
                                         "Titel",
                                         "Subtitle",
                                         "Washington, 1971: Vor dem Obersten Gerichtshof wird über die Kriegsdienstverweigerung von Box-Ikone Cassius Clay aka Muhammad Ali verhandelt. Während draußen Tausende gegen den Vietnamkrieg protestieren, verteidigen acht weiße, alte Bundesrichter unter dem Vorsitzenden Warren Burger (Frank Langella) die harte Linie der Regierung Nixon. Doch Kevin Connolly (Benjamin Walker), ein idealistischer junger Mitarbeiter von Richter Harlan (Christopher Plummer), gibt nicht auf. - Muhammad Alis Kiegsdienst-Verweigerungsprozess, als Mix aus Kammerspiel und Archivaufnahmen starbesetzt verfilmt. 94 Min. USA 2012. Von Stephen Frears, mit Danny Glover, Barry Levinson, Bob Balaban. Ab 12 Jahren");

    PRINT_EVENT(*event2);
    fixup.Fix(*event2);
    PRINT_EVENT(*event2);
    QCOMPARE(event2->description, QString("Washington, 1971: Vor dem Obersten Gerichtshof wird über die Kriegsdienstverweigerung von Box-Ikone Cassius Clay aka Muhammad Ali verhandelt. Während draußen Tausende gegen den Vietnamkrieg protestieren, verteidigen acht weiße, alte Bundesrichter unter dem Vorsitzenden Warren Burger (Frank Langella) die harte Linie der Regierung Nixon. Doch Kevin Connolly (Benjamin Walker), ein idealistischer junger Mitarbeiter von Richter Harlan (Christopher Plummer), gibt nicht auf. - Muhammad Alis Kiegsdienst-Verweigerungsprozess, als Mix aus Kammerspiel und Archivaufnahmen starbesetzt verfilmt. Ab 12 Jahren"));
    QCOMPARE(event2->season,  0u);
    QCOMPARE(event2->episode, 0u);
    delete event2;

    DBEventEIT *event3 = SimpleDBEventEIT (EITFixUp::kFixPremiere,
                                         "Titel",
                                         "Subtitle",
                                         "50 Min. USA 2008. Von Leslie Libman, mit Rob Morrow, David Krumholtz, Judd Hirsch. Ab 12 Jahren");

    PRINT_EVENT(*event3);
    fixup.Fix(*event3);
    PRINT_EVENT(*event3);
    QCOMPARE(event3->description, QString("Ab 12 Jahren"));
    QCOMPARE(event3->season,  0u);
    QCOMPARE(event3->episode, 0u);
    delete event3;
}

void TestEITFixups::testUnitymedia()
{
    EITFixUp fixup;

    DBEventEIT *event = SimpleDBEventEIT (EITFixUp::kFixUnitymedia,
                                         "Titel",
                                         "Beschreib",
                                         "Beschreibung ... IMDb Rating: 8.9 /10");
    QMap<QString,QString> cast;
    cast.insertMulti ("Role Player", "Great Actor");
    cast.insertMulti ("Role Player", "Other Actor");
    cast.insertMulti ("Director", "Great Director");
    cast.insertMulti ("Unhandled", "lets fix it up");
    event->items = cast;

    QVERIFY(!event->HasCredits());
    QCOMPARE(event->items.count(), 4);

    PRINT_EVENT(*event);
    fixup.Fix(*event);
    PRINT_EVENT(*event);

    QVERIFY(event->HasCredits());
    QCOMPARE(event->credits->size(), (size_t)3);
    QVERIFY(event->subtitle.isEmpty());
    QCOMPARE(event->description, QString("Beschreibung ..."));
    QCOMPARE(event->stars, 0.89f);
    QCOMPARE(event->items.count(), 1);
    delete event;
}

QTEST_APPLESS_MAIN(TestEITFixups)
