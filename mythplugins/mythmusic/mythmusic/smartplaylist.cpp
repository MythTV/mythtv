// c/c++
#include <unistd.h>
#include <iostream>
using namespace std;

// qt
#include <QSqlDriver>
#include <QKeyEvent>
#include <QSqlField>

// mythtv
#include <mythcontext.h>
#include <mythmainwindow.h>
#include <mythdb.h>
#include <mythuihelper.h>
#include <mythscreentype.h>
#include <mythuitext.h>
#include <mythuitextedit.h>
#include <mythuibuttonlist.h>
#include <mythuibutton.h>
#include <mythuispinbox.h>
#include <mythuicheckbox.h>
#include <mythdialogbox.h>
#include <mythdate.h>
#include <musicmetadata.h>

// mythmusic
#include "musicdata.h"
#include "smartplaylist.h"
#include "musiccommon.h"

struct SmartPLField
{
    QString name;
    QString sqlName;
    SmartPLFieldType type;
    int     minValue;
    int     maxValue;
    int     defaultValue;
};

static SmartPLField SmartPLFields[] =
{
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
    { "Last Play",     "music_songs.lastplay",           ftDate,     0,    0,    0 },
    { "Date Imported", "music_songs.date_entered",       ftDate,     0,    0,    0 },
};

struct SmartPLOperator
{
    QString name;
    int     noOfArguments;
    bool    stringOnly;
    bool    validForBoolean;
};

static SmartPLOperator SmartPLOperators[] =
{
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
};

static int SmartPLOperatorsCount = sizeof(SmartPLOperators) / sizeof(SmartPLOperators[0]);
static int SmartPLFieldsCount = sizeof(SmartPLFields) / sizeof(SmartPLFields[0]);

static SmartPLOperator *lookupOperator(QString name)
{
    for (int x = 0; x < SmartPLOperatorsCount; x++)
    {
        if (SmartPLOperators[x].name == name)
            return &SmartPLOperators[x];
    }
    return NULL;
}

static SmartPLField *lookupField(QString name)
{
    for (int x = 0; x < SmartPLFieldsCount; x++)
    {
        if (SmartPLFields[x].name == name)
            return &SmartPLFields[x];
    }
    return NULL;
}

QString formattedFieldValue(const QVariant &value)
{
    QSqlField field("", value.type());
    if (value.isNull())
        field.clear();
    else
        field.setValue(value);

    MSqlQuery query(MSqlQuery::InitCon());
    QString result = QString::fromUtf8(query.driver()->formatValue(field).toAscii().data());
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

QString getCriteriaSQL(QString fieldName, QString operatorName,
                       QString value1, QString value2)
{
    QString result;

    if (fieldName.isEmpty())
        return result;

    SmartPLField *Field;
    Field = lookupField(fieldName);
    if (!Field)
    {
        return "";
    }

    result = Field->sqlName;

    SmartPLOperator *Operator;
    Operator = lookupOperator(operatorName);
    if (!Operator)
    {
        return QString();
    }

    // convert boolean and date values
    if (Field->type == ftBoolean)
    {
        // compilation field uses 0 = false;  1 = true
        value1 = (value1 == "Yes") ? "1":"0";
        value2 = (value2 == "Yes") ? "1":"0";
    }
    else if (Field->type == ftDate)
    {
        value1 = evaluateDateValue(value1);
        value2 = evaluateDateValue(value2);
    }

    if (Operator->name == "is equal to")
    {
        result = result + " = " + formattedFieldValue(value1);
    }
    else if (Operator->name == "is not equal to")
    {
        result = result + " != " + formattedFieldValue(value1);
    }
    else if (Operator->name == "is greater than")
    {
        result = result + " > " + formattedFieldValue(value1);
    }
    else if (Operator->name == "is less than")
    {
        result = result + " < " + formattedFieldValue(value1);
    }
    else if (Operator->name == "starts with")
    {
        result = result + " LIKE " + formattedFieldValue(value1 + QString("%"));
    }
    else if (Operator->name == "ends with")
    {
        result = result + " LIKE " + formattedFieldValue(QString("%") + value1);
    }
    else if (Operator->name == "contains")
    {
        result = result + " LIKE " + formattedFieldValue(QString("%") + value1 + "%");
    }
    else if (Operator->name == "does not contain")
    {
        result = result + " NOT LIKE " + formattedFieldValue(QString("%") + value1 + "%");
    }
    else if (Operator->name == "is between")
    {
        result = result + " BETWEEN " + formattedFieldValue(value1) +
                          " AND " + formattedFieldValue(value2);
    }
    else if (Operator->name == "is set")
    {
        result = result + " IS NOT NULL";
    }
    else if (Operator->name == "is not set")
    {
        result = result + " IS NULL";
    }
    else
    {
        result.clear();
        LOG(VB_GENERAL, LOG_ERR,
            QString("getCriteriaSQL(): invalid operator '%1'")
                .arg(Operator->name));
    }

    return result;
}

QString getOrderBySQL(QString orderByFields)
{
    if (orderByFields.isEmpty())
        return QString();

    QStringList list = orderByFields.split(",");
    QString fieldName, result, order;
    bool bFirst = true;

    for (int x = 0; x < list.count(); x++)
    {
        fieldName = list[x].trimmed();
        SmartPLField *Field;
        Field = lookupField(fieldName.left(fieldName.length() - 4));
        if (Field)
        {
            if (fieldName.right(3) == "(D)")
                order = " DESC";
            else
                order = " ASC";

           if (bFirst)
           {
               bFirst = false;
               result = " ORDER BY " + Field->sqlName + order;
           }
           else
               result += ", " + Field->sqlName + order;
        }
    }

    return result;
}

QString getSQLFieldName(QString fieldName)
{
    SmartPLField *Field;
    Field = lookupField(fieldName);
    if (!Field)
    {
        return "";
    }

    return Field->sqlName;
}

/*
///////////////////////////////////////////////////////////////////////
*/

SmartPLCriteriaRow::SmartPLCriteriaRow(const QString &_Field, const QString &_Operator,
                                       const QString &_Value1, const QString &_Value2)
{
    Field = _Field;
    Operator = _Operator;
    Value1 = _Value1;
    Value2 = _Value2;
}

SmartPLCriteriaRow::SmartPLCriteriaRow(void) :
    Field(""), Operator(""), Value1(""), Value2("")
{
}

SmartPLCriteriaRow::~SmartPLCriteriaRow()
{
}

QString SmartPLCriteriaRow::getSQL(void)
{
    if (Field.isEmpty())
        return QString::null;

    QString result;

    result = getCriteriaSQL(Field, Operator, Value1, Value2);

    return result;
}

// return false on error
bool SmartPLCriteriaRow::saveToDatabase(int smartPlaylistID)
{
    // save playlistitem to database

    if (Field.isEmpty())
        return true;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO music_smartplaylist_items (smartplaylistid, field, operator,"
                  " value1, value2)"
                  "VALUES (:SMARTPLAYLISTID, :FIELD, :OPERATOR, :VALUE1, :VALUE2);");
    query.bindValue(":SMARTPLAYLISTID", smartPlaylistID);
    query.bindValue(":FIELD", Field);
    query.bindValue(":OPERATOR", Operator);
    query.bindValue(":VALUE1", Value1);
    query.bindValue(":VALUE2", Value2.isNull() ? "" : Value2);

    if (!query.exec())
    {
        MythDB::DBError("Inserting new smartplaylist item", query);
        return false;
    }

    return true;
}

QString SmartPLCriteriaRow::toString(void)
{
    SmartPLOperator *PLOperator = lookupOperator(Operator);
    if (PLOperator)
    {
        QString result;
        if (PLOperator->noOfArguments == 0)
            result = Field + " " + Operator;
        else if (PLOperator->noOfArguments == 1)
            result = Field + " " + Operator + " " + Value1;
        else
        {
            result = Field + " " + Operator + " " + Value1;
            result += " " + tr("and") + " " + Value2;
        }

        return result;
    }

    return QString();
}

/*
---------------------------------------------------------------------
*/

SmartPlaylistEditor::SmartPlaylistEditor(MythScreenStack *parent)
              : MythScreenType(parent, "smartplaylisteditor"),
                m_tempCriteriaRow(NULL), m_matchesCount(0),
                m_newPlaylist(false), m_playlistIsValid(false),
                m_categorySelector(NULL), m_categoryButton(NULL),
                m_titleEdit(NULL), m_matchSelector(NULL),
                m_criteriaList(NULL), m_orderBySelector(NULL),
                m_orderByButton(NULL), m_matchesText(NULL),
                m_limitSpin(NULL), m_cancelButton(NULL),
                m_saveButton(NULL), m_showResultsButton(NULL)
{
}

SmartPlaylistEditor::~SmartPlaylistEditor(void)
{
    while (!m_criteriaRows.empty())
    {
        delete m_criteriaRows.back();
        m_criteriaRows.pop_back();
    }

    if (m_tempCriteriaRow)
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
    connect(m_matchSelector, SIGNAL(itemSelected(MythUIButtonListItem*)), SLOT(updateMatches()));

    for (int x = 0; x < SmartPLFieldsCount; x++)
    {
        if (SmartPLFields[x].name == "")
            new MythUIButtonListItem(m_orderBySelector, SmartPLFields[x].name);
        else
            new MythUIButtonListItem(m_orderBySelector, SmartPLFields[x].name + " (A)");
    }

    m_limitSpin->SetRange(0, 9999, 10);

    connect(m_orderByButton, SIGNAL(Clicked()), SLOT(orderByClicked()));
    connect(m_saveButton, SIGNAL(Clicked()), SLOT(saveClicked()));
    connect(m_cancelButton, SIGNAL(Clicked()), SLOT(Close()));
    connect(m_categoryButton, SIGNAL(Clicked()), SLOT(showCategoryMenu()));
    connect(m_showResultsButton, SIGNAL(Clicked()), SLOT(showResultsClicked()));
    connect(m_criteriaList, SIGNAL(itemClicked(MythUIButtonListItem*)), SLOT(editCriteria()));

    BuildFocusList();

    return true;
}

bool SmartPlaylistEditor::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Music", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
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
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void SmartPlaylistEditor::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);

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

                MythTextInputDialog *input = new MythTextInputDialog(popupStack, label);

                connect(input, SIGNAL(haveResult(QString)),
                        SLOT(newCategory(QString)));

                if (input->Create())
                    popupStack->AddScreen(input);
                else
                    delete input;
            }
            else if (resulttext == tr("Delete Category"))
                startDeleteCategory(m_categorySelector->GetValue());
            else if (resulttext == tr("Rename Category"))
            {
                MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
                QString label = tr("Enter New Name For Category: %1").arg(m_categorySelector->GetValue());

                MythTextInputDialog *input = new MythTextInputDialog(popupStack, label);

                connect(input, SIGNAL(haveResult(QString)),
                        SLOT(renameCategory(QString)));

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
        m_tempCriteriaRow = NULL;
    }

    MythUIButtonListItem *item = m_criteriaList->GetItemCurrent();

    if (!item)
        return;

    SmartPLCriteriaRow *row = qVariantValue<SmartPLCriteriaRow*> (item->GetData());

    if (!row)
        return;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    CriteriaRowEditor *editor = new CriteriaRowEditor(popupStack, row);

    if (!editor->Create())
    {
        delete editor;
        return;
    }

    connect(editor, SIGNAL(criteriaChanged()), SLOT(criteriaChanged()));

    popupStack->AddScreen(editor);
}

void SmartPlaylistEditor::deleteCriteria(void)
{
    // make sure we have something to delete
    MythUIButtonListItem *item = m_criteriaList->GetItemCurrent();

    if (!item)
        return;

    ShowOkPopup(tr("Delete Criteria?"), this, SLOT(doDeleteCriteria(bool)), true);
}

void SmartPlaylistEditor::doDeleteCriteria(bool doit)
{
    if (doit)
    {
        MythUIButtonListItem *item = m_criteriaList->GetItemCurrent();
        if (!item)
            return;

        SmartPLCriteriaRow *row = qVariantValue<SmartPLCriteriaRow*> (item->GetData());

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

    MythUIButtonListItem *item = new MythUIButtonListItem(m_criteriaList, row->toString(), qVariantFromValue(row));

    m_criteriaList->SetItemCurrent(item);

    editCriteria();
    */

    if (m_tempCriteriaRow)
        delete m_tempCriteriaRow;

    m_tempCriteriaRow = new SmartPLCriteriaRow();

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    CriteriaRowEditor *editor = new CriteriaRowEditor(popupStack, m_tempCriteriaRow);

    if (!editor->Create())
    {
        delete editor;
        return;
    }

    connect(editor, SIGNAL(criteriaChanged()), SLOT(criteriaChanged()));

    popupStack->AddScreen(editor);
}

void SmartPlaylistEditor::criteriaChanged()
{
    MythUIButtonListItem *item = NULL;

    if (m_tempCriteriaRow)
    {
        // this is a new row so add it to the list
        m_criteriaRows.append(m_tempCriteriaRow);

        item = new MythUIButtonListItem(m_criteriaList, m_tempCriteriaRow->toString(),
                                        qVariantFromValue(m_tempCriteriaRow));

        m_criteriaList->SetItemCurrent(item);

        m_tempCriteriaRow = NULL;
    }
    else
    {
        // update the existing row
        item = m_criteriaList->GetItemCurrent();
        if (!item)
            return;

        SmartPLCriteriaRow *row = qVariantValue<SmartPLCriteriaRow*> (item->GetData());

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

    MythDialogBox *menu = new MythDialogBox(label, popupStack, "actionmenu");

    if (!menu->Create())
    {
        delete menu;
        return;
    }

    menu->SetReturnEvent(this, "categorymenu");

    menu->AddButton(tr("New Category"),    NULL);
    menu->AddButton(tr("Delete Category"), NULL);
    menu->AddButton(tr("Rename Category"), NULL);

    popupStack->AddScreen(menu);
}

void SmartPlaylistEditor::showCriteriaMenu(void)
{
    QString label = tr("Criteria Actions");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythDialogBox *menu = new MythDialogBox(label, popupStack, "actionmenu");

    if (!menu->Create())
    {
        delete menu;
        return;
    }

    menu->SetReturnEvent(this, "criteriamenu");

    MythUIButtonListItem *item = m_criteriaList->GetItemCurrent();

    if (item)
        menu->AddButton(tr("Edit Criteria"), SLOT(editCriteria()));

    menu->AddButton(tr("Add Criteria"), SLOT(addCriteria()));

    if (item)
        menu->AddButton(tr("Delete Criteria"), SLOT(deleteCriteria()));

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

    m_playlistIsValid = (m_criteriaRows.size() > 0);
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
    int ID;
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
    for (int x = 0; x < m_criteriaRows.size(); x++)
        m_criteriaRows[x]->saveToDatabase(ID);

    emit smartPLChanged(category, name);

    Close();
}

void SmartPlaylistEditor::newSmartPlaylist(QString category)
{
    m_categorySelector->SetValue(category);
    m_titleEdit->Reset();
    m_originalCategory = category;
    m_originalName.clear();

    m_newPlaylist = true;

    updateMatches();
}

void SmartPlaylistEditor::editSmartPlaylist(QString category, QString name)
{
    m_originalCategory = category;
    m_originalName = name;
    m_newPlaylist = false;
    loadFromDatabase(category, name);
    updateMatches();
}

void SmartPlaylistEditor::loadFromDatabase(QString category, QString name)
{
    // load smartplaylist from database
    int categoryid = SmartPlaylistEditor::lookupCategoryID(category);

    MSqlQuery query(MSqlQuery::InitCon());
    int ID;

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

    // load smartplaylist items
    SmartPLCriteriaRow *row;

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
            row = new SmartPLCriteriaRow(Field, Operator, Value1, Value2);
            m_criteriaRows.append(row);

            new MythUIButtonListItem(m_criteriaList, row->toString(), qVariantFromValue(row));
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

QString SmartPlaylistEditor::getSQL(QString fields)
{
    QString sql, whereClause, orderByClause, limitClause;
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
    if (m_criteriaRows.size() == 0)
        return QString();

    bool bFirst = true;
    QString sql = "WHERE ";

    for (int x = 0; x <  m_criteriaRows.size(); x++)
    {
        QString criteria = m_criteriaRows[x]->getSQL();
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

    SmartPLResultViewer *resultViewer = new SmartPLResultViewer(mainStack);

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

    SmartPLOrderByDialog *orderByDialog = new SmartPLOrderByDialog(popupStack);

    if (!orderByDialog->Create())
    {
        delete orderByDialog;
        return;
    }

    orderByDialog->setFieldList(m_orderBySelector->GetValue());

    connect(orderByDialog, SIGNAL(orderByChanged(QString)), SLOT(orderByChanged(QString)));

    popupStack->AddScreen(orderByDialog);
}

void SmartPlaylistEditor::orderByChanged(QString orderBy)
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
bool SmartPlaylistEditor::deleteSmartPlaylist(QString category, QString name)
{
    // get categoryid
    int categoryid = SmartPlaylistEditor::lookupCategoryID(category);

    MSqlQuery query(MSqlQuery::InitCon());

    // get playlist ID
    int ID;
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
bool SmartPlaylistEditor::deleteCategory(QString category)
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
int SmartPlaylistEditor::lookupCategoryID(QString category)
{
    int ID;
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

CriteriaRowEditor::CriteriaRowEditor(MythScreenStack* parent, SmartPLCriteriaRow* row)
                 : MythScreenType(parent, "CriteriaRowEditor"),
                m_criteriaRow(NULL), m_fieldSelector(NULL),
                m_operatorSelector(NULL), m_value1Edit(NULL),
                m_value2Edit(NULL), m_value1Selector(NULL),
                m_value2Selector(NULL), m_value1Spinbox(NULL),
                m_value2Spinbox(NULL), m_value1Button(NULL),
                m_value2Button(NULL), m_andText(NULL),
                m_cancelButton(NULL), m_saveButton(NULL)
{
    m_criteriaRow = row;
}

CriteriaRowEditor::~CriteriaRowEditor(void)
{
}

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

    connect(m_fieldSelector, SIGNAL(itemSelected(MythUIButtonListItem*)), SLOT(fieldChanged()));
    connect(m_operatorSelector, SIGNAL(itemSelected(MythUIButtonListItem*)), SLOT(operatorChanged()));

    connect(m_value1Edit, SIGNAL(valueChanged()), SLOT(valueEditChanged()));
    connect(m_value2Edit, SIGNAL(valueChanged()), SLOT(valueEditChanged()));
    connect(m_value1Selector, SIGNAL(itemSelected(MythUIButtonListItem*)), SLOT(valueEditChanged()));
    connect(m_value2Selector, SIGNAL(itemSelected(MythUIButtonListItem*)), SLOT(valueEditChanged()));

    connect(m_value1Button, SIGNAL(Clicked()), SLOT(valueButtonClicked()));
    connect(m_value2Button, SIGNAL(Clicked()), SLOT(valueButtonClicked()));

    connect(m_cancelButton, SIGNAL(Clicked()), SLOT(Close()));
    connect(m_saveButton, SIGNAL(Clicked()), SLOT(saveClicked()));

    BuildFocusList();

    return true;
}

void CriteriaRowEditor::updateFields(void)
{
    for (int x = 0; x < SmartPLFieldsCount; x++)
        new MythUIButtonListItem(m_fieldSelector, SmartPLFields[x].name);

    m_fieldSelector->SetValue(m_criteriaRow->Field);
}

void CriteriaRowEditor::updateOperators(void)
{
    for (int x = 0; x < SmartPLOperatorsCount; x++)
        new MythUIButtonListItem(m_operatorSelector, SmartPLOperators[x].name);

    m_operatorSelector->SetValue(m_criteriaRow->Operator);
}

void CriteriaRowEditor::valueEditChanged(void)
{
    enableSaveButton();
}

void CriteriaRowEditor::updateValues(void)
{
    m_value1Edit->SetText(m_criteriaRow->Value1);
    m_value2Edit->SetText(m_criteriaRow->Value2);
    m_value1Spinbox->SetValue(m_criteriaRow->Value1);
    m_value2Spinbox->SetValue(m_criteriaRow->Value2);

    if (!m_value1Selector->MoveToNamedPosition(m_criteriaRow->Value1))
    {
        // not found so add it to the selector
        new MythUIButtonListItem(m_value1Selector, m_criteriaRow->Value1);
        m_value1Selector->SetValue(m_criteriaRow->Value1);
    }

    if (!m_value2Selector->MoveToNamedPosition(m_criteriaRow->Value2))
    {
        // not found so add it to the selector
        new MythUIButtonListItem(m_value2Selector, m_criteriaRow->Value2);
        m_value2Selector->SetValue(m_criteriaRow->Value2);
    }
}

void CriteriaRowEditor::saveClicked()
{
    SmartPLField *Field;
    Field = lookupField(m_fieldSelector->GetValue());
    if (!Field)
        return;

    m_criteriaRow->Field = m_fieldSelector->GetValue();
    m_criteriaRow->Operator = m_operatorSelector->GetValue();

    if (Field->type == ftNumeric)
    {
        m_criteriaRow->Value1 = m_value1Spinbox->GetValue();
        m_criteriaRow->Value2 = m_value2Spinbox->GetValue();
    }
    else if (Field->type == ftBoolean || Field->type == ftDate)
    {
        m_criteriaRow->Value1 = m_value1Selector->GetValue();
        m_criteriaRow->Value2 = m_value2Selector->GetValue();
    }
    else // ftString
    {
        m_criteriaRow->Value1 = m_value1Edit->GetText();
        m_criteriaRow->Value2 = m_value2Edit->GetText();
    }

    emit criteriaChanged();

    Close();
}

void CriteriaRowEditor::enableSaveButton()
{
    bool enabled = false;

    SmartPLField *Field;
    Field = lookupField(m_fieldSelector->GetValue());

    SmartPLOperator *Operator;
    Operator = lookupOperator(m_operatorSelector->GetValue());

    if (Field && Operator)
    {
        if (Field->type == ftNumeric || Field->type == ftBoolean)
            enabled = true;
        else if (Field->type == ftDate)
        {
            if (Operator->noOfArguments == 0)
                enabled = true;
            else if (Operator->noOfArguments == 1 && !m_value1Selector->GetValue().isEmpty())
                enabled = true;
            else if (Operator->noOfArguments == 2 && !m_value1Selector->GetValue().isEmpty()
                                                  && !m_value2Selector->GetValue().isEmpty())
                enabled = true;
        }
        else // ftString
        {
            if (Operator->noOfArguments == 0)
                enabled = true;
            else if (Operator->noOfArguments == 1 && !m_value1Edit->GetText().isEmpty())
                enabled = true;
            else if (Operator->noOfArguments == 2 && !m_value1Edit->GetText().isEmpty()
                                                  && !m_value2Edit->GetText().isEmpty())
                enabled = true;
        }
    }

    m_saveButton->SetEnabled(enabled);
}

void CriteriaRowEditor::fieldChanged(void)
{
    SmartPLField *Field;
    Field = lookupField(m_fieldSelector->GetValue());
    if (!Field)
        return;

    if (Field->type == ftBoolean)
    {
        // add yes / no items to combo
        m_value1Selector->Reset();
        new MythUIButtonListItem(m_value1Selector, "No");
        new MythUIButtonListItem(m_value1Selector, "Yes");
        m_value2Selector->Reset();
        new MythUIButtonListItem(m_value2Selector, "No");
        new MythUIButtonListItem(m_value2Selector, "Yes");
    }
    else if (Field->type == ftDate)
    {
        // add a couple of date values to the combo
        m_value1Selector->Reset();
        new MythUIButtonListItem(m_value1Selector, "$DATE");
        new MythUIButtonListItem(m_value1Selector, "$DATE - 30 days");
        new MythUIButtonListItem(m_value1Selector, "$DATE - 60 days");

        if (!m_value1Selector->MoveToNamedPosition(m_criteriaRow->Value1))
        {
            // not found so add it to the selector
            new MythUIButtonListItem(m_value1Selector, m_criteriaRow->Value1);
            m_value1Selector->SetValue(m_criteriaRow->Value1);
        }


        m_value2Selector->Reset();
        new MythUIButtonListItem(m_value2Selector, "$DATE");
        new MythUIButtonListItem(m_value2Selector, "$DATE - 30 days");
        new MythUIButtonListItem(m_value2Selector, "$DATE - 60 days");

        if (!m_value2Selector->MoveToNamedPosition(m_criteriaRow->Value2))
        {
            // not found so add it to the selector
            new MythUIButtonListItem(m_value2Selector, m_criteriaRow->Value2);
            m_value2Selector->SetValue(m_criteriaRow->Value2);
        }
    }

    // get list of operators valid for this field type
    getOperatorList(Field->type);

    enableSaveButton();
}

void CriteriaRowEditor::operatorChanged(void)
{
    SmartPLField *Field;
    Field = lookupField(m_fieldSelector->GetValue());
    if (!Field)
        return;

    SmartPLOperator *Operator;
    Operator = lookupOperator(m_operatorSelector->GetValue());
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
    if (Field->type == ftNumeric)
    {
        if (Operator->noOfArguments >= 1)
        {
            m_value1Spinbox->Show();
            int currentValue = m_value1Spinbox->GetIntValue();
            m_value1Spinbox->SetRange(Field->minValue, Field->maxValue, 1);

            if (currentValue < Field->minValue || currentValue > Field->maxValue)
                m_value1Spinbox->SetValue(Field->defaultValue);
        }

        if (Operator->noOfArguments == 2)
        {
            m_value2Spinbox->Show();
            int currentValue = m_value2Spinbox->GetIntValue();
            m_value2Spinbox->SetRange(Field->minValue, Field->maxValue, 1);

            if (currentValue < Field->minValue || currentValue > Field->maxValue)
                m_value2Spinbox->SetValue(Field->defaultValue);
        }
    }
    else if (Field->type == ftBoolean)
    {
        // only show value1combo
        m_value1Selector->Show();
    }
    else if (Field->type == ftDate)
    {
        if (Operator->noOfArguments >= 1)
        {
            m_value1Selector->Show();
            m_value1Button->Show();
        }

        if (Operator->noOfArguments == 2)
        {
            m_value2Selector->Show();
            m_value2Button->Show();
        }
    }
    else // ftString
    {
        if (Operator->noOfArguments >= 1)
        {
            m_value1Edit->Show();
            m_value1Button->Show();
        }

        if (Operator->noOfArguments == 2)
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

    for (int x = 0; x < SmartPLOperatorsCount; x++)
    {
        // don't add operators that only work with string fields
        if (fieldType != ftString && SmartPLOperators[x].stringOnly)
            continue;

        // don't add operators that only work with boolean fields
        if (fieldType == ftBoolean && !SmartPLOperators[x].validForBoolean)
            continue;

        new MythUIButtonListItem(m_operatorSelector, SmartPLOperators[x].name);
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
    else if (m_fieldSelector->GetValue() == "Last Play")
    {
        editDate();
        return;
    }
    else if (m_fieldSelector->GetValue() == "Date Imported")
    {
        editDate();
        return;
    }

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythUISearchDialog *searchDlg = new MythUISearchDialog(popupStack, msg, searchList, false, s);

    if (!searchDlg->Create())
    {
        delete searchDlg;
        return;
    }

    connect(searchDlg, SIGNAL(haveResult(QString)), SLOT(setValue(QString)));

    popupStack->AddScreen(searchDlg);
}

void CriteriaRowEditor::setValue(QString value)
{
    if (GetFocusWidget() && GetFocusWidget() == m_value1Button)
        m_value1Edit->SetText(value);
    else
        m_value2Edit->SetText(value);
}

void CriteriaRowEditor::editDate(void)
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    SmartPLDateDialog *dateDlg = new SmartPLDateDialog(popupStack);
    QString date = GetFocusWidget() == m_value1Button ? m_value1Selector->GetValue() : m_value2Selector->GetValue();

    if (!dateDlg->Create())
    {
        delete dateDlg;
        return;
    }

    dateDlg->setDate(date);

    connect(dateDlg, SIGNAL(dateChanged(QString)), SLOT(setDate(QString)));

    popupStack->AddScreen(dateDlg);
}

void CriteriaRowEditor::setDate(QString date)
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


SmartPLResultViewer::SmartPLResultViewer(MythScreenStack *parent)
                   : MythScreenType(parent, "SmartPLResultViewer"),
                   m_trackList(NULL), m_positionText(NULL)
{
}

SmartPLResultViewer::~SmartPLResultViewer()
{
}

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

    connect(m_trackList, SIGNAL(itemVisible(MythUIButtonListItem*)),
            this, SLOT(trackVisible(MythUIButtonListItem*)));
    connect(m_trackList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT(trackSelected(MythUIButtonListItem*)));

    BuildFocusList();

    return true;
}

bool SmartPLResultViewer::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Music", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
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
        MusicMetadata *mdata = qVariantValue<MusicMetadata*> (item->GetData());
        if (mdata)
        {
            QString artFile = mdata->getAlbumArtFile();
            if (artFile.isEmpty())
                item->SetImage("mm_nothumb.png");
            else
                item->SetImage(mdata->getAlbumArtFile());
        }
        else
            item->SetImage("mm_nothumb.png");
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

    MusicMetadata *mdata = qVariantValue<MusicMetadata*> (item->GetData());
    if (!mdata)
        return;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    TrackInfoDialog *dlg = new TrackInfoDialog(popupStack, mdata, "trackinfopopup");

    if (!dlg->Create())
    {
        delete dlg;
        return;
    }

    popupStack->AddScreen(dlg);
}

void SmartPLResultViewer::setSQL(QString sql)
{
    m_trackList->Reset();;

    MSqlQuery query(MSqlQuery::InitCon());

    if (query.exec(sql))
    {
        while (query.next())
        {
            MusicMetadata *mdata = gMusicData->all_music->getMetadata(query.value(0).toInt());
            if (mdata)
            {
                InfoMap metadataMap;
                mdata->toMap(metadataMap);

                MythUIButtonListItem *item = new MythUIButtonListItem(m_trackList, "", qVariantFromValue(mdata));
                item->SetTextFromMap(metadataMap);
            }
        }
    }

    trackSelected(m_trackList->GetItemCurrent());
}


/*
---------------------------------------------------------------------
*/

SmartPLOrderByDialog::SmartPLOrderByDialog(MythScreenStack *parent)
                 :MythScreenType(parent, "SmartPLOrderByDialog"),
        m_fieldList(NULL), m_orderSelector(NULL), m_addButton(NULL),
        m_deleteButton(NULL), m_moveUpButton(NULL), m_moveDownButton(NULL),
        m_ascendingButton(NULL), m_descendingButton(NULL), m_cancelButton(NULL),
        m_okButton(NULL)
{
}

SmartPLOrderByDialog::~SmartPLOrderByDialog(void)
{
}

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

    connect(m_addButton, SIGNAL(Clicked()), this, SLOT(addPressed()));
    connect(m_deleteButton, SIGNAL(Clicked()), this, SLOT(deletePressed()));
    connect(m_moveUpButton, SIGNAL(Clicked()), this, SLOT(moveUpPressed()));
    connect(m_moveDownButton, SIGNAL(Clicked()), this, SLOT(moveDownPressed()));
    connect(m_ascendingButton, SIGNAL(Clicked()), this, SLOT(ascendingPressed()));
    connect(m_descendingButton, SIGNAL(Clicked()), this, SLOT(descendingPressed()));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(Close()));
    connect(m_okButton, SIGNAL(Clicked()), this, SLOT(okPressed()));

    connect(m_orderSelector, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT(orderByChanged(void)));
    connect(m_fieldList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT(fieldListSelectionChanged(MythUIButtonListItem*)));

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
            result += ", " + m_fieldList->GetItemAt(i)->GetText();
    }

    return result;
}

void SmartPLOrderByDialog::setFieldList(const QString &fieldList)
{
    m_fieldList->Reset();
    QStringList list = fieldList.split(",");

    for (int x = 0; x < list.count(); x++)
    {
        MythUIButtonListItem *item = new MythUIButtonListItem(m_fieldList, list[x].trimmed());
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
    MythUIButtonListItem *item = new MythUIButtonListItem(m_fieldList, m_orderSelector->GetValue() + " (A)");
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

void SmartPLOrderByDialog::getOrderByFields(void)
{
    m_orderSelector->Reset();
    for (int x = 1; x < SmartPLFieldsCount; x++)
        new MythUIButtonListItem(m_orderSelector, SmartPLFields[x].name);
}

/*
---------------------------------------------------------------------
*/

SmartPLDateDialog::SmartPLDateDialog(MythScreenStack *parent)
                 :MythScreenType(parent, "SmartPLDateDialog"),
                  m_updating(false), m_fixedRadio(NULL), m_daySpin(NULL),
                  m_monthSpin(NULL), m_yearSpin(NULL), m_nowRadio(NULL),
                  m_addDaysSpin(NULL), m_statusText(NULL),
                  m_cancelButton(NULL), m_okButton(NULL)
{
    m_updating = false;
}

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


    connect(m_fixedRadio, SIGNAL(toggled(bool)), this, SLOT(fixedCheckToggled(bool)));
    connect(m_nowRadio, SIGNAL(toggled(bool)), this, SLOT(nowCheckToggled(bool)));
    //connect(addDaysCheck, SIGNAL(toggled(bool)), this, SLOT(addDaysCheckToggled(bool)));
    connect(m_addDaysSpin, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT(valueChanged(void)));
    connect(m_daySpin, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT(valueChanged(void)));
    connect(m_monthSpin, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT(valueChanged(void)));
    connect(m_yearSpin, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT(valueChanged(void)));

    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(Close()));
    connect(m_okButton, SIGNAL(Clicked()), this, SLOT(okPressed()));

    valueChanged();

    BuildFocusList();

    return true;
}

SmartPLDateDialog::~SmartPLDateDialog(void)
{
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
       sResult = m_statusText->GetText();

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
            m_addDaysSpin->SetValue(0);

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

