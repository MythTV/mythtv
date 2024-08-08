// c/c++
#include <iostream>
#include <unistd.h>
#include <utility>

// qt
#include <QKeyEvent>
#include <QSqlDriver>
#include <QSqlField>

// MythTV
#include <libmyth/mythcontext.h>
#include <libmythbase/mythdate.h>
#include <libmythbase/mythdb.h>
#include <libmythmetadata/musicmetadata.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythscreentype.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuicheckbox.h>
#include <libmythui/mythuihelper.h>
#include <libmythui/mythuispinbox.h>
#include <libmythui/mythuitext.h>
#include <libmythui/mythuitextedit.h>

// mythmusic
#include "musiccommon.h"
#include "musicdata.h"
#include "smartplaylist.h"

struct SmartPLField
{
    QString          m_name;
    QString          m_sqlName;
    SmartPLFieldType m_type;
    int              m_minValue;
    int              m_maxValue;
    int              m_defaultValue;
};

static const std::array<const SmartPLField,13> SmartPLFields
{{
    { "",              "",                               ftString,   0,    0,    0 },
    { "Artist",        "music_artists.artist_name",      ftString,   0,    0,    0 },
    { "Album",         "music_albums.album_name",        ftString,   0,    0,    0 },
    { "Title",         "music_songs.name",               ftString,   0,    0,    0 },
    { "Genre",         "music_genres.genre",             ftString,   0,    0,    0 },
    { "Year",          "music_songs.year",               ftNumeric,  1900, 2099, 2000 },
    { "Track No.",     "music_songs.track",              ftNumeric,  0,    99,   0 },
    { "Rating",        "music_songs.rating",             ftNumeric,  0,    10,   0 },
    { "Play Count",    "music_songs.numplays",           ftNumeric,  0,    9999, 0 },
    { "Compilation",   "music_albums.compilation",       ftBoolean,  0,    0,    0 },
    { "Comp. Artist",  "music_comp_artists.artist_name", ftString,   0,    0,    0 },
    { "Last Play",     "FROM_DAYS(TO_DAYS(music_songs.lastplay))",
                                                         ftDate,     0,    0,    0 },
    { "Date Imported", "FROM_DAYS(TO_DAYS(music_songs.date_entered))",
                                                         ftDate,     0,    0,    0 },
}};

struct SmartPLOperator
{
    QString m_name;
    int     m_noOfArguments;
    bool    m_stringOnly;
    bool    m_validForBoolean;
};

static const std::array<const SmartPLOperator,11> SmartPLOperators
{{
    { "is equal to",      1,  false, true },
    { "is not equal to",  1,  false, true },
    { "is greater than",  1,  false, false },
    { "is less than",     1,  false, false },
    { "starts with",      1,  true,  false },
    { "ends with",        1,  true,  false },
    { "contains",         1,  true,  false },
    { "does not contain", 1,  true,  false },
    { "is between",       2,  false, false },
    { "is set",           0,  false, false },
    { "is not set",       0,  false, false },
}};

static const SmartPLOperator *lookupOperator(const QString& name)
{
    for (const auto & oper : SmartPLOperators)
    {
        if (oper.m_name == name)
            return &oper;
    }
    return nullptr;
}

static const SmartPLField *lookupField(const QString& name)
{
    for (const auto & field : SmartPLFields)
    {
        if (field.m_name == name)
            return &field;
    }
    return nullptr;
}

QString formattedFieldValue(const QVariant &value)
{
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QSqlField field("", value.type());
#else
    QSqlField field("", value.metaType());
#endif
    if (value.isNull())
        field.clear();
    else
        field.setValue(value);

    MSqlQuery query(MSqlQuery::InitCon());
    QString result = QString::fromUtf8(query.driver()->formatValue(field).toLatin1().data());
    return result;
}

static QString evaluateDateValue(QString sDate)
{
    if (sDate.startsWith("$DATE"))
    {
        QDate date = MythDate::current().toLocalTime().date();

        if (sDate.length() > 9)
        {
            bool bNegative = false;
            if (sDate[6] == '-')
                bNegative = true;

            if (sDate.endsWith(" days"))
                sDate = sDate.left(sDate.length() - 5);

            int nDays = sDate.mid(8).toInt();
            if (bNegative)
                nDays = -nDays;

            date = date.addDays(nDays);
        }

        return date.toString(Qt::ISODate);
    }

    return sDate;
}

QString getCriteriaSQL(const QString& fieldName, const QString &operatorName,
                       QString value1, QString value2)
{
    QString result;

    if (fieldName.isEmpty())
        return result;

    const SmartPLField *Field = lookupField(fieldName);
    if (!Field)
    {
        return "";
    }

    result = Field->m_sqlName;

    const SmartPLOperator *Operator = lookupOperator(operatorName);
    if (!Operator)
    {
        return {};
    }

    // convert boolean and date values
    if (Field->m_type == ftBoolean)
    {
        // compilation field uses 0 = false;  1 = true
        value1 = (value1 == "Yes") ? "1":"0";
        value2 = (value2 == "Yes") ? "1":"0";
    }
    else if (Field->m_type == ftDate)
    {
        value1 = evaluateDateValue(value1);
        value2 = evaluateDateValue(value2);
    }

    if (Operator->m_name == "is equal to")
    {
        result = result + " = " + formattedFieldValue(value1);
    }
    else if (Operator->m_name == "is not equal to")
    {
        result = result + " != " + formattedFieldValue(value1);
    }
    else if (Operator->m_name == "is greater than")
    {
        result = result + " > " + formattedFieldValue(value1);
    }
    else if (Operator->m_name == "is less than")
    {
        result = result + " < " + formattedFieldValue(value1);
    }
    else if (Operator->m_name == "starts with")
    {
        result = result + " LIKE " + formattedFieldValue(value1 + QString("%"));
    }
    else if (Operator->m_name == "ends with")
    {
        result = result + " LIKE " + formattedFieldValue(QString("%") + value1);
    }
    else if (Operator->m_name == "contains")
    {
        result = result + " LIKE " + formattedFieldValue(QString("%") + value1 + "%");
    }
    else if (Operator->m_name == "does not contain")
    {
        result = result + " NOT LIKE " + formattedFieldValue(QString("%") + value1 + "%");
    }
    else if (Operator->m_name == "is between")
    {
        result = result + " BETWEEN " + formattedFieldValue(value1) +
                          " AND " + formattedFieldValue(value2);
    }
    else if (Operator->m_name == "is set")
    {
        result = result + " IS NOT NULL";
    }
    else if (Operator->m_name == "is not set")
    {
        result = result + " IS NULL";
    }
    else
    {
        result.clear();
        LOG(VB_GENERAL, LOG_ERR,
            QString("getCriteriaSQL(): invalid operator '%1'")
                .arg(Operator->m_name));
    }

    return result;
}

QString getOrderBySQL(const QString& orderByFields)
{
    if (orderByFields.isEmpty())
        return {};

    QStringList list = orderByFields.split(",");
    QString fieldName;
    QString result;
    QString order;
    bool bFirst = true;

    for (int x = 0; x < list.count(); x++)
    {
        fieldName = list[x].trimmed();
        const SmartPLField *Field = lookupField(fieldName.left(fieldName.length() - 4));
        if (Field)
        {
            if (fieldName.right(3) == "(D)")
                order = " DESC";
            else
                order = " ASC";

           if (bFirst)
           {
               bFirst = false;
               result = " ORDER BY " + Field->m_sqlName + order;
           }
           else
           {
               result += ", " + Field->m_sqlName + order;
           }
        }
    }

    return result;
}

QString getSQLFieldName(const QString &fieldName)
{
    const SmartPLField *Field = lookupField(fieldName);
    if (!Field)
    {
        return "";
    }

    return Field->m_sqlName;
}

/*
///////////////////////////////////////////////////////////////////////
*/

QString SmartPLCriteriaRow::getSQL(void) const
{
    if (m_field.isEmpty())
        return {};

    QString result;

    result = getCriteriaSQL(m_field, m_operator, m_value1, m_value2);

    return result;
}

// return false on error
bool SmartPLCriteriaRow::saveToDatabase(int smartPlaylistID) const
{
    // save playlistitem to database

    if (m_field.isEmpty())
        return true;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO music_smartplaylist_items (smartplaylistid, field, operator,"
                  " value1, value2)"
                  "VALUES (:SMARTPLAYLISTID, :FIELD, :OPERATOR, :VALUE1, :VALUE2);");
    query.bindValue(":SMARTPLAYLISTID", smartPlaylistID);
    query.bindValueNoNull(":FIELD", m_field);
    query.bindValueNoNull(":OPERATOR", m_operator);
    query.bindValueNoNull(":VALUE1", m_value1);
    query.bindValueNoNull(":VALUE2", m_value2);

    if (!query.exec())
    {
        MythDB::DBError("Inserting new smartplaylist item", query);
        return false;
    }

    return true;
}

QString SmartPLCriteriaRow::toString(void) const
{
    const SmartPLOperator *PLOperator = lookupOperator(m_operator);
    if (PLOperator)
    {
        QString result;
        if (PLOperator->m_noOfArguments == 0)
            result = m_field + " " + m_operator;
        else if (PLOperator->m_noOfArguments == 1)
            result = m_field + " " + m_operator + " " + m_value1;
        else
        {
            result = m_field + " " + m_operator + " " + m_value1;
            result += " " + tr("and") + " " + m_value2;
        }

        return result;
    }

    return {};
}

/*
---------------------------------------------------------------------
*/

SmartPlaylistEditor::~SmartPlaylistEditor(void)
{
    while (!m_criteriaRows.empty())
    {
        delete m_criteriaRows.back();
        m_criteriaRows.pop_back();
    }

    delete m_tempCriteriaRow;
}


bool SmartPlaylistEditor::Create(void)
{
    if (!LoadWindowFromXML("music-ui.xml", "smartplaylisteditor", this))
        return false;

    bool err = false;

    UIUtilE::Assign(this, m_categorySelector,  "categoryselector",  &err);
    UIUtilE::Assign(this, m_categoryButton,    "categorybutton",    &err);
    UIUtilE::Assign(this, m_titleEdit,         "titleedit",         &err);
    UIUtilE::Assign(this, m_matchSelector,     "matchselector",     &err);
    UIUtilE::Assign(this, m_criteriaList,      "criterialist",      &err);
    UIUtilE::Assign(this, m_orderBySelector,   "orderbyselector",   &err);
    UIUtilE::Assign(this, m_orderByButton,     "orderbybutton",     &err);
    UIUtilE::Assign(this, m_matchesText,       "matchestext",       &err);
    UIUtilE::Assign(this, m_limitSpin,         "limitspin",         &err);

    UIUtilE::Assign(this, m_cancelButton,      "cancelbutton",      &err);
    UIUtilE::Assign(this, m_saveButton,        "savebutton",        &err);
    UIUtilE::Assign(this, m_showResultsButton, "showresultsbutton", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'smartplaylisteditor'");
        return false;
    }

    getSmartPlaylistCategories();

    new MythUIButtonListItem(m_matchSelector, tr("All"));
    new MythUIButtonListItem(m_matchSelector, tr("Any"));
    connect(m_matchSelector, &MythUIButtonList::itemSelected, this, &SmartPlaylistEditor::updateMatches);

    for (const auto & field : SmartPLFields)
    {
        if (field.m_name == "")
            new MythUIButtonListItem(m_orderBySelector, field.m_name);
        else
            new MythUIButtonListItem(m_orderBySelector, field.m_name + " (A)");
    }

    m_limitSpin->SetRange(0, 9999, 10);

    connect(m_orderByButton, &MythUIButton::Clicked, this, &SmartPlaylistEditor::orderByClicked);
    connect(m_saveButton, &MythUIButton::Clicked, this, &SmartPlaylistEditor::saveClicked);
    connect(m_cancelButton, &MythUIButton::Clicked, this, &MythScreenType::Close);
    connect(m_categoryButton, &MythUIButton::Clicked, this, &SmartPlaylistEditor::showCategoryMenu);
    connect(m_showResultsButton, &MythUIButton::Clicked, this, &SmartPlaylistEditor::showResultsClicked);
    connect(m_criteriaList, &MythUIButtonList::itemClicked, this, &SmartPlaylistEditor::editCriteria);

    BuildFocusList();

    return true;
}

bool SmartPlaylistEditor::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Music", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "MENU")
        {
            showCriteriaMenu();
        }
        else if (action == "DELETE" && GetFocusWidget() == m_criteriaList)
        {
            deleteCriteria();
        }
        else if (action == "EDIT" && GetFocusWidget() == m_criteriaList)
        {
            editCriteria();
        }
        else
        {
            handled = false;
        }
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void SmartPlaylistEditor::customEvent(QEvent *event)
{
    if (auto *dce = dynamic_cast<DialogCompletionEvent*>(event))
    {
        // make sure the user didn't ESCAPE out of the menu
        if (dce->GetResult() < 0)
            return;

        QString resultid   = dce->GetId();
        QString resulttext = dce->GetResultText();
        if (resultid == "categorymenu")
        {
            if (resulttext == tr("New Category"))
            {
                MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
                QString label = tr("Enter Name Of New Category");

                auto *input = new MythTextInputDialog(popupStack, label);

                connect(input, &MythTextInputDialog::haveResult,
                        this, &SmartPlaylistEditor::newCategory);

                if (input->Create())
                    popupStack->AddScreen(input);
                else
                    delete input;
            }
            else if (resulttext == tr("Delete Category"))
            {
                startDeleteCategory(m_categorySelector->GetValue());
            }
            else if (resulttext == tr("Rename Category"))
            {
                MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
                QString label = tr("Enter New Name For Category: %1").arg(m_categorySelector->GetValue());

                auto *input = new MythTextInputDialog(popupStack, label);

                connect(input, &MythTextInputDialog::haveResult,
                        this, &SmartPlaylistEditor::renameCategory);

                if (input->Create())
                    popupStack->AddScreen(input);
                else
                    delete input;
            }
        }
    }
}

void SmartPlaylistEditor::editCriteria(void)
{
    if (m_tempCriteriaRow)
    {
        delete m_tempCriteriaRow;
        m_tempCriteriaRow = nullptr;
    }

    MythUIButtonListItem *item = m_criteriaList->GetItemCurrent();

    if (!item)
        return;

    auto *row = item->GetData().value<SmartPLCriteriaRow*>();

    if (!row)
        return;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *editor = new CriteriaRowEditor(popupStack, row);

    if (!editor->Create())
    {
        delete editor;
        return;
    }

    connect(editor, &CriteriaRowEditor::criteriaChanged, this, &SmartPlaylistEditor::criteriaChanged);

    popupStack->AddScreen(editor);
}

void SmartPlaylistEditor::deleteCriteria(void)
{
    // make sure we have something to delete
    MythUIButtonListItem *item = m_criteriaList->GetItemCurrent();

    if (!item)
        return;

    ShowOkPopup(tr("Delete Criteria?"), this, &SmartPlaylistEditor::doDeleteCriteria, true);
}

void SmartPlaylistEditor::doDeleteCriteria(bool doit)
{
    if (doit)
    {
        MythUIButtonListItem *item = m_criteriaList->GetItemCurrent();
        if (!item)
            return;

        auto *row = item->GetData().value<SmartPLCriteriaRow*>();

        if (!row)
            return;

        m_criteriaRows.removeAll(row);
        m_criteriaList->RemoveItem(item);

        criteriaChanged();
    }
}

void SmartPlaylistEditor::addCriteria(void)
{
    /*
    SmartPLCriteriaRow *row = new SmartPLCriteriaRow();
    m_criteriaRows.append(row);

    MythUIButtonListItem *item = new MythUIButtonListItem(m_criteriaList, row->toString(), QVariant::fromValue(row));

    m_criteriaList->SetItemCurrent(item);

    editCriteria();
    */

    delete m_tempCriteriaRow;
    m_tempCriteriaRow = new SmartPLCriteriaRow();

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *editor = new CriteriaRowEditor(popupStack, m_tempCriteriaRow);

    if (!editor->Create())
    {
        delete editor;
        return;
    }

    connect(editor, &CriteriaRowEditor::criteriaChanged, this, &SmartPlaylistEditor::criteriaChanged);

    popupStack->AddScreen(editor);
}

void SmartPlaylistEditor::criteriaChanged()
{
    MythUIButtonListItem *item = nullptr;

    if (m_tempCriteriaRow)
    {
        // this is a new row so add it to the list
        m_criteriaRows.append(m_tempCriteriaRow);

        item = new MythUIButtonListItem(m_criteriaList, m_tempCriteriaRow->toString(),
                                        QVariant::fromValue(m_tempCriteriaRow));

        m_criteriaList->SetItemCurrent(item);

        m_tempCriteriaRow = nullptr;
    }
    else
    {
        // update the existing row
        item = m_criteriaList->GetItemCurrent();
        if (!item)
            return;

        auto *row = item->GetData().value<SmartPLCriteriaRow*>();

        if (!row)
            return;

        item->SetText(row->toString());
    }

    updateMatches();
}

void SmartPlaylistEditor::showCategoryMenu(void)
{
    QString label = tr("Category Actions");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *menu = new MythDialogBox(label, popupStack, "actionmenu");

    if (!menu->Create())
    {
        delete menu;
        return;
    }

    menu->SetReturnEvent(this, "categorymenu");

    menu->AddButton(tr("New Category"),    nullptr);
    menu->AddButton(tr("Delete Category"), nullptr);
    menu->AddButton(tr("Rename Category"), nullptr);

    popupStack->AddScreen(menu);
}

void SmartPlaylistEditor::showCriteriaMenu(void)
{
    QString label = tr("Criteria Actions");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *menu = new MythDialogBox(label, popupStack, "actionmenu");

    if (!menu->Create())
    {
        delete menu;
        return;
    }

    menu->SetReturnEvent(this, "criteriamenu");

    MythUIButtonListItem *item = m_criteriaList->GetItemCurrent();

    if (item)
        menu->AddButton(tr("Edit Criteria"), &SmartPlaylistEditor::editCriteria);

    menu->AddButton(tr("Add Criteria"), &SmartPlaylistEditor::addCriteria);

    if (item)
        menu->AddButton(tr("Delete Criteria"), &SmartPlaylistEditor::deleteCriteria);

    popupStack->AddScreen(menu);
}

void SmartPlaylistEditor::titleChanged(void)
{
    m_saveButton->SetEnabled((m_playlistIsValid && !m_titleEdit->GetText().isEmpty()));
}

void SmartPlaylistEditor::updateMatches(void)
{
    QString sql =
        "SELECT count(*) "
        "FROM music_songs "
        "LEFT JOIN music_artists ON "
        "    music_songs.artist_id=music_artists.artist_id "
        "LEFT JOIN music_albums ON music_songs.album_id=music_albums.album_id "
        "LEFT JOIN music_artists AS music_comp_artists ON "
        "    music_albums.artist_id=music_comp_artists.artist_id "
        "LEFT JOIN music_genres ON music_songs.genre_id=music_genres.genre_id ";

    sql += getWhereClause();

    m_matchesCount = 0;

    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.exec(sql))
        MythDB::DBError("SmartPlaylistEditor::updateMatches", query);
    else if (query.next())
        m_matchesCount = query.value(0).toInt();

    m_matchesText->SetText(QString::number(m_matchesCount));

    m_playlistIsValid = !m_criteriaRows.empty();
    m_showResultsButton->SetEnabled((m_matchesCount > 0));
    titleChanged();
}

void SmartPlaylistEditor::saveClicked(void)
{
    // save smartplaylist to database

    QString name = m_titleEdit->GetText();
    QString category = m_categorySelector->GetValue();
    QString matchType = (m_matchSelector->GetValue() == tr("All") ? "All" : "Any");
    QString orderBy = m_orderBySelector->GetValue();
    QString limit = m_limitSpin->GetValue();

    // lookup categoryid
    int categoryid = SmartPlaylistEditor::lookupCategoryID(category);

    // easier to delete any existing smartplaylist and recreate a new one
    if (!m_newPlaylist)
        SmartPlaylistEditor::deleteSmartPlaylist(m_originalCategory, m_originalName);
    else
        SmartPlaylistEditor::deleteSmartPlaylist(category, name);

    MSqlQuery query(MSqlQuery::InitCon());
    // insert new smartplaylist
    query.prepare("INSERT INTO music_smartplaylists (name, categoryid, matchtype, orderby, limitto) "
                "VALUES (:NAME, :CATEGORYID, :MATCHTYPE, :ORDERBY, :LIMIT);");
    query.bindValue(":NAME", name);
    query.bindValue(":CATEGORYID", categoryid);
    query.bindValue(":MATCHTYPE", matchType);
    query.bindValue(":ORDERBY", orderBy);
    query.bindValue(":LIMIT", limit);

    if (!query.exec())
    {
        MythDB::DBError("Inserting new playlist", query);
        return;
    }

    // get smartplaylistid
    int ID = -1;
    query.prepare("SELECT smartplaylistid FROM music_smartplaylists "
                  "WHERE categoryid = :CATEGORYID AND name = :NAME;");
    query.bindValue(":CATEGORYID", categoryid);
    query.bindValue(":NAME", name);
    if (query.exec())
    {
        if (query.isActive() && query.size() > 0)
        {
            query.first();
            ID = query.value(0).toInt();
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Failed to find ID for smartplaylist: %1").arg(name));
            return;
        }
    }
    else
    {
        MythDB::DBError("Getting smartplaylist ID", query);
        return;
    }

    // save smartplaylist items
    for (const auto & row : std::as_const(m_criteriaRows))
        row->saveToDatabase(ID);

    emit smartPLChanged(category, name);

    Close();
}

void SmartPlaylistEditor::newSmartPlaylist(const QString& category)
{
    m_categorySelector->SetValue(category);
    m_titleEdit->Reset();
    m_originalCategory = category;
    m_originalName.clear();

    m_newPlaylist = true;

    updateMatches();
}

void SmartPlaylistEditor::editSmartPlaylist(const QString& category, const QString& name)
{
    m_originalCategory = category;
    m_originalName = name;
    m_newPlaylist = false;
    loadFromDatabase(category, name);
    updateMatches();
}

void SmartPlaylistEditor::loadFromDatabase(const QString& category, const QString& name)
{
    // load smartplaylist from database
    int categoryid = SmartPlaylistEditor::lookupCategoryID(category);

    MSqlQuery query(MSqlQuery::InitCon());
    int ID = -1;

    query.prepare("SELECT smartplaylistid, name, categoryid, matchtype, orderby, limitto "
                  "FROM music_smartplaylists WHERE name = :NAME AND categoryid = :CATEGORYID;");
    query.bindValue(":NAME", name);
    query.bindValue(":CATEGORYID", categoryid);
    if (query.exec())
    {
        if (query.isActive() && query.size() > 0)
        {
            query.first();
            ID = query.value(0).toInt();
            m_titleEdit->SetText(name);
            m_categorySelector->SetValue(category);
            if (query.value(3).toString() == "All")
                m_matchSelector->SetValue(tr("All"));
            else
                m_matchSelector->SetValue(tr("Any"));

            QString orderBy = query.value(4).toString();
            if (!m_orderBySelector->Find(orderBy))
            {
                // not found so add it to the selector
                new MythUIButtonListItem(m_orderBySelector, orderBy);
                m_orderBySelector->SetValue(orderBy);
            }

            m_limitSpin->SetValue(query.value(5).toInt());
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Cannot find smartplaylist: %1").arg(name));
            return;
        }
    }
    else
    {
        MythDB::DBError("Load smartplaylist", query);
        return;
    }

    m_criteriaList->Reset();

    query.prepare("SELECT field, operator, value1, value2 "
                  "FROM music_smartplaylist_items WHERE smartplaylistid = :ID "
                  "ORDER BY smartplaylistitemid;");
    query.bindValue(":ID", ID);
    if (!query.exec())
        MythDB::DBError("Load smartplaylist items", query);

    if (query.size() > 0)
    {
        while (query.next())
        {
            QString Field = query.value(0).toString();
            QString Operator = query.value(1).toString();
            QString Value1 = query.value(2).toString();
            QString Value2 = query.value(3).toString();
            // load smartplaylist items
            auto *row = new SmartPLCriteriaRow(Field, Operator, Value1, Value2);
            m_criteriaRows.append(row);

            new MythUIButtonListItem(m_criteriaList, row->toString(), QVariant::fromValue(row));
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_WARNING,
            QString("Got no smartplaylistitems for ID: ").arg(ID));
    }
}

void SmartPlaylistEditor::newCategory(const QString &category)
{
    // insert new smartplaylistcategory

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO music_smartplaylist_categories (name) "
                "VALUES (:NAME);");
    query.bindValue(":NAME", category);

    if (!query.exec())
    {
        MythDB::DBError("Inserting new smartplaylist category", query);
        return;
    }

    getSmartPlaylistCategories();
    m_categorySelector->SetValue(category);
}

void SmartPlaylistEditor::startDeleteCategory(const QString &category)
{
    if (category.isEmpty())
        return;

//FIXME::
#if 0
    if (!MythPopupBox::showOkCancelPopup(GetMythMainWindow(),
            "Delete Category",
            tr("Are you sure you want to delete this Category?")
            + "\n\n\"" + category + "\"\n\n"
            + tr("It will also delete any Smart Playlists belonging to this category."),
             false))
        return;

    SmartPlaylistEditor::deleteCategory(category);
#endif
    getSmartPlaylistCategories();
    m_titleEdit->Reset();
}

void SmartPlaylistEditor::renameCategory(const QString &category)
{
    if (m_categorySelector->GetValue() == category)
        return;

    // change the category
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE music_smartplaylist_categories SET name = :NEW_CATEGORY "
                  "WHERE name = :OLD_CATEGORY;");
    query.bindValue(":OLD_CATEGORY", m_categorySelector->GetValue());
    query.bindValue(":NEW_CATEGORY", category);

    if (!query.exec())
        MythDB::DBError("Rename smartplaylist", query);

    if (!m_newPlaylist)
        m_originalCategory = m_categorySelector->GetValue();

    getSmartPlaylistCategories();
    m_categorySelector->SetValue(category);
}

QString SmartPlaylistEditor::getSQL(const QString& fields)
{
    QString sql;
    QString whereClause;
    QString orderByClause;
    QString limitClause;
    sql = "SELECT " + fields + " FROM music_songs "
          "LEFT JOIN music_artists ON music_songs.artist_id=music_artists.artist_id "
          "LEFT JOIN music_albums ON music_songs.album_id=music_albums.album_id "
          "LEFT JOIN music_artists AS music_comp_artists ON music_albums.artist_id=music_comp_artists.artist_id "
          "LEFT JOIN music_genres ON music_songs.genre_id=music_genres.genre_id ";

    whereClause = getWhereClause();
    orderByClause = getOrderByClause();
    if (m_limitSpin->GetIntValue() > 0)
        limitClause = " LIMIT " + m_limitSpin->GetValue();

    sql = sql + whereClause + orderByClause + limitClause;

    return sql;
}

QString SmartPlaylistEditor::getOrderByClause(void)
{
    return getOrderBySQL(m_orderBySelector->GetValue());
}

QString SmartPlaylistEditor::getWhereClause(void)
{
    if (m_criteriaRows.empty())
        return {};

    bool bFirst = true;
    QString sql = "WHERE ";

    for (const auto & row : std::as_const(m_criteriaRows))
    {
        QString criteria = row->getSQL();
        if (criteria.isEmpty())
            continue;

        if (bFirst)
        {
            sql += criteria;
            bFirst = false;
        }
        else
        {
            if (m_matchSelector->GetValue() == tr("Any"))
                sql += " OR " + criteria;
            else
                sql += " AND " + criteria;
        }
    }

    return sql;
}

void SmartPlaylistEditor::showResultsClicked(void)
{
    QString sql = getSQL("song_id, music_artists.artist_name, album_name, "
                         "name, genre, music_songs.year, track");

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *resultViewer = new SmartPLResultViewer(mainStack);

    if (!resultViewer->Create())
    {
        delete resultViewer;
        return;
    }

    resultViewer->setSQL(sql);

    mainStack->AddScreen(resultViewer);
}

void SmartPlaylistEditor::orderByClicked(void)
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *orderByDialog = new SmartPLOrderByDialog(popupStack);

    if (!orderByDialog->Create())
    {
        delete orderByDialog;
        return;
    }

    orderByDialog->setFieldList(m_orderBySelector->GetValue());

    connect(orderByDialog, qOverload<QString>(&SmartPLOrderByDialog::orderByChanged),
            this, &SmartPlaylistEditor::orderByChanged);

    popupStack->AddScreen(orderByDialog);
}

void SmartPlaylistEditor::orderByChanged(const QString& orderBy)
{
    if (m_orderBySelector->MoveToNamedPosition(orderBy))
        return;

    // not found so add it to the selector
    new MythUIButtonListItem(m_orderBySelector, orderBy);
    m_orderBySelector->SetValue(orderBy);
}

void SmartPlaylistEditor::getSmartPlaylistCategories(void)
{
    m_categorySelector->Reset();
    MSqlQuery query(MSqlQuery::InitCon());

    if (query.exec("SELECT name FROM music_smartplaylist_categories ORDER BY name;"))
    {
        if (query.isActive() && query.size() > 0)
        {
            while (query.next())
                new MythUIButtonListItem(m_categorySelector, query.value(0).toString());
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR,
                "Could not find any smartplaylist categories");
        }
    }
    else
    {
        MythDB::DBError("Load smartplaylist categories", query);
    }
}

// static function to delete a smartplaylist and any associated smartplaylist items
bool SmartPlaylistEditor::deleteSmartPlaylist(const QString &category, const QString& name)
{
    // get categoryid
    int categoryid = SmartPlaylistEditor::lookupCategoryID(category);

    MSqlQuery query(MSqlQuery::InitCon());

    // get playlist ID
    int ID = -1;
    query.prepare("SELECT smartplaylistid FROM music_smartplaylists WHERE name = :NAME "
                  "AND categoryid = :CATEGORYID;");
    query.bindValue(":NAME", name);
    query.bindValue(":CATEGORYID", categoryid);
    if (query.exec())
    {
        if (query.isActive() && query.size() > 0)
        {
            query.first();
            ID = query.value(0).toInt();
        }
        else
        {
            // not always an error maybe we are trying to delete a playlist
            // that does not exist
            return true;
        }
    }
    else
    {
        MythDB::DBError("Delete smartplaylist", query);
        return false;
    }

    //delete smartplaylist items
    query.prepare("DELETE FROM music_smartplaylist_items WHERE smartplaylistid = :ID;");
    query.bindValue(":ID", ID);
    if (!query.exec())
        MythDB::DBError("Delete smartplaylist items", query);

    //delete smartplaylist
    query.prepare("DELETE FROM music_smartplaylists WHERE smartplaylistid = :ID;");
    query.bindValue(":ID", ID);
    if (!query.exec())
        MythDB::DBError("Delete smartplaylist", query);

    return true;
}

// static function to delete all smartplaylists belonging to the given category
// will also delete any associated smartplaylist items
bool SmartPlaylistEditor::deleteCategory(const QString& category)
{
    int categoryid = SmartPlaylistEditor::lookupCategoryID(category);
    MSqlQuery query(MSqlQuery::InitCon());

    //delete all smartplaylists with the selected category
    query.prepare("SELECT name FROM music_smartplaylists "
                  "WHERE categoryid = :CATEGORYID;");
    query.bindValue(":CATEGORYID", categoryid);
    if (!query.exec())
    {
        MythDB::DBError("Delete SmartPlaylist Category", query);
        return false;
    }

    if (query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            SmartPlaylistEditor::deleteSmartPlaylist(category, query.value(0).toString());
        }
    }

    // delete the category
    query.prepare("DELETE FROM music_smartplaylist_categories WHERE categoryid = :ID;");
    query.bindValue(":ID", categoryid);
    if (!query.exec())
        MythDB::DBError("Delete smartplaylist category", query);

    return true;
}

// static function to lookup the categoryid given its name
int SmartPlaylistEditor::lookupCategoryID(const QString& category)
{
    int ID = -1;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT categoryid FROM music_smartplaylist_categories "
                  "WHERE name = :CATEGORY;");
    query.bindValue(":CATEGORY", category);

    if (query.exec())
    {
        if (query.isActive() && query.size() > 0)
        {
            query.first();
            ID = query.value(0).toInt();
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Failed to find smart playlist category: %1")
                    .arg(category));
            ID = -1;
        }
    }
    else
    {
        MythDB::DBError("Getting category ID", query);
        ID = -1;
    }

    return ID;
}

void  SmartPlaylistEditor::getCategoryAndName(QString &category, QString &name)
{
    category = m_categorySelector->GetValue();
    name = m_titleEdit->GetText();
}

/*
---------------------------------------------------------------------
*/

bool CriteriaRowEditor::Create(void)
{
    if (!LoadWindowFromXML("music-ui.xml", "criteriaroweditor", this))
        return false;

    bool err = false;

    UIUtilE::Assign(this, m_fieldSelector,     "fieldselector",    &err);
    UIUtilE::Assign(this, m_operatorSelector,  "operatorselector", &err);
    UIUtilE::Assign(this, m_value1Edit,        "value1edit",       &err);
    UIUtilE::Assign(this, m_value2Edit,        "value2edit",       &err);
    UIUtilE::Assign(this, m_value1Selector,    "value1selector",   &err);
    UIUtilE::Assign(this, m_value2Selector,    "value2selector",   &err);
    UIUtilE::Assign(this, m_value1Spinbox,     "value1spinbox",    &err);
    UIUtilE::Assign(this, m_value2Spinbox,     "value2spinbox",    &err);
    UIUtilE::Assign(this, m_value1Button,      "value1button",     &err);
    UIUtilE::Assign(this, m_value2Button,      "value2button",     &err);
    UIUtilE::Assign(this, m_cancelButton,      "cancelbutton",     &err);
    UIUtilE::Assign(this, m_saveButton,        "savebutton",       &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'criteriaroweditor'");
        return false;
    }

    updateFields();
    updateOperators();
    updateValues();

    connect(m_fieldSelector, &MythUIButtonList::itemSelected, this, &CriteriaRowEditor::fieldChanged);
    connect(m_operatorSelector, &MythUIButtonList::itemSelected, this, &CriteriaRowEditor::operatorChanged);

    connect(m_value1Edit, &MythUITextEdit::valueChanged, this, &CriteriaRowEditor::valueEditChanged);
    connect(m_value2Edit, &MythUITextEdit::valueChanged, this, &CriteriaRowEditor::valueEditChanged);
    connect(m_value1Selector, &MythUIButtonList::itemSelected, this, &CriteriaRowEditor::valueEditChanged);
    connect(m_value2Selector, &MythUIButtonList::itemSelected, this, &CriteriaRowEditor::valueEditChanged);

    connect(m_value1Button, &MythUIButton::Clicked, this, &CriteriaRowEditor::valueButtonClicked);
    connect(m_value2Button, &MythUIButton::Clicked, this, &CriteriaRowEditor::valueButtonClicked);

    connect(m_cancelButton, &MythUIButton::Clicked, this, &MythScreenType::Close);
    connect(m_saveButton, &MythUIButton::Clicked, this, &CriteriaRowEditor::saveClicked);

    BuildFocusList();

    return true;
}

void CriteriaRowEditor::updateFields(void)
{
    for (const auto & field : SmartPLFields)
        new MythUIButtonListItem(m_fieldSelector, field.m_name);

    m_fieldSelector->SetValue(m_criteriaRow->m_field);
}

void CriteriaRowEditor::updateOperators(void)
{
    for (const auto & oper : SmartPLOperators)
        new MythUIButtonListItem(m_operatorSelector, oper.m_name);

    m_operatorSelector->SetValue(m_criteriaRow->m_operator);
}

void CriteriaRowEditor::valueEditChanged(void)
{
    enableSaveButton();
}

void CriteriaRowEditor::updateValues(void)
{
    m_value1Edit->SetText(m_criteriaRow->m_value1);
    m_value2Edit->SetText(m_criteriaRow->m_value2);
    m_value1Spinbox->SetValue(m_criteriaRow->m_value1);
    m_value2Spinbox->SetValue(m_criteriaRow->m_value2);

    if (!m_value1Selector->MoveToNamedPosition(m_criteriaRow->m_value1))
    {
        // not found so add it to the selector
        new MythUIButtonListItem(m_value1Selector, m_criteriaRow->m_value1);
        m_value1Selector->SetValue(m_criteriaRow->m_value1);
    }

    if (!m_value2Selector->MoveToNamedPosition(m_criteriaRow->m_value2))
    {
        // not found so add it to the selector
        new MythUIButtonListItem(m_value2Selector, m_criteriaRow->m_value2);
        m_value2Selector->SetValue(m_criteriaRow->m_value2);
    }
}

void CriteriaRowEditor::saveClicked()
{
    const SmartPLField *Field = lookupField(m_fieldSelector->GetValue());
    if (!Field)
        return;

    m_criteriaRow->m_field = m_fieldSelector->GetValue();
    m_criteriaRow->m_operator = m_operatorSelector->GetValue();

    if (Field->m_type == ftNumeric)
    {
        m_criteriaRow->m_value1 = m_value1Spinbox->GetValue();
        m_criteriaRow->m_value2 = m_value2Spinbox->GetValue();
    }
    else if (Field->m_type == ftBoolean || Field->m_type == ftDate)
    {
        m_criteriaRow->m_value1 = m_value1Selector->GetValue();
        m_criteriaRow->m_value2 = m_value2Selector->GetValue();
    }
    else // ftString
    {
        m_criteriaRow->m_value1 = m_value1Edit->GetText();
        m_criteriaRow->m_value2 = m_value2Edit->GetText();
    }

    // NOLINTNEXTLINE(readability-misleading-indentation)
    emit criteriaChanged();

    Close();
}

void CriteriaRowEditor::enableSaveButton()
{
    bool enabled = false;

    const SmartPLField *Field = lookupField(m_fieldSelector->GetValue());

    const SmartPLOperator *Operator = lookupOperator(m_operatorSelector->GetValue());

    if (Field && Operator)
    {
        if (Field->m_type == ftNumeric || Field->m_type == ftBoolean)
            enabled = true;
        else if (Field->m_type == ftDate)
        {
            if ((Operator->m_noOfArguments == 0) ||
                (Operator->m_noOfArguments == 1 && !m_value1Selector->GetValue().isEmpty()) ||
                (Operator->m_noOfArguments == 2 && !m_value1Selector->GetValue().isEmpty()
                                                  && !m_value2Selector->GetValue().isEmpty()))
                enabled = true;
        }
        else // ftString
        {
            if ((Operator->m_noOfArguments == 0) ||
                (Operator->m_noOfArguments == 1 && !m_value1Edit->GetText().isEmpty()) ||
                (Operator->m_noOfArguments == 2 && !m_value1Edit->GetText().isEmpty()
                                                  && !m_value2Edit->GetText().isEmpty()))
                enabled = true;
        }
    }

    m_saveButton->SetEnabled(enabled);
}

void CriteriaRowEditor::fieldChanged(void)
{
    const SmartPLField *Field = lookupField(m_fieldSelector->GetValue());
    if (!Field)
        return;

    if (Field->m_type == ftBoolean)
    {
        // add yes / no items to combo
        m_value1Selector->Reset();
        new MythUIButtonListItem(m_value1Selector, "No");
        new MythUIButtonListItem(m_value1Selector, "Yes");
        m_value2Selector->Reset();
        new MythUIButtonListItem(m_value2Selector, "No");
        new MythUIButtonListItem(m_value2Selector, "Yes");
    }
    else if (Field->m_type == ftDate)
    {
        // add a couple of date values to the combo
        m_value1Selector->Reset();
        new MythUIButtonListItem(m_value1Selector, "$DATE");
        new MythUIButtonListItem(m_value1Selector, "$DATE - 30 days");
        new MythUIButtonListItem(m_value1Selector, "$DATE - 60 days");

        if (!m_value1Selector->MoveToNamedPosition(m_criteriaRow->m_value1))
        {
            // not found so add it to the selector
            new MythUIButtonListItem(m_value1Selector, m_criteriaRow->m_value1);
            m_value1Selector->SetValue(m_criteriaRow->m_value1);
        }


        m_value2Selector->Reset();
        new MythUIButtonListItem(m_value2Selector, "$DATE");
        new MythUIButtonListItem(m_value2Selector, "$DATE - 30 days");
        new MythUIButtonListItem(m_value2Selector, "$DATE - 60 days");

        if (!m_value2Selector->MoveToNamedPosition(m_criteriaRow->m_value2))
        {
            // not found so add it to the selector
            new MythUIButtonListItem(m_value2Selector, m_criteriaRow->m_value2);
            m_value2Selector->SetValue(m_criteriaRow->m_value2);
        }
    }

    // get list of operators valid for this field type
    getOperatorList(Field->m_type);

    enableSaveButton();
}

void CriteriaRowEditor::operatorChanged(void)
{
    const SmartPLField *Field = lookupField(m_fieldSelector->GetValue());
    if (!Field)
        return;

    const SmartPLOperator *Operator = lookupOperator(m_operatorSelector->GetValue());
    if (!Operator)
        return;

    // hide all widgets
    m_value1Edit->Hide();
    m_value2Edit->Hide();
    m_value1Button->Hide();
    m_value2Button->Hide();
    m_value1Selector->Hide();
    m_value2Selector->Hide();
    m_value1Spinbox->Hide();
    m_value2Spinbox->Hide();

    // show spin edits
    if (Field->m_type == ftNumeric)
    {
        if (Operator->m_noOfArguments >= 1)
        {
            m_value1Spinbox->Show();
            int currentValue = m_value1Spinbox->GetIntValue();
            m_value1Spinbox->SetRange(Field->m_minValue, Field->m_maxValue, 1);

            if (currentValue < Field->m_minValue || currentValue > Field->m_maxValue)
                m_value1Spinbox->SetValue(Field->m_defaultValue);
        }

        if (Operator->m_noOfArguments == 2)
        {
            m_value2Spinbox->Show();
            int currentValue = m_value2Spinbox->GetIntValue();
            m_value2Spinbox->SetRange(Field->m_minValue, Field->m_maxValue, 1);

            if (currentValue < Field->m_minValue || currentValue > Field->m_maxValue)
                m_value2Spinbox->SetValue(Field->m_defaultValue);
        }
    }
    else if (Field->m_type == ftBoolean)
    {
        // only show value1combo
        m_value1Selector->Show();
    }
    else if (Field->m_type == ftDate)
    {
        if (Operator->m_noOfArguments >= 1)
        {
            m_value1Selector->Show();
            m_value1Button->Show();
        }

        if (Operator->m_noOfArguments == 2)
        {
            m_value2Selector->Show();
            m_value2Button->Show();
        }
    }
    else // ftString
    {
        if (Operator->m_noOfArguments >= 1)
        {
            m_value1Edit->Show();
            m_value1Button->Show();
        }

        if (Operator->m_noOfArguments == 2)
        {
            m_value2Edit->Show();
            m_value2Button->Show();
        }
    }

    enableSaveButton();
}

void CriteriaRowEditor::getOperatorList(SmartPLFieldType fieldType)
{
    QString currentOperator = m_operatorSelector->GetValue();

    m_operatorSelector->Reset();

    for (const auto & oper : SmartPLOperators)
    {
        // don't add operators that only work with string fields
        if (fieldType != ftString && oper.m_stringOnly)
            continue;

        // don't add operators that only work with boolean fields
        if (fieldType == ftBoolean && !oper.m_validForBoolean)
            continue;

        new MythUIButtonListItem(m_operatorSelector, oper.m_name);
    }

    // try to set the operatorCombo to the same operator or else the first item
    m_operatorSelector->SetValue(currentOperator);
}

void CriteriaRowEditor::valueButtonClicked(void)
{
    QString msg;
    QStringList searchList;
    QString s = GetFocusWidget() == m_value1Button ? m_value1Edit->GetText() : m_value2Edit->GetText();

    if (m_fieldSelector->GetValue() == "Artist")
    {
        msg = tr("Select an Artist");
        searchList = MusicMetadata::fillFieldList("artist");
    }
    else if (m_fieldSelector->GetValue() == "Comp. Artist")
    {
        msg = tr("Select a Compilation Artist");
        searchList = MusicMetadata::fillFieldList("compilation_artist");
    }
    else if (m_fieldSelector->GetValue() == "Album")
    {
        msg = tr("Select an Album");
        searchList = MusicMetadata::fillFieldList("album");
    }
    else if (m_fieldSelector->GetValue() == "Genre")
    {
        msg = tr("Select a Genre");
        searchList = MusicMetadata::fillFieldList("genre");
    }
    else if (m_fieldSelector->GetValue() == "Title")
    {
        msg = tr("Select a Title");
        searchList = MusicMetadata::fillFieldList("title");
    }
    else if ((m_fieldSelector->GetValue() == "Last Play") ||
             (m_fieldSelector->GetValue() == "Date Imported"))
    {
        editDate();
        return;
    }

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *searchDlg = new MythUISearchDialog(popupStack, msg, searchList, false, s);

    if (!searchDlg->Create())
    {
        delete searchDlg;
        return;
    }

    connect(searchDlg, &MythUISearchDialog::haveResult, this, &CriteriaRowEditor::setValue);

    popupStack->AddScreen(searchDlg);
}

void CriteriaRowEditor::setValue(const QString& value)
{
    if (GetFocusWidget() && GetFocusWidget() == m_value1Button)
        m_value1Edit->SetText(value);
    else
        m_value2Edit->SetText(value);
}

void CriteriaRowEditor::editDate(void)
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *dateDlg = new SmartPLDateDialog(popupStack);
    QString date = GetFocusWidget() == m_value1Button ? m_value1Selector->GetValue() : m_value2Selector->GetValue();

    if (!dateDlg->Create())
    {
        delete dateDlg;
        return;
    }

    dateDlg->setDate(date);

    connect(dateDlg, &SmartPLDateDialog::dateChanged, this, &CriteriaRowEditor::setDate);

    popupStack->AddScreen(dateDlg);
}

void CriteriaRowEditor::setDate(const QString& date)
{
    if (GetFocusWidget() && GetFocusWidget() == m_value1Button)
    {
        if (m_value1Selector->MoveToNamedPosition(date))
            return;

        // not found so add it to the selector
        new MythUIButtonListItem(m_value1Selector, date);
        m_value1Selector->SetValue(date);
    }
    else
    {
        if (m_value2Selector->MoveToNamedPosition(date))
            return;

        // not found so add it to the selector
        new MythUIButtonListItem(m_value2Selector, date);
        m_value2Selector->SetValue(date);
    }
}

/*
---------------------------------------------------------------------
*/


bool SmartPLResultViewer::Create(void)
{
    if (!LoadWindowFromXML("music-ui.xml", "smartplresultviewer", this))
        return false;

    bool err = false;

    UIUtilE::Assign(this, m_trackList, "tracklist", &err);
    UIUtilW::Assign(this, m_positionText, "position", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'smartplresultviewer'");
        return false;
    }

    connect(m_trackList, &MythUIButtonList::itemVisible,
            this, &SmartPLResultViewer::trackVisible);
    connect(m_trackList, &MythUIButtonList::itemSelected,
            this, &SmartPLResultViewer::trackSelected);

    BuildFocusList();

    return true;
}

bool SmartPLResultViewer::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Music", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "INFO")
            showTrackInfo();
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void SmartPLResultViewer::trackVisible(MythUIButtonListItem *item)
{
    if (!item)
        return;

    if (item->GetImageFilename().isEmpty())
    {
        auto *mdata = item->GetData().value<MusicMetadata *>();
        if (mdata)
        {
            QString artFile = mdata->getAlbumArtFile();
            if (artFile.isEmpty())
                item->SetImage("mm_nothumb.png");
            else
                item->SetImage(mdata->getAlbumArtFile());
        }
        else
        {
            item->SetImage("mm_nothumb.png");
        }
    }
}

void SmartPLResultViewer::trackSelected(MythUIButtonListItem *item)
{
    if (!item || !m_positionText)
        return;

    m_positionText->SetText(tr("%1 of %2").arg(m_trackList->IsEmpty() ? 0 : m_trackList->GetCurrentPos() + 1)
                                          .arg(m_trackList->GetCount()));
}
void SmartPLResultViewer::showTrackInfo(void)
{
    MythUIButtonListItem *item = m_trackList->GetItemCurrent();
    if (!item)
        return;

    auto *mdata = item->GetData().value<MusicMetadata *>();
    if (!mdata)
        return;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *dlg = new TrackInfoDialog(popupStack, mdata, "trackinfopopup");

    if (!dlg->Create())
    {
        delete dlg;
        return;
    }

    popupStack->AddScreen(dlg);
}

void SmartPLResultViewer::setSQL(const QString& sql)
{
    m_trackList->Reset();;

    MSqlQuery query(MSqlQuery::InitCon());

    if (query.exec(sql))
    {
        while (query.next())
        {
            MusicMetadata *mdata = gMusicData->m_all_music->getMetadata(query.value(0).toInt());
            if (mdata)
            {
                InfoMap metadataMap;
                mdata->toMap(metadataMap);

                auto *item = new MythUIButtonListItem(m_trackList, "", QVariant::fromValue(mdata));
                item->SetTextFromMap(metadataMap);
            }
        }
    }

    trackSelected(m_trackList->GetItemCurrent());
}


/*
---------------------------------------------------------------------
*/

bool SmartPLOrderByDialog::Create(void)
{
    if (!LoadWindowFromXML("music-ui.xml", "orderbydialog", this))
        return false;

    bool err = false;

    UIUtilE::Assign(this, m_fieldList,        "fieldlist",        &err);
    UIUtilE::Assign(this, m_orderSelector,    "fieldselector",    &err);
    UIUtilE::Assign(this, m_addButton,        "addbutton",        &err);
    UIUtilE::Assign(this, m_deleteButton,     "deletebutton",     &err);
    UIUtilE::Assign(this, m_moveUpButton,     "moveupbutton",     &err);
    UIUtilE::Assign(this, m_moveDownButton,   "movedownbutton",   &err);
    UIUtilE::Assign(this, m_ascendingButton,  "ascendingbutton",  &err);
    UIUtilE::Assign(this, m_descendingButton, "descendingbutton", &err);
    UIUtilE::Assign(this, m_cancelButton,     "cancelbutton",     &err);
    UIUtilE::Assign(this, m_okButton,         "okbutton",         &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'orderbydialog'");
        return false;
    }

    connect(m_addButton, &MythUIButton::Clicked, this, &SmartPLOrderByDialog::addPressed);
    connect(m_deleteButton, &MythUIButton::Clicked, this, &SmartPLOrderByDialog::deletePressed);
    connect(m_moveUpButton, &MythUIButton::Clicked, this, &SmartPLOrderByDialog::moveUpPressed);
    connect(m_moveDownButton, &MythUIButton::Clicked, this, &SmartPLOrderByDialog::moveDownPressed);
    connect(m_ascendingButton, &MythUIButton::Clicked, this, &SmartPLOrderByDialog::ascendingPressed);
    connect(m_descendingButton, &MythUIButton::Clicked, this, &SmartPLOrderByDialog::descendingPressed);
    connect(m_cancelButton, &MythUIButton::Clicked, this, &MythScreenType::Close);
    connect(m_okButton, &MythUIButton::Clicked, this, &SmartPLOrderByDialog::okPressed);

    connect(m_orderSelector,  &MythUIButtonList::itemSelected,
            this, qOverload<MythUIButtonListItem *>(&SmartPLOrderByDialog::orderByChanged));
    connect(m_fieldList, &MythUIButtonList::itemSelected,
            this, &SmartPLOrderByDialog::fieldListSelectionChanged);

    getOrderByFields();

    orderByChanged();

    BuildFocusList();

    return true;
}

QString SmartPLOrderByDialog::getFieldList(void)
{
    QString result;
    bool bFirst = true;

    for (int i = 0; i < m_fieldList->GetCount(); i++)
    {
        if (bFirst)
        {
            bFirst = false;
            result = m_fieldList->GetItemAt(i)->GetText();
        }
        else
        {
            result += ", " + m_fieldList->GetItemAt(i)->GetText();
        }
    }

    return result;
}

void SmartPLOrderByDialog::setFieldList(const QString &fieldList)
{
    m_fieldList->Reset();
    QStringList list = fieldList.split(",");

    for (int x = 0; x < list.count(); x++)
    {
        auto *item = new MythUIButtonListItem(m_fieldList, list[x].trimmed());
        QString state = list[x].contains("(A)") ? "ascending" : "descending";
        item->DisplayState(state, "sortstate");
    }

    orderByChanged();
}

void SmartPLOrderByDialog::fieldListSelectionChanged(MythUIButtonListItem *item)
{
    if (!item)
        return;

    m_orderSelector->SetValue(item->GetText().left(item->GetText().length() - 4));
}

void SmartPLOrderByDialog::ascendingPressed(void)
{
    if (!m_fieldList->GetItemCurrent())
        return;

    m_fieldList->GetItemCurrent()->SetText(m_orderSelector->GetValue() + " (A)");
    m_fieldList->GetItemCurrent()->DisplayState("ascending", "sortstate");

    orderByChanged();
    SetFocusWidget(m_descendingButton);
}

void SmartPLOrderByDialog::descendingPressed(void)
{
    if (!m_fieldList->GetItemCurrent())
        return;

    m_fieldList->GetItemCurrent()->SetText(m_orderSelector->GetValue() + " (D)");
    m_fieldList->GetItemCurrent()->DisplayState("descending", "sortstate");

    orderByChanged();
    SetFocusWidget(m_ascendingButton);
}

void SmartPLOrderByDialog::addPressed(void)
{
    auto *item = new MythUIButtonListItem(m_fieldList, m_orderSelector->GetValue() + " (A)");
    item->DisplayState("ascending", "sortstate");

    orderByChanged();
    SetFocusWidget(m_orderSelector);
}

void SmartPLOrderByDialog::deletePressed(void)
{
    m_fieldList->RemoveItem(m_fieldList->GetItemCurrent());
    orderByChanged();

    if (!m_deleteButton->IsEnabled())
        SetFocusWidget(m_addButton);
    else
        SetFocusWidget(m_deleteButton);
}

void SmartPLOrderByDialog::moveUpPressed(void)
{
    MythUIButtonListItem *item = m_fieldList->GetItemCurrent();

    if (item)
        item->MoveUpDown(true);

    orderByChanged();

    if (!m_moveUpButton->IsEnabled())
        SetFocusWidget(m_moveDownButton);
    else
        SetFocusWidget(m_moveUpButton);
}

void SmartPLOrderByDialog::moveDownPressed(void)
{
    MythUIButtonListItem *item = m_fieldList->GetItemCurrent();

    if (item)
        item->MoveUpDown(false);

    orderByChanged();

    if (!m_moveDownButton->IsEnabled())
        SetFocusWidget(m_moveUpButton);
    else
        SetFocusWidget(m_moveDownButton);
}

void SmartPLOrderByDialog::okPressed(void)
{
    emit orderByChanged(getFieldList());
    Close();
}

void SmartPLOrderByDialog::orderByChanged(void)
{
    bool found = false;
    for (int i = 0 ; i < m_fieldList->GetCount() ; ++i)
    {
        if (m_fieldList->GetItemAt(i)->GetText().startsWith(m_orderSelector->GetValue()))
        {
            m_fieldList->SetItemCurrent(i);
            found = true;
        }
    }

    if (found)
    {
        m_addButton->SetEnabled(false);
        m_deleteButton->SetEnabled(true);
        m_moveUpButton->SetEnabled((m_fieldList->GetCurrentPos() != 0));
        m_moveDownButton->SetEnabled((m_fieldList->GetCurrentPos() != m_fieldList->GetCount() - 1) );
        m_ascendingButton->SetEnabled((m_fieldList->GetValue().right(3) == "(D)") );
        m_descendingButton->SetEnabled((m_fieldList->GetValue().right(3) == "(A)"));
    }
    else
    {
        m_addButton->SetEnabled(true);
        m_deleteButton->SetEnabled(false);
        m_moveUpButton->SetEnabled(false);
        m_moveDownButton->SetEnabled(false);
        m_ascendingButton->SetEnabled(false);
        m_descendingButton->SetEnabled(false);
    }
}

void SmartPLOrderByDialog::orderByChanged(MythUIButtonListItem */*item*/)
{
    orderByChanged();
}

void SmartPLOrderByDialog::getOrderByFields(void)
{
    m_orderSelector->Reset();
    for (const auto & field : SmartPLFields)
        new MythUIButtonListItem(m_orderSelector, field.m_name);
}

/*
---------------------------------------------------------------------
*/

bool SmartPLDateDialog::Create(void)
{
    if (!LoadWindowFromXML("music-ui.xml", "dateeditordialog", this))
        return false;

    bool err = false;

    UIUtilE::Assign(this, m_fixedRadio,   "fixeddatecheck", &err);
    UIUtilE::Assign(this, m_daySpin,      "dayspinbox",     &err);
    UIUtilE::Assign(this, m_monthSpin,    "monthspinbox",   &err);
    UIUtilE::Assign(this, m_yearSpin,     "yearspinbox",    &err);
    UIUtilE::Assign(this, m_nowRadio,     "nowcheck",       &err);
    UIUtilE::Assign(this, m_addDaysSpin,  "adddaysspinbox", &err);
    UIUtilE::Assign(this, m_statusText,   "statustext",     &err);
    UIUtilE::Assign(this, m_cancelButton, "cancelbutton",   &err);
    UIUtilE::Assign(this, m_okButton,     "okbutton",       &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'dateeditordialog'");
        return false;
    }

    m_daySpin->SetRange(1, 31, 1);
    m_monthSpin->SetRange(1, 12, 1);
    m_yearSpin->SetRange(1900, 2099, 1);
    m_addDaysSpin->SetRange(-9999, 9999, 1);


    connect(m_fixedRadio, &MythUICheckBox::toggled, this, &SmartPLDateDialog::fixedCheckToggled);
    connect(m_nowRadio, &MythUICheckBox::toggled, this, &SmartPLDateDialog::nowCheckToggled);
    connect(m_addDaysSpin, &MythUIButtonList::itemSelected,
            this, &SmartPLDateDialog::valueChanged);
    connect(m_daySpin, &MythUIButtonList::itemSelected,
            this, &SmartPLDateDialog::valueChanged);
    connect(m_monthSpin, &MythUIButtonList::itemSelected,
            this, &SmartPLDateDialog::valueChanged);
    connect(m_yearSpin, &MythUIButtonList::itemSelected,
            this, &SmartPLDateDialog::valueChanged);

    connect(m_cancelButton, &MythUIButton::Clicked, this, &MythScreenType::Close);
    connect(m_okButton, &MythUIButton::Clicked, this, &SmartPLDateDialog::okPressed);

    valueChanged();

    BuildFocusList();

    return true;
}

QString SmartPLDateDialog::getDate(void)
{
    QString sResult;

    if (m_fixedRadio->GetBooleanCheckState())
    {
        QString day = m_daySpin->GetValue();
        if (m_daySpin->GetIntValue() < 10)
            day = "0" + day;

        QString month = m_monthSpin->GetValue();
        if (m_monthSpin->GetIntValue() < 10)
            month = "0" + month;

        sResult = m_yearSpin->GetValue() + "-" + month + "-" + day;
    }
    else
    {
       sResult = m_statusText->GetText();
    }

    return sResult;
}

void SmartPLDateDialog::setDate(QString date)
{
    if (date.startsWith("$DATE"))
    {
        m_nowRadio->SetCheckState(true);
        m_fixedRadio->SetCheckState(false);

        if (date.length() > 9)
        {
            bool bNegative = false;
            if (date[6] == '-')
                bNegative = true;

            if (date.endsWith(" days"))
                date = date.left(date.length() - 5);

            int nDays = date.mid(8).toInt();
            if (bNegative)
                nDays = -nDays;

            m_addDaysSpin->SetValue(nDays);
        }
        else
        {
            m_addDaysSpin->SetValue(0);
        }

        nowCheckToggled(true);
    }
    else
    {
        int nYear = date.mid(0, 4).toInt();
        int nMonth = date.mid(5, 2).toInt();
        int nDay = date.mid(8, 2).toInt();

        m_daySpin->SetValue(nDay);
        m_monthSpin->SetValue(nMonth);
        m_yearSpin->SetValue(nYear);

        fixedCheckToggled(true);
    }
}

void SmartPLDateDialog::fixedCheckToggled(bool on)
{
    if (m_updating)
        return;

    m_updating = true;
    m_daySpin->SetEnabled(on);
    m_monthSpin->SetEnabled(on);
    m_yearSpin->SetEnabled(on);

    m_nowRadio->SetCheckState(!on);
    m_addDaysSpin->SetEnabled(!on);

    valueChanged();

    m_updating = false;
}

void SmartPLDateDialog::nowCheckToggled(bool on)
{
    if (m_updating)
        return;

    m_updating = true;

    m_fixedRadio->SetCheckState(!on);
    m_daySpin->SetEnabled(!on);
    m_monthSpin->SetEnabled(!on);
    m_yearSpin->SetEnabled(!on);

    m_addDaysSpin->SetEnabled(on);

    valueChanged();

    m_updating = false;
}

void SmartPLDateDialog::okPressed(void )
{
    QString date = getDate();

    emit dateChanged(date);

    Close();
}

void SmartPLDateDialog::valueChanged(void)
{
    bool bValidDate = true;

    if (m_fixedRadio->GetBooleanCheckState())
    {
        QString day = m_daySpin->GetValue();
        if (m_daySpin->GetIntValue() < 10)
            day = "0" + day;

        QString month = m_monthSpin->GetValue();
        if (m_monthSpin->GetIntValue() < 10)
            month = "0" + month;

        QString sDate = m_yearSpin->GetValue() + "-" + month + "-" + day;
        QDate date = QDate::fromString(sDate, Qt::ISODate);
        if (date.isValid())
            m_statusText->SetText(date.toString("dddd, d MMMM yyyy"));
        else
        {
            bValidDate = false;
            m_statusText->SetText(tr("Invalid Date"));
        }
    }
    else if (m_nowRadio->GetBooleanCheckState())
    {
        QString days;
        if (m_addDaysSpin->GetIntValue() > 0)
            days = QString("$DATE + %1 days").arg(m_addDaysSpin->GetIntValue());
        else if (m_addDaysSpin->GetIntValue() == 0)
            days = QString("$DATE");
        else
            days = QString("$DATE - %1 days").arg(
                m_addDaysSpin->GetValue().right(m_addDaysSpin->GetValue().length() - 1));

        m_statusText->SetText(days);
    }

    if (bValidDate)
        m_statusText->SetFontState("valid");
    else
        m_statusText->SetFontState("error");

    m_okButton->SetEnabled(bValidDate);
}

