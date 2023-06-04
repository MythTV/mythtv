/*
 *  Class TestMythGenericTree
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

#include "test_mythgenerictree.h"

void TestMythGenericTree::initTestCase()
{
}

void TestMythGenericTree::test_text(void)
{
    MythGenericTree node("title");
    QCOMPARE(QString("title"), node.GetText());

    node.SetText("new title");
    QCOMPARE(QString("new title"), node.GetText());

    node.SetText("alpha", "key1");
    node.SetText("beta",  "key2");
    node.SetText("gamma", "key3");

    QCOMPARE(QString("alpha"), node.GetText("key1"));
    QCOMPARE(QString("beta"),  node.GetText("key2"));
    QCOMPARE(QString("gamma"), node.GetText("key3"));

    node.SetText("delta", "key1");
    QCOMPARE(QString("delta"), node.GetText("key1"));

    InfoMap map = {{"one", "uno"}, {"two", "dos"}, {"three", "tres"}};
    node.SetTextFromMap(map);
    QCOMPARE(QString("uno"),  node.GetText("one"));
    QCOMPARE(QString("dos"),  node.GetText("two"));
    QCOMPARE(QString("tres"), node.GetText("three"));
}

void TestMythGenericTree::test_image(void)
{
    MythGenericTree node("no title");

    node.SetImage("alpha", "key1");
    node.SetImage("beta",  "key2");
    node.SetImage("gamma", "key3");

    QCOMPARE(QString("alpha"), node.GetImage("key1"));
    QCOMPARE(QString("beta"),  node.GetImage("key2"));
    QCOMPARE(QString("gamma"), node.GetImage("key3"));

    node.SetImage("delta", "key1");
    QCOMPARE(QString("delta"), node.GetImage("key1"));

    InfoMap map = {{"one", "uno"}, {"two", "dos"}, {"three", "tres"}};
    node.SetImageFromMap(map);
    QCOMPARE(QString("uno"),  node.GetImage("one"));
    QCOMPARE(QString("dos"),  node.GetImage("two"));
    QCOMPARE(QString("tres"), node.GetImage("three"));
}

void TestMythGenericTree::test_state(void)
{
    MythGenericTree node("no title");

    node.DisplayState("alpha", "key1");
    node.DisplayState("beta",  "key2");
    node.DisplayState("gamma", "key3");

    QCOMPARE(QString("alpha"), node.GetState("key1"));
    QCOMPARE(QString("beta"),  node.GetState("key2"));
    QCOMPARE(QString("gamma"), node.GetState("key3"));

    node.DisplayState("delta", "key1");
    QCOMPARE(QString("delta"), node.GetState("key1"));

    InfoMap map = {{"one", "uno"}, {"two", "dos"}, {"three", "tres"}};
    node.DisplayStateFromMap(map);
    QCOMPARE(QString("uno"),  node.GetState("one"));
    QCOMPARE(QString("dos"),  node.GetState("two"));
    QCOMPARE(QString("tres"), node.GetState("three"));
}

static QString test_cb_fn(const QString &name, [[maybe_unused]] void *data)
{
    if (name == QStringLiteral("key2"))
        return QStringLiteral("beta");
    if (name == QStringLiteral("key3"))
        return QStringLiteral("gamma");
    if (name == QStringLiteral("two"))
        return QStringLiteral("dos");
    if (name == QStringLiteral("three"))
        return QStringLiteral("tres");
    return {};
}

void TestMythGenericTree::test_text_cb(void)
{
    MythGenericTree node("title");
    node.SetTextCb(test_cb_fn, nullptr);

    QCOMPARE(QString("title"), node.GetText());

    node.SetText("new title");
    QCOMPARE(QString("new title"), node.GetText());

    node.SetText("alpha", "key1");
    QCOMPARE(QString("alpha"), node.GetText("key1"));
    QCOMPARE(QString("beta"),  node.GetText("key2"));
    QCOMPARE(QString("gamma"), node.GetText("key3"));

    node.SetText("delta", "key1");
    QCOMPARE(QString("delta"), node.GetText("key1"));

    InfoMap map = {{"one", "uno"}};
    node.SetTextFromMap(map);
    QCOMPARE(QString("uno"),  node.GetText("one"));
    QCOMPARE(QString("dos"),  node.GetText("two"));
    QCOMPARE(QString("tres"), node.GetText("three"));
}

void TestMythGenericTree::test_image_cb(void)
{
    MythGenericTree node("no title");

    node.SetImageCb(test_cb_fn, nullptr);

    node.SetImage("alpha", "key1");
    QCOMPARE(QString("alpha"), node.GetImage("key1"));
    QCOMPARE(QString("beta"),  node.GetImage("key2"));
    QCOMPARE(QString("gamma"), node.GetImage("key3"));

    node.SetImage("delta", "key1");
    QCOMPARE(QString("delta"), node.GetImage("key1"));

    InfoMap map = {{"one", "uno"}};
    node.SetImageFromMap(map);
    QCOMPARE(QString("uno"),  node.GetImage("one"));
    QCOMPARE(QString("dos"),  node.GetImage("two"));
    QCOMPARE(QString("tres"), node.GetImage("three"));
}

void TestMythGenericTree::test_state_cb(void)
{
    MythGenericTree node("no title");

    node.SetStateCb(test_cb_fn, nullptr);

    node.DisplayState("alpha", "key1");
    QCOMPARE(QString("alpha"), node.GetState("key1"));
    QCOMPARE(QString("beta"),  node.GetState("key2"));
    QCOMPARE(QString("gamma"), node.GetState("key3"));

    node.DisplayState("delta", "key1");
    QCOMPARE(QString("delta"), node.GetState("key1"));

    InfoMap map = {{"one", "uno"}};
    node.DisplayStateFromMap(map);
    QCOMPARE(QString("uno"),  node.GetState("one"));
    QCOMPARE(QString("dos"),  node.GetState("two"));
    QCOMPARE(QString("tres"), node.GetState("three"));
}

void TestMythGenericTree::cleanupTestCase()
{
}

QTEST_APPLESS_MAIN(TestMythGenericTree)
