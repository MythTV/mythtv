/*
 *  Class TestVideoList
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

#include "test_videolist.h"

#include "libmythbase/mythcorecontext.h"
#include "libmythmetadata/videometadatalistmanager.h"
#include "libmythui/mythgenerictree.h"

#include "videofilter.h"

void TestVideoList::initTestCase()
{
    gCoreContext = new MythCoreContext("test_videolist_1.0", nullptr);

    QMap<QString,QString> sOverrides;
    sOverrides["FilenameTemplate"] = "abc";
    gCoreContext->setTestStringSettings(sOverrides);

    QMap<QString,int> iOverrides;
    iOverrides["VideoDefaultCategory"] = kCategoryFilterAll;
    iOverrides["VideoDefaultGenre"] = kGenreFilterAll;
    iOverrides["VideoDefaultCountry"] = kCountryFilterAll;
    iOverrides["VideoDefaultCast"] = kCastFilterAll;
    iOverrides["VideoDefaultYear"] = kYearFilterAll;
    iOverrides["VideoDefaultRuntime"] = kRuntimeFilterAll;
    iOverrides["VideoDefaultUserrating"] = kUserRatingFilterAll;
    iOverrides["VideoDefaultBrowse"] = kBrowseFilterAll;
    iOverrides["VideoDefaultWatched"] = kWatchedFilterAll;
    iOverrides["VideoDefaultInetref"] = kInetRefFilterAll;
    iOverrides["VideoDefaultCoverfile"] = kCoverFileFilterAll;
//    iOverrides["VideoDefaultOrderby"] = kOrderByTitle;
    gCoreContext->setTestIntSettings(iOverrides);
}

void TestVideoList::testGenericTree ()
{
    auto metadata = VideoMetadata(
        QStringLiteral("TV Series/Flash (2014)/Season 3/Flash (2014) - S03E04.mkv"), // filename
        QString(), // sortfilename
        "c7097018f6b14e43", // hash
        "Bogus Flash trailer name", // trailer
        "The Flash (2014) Season 3_coverart.jpg", // coverfile
        "The Flash (2014) Season 3x4_screenshot.jpg",
        "The Flash (2014) Season 3_banner.jpg",
        "The Flash (2014) Season 3_fanart.jpg",
        "The Flash (2014)", "flash (2014)", // title, sorttitle
        "The New Rogues", "new rogues", // subtitle, sortsubtitle
        QString(), // tagline
        2016, QDate(2016,10,25), // year, releasedate
        "ttvdb4.py_279121", //inetref
        0, // collectionref
        "http://thetvdb.com/?tab=episode&seriesid=279121&seasonid=671084&id=5714605", // homepage
        "Stefan Pleszczynski", // director
        "The CW", // studio
        "Barry continues to train Jesse and when a new meta human, " \
        "Mirror Master, appears on the scene he lets her tag along. " \
        "Mirror Master has teamed up with his old partner, Top, and " \
        "is looking for Snart to even a score. Jesse is quick to join " \
        "the chase but defies one of Barry's orders which results in " \
        "disastrous consequences.", // plot
        7.8F, "TV-14", // userrating, rating
        45, 5, // length, playcount
        3, 4, // season, eposide
        QDate(2016,11,3), // insertdate
        21749, // id
        ParentalLevel::plLowest,
        0, -1, true, true); // categoryID, childID, browse, watched

    // Build tree
    auto *root = new MythGenericTree("Video Home", kRootNode, false);
    AddFileNode(root, "The Flash", &metadata);
    QCOMPARE(root->childCount(), 1);

    // Validate GetText call
    auto *child = root->getChildAt(0);
    QVERIFY(child != nullptr);
    QCOMPARE(child->GetText(),            QString("The Flash"));
    QCOMPARE(child->GetText("filename"),  QString("TV Series/Flash (2014)/Season 3/Flash (2014) - S03E04.mkv"));
    QCOMPARE(child->GetText("director"),  QString("Stefan Pleszczynski"));
    QCOMPARE(child->GetText("rating"),    QString("TV-14"));
    QCOMPARE(child->GetText("length"),    QString("45 minute(s)"));
    QCOMPARE(child->GetText("playcount"), QString("5"));
    QCOMPARE(child->GetText("year"),      QString("2016"));
    QCOMPARE(child->GetText("season"),    QString("3"));
    QCOMPARE(child->GetText("episode"),   QString("4"));
    QCOMPARE(child->GetText("s00e00"),    QString("s03e04"));
    QCOMPARE(child->GetText("00x00"),     QString("3x04"));

    // Validate all possible GetImage values
    QCOMPARE(child->GetImage(),                 QString());
    QCOMPARE(child->GetImage("coverfile"),      QString("The Flash (2014) Season 3_coverart.jpg"));
    QCOMPARE(child->GetImage("screenshotfile"), QString("The Flash (2014) Season 3x4_screenshot.jpg"));
    QCOMPARE(child->GetImage("screenshotfile"), QString("The Flash (2014) Season 3x4_screenshot.jpg"));
    QCOMPARE(child->GetImage("bannerfile"),     QString("The Flash (2014) Season 3_banner.jpg"));
    QCOMPARE(child->GetImage("fanartfile"),     QString("The Flash (2014) Season 3_fanart.jpg"));
    QCOMPARE(child->GetImage("smartimage"),     QString("The Flash (2014) Season 3x4_screenshot.jpg"));
    QCOMPARE(child->GetImage("buttonimage"),    QString("The Flash (2014) Season 3x4_screenshot.jpg"));
    // Also included in GetText.
    QCOMPARE(child->GetText("coverfile"),       QString("The Flash (2014) Season 3_coverart.jpg"));
    QCOMPARE(child->GetText("screenshotfile"),  QString("The Flash (2014) Season 3x4_screenshot.jpg"));
    QCOMPARE(child->GetText("bannerfile"),      QString("The Flash (2014) Season 3_banner.jpg"));
    QCOMPARE(child->GetText("fanartfile"),      QString("The Flash (2014) Season 3_fanart.jpg"));
    QCOMPARE(child->GetText("smartimage"),      QString("The Flash (2014) Season 3x4_screenshot.jpg"));

    // Validate all possible GetState values
    QCOMPARE(child->GetState(),                  QString());
    QCOMPARE(child->GetState("trailerstate"),    QString("hasTrailer"));
    QCOMPARE(child->GetState("userratingstate"), QString("7")); // not 7.8
    QCOMPARE(child->GetState("watchedstate"),    QString("yes"));
    QCOMPARE(child->GetState("videolevel"),      QString("Lowest"));
    // Also included in GetText.
    QCOMPARE(child->GetText("trailerstate"),     QString("hasTrailer"));
    QCOMPARE(child->GetText("userratingstate"),  QString("7")); // not 7.8
    QCOMPARE(child->GetText("watchedstate"),     QString("yes"));
    QCOMPARE(child->GetText("videolevel"),       QString("Lowest"));

    delete root;
}

void TestVideoList::cleanupTestCase()
{
}

QTEST_GUILESS_MAIN(TestVideoList)
