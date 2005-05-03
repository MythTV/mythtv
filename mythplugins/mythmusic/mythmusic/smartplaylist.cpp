#include <qlayout.h>
#include <qpushbutton.h>
#include <qlabel.h>
#include <qsqlrecord.h>
#include <qsqlfield.h> 
#include <qsqldriver.h>
#include <qsqlcursor.h>
#include <qhbox.h>

#include <unistd.h>
#include <iostream>

using namespace std;

#include "smartplaylist.h"

#include <mythtv/mythcontext.h>
#include <mythtv/dialogbox.h>
#include <mythtv/mythdialogs.h>
#include <mythtv/mythdbcon.h>


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
    { "Artist",        "artist",                         ftString,   0,    0,    0 },
    { "Album",         "album",                          ftString,   0,    0,    0 },
    { "Title",         "title",                          ftString,   0,    0,    0 },
    { "Genre",         "genre",                          ftString,   0,    0,    0 },
    { "Year",          "year",                           ftNumeric,  1900, 2099, 2000 },
    { "Track No.",     "tracknum",                       ftNumeric,  0,    99,   0 },
    { "Rating",        "rating",                         ftNumeric,  0,    10,   0 },
    { "Play Count",    "playcount",                      ftNumeric,  0,    9999, 0 },
    { "Compilation",   "compilation",                    ftBoolean,  0,    0,    0 },
    { "Comp. Artist",  "compilation_artist",             ftString,   0,    0,    0 },
    { "Last Play",     "FROM_DAYS(TO_DAYS(lastplay))",   ftDate,     0,    0,    0 },
    { "Date Imported", "FROM_DAYS(TO_DAYS(date_added))", ftDate,     0,    0,    0 },
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
    { "is equal to",     1,  false, true },
    { "is not equal to", 1,  false, true },
    { "is greater than", 1,  false, false },
    { "is less than",    1,  false, false },
    { "starts with",     1,  true,  false },
    { "ends with",       1,  true,  false },
    { "contains",        1,  true,  false },
    { "is between",      2,  false, false },
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
        field.setNull();
    else
        field.setValue(value);

    MSqlQuery query(MSqlQuery::InitCon());
    QString result = query.driver()->formatValue(&field);
    return result;
}

QString evaluateDateValue(QString sDate)
{
    if (sDate.startsWith("$DATE"))
    {
        QDate date = QDate::currentDate();
        
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
    
    if (fieldName == "")
        return "";
        
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
        return "";
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
    
    value1 = value1.utf8();
    value2 = value2.utf8();
    
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
        result = result + " LIKE " + formattedFieldValue(QString("%") + value1);
    }
    else if (Operator->name == "ends with")
    {
        result = result + " LIKE " + formattedFieldValue(value1 + "%");
    }
    else if (Operator->name == "contains")
    {
        result = result + " LIKE " + formattedFieldValue(QString("%") + value1 + "%");
    }
    else if (Operator->name == "is between")
    {
        result = result + " BETWEEN " + formattedFieldValue(value1) +
                          " AND " + formattedFieldValue(value2);
    }
    else
    {
        result = "";
        cout << "getCriteriaSQL(): invalid operator '" << Operator->name  << "'" << endl;  
    }
    
    return result;    
}

QString getOrderBySQL(QString orderByFields)
{
    if (orderByFields == "")
        return "";
    
    QStringList list = QStringList::split(",", orderByFields);
    QString fieldName, result = "", order;
    bool bFirst = true;
    
    for (uint x = 0; x < list.count(); x++)
    {
        fieldName = list[x].stripWhiteSpace();
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

SmartPLCriteriaRow::SmartPLCriteriaRow(QWidget *parent, QHBoxLayout *hbox)
{
    // field combo
    fieldCombo = new MythComboBox(false, parent, "field" );
    for (int x = 0; x < SmartPLFieldsCount; x++)
        fieldCombo->insertItem(SmartPLFields[x].name);

    fieldCombo->setBackgroundOrigin(parent->WindowOrigin);
    fieldCombo->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(fieldCombo);

    // operator combo
    operatorCombo = new MythComboBox(false, parent, "criteria" );
    for (int x = 0; x < SmartPLOperatorsCount; x++)
        operatorCombo->insertItem(SmartPLOperators[x].name);
    
    operatorCombo->setBackgroundOrigin(parent->WindowOrigin);
    operatorCombo->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(operatorCombo);
    
    //value1 edit
    value1Edit = new MythRemoteLineEdit( parent, "valueEdit1" );
    value1Edit->setBackgroundOrigin(parent->WindowOrigin);
    value1Edit->setMinimumWidth(50);
    hbox->addWidget(value1Edit);
    
    // value1 spin edit
    value1SpinEdit = new MythSpinBox( parent, "value1SpinEdit" );
    value1SpinEdit->setBackgroundOrigin(parent->WindowOrigin);
    value1SpinEdit->setMinValue(0);
    value1SpinEdit->setMaxValue(9999);
    value1SpinEdit->hide();
    hbox->addWidget(value1SpinEdit);

    // value1 combo
    value1Combo = new MythComboBox(false, parent, "value1Combo" );
    value1Combo->setBackgroundOrigin(parent->WindowOrigin);
    value1Combo->setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed));
    value1Combo->hide();
    hbox->addWidget(value1Combo);
    
    // value1 button
    value1Button = new MythPushButton( parent, "value1Button" );
    value1Button->setBackgroundOrigin(parent->WindowOrigin);
    value1Button->setText( "" );
    value1Button->setEnabled(true);
    value1Button->setMinimumHeight(fieldCombo->height());
    value1Button->setMaximumHeight(fieldCombo->height());
    value1Button->setMinimumWidth(fieldCombo->height());
    value1Button->setMaximumWidth(fieldCombo->height());
    hbox->addWidget(value1Button);
        
    // value2 edit 
    value2Edit = new MythRemoteLineEdit( parent, "valueEdit2" );
    value2Edit->setBackgroundOrigin(parent->WindowOrigin);
    value2Edit->hide();
    value2Edit->setMinimumWidth(50);
    hbox->addWidget(value2Edit);
    
    // value2 spin edit
    value2SpinEdit = new MythSpinBox( parent, "value2SpinEdit" );
    value2SpinEdit->setBackgroundOrigin(parent->WindowOrigin);
    value2SpinEdit->setMinValue(0);
    value2SpinEdit->setMaxValue(9999);
    value2SpinEdit->hide();
    hbox->addWidget(value2SpinEdit);

    // value2 combo
    value2Combo = new MythComboBox(false, parent, "value2Combo" );
    value2Combo->setBackgroundOrigin(parent->WindowOrigin);
    value1Combo->setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed));
    value2Combo->hide();
    hbox->addWidget(value2Combo);

    // value2 button
    value2Button = new MythPushButton( parent, "value1Button" );
    value2Button->setBackgroundOrigin(parent->WindowOrigin);
    value2Button->setText( "" );
    value2Button->setEnabled(true);
    value2Button->setMinimumHeight(fieldCombo->height());
    value2Button->setMaximumHeight(fieldCombo->height());
    value2Button->setMinimumWidth(fieldCombo->height());
    value2Button->setMaximumWidth(fieldCombo->height());
    value2Button->hide();
    hbox->addWidget(value2Button);
    

    connect(fieldCombo, SIGNAL(activated(int)), this, SLOT(fieldChanged(void)));
    connect(fieldCombo, SIGNAL(highlighted(int)), this, SLOT(fieldChanged(void)));    
    connect(operatorCombo, SIGNAL(activated(int)), this, SLOT(operatorChanged(void)));
    connect(operatorCombo, SIGNAL(highlighted(int)), this, SLOT(operatorChanged(void)));
    connect(value1Button, SIGNAL(clicked()), this, SLOT(value1ButtonClicked(void)));
    connect(value2Button, SIGNAL(clicked()), this, SLOT(value2ButtonClicked(void)));
    connect(value1Edit, SIGNAL(textChanged(void)), this, SLOT(valueChanged(void)));
    connect(value2Edit, SIGNAL(textChanged(void)), this, SLOT(valueChanged(void)));
    connect(value1SpinEdit, SIGNAL(valueChanged(const QString &)), this, SLOT(valueChanged(void)));
    connect(value2SpinEdit, SIGNAL(valueChanged(const QString &)), this, SLOT(valueChanged(void)));
    connect(value1Combo, SIGNAL(activated(int)), this, SLOT(valueChanged(void)));
    connect(value1Combo, SIGNAL(highlighted(int)), this, SLOT(valueChanged(void)));    
    connect(value2Combo, SIGNAL(activated(int)), this, SLOT(valueChanged(void)));
    connect(value2Combo, SIGNAL(highlighted(int)), this, SLOT(valueChanged(void)));    

        
    bUpdating = false;
    fieldChanged();
}

SmartPLCriteriaRow::~SmartPLCriteriaRow()
{
}

void SmartPLCriteriaRow::fieldChanged(void)
{
    bUpdating = true; // flag to prevent lots of criteria changed events
    
    if (fieldCombo->currentText() == "")
    {
        operatorCombo->setEnabled(false);
        value1Edit->setEnabled(false);       
        value2Edit->setEnabled(false);       
        value1SpinEdit->setEnabled(false);       
        value2SpinEdit->setEnabled(false);
        value1Button->setEnabled(false);
        value2Button->setEnabled(false);
        value1Combo->setEnabled(false);
        value2Combo->setEnabled(false);
    }
    else
    {
        operatorCombo->setEnabled(true);
        value1Edit->setEnabled(true);       
        value2Edit->setEnabled(true);       
        value1SpinEdit->setEnabled(true);       
        value2SpinEdit->setEnabled(true);
        value1Button->setEnabled(true);
        value2Button->setEnabled(true);
        value1Combo->setEnabled(true);
        value2Combo->setEnabled(true);
    }
    
    SmartPLField *Field;
    Field = lookupField(fieldCombo->currentText());
    if (!Field)
    {
        emit criteriaChanged();
        return;
    }
    
    if (Field->type == ftBoolean) 
    {
        // add yes / no items to combo
        value1Combo->clear();
        value1Combo->insertItem("No");
        value1Combo->insertItem("Yes");
        value2Combo->clear();
        value2Combo->insertItem("No");
        value2Combo->insertItem("Yes");
    }
    else if (Field->type == ftDate)
    {
        // add a couple of date values to the combo
        value1Combo->clear();
        value1Combo->insertItem("$DATE");
        value1Combo->insertItem("$DATE - 30 days");
        value1Combo->insertItem("$DATE - 60 days"); 

        value2Combo->clear();
        value2Combo->insertItem("$DATE");
        value2Combo->insertItem("$DATE - 30 days");
        value2Combo->insertItem("$DATE - 60 days"); 
    }
    
    // get list of operators valid for this field type
    getOperatorList(Field->type);
    
    operatorChanged();
    
    bUpdating = false;
}    

void SmartPLCriteriaRow::operatorChanged(void)
{
    bUpdating = true; // flag to prevent lots of criteria changed events

    SmartPLField *Field;
    Field = lookupField(fieldCombo->currentText());
    if (!Field)
    {
        emit criteriaChanged();
        return;
    }

    SmartPLOperator *Operator;
    Operator = lookupOperator(operatorCombo->currentText());
    if (!Operator)
    {    
        emit criteriaChanged();
        return;
    }
        
    // show/hide spin edits
    if (Field->type == ftNumeric)
    {
        // show hide second values
        if (Operator->noOfArguments == 2)
        {
            int currentValue = value2SpinEdit->value();
            value2SpinEdit->setMinValue(Field->minValue);
            value2SpinEdit->setMaxValue(Field->maxValue);
            
            if (currentValue < Field->minValue || currentValue > Field->maxValue)
                value2SpinEdit->setValue(Field->defaultValue);
     
            value2SpinEdit->show();
            value2Button->show();
        }
        else
        {
            value2SpinEdit->hide();
            value2Button->hide();    
        }    

        value1Edit->hide();
        value2Edit->hide();
        value1Button->hide();
        value2Button->hide();
        value1Combo->hide();
        value2Combo->hide();

        value1SpinEdit->show();
        
        int currentValue = value1SpinEdit->value();
        value1SpinEdit->setMinValue(Field->minValue);
        value1SpinEdit->setMaxValue(Field->maxValue);

        if (currentValue < Field->minValue || currentValue > Field->maxValue)
            value1SpinEdit->setValue(Field->defaultValue);
    }
    else if (Field->type == ftBoolean) 
    {
        // only show value1combo
        value1Edit->hide();
        value2Edit->hide();
        value1Button->hide();
        value2Button->hide();
        value1SpinEdit->hide();
        value2SpinEdit->hide();
        value2Combo->hide();
        
        value1Combo->show();
    }
    else if (Field->type == ftDate)
    {
        // show/hide second values
        if (Operator->noOfArguments == 2)
        {
            value2Combo->show();
            value2Button->show();
        }
        else
        {
            value2Combo->hide();
            value2Button->hide();    
        }    

        value1Edit->hide();
        value2Edit->hide();
        value1SpinEdit->hide();
        value2SpinEdit->hide();

        value1Combo->show();
        value1Button->show();
    }
    else // ftString
    {
        // show/hide second values
        if (Operator->noOfArguments == 2)
        {
            value2Edit->show();
            value2Button->show();
        }
        else
        {
            value2Edit->hide();
            value2Button->hide();    
        }    

        value1SpinEdit->hide();
        value2SpinEdit->hide();
        value1Combo->hide();
        value2Combo->hide();

        value1Edit->show();
        value1Button->show();
    }
    
    bUpdating = false;
    
    emit criteriaChanged();     
}

void SmartPLCriteriaRow::valueChanged(void)
{
    if (!bUpdating)
        emit criteriaChanged();
}

void SmartPLCriteriaRow::value1ButtonClicked(void)
{
    if (fieldCombo->currentText() == "Artist")
        searchArtist(value1Edit);
    else if (fieldCombo->currentText() == "Comp. Artist")
        searchCompilationArtist(value1Edit);
    else if (fieldCombo->currentText() == "Album")
        searchAlbum(value1Edit);
    else if (fieldCombo->currentText() == "Genre")
        searchGenre(value1Edit);
    else if (fieldCombo->currentText() == "Title")
        searchTitle(value1Edit);
    else if (fieldCombo->currentText() == "Last Play")
        editDate(value1Combo);
    else if (fieldCombo->currentText() == "Date Imported")
        editDate(value1Combo);
        
    value1Button->setFocus();    
}

void SmartPLCriteriaRow::value2ButtonClicked(void)  
{
    if (fieldCombo->currentText() == "Artist")
        searchArtist(value2Edit);
    else if (fieldCombo->currentText() == "Comp. Artist")
        searchCompilationArtist(value2Edit);
    else if (fieldCombo->currentText() == "Album")
        searchAlbum(value2Edit);
    else if (fieldCombo->currentText() == "Genre")
        searchGenre(value2Edit);
    else if (fieldCombo->currentText() == "Title")
        searchTitle(value2Edit);
    else if (fieldCombo->currentText() == "Last Play")
        editDate(value2Combo);
    else if (fieldCombo->currentText() == "Date Imported")
        editDate(value2Combo);
    
    value2Button->setFocus();        
}

void SmartPLCriteriaRow::editDate(MythComboBox *combo)
{
    bool res = false;
    
    SmartPLDateDialog *dateDialog = new SmartPLDateDialog(gContext->GetMainWindow(), "");
    dateDialog->setDate(combo->currentText());
    if (dateDialog->ExecPopup() == 0)
    {
        combo->insertItem(dateDialog->getDate());
        combo->setCurrentText(dateDialog->getDate());
        res = true;
    }
    
    delete dateDialog;
}
          
bool SmartPLCriteriaRow::showList(QString caption, QString &value)
{
    bool res = false;
    
    MythSearchDialog *searchDialog = new MythSearchDialog(gContext->GetMainWindow(), "");
    searchDialog->setCaption(caption);
    searchDialog->setSearchText(value);
    searchDialog->setItems(searchList);
    if (searchDialog->ExecPopup() == 0)
    {
        value = searchDialog->getResult();
        res = true;
    }
    
    delete searchDialog;
    
    return res;     
}

void SmartPLCriteriaRow::fillSearchList(QString field)
{
    searchList.clear();
    
    MSqlQuery query(MSqlQuery::InitCon());
    QString querystr;
    querystr = QString("SELECT DISTINCT %1 FROM musicmetadata ORDER BY %2").arg(field).arg(field);
        
    query.exec(querystr);
    if (query.isActive() && query.numRowsAffected())
    {
        while (query.next())
        {
            searchList << QString::fromUtf8(query.value(0).toString());
        }
    }         
}
    
void SmartPLCriteriaRow::searchArtist(MythRemoteLineEdit *editor)
{
    QString s;
    
    fillSearchList("artist");
    
    s = editor->text();
    if (showList(tr("Select an Artist"), s))
    {
        editor->setText(s);
    }
}

void SmartPLCriteriaRow::searchCompilationArtist(MythRemoteLineEdit *editor)
{
    QString s;
    
    fillSearchList("compilation_artist");
    
    s = editor->text();
    if (showList(tr("Select a Compilation Artist"), s))
    {
        editor->setText(s);
    }
}

void SmartPLCriteriaRow::searchAlbum(MythRemoteLineEdit *editor)
{
    QString s;
    
    fillSearchList("album");
    
    s = editor->text();
    if (showList(tr("Select an Album"), s))
    {
        editor->setText(s);
    }
}

void SmartPLCriteriaRow::searchGenre(MythRemoteLineEdit *editor)
{
    QString s;

    fillSearchList("genre");

    s = editor->text();
    if (showList(tr("Select a Genre"), s))
    {
        editor->setText(s);
    }
}

void SmartPLCriteriaRow::searchTitle(MythRemoteLineEdit *editor)
{
    QString s;

    fillSearchList("title");

    s = editor->text();
    if (showList(tr("Select a Title"), s))
    {
        editor->setText(s);
    }
}

QString SmartPLCriteriaRow::getSQL(void)
{
    if (fieldCombo->currentText() == "") 
        return QString::null;
        
    QString result = "";
    
    
    SmartPLField *Field;
    Field = lookupField(fieldCombo->currentText());
    if (!Field)
        return QString::null;
    
        
    QString value1, value2;
    
    if (Field->type == ftNumeric)    
    {
        value1 = value1SpinEdit->text();
        value2 = value2SpinEdit->text();
    }
    else if (Field->type == ftBoolean || Field->type == ftDate)
    {
        value1 = value1Combo->currentText();
        value2 = value2Combo->currentText();
    }
    else // ftString
    {
        value1 = value1Edit->text();
        value2 = value2Edit->text();
    }  

    result = getCriteriaSQL(fieldCombo->currentText(), operatorCombo->currentText(), 
                           value1, value2);
    
    return result;                              
}

// return false on error
bool SmartPLCriteriaRow::saveToDatabase(int smartPlaylistID)
{
    // save playlistitem to database
    
    if (fieldCombo->currentText() == "") 
        return true;                     
        
    QString Field = fieldCombo->currentText();
    QString Operator = operatorCombo->currentText();
    
    // get correct values
    SmartPLField *PLField;
    QString Value1;
    QString Value2;

    PLField = lookupField(fieldCombo->currentText());
    if (!PLField)
        return false;
    
    if (PLField->type == ftNumeric)    
    {
        Value1 = value1SpinEdit->text();
        Value2 = value2SpinEdit->text();
    }
    else if (PLField->type == ftBoolean)
    {
        // compilation field uses 0 = false;  >0 = true
        Value1 = (value1Combo->currentText() == "Yes") ? "1":"0";
        Value2 = (value2Combo->currentText() == "Yes") ? "1":"0";
    }
    else if (PLField->type == ftDate)
    {
        // TODO: date conversion
        Value1 = value1Combo->currentText();
        Value2 = value2Combo->currentText();
    }
    else // ftString
    {
        Value1 = value1Edit->text();
        Value2 = value2Edit->text();
    }
    
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO smartplaylistitem (smartplaylistid, field, operator,"
                  " value1, value2)"
                  "VALUES (:SMARTPLAYLISTID, :FIELD, :OPERATOR, :VALUE1, :VALUE2);");
    query.bindValue(":SMARTPLAYLISTID", smartPlaylistID);
    query.bindValue(":FIELD", Field.utf8());
    query.bindValue(":OPERATOR", Operator.utf8());
    query.bindValue(":VALUE1", Value1.utf8());
    query.bindValue(":VALUE2", Value2.utf8());                
    
    if (!query.exec())
    {
        MythContext::DBError("Inserting new smartplaylist item", query);
        return false;   
    }
    
    return true;
}

void SmartPLCriteriaRow::initValues(QString Field, QString Operator, QString Value1, QString Value2)
{
    fieldCombo->setCurrentText(Field);
    operatorCombo->setCurrentText(Operator);
    
    SmartPLField *PLField;
    PLField = lookupField(Field);
    if (PLField)
    {
        if (PLField->type == ftNumeric)
        {
            value1SpinEdit->setValue(Value1.toInt());
            value2SpinEdit->setValue(Value2.toInt());
        }
        else if (PLField->type == ftBoolean)
        {
            value1Combo->setCurrentText( (Value1 == "1") ? "Yes":"No" );
            value2Combo->setCurrentText( (Value2 == "1") ? "Yes":"No" );
        }
        else if (PLField->type == ftDate)
        {
            value1Combo->setCurrentText(Value1);
            value2Combo->setCurrentText(Value2);
        }
        else //ftString
        {
            value1Edit->setText(Value1);
            value2Edit->setText(Value2);
        }
    }
    else
    {
        value1SpinEdit->setValue(PLField->defaultValue);
        value2SpinEdit->setValue(PLField->defaultValue);
        value1Edit->setText("");
        value2Edit->setText("");
    }
}

void SmartPLCriteriaRow::getOperatorList(SmartPLFieldType fieldType)
{
    QString currentOperator = operatorCombo->currentText();
    
    operatorCombo->clear();
    
    for (int x = 0; x < SmartPLOperatorsCount; x++)
    {
        // don't add operators that only work with string fields 
        if (fieldType != ftString && SmartPLOperators[x].stringOnly)
            continue;
            
        // don't add operators that only work with boolean fields 
        if (fieldType == ftBoolean && !SmartPLOperators[x].validForBoolean)
            continue;
       
        operatorCombo->insertItem(SmartPLOperators[x].name);
    }
    
    // try to set the operatorCombo to the same operator or else the first item
    for (int x = 0; x < operatorCombo->count(); x++)
    {
        if (operatorCombo->text(x) == currentOperator)
        {
            operatorCombo->setCurrentItem(x);
            return;
        }
    }
    
    // default to first item in list
    operatorCombo->setCurrentItem(0);
}

/*
---------------------------------------------------------------------
*/

SmartPlaylistEditor::SmartPlaylistEditor(MythMainWindow *parent, const char *name)
              : MythDialog(parent, name)
{
    QVBoxLayout *vbox = new QVBoxLayout(this, (int)(15 * wmult));
    QHBoxLayout *hbox = new QHBoxLayout(vbox, (int)(0 * wmult));
     
    // Window title
    QString message = tr("Smart Playlist Editor");
    QLabel *label = new QLabel(message, this);
    QFont font = label->font();
    font.setPointSize(int (font.pointSize() * 1.2));
    font.setBold(true);
    label->setFont(font);
    label->setPaletteForegroundColor(QColor("yellow"));
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(label);

    // category
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    message = tr("Category:");
    label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(label);

    // category combo
    categoryCombo = new MythComboBox(false, this, "category" );
    getSmartPlaylistCategories();
    hbox->addWidget(categoryCombo);
    
    // category button
    categoryButton = new MythPushButton( this, "categoryButton" );
    categoryButton->setBackgroundOrigin(WindowOrigin);
    categoryButton->setText( "" );
    categoryButton->setEnabled(true);
    categoryButton->setMinimumHeight(categoryCombo->height());
    categoryButton->setMaximumHeight(categoryCombo->height());
    categoryButton->setMinimumWidth(categoryCombo->height());
    categoryButton->setMaximumWidth(categoryCombo->height());
    hbox->addWidget(categoryButton);
 
    // Title edit box
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    message = tr("Title:");
    label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(label);
    
    titleEdit = new MythRemoteLineEdit( this, "title" );
    titleEdit->setBackgroundOrigin(WindowOrigin);
    hbox->addWidget(titleEdit);

    // match
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));
    message = tr("Match");
    label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(label);

    matchCombo = new MythComboBox(false, this, "match" );
    matchCombo->insertItem(tr("All"));
    matchCombo->insertItem(tr("Any"));
    matchCombo->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(matchCombo);

    message = tr("Of The Following Conditions");
    label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    hbox->addWidget(label);

    // criteria Rows
    SmartPLCriteriaRow *row;
    criteriaRows.setAutoDelete(true);
    
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));
    row = new SmartPLCriteriaRow(this, hbox);
    connect(row, SIGNAL(criteriaChanged(void)), this, SLOT(updateMatches(void)));
    criteriaRows.append(row);
    
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));
    row = new SmartPLCriteriaRow(this, hbox);
    connect(row, SIGNAL(criteriaChanged(void)), this, SLOT(updateMatches(void)));
    criteriaRows.append(row);
    
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));
    row = new SmartPLCriteriaRow(this, hbox);
    connect(row, SIGNAL(criteriaChanged(void)), this, SLOT(updateMatches(void)));
    criteriaRows.append(row);
    
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));
    row = new SmartPLCriteriaRow(this, hbox);
    connect(row, SIGNAL(criteriaChanged(void)), this, SLOT(updateMatches(void)));
    criteriaRows.append(row);
   
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));
    message = tr("Order By:");
    label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(label);

    orderByCombo = new MythComboBox(false, this, "field" );
    for (int x = 0; x < SmartPLFieldsCount; x++)
    {
        if (SmartPLFields[x].name == "")
            orderByCombo->insertItem(SmartPLFields[x].name);
        else    
            orderByCombo->insertItem(SmartPLFields[x].name + " (A)");
    }
    hbox->addWidget(orderByCombo);
     
     // orderby button
    orderByButton = new MythPushButton( this, "orderByButton" );
    orderByButton->setBackgroundOrigin(WindowOrigin);
    orderByButton->setText( "" );
    orderByButton->setEnabled(true);
    orderByButton->setMinimumHeight(categoryCombo->height());
    orderByButton->setMaximumHeight(categoryCombo->height());
    orderByButton->setMinimumWidth(categoryCombo->height());
    orderByButton->setMaximumWidth(categoryCombo->height());
    hbox->addWidget(orderByButton);
 
    // matches
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));
    message = tr("Matches:");
    label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(label);
    
    message = "0";
    matchesLabel = new QLabel(message, this);
    matchesLabel->setLineWidth(2);
    matchesLabel->setFrameShape(QFrame::Panel);
    matchesLabel->setFrameShadow(QFrame::Sunken);
    matchesLabel->setBackgroundOrigin(WindowOrigin);
    matchesLabel->setMinimumWidth((int) (90 * hmult));
    matchesLabel->setMaximumWidth((int) (90 * hmult));
    matchesLabel->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(matchesLabel);
    matchesCount = 0;
    
    // limit
    message = tr("Limit:");
    label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(label);
    
    limitSpinEdit = new MythSpinBox( this, "limit" );
    limitSpinEdit->setBackgroundOrigin(WindowOrigin);
    limitSpinEdit->setMinValue(0);
    limitSpinEdit->setMaxValue(9999);
    limitSpinEdit->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(limitSpinEdit);
    
    message = " "; // spacer
    label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    hbox->addWidget(label);
       
    //  Exit Button
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    cancelButton = new MythPushButton( this, "cancel" );
    cancelButton->setBackgroundOrigin(WindowOrigin);
    cancelButton->setText( tr( "Cancel" ) );
    cancelButton->setEnabled(true);

    hbox->addWidget(cancelButton);

    // save button
    saveButton = new MythPushButton( this, "save" );
    saveButton->setBackgroundOrigin(WindowOrigin);
    saveButton->setText( tr( "Save" ) );
    saveButton->setEnabled(false);

    hbox->addWidget(saveButton);

    // showResults Button
    showResultsButton = new MythPushButton( this, "showresults" );
    showResultsButton->setBackgroundOrigin(WindowOrigin);
    showResultsButton->setText( tr( "Show Results" ) );
    showResultsButton->setEnabled(false);

    hbox->addWidget(showResultsButton);

    connect(matchCombo, SIGNAL(highlighted(int)), this, SLOT(updateMatches(void)));
    connect(titleEdit, SIGNAL(textChanged(void)), this, SLOT(titleChanged(void)));
    connect(categoryButton, SIGNAL(clicked()), this, SLOT(categoryClicked()));
    connect(saveButton, SIGNAL(clicked()), this, SLOT(saveClicked()));
    connect(cancelButton, SIGNAL(clicked()), this, SLOT(cancelClicked()));
    connect(showResultsButton, SIGNAL(clicked()), this, SLOT(showResultsClicked()));
    connect(orderByButton, SIGNAL(clicked()), this, SLOT(orderByClicked()));
    
    titleEdit->setFocus();
    category_popup = NULL;
    bPlaylistIsValid = false;
    
    gContext->addListener(this);
}

SmartPlaylistEditor::~SmartPlaylistEditor(void)
{
    gContext->removeListener(this);
}

void SmartPlaylistEditor::titleChanged(void)
{
    saveButton->setEnabled((bPlaylistIsValid && !titleEdit->text().isEmpty()));
}

void SmartPlaylistEditor::updateMatches(void)
{
    bPlaylistIsValid = true;
    
    QString sql = "select count(*) from musicmetadata ";
    sql += getWhereClause();
    
    MSqlQuery query(MSqlQuery::InitCon());
    if (query.exec(sql))
    {
        if (query.numRowsAffected() > 0)
        {
            query.first();
            matchesCount = query.value(0).toInt();
        }
        else
            matchesCount = 0;
    }    
    else
    {    
        bPlaylistIsValid = false;
        matchesCount = 0;
    }
        
    matchesLabel->setText(QString().setNum(matchesCount));    
    
    showResultsButton->setEnabled(matchesCount > 0);
    titleChanged();
}

void SmartPlaylistEditor::saveClicked(void)
{    
    // save smartplaylist to database
    
    QString name = titleEdit->text();
    QString category = categoryCombo->currentText();
    QString matchType = matchCombo->currentText();
    QString orderBy = orderByCombo->currentText();
    QString limit = limitSpinEdit->text();
    
    // lookup categoryid
    int categoryid = SmartPlaylistEditor::lookupCategoryID(category);
    
    // easier to delete any existing smartplaylist and recreate a new one
    if (!bNewPlaylist)
        SmartPlaylistEditor::deleteSmartPlaylist(originalCategory, originalName);
    else
        SmartPlaylistEditor::deleteSmartPlaylist(category, name);
    
    MSqlQuery query(MSqlQuery::InitCon());
    // insert new smartplaylist
    query.prepare("INSERT INTO smartplaylist (name, categoryid, matchtype, orderby, limitto) "
                "VALUES (:NAME, :CATEGORYID, :MATCHTYPE, :ORDERBY, :LIMIT);");
    query.bindValue(":NAME", name.utf8());
    query.bindValue(":CATEGORYID", categoryid);
    query.bindValue(":MATCHTYPE", matchType);
    query.bindValue(":ORDERBY", orderBy.utf8());
    query.bindValue(":LIMIT", limit);                
    
    if (!query.exec())
    {
        MythContext::DBError("Inserting new playlist", query);
        return;   
    }
    
    // get smartplaylistid
    int ID;
    query.prepare("SELECT smartplaylistid FROM smartplaylist "
                  "WHERE categoryid = :CATEGORYID AND name = :NAME;");
    query.bindValue(":CATEGORYID", categoryid);
    query.bindValue(":NAME", name.utf8());
    if (query.exec())
    {
        if (query.isActive() && query.numRowsAffected() > 0)
        {
            query.first();
            ID = query.value(0).toInt();
        }
        else
        {
            cout << "Failed to find ID for smartplaylist: " << name << endl;
            return;
        }
    }    
    else    
    {
        MythContext::DBError("Getting smartplaylist ID", query);
        return;   
    }
    
    // save smartplaylist items
    SmartPLCriteriaRow *row;    
    for (row = criteriaRows.first(); row; row = criteriaRows.next())
    {
        row->saveToDatabase(ID);
    }
    
    done(0);        
}

void SmartPlaylistEditor::newSmartPlaylist(QString category)
{
    categoryCombo->setCurrentText(category);
    titleEdit->setText("");
    originalCategory = category;
    originalName = "";

    bNewPlaylist = true;
}

void SmartPlaylistEditor::editSmartPlaylist(QString category, QString name)
{
    originalCategory = category;
    originalName = name;
    bNewPlaylist = false;
    loadFromDatabase(category, name);
}

void SmartPlaylistEditor::loadFromDatabase(QString category, QString name)
{    
    // load smartplaylist from database
    int categoryid = SmartPlaylistEditor::lookupCategoryID(category);
    
    MSqlQuery query(MSqlQuery::InitCon());
    int ID;
    
    query.prepare("SELECT smartplaylistid, name, categoryid, matchtype, orderby, limitto " 
                  "FROM smartplaylist WHERE name = :NAME AND categoryid = :CATEGORYID;");
    query.bindValue(":NAME", name.utf8());
    query.bindValue(":CATEGORYID", categoryid);
    if (query.exec())
    {
        if (query.isActive() && query.numRowsAffected() > 0)
        {
            query.first();
            ID = query.value(0).toInt();
            titleEdit->setText(name);
            categoryCombo->setCurrentText(category);
            matchCombo->setCurrentText(query.value(3).toString());
            orderByCombo->setCurrentText(QString::fromUtf8(query.value(4).toString()));
            limitSpinEdit->setValue(query.value(5).toInt());
        }
        else
        {
            cout << "Cannot find smartplaylist: " << name << endl;
            return;
        }
    }
    else
    {
        MythContext::DBError("Load smartplaylist", query);
        return;   
    }
    
    // load smartplaylist items
    SmartPLCriteriaRow *row;    
    uint rowCount;
    
    query.prepare("SELECT field, operator, value1, value2 " 
                  "FROM smartplaylistitem WHERE smartplaylistid = :ID "
                  "ORDER BY smartplaylistitemid;");
    query.bindValue(":ID", ID);
    if (!query.exec())
        MythContext::DBError("Load smartplaylist items", query);
    
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        rowCount = query.numRowsAffected();
        if (rowCount > criteriaRows.count())
        {
            rowCount = criteriaRows.count();
            cout << "Warning: got too many smartplaylistitems:" << rowCount << endl; 
        }
        
        query.first();
        for (uint x = 0; x < rowCount; x++)
        {
            row = criteriaRows.at(x);
            QString Field = QString::fromUtf8(query.value(0).toString());
            QString Operator = QString::fromUtf8(query.value(1).toString());
            QString Value1 = QString::fromUtf8(query.value(2).toString());
            QString Value2 = QString::fromUtf8(query.value(3).toString());
            if (row)
                row->initValues(Field, Operator, Value1, Value2);
                
            query.next();
        }    
    }
    else
    {
        cout << "Warning got no smartplaylistitems for ID:" << ID << endl;
    }
}

void SmartPlaylistEditor::cancelClicked(void)
{
    done(-1);
}

void SmartPlaylistEditor::categoryClicked(void)
{
   showCategoryPopup();
}

void SmartPlaylistEditor::showCategoryPopup()
{
    if (category_popup)
        return;

    category_popup = new MythPopupBox(gContext->GetMainWindow(), "category_popup");

    category_popup->addLabel(tr("Smart Playlist Categories"));
    
    categoryEdit = new MythRemoteLineEdit(category_popup, "categoryEdit" );
    categoryEdit->setBackgroundOrigin(category_popup->WindowOrigin);
    categoryEdit->setText(categoryCombo->currentText());
    connect(categoryEdit, SIGNAL(textChanged(void)), this, SLOT(categoryEditChanged(void)));

    category_popup->addWidget(categoryEdit);

    newCategoryButton = category_popup->addButton(tr("New Category"), this,
                              SLOT(newCategory()));
    deleteCategoryButton = category_popup->addButton(tr("Delete Category"), this,
                              SLOT(deleteCategory()));
    renameCategoryButton = category_popup->addButton(tr("Rename Category"), this,
                              SLOT(renameCategory()));
    category_popup->addButton(tr("Cancel"), this,
                              SLOT(closeCategoryPopup()));
    newCategoryButton->setFocus();
    categoryEditChanged();
    
    category_popup->ShowPopup(this, SLOT(closeCategoryPopup()));
}

void SmartPlaylistEditor::categoryEditChanged(void)
{
    if (categoryEdit->text() == categoryCombo->currentText())
    {
        newCategoryButton->setEnabled(false);
        deleteCategoryButton->setEnabled(true);
        renameCategoryButton->setEnabled(false);
    }
    else
    {
        newCategoryButton->setEnabled( (categoryEdit->text() != "") );
        deleteCategoryButton->setEnabled(false);
        renameCategoryButton->setEnabled( (categoryEdit->text() != "") );
    }
}

void SmartPlaylistEditor::closeCategoryPopup(void)
{
    if (!category_popup)
        return;

    category_popup->hide();
    delete category_popup;
    category_popup = NULL;
    categoryButton->setFocus();
}

void SmartPlaylistEditor::newCategory(void)
{
    // insert new smartplaylistcategory

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO smartplaylistcategory (name) "
                "VALUES (:NAME);");
    query.bindValue(":NAME", categoryEdit->text().utf8());
    
    if (!query.exec())
    {
        MythContext::DBError("Inserting new smartplaylist category", query);
        return;   
    }

    getSmartPlaylistCategories();
    categoryCombo->setCurrentText(categoryEdit->text());
    
    closeCategoryPopup();
}

void SmartPlaylistEditor::deleteCategory(void)
{
    QString category = categoryEdit->text();
    
    closeCategoryPopup();
    
    if (category.isNull() || category == "")
        return;
    
    if (!MythPopupBox::showOkCancelPopup(gContext->GetMainWindow(), 
            "Delete Category",
            tr("Are you sure you want to delete this Category?")
            + "\n\n\"" + category + "\"\n\n"
            + tr("It will also delete any Smart Playlists belonging to this category."), 
             false))
        return;    
    
    SmartPlaylistEditor::deleteCategory(category);
    
    getSmartPlaylistCategories();
    titleEdit->setText("");    
}

void SmartPlaylistEditor::renameCategory(void)
{
    if (categoryCombo->currentText() == categoryEdit->text())
        return;
        
    // change the category     
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE smartplaylistcategory SET name = :NEW_CATEGORY "
                  "WHERE name = :OLD_CATEGORY;");
    query.bindValue(":OLD_CATEGORY", categoryCombo->currentText().utf8());
    query.bindValue(":NEW_CATEGORY", categoryEdit->text().utf8());

    if (!query.exec())
        MythContext::DBError("Rename smartplaylist", query);
    
    if (!bNewPlaylist)
        originalCategory = categoryEdit->text();

    getSmartPlaylistCategories();
    categoryCombo->setCurrentText(categoryEdit->text());
    
    closeCategoryPopup();
}

QString SmartPlaylistEditor::getSQL(QString fields)
{
    QString sql, whereClause, orderByClause, limitClause;
    
    sql = "SELECT " + fields + " FROM musicmetadata ";
    whereClause = getWhereClause();
    orderByClause = getOrderByClause();
    if (limitSpinEdit->value() > 0)
        limitClause = " LIMIT " + limitSpinEdit->text();
    
    sql = sql + whereClause + orderByClause + limitClause;

    return sql;
}

QString SmartPlaylistEditor::getOrderByClause(void)
{
    return getOrderBySQL(orderByCombo->currentText());
}

QString SmartPlaylistEditor::getWhereClause(void)
{
    SmartPLCriteriaRow *row;
    QString sql, criteria, matchType;
    
    bool bFirst = true;
    
    sql = "WHERE ";
    for (row = criteriaRows.first(); row; row = criteriaRows.next())
    {
        criteria = row->getSQL();
        if (not criteria.isNull() && criteria != "")
        if (bFirst)
        {
            sql += criteria;
            bFirst = false;
        }
        else
        {
            if (matchCombo->currentText() == "Any")
                sql += " OR " + criteria;
            else
                sql += " AND " + criteria;    
        }               
    }
    
    return sql;
}

void SmartPlaylistEditor::showResultsClicked(void)
{
    QString sql = getSQL("intid, artist, album, title, genre, year, tracknum");
    
    SmartPLResultViewer *resultViewer = new SmartPLResultViewer(gContext->GetMainWindow(), "resultviewer");
    resultViewer->setSQL(sql);
    resultViewer->exec();
    delete resultViewer;
    
    showResultsButton->setFocus();
}

void SmartPlaylistEditor::orderByClicked(void)
{
    SmartPLOrderByDialog *orderByDialog = new SmartPLOrderByDialog(gContext->GetMainWindow(), "SmartPLOrderByDialog");
    
    orderByDialog->setFieldList(orderByCombo->currentText());
    
    if (orderByDialog->ExecPopup() == 0)
        orderByCombo->setCurrentText(orderByDialog->getFieldList());
    
    delete orderByDialog;
    
    orderByButton->setFocus();
}

void SmartPlaylistEditor::getSmartPlaylistCategories(void)
{
    categoryCombo->clear();
    MSqlQuery query(MSqlQuery::InitCon());

    if (query.exec("SELECT name FROM smartplaylistcategory ORDER BY name;"))
    {
        if (query.isActive() && query.numRowsAffected() > 0)
        {
            while (query.next())
                categoryCombo->insertItem(QString::fromUtf8(query.value(0).toString()));
        }
        else
        {
            cout << "Could not find any smartplaylist categories" << endl;
        }
    }
    else
    {
        MythContext::DBError("Load smartplaylist categories", query);
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
    query.prepare("SELECT smartplaylistid FROM smartplaylist WHERE name = :NAME "
                  "AND categoryid = :CATEGORYID;");
    query.bindValue(":NAME", name.utf8());
    query.bindValue(":CATEGORYID", categoryid);
    if (query.exec())
    {
        if (query.isActive() && query.numRowsAffected() > 0)
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
        MythContext::DBError("Delete smartplaylist", query);
        return false;
    } 
    
    //delete smartplaylist items
    query.prepare("DELETE FROM smartplaylistitem WHERE smartplaylistid = :ID;");
    query.bindValue(":ID", ID);
    if (!query.exec())
        MythContext::DBError("Delete smartplaylist items", query);

    //delete smartplaylist
    query.prepare("DELETE FROM smartplaylist WHERE smartplaylistid = :ID;");
    query.bindValue(":ID", ID);
    if (!query.exec())
        MythContext::DBError("Delete smartplaylist", query);
   
    return true;     
}

// static function to delete all smartplaylists belonging to the given category
// will also delete any associated smartplaylist items
bool SmartPlaylistEditor::deleteCategory(QString category)
{
    int categoryid = SmartPlaylistEditor::lookupCategoryID(category);
    MSqlQuery query(MSqlQuery::InitCon());
    
    //delete all smartplaylists with the selected category
    query.prepare("SELECT name FROM smartplaylist "
                  "WHERE categoryid = :CATEGORYID;");
    query.bindValue(":CATEGORYID", categoryid);
    if (!query.exec())
    {
        MythContext::DBError("Delete SmartPlaylist Category", query);
        return false;
    }
    
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            SmartPlaylistEditor::deleteSmartPlaylist(category, 
                   QString::fromUtf8(query.value(0).toString()));
        }                                             
    }
    
    // delete the category
    query.prepare("DELETE FROM smartplaylistcategory WHERE categoryid = :ID;");
    query.bindValue(":ID", categoryid);
    if (!query.exec())
        MythContext::DBError("Delete smartplaylist category", query);

    return true;
}

// static function to lookup the categoryid given its name
int SmartPlaylistEditor::lookupCategoryID(QString category)
{    
    int ID;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT categoryid FROM smartplaylistcategory "
                  "WHERE name = :CATEGORY;");
    query.bindValue(":CATEGORY", category.utf8());

    if (query.exec())
    {    
        if (query.isActive() && query.numRowsAffected() > 0)
        {
            query.first();
            ID = query.value(0).toInt();
        }
        else
        {
            cout << "Failed to find smart playlist category: " << category << endl;
            ID = -1;
        }
    }
    else
    {
        MythContext::DBError("Getting category ID", query);
        ID = -1;   
    }
    
    return ID;
}

void  SmartPlaylistEditor::getCategoryAndName(QString &category, QString &name)
{
    category = categoryCombo->currentText();
    name = titleEdit->text();
}

/*
---------------------------------------------------------------------
*/

SmartPLResultViewer::SmartPLResultViewer(MythMainWindow *parent, const char *name)
              : MythDialog(parent, name)
{
    QVBoxLayout *vbox = new QVBoxLayout(this, (int)(20 * wmult));
    QHBoxLayout *hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    // Window title
    QString message = tr("Smart Playlist Result Viewer");
    QLabel *label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(label);

    // listview
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));
    listView = new MythListView(this);
    listView->addColumn(tr("ID"));
    listView->addColumn(tr("Artist"));
    listView->addColumn(tr("Album"));
    listView->addColumn(tr("Title"));
    listView->addColumn(tr("Genre"));
    listView->addColumn(tr("Year"));
    listView->addColumn(tr("Track No."));
    listView->setSorting(-1);         // disable sorting
    hbox->addWidget(listView);    
    
    //  Exit Button
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    exitButton = new MythPushButton( this, "Program" );
    exitButton->setBackgroundOrigin(WindowOrigin);
    exitButton->setText( tr( "Exit" ) );
    exitButton->setEnabled(true);
    hbox->addWidget(exitButton);
    connect(exitButton, SIGNAL(clicked()), this, SLOT(exitClicked()));

    listView->setFocus();
}

SmartPLResultViewer::~SmartPLResultViewer()
{
}

void SmartPLResultViewer::exitClicked(void)
{
    accept();
}

void SmartPLResultViewer::setSQL(QString sql)
{
    listView->clear();

    MSqlQuery query(MSqlQuery::InitCon());
    query.exec(sql);
    
    QListViewItem *item;
    if (query.last())
    {
        do
        {
            item = new QListViewItem(listView, 
                query.value(0).toString(),
                QString::fromUtf8(query.value(1).toString()), 
                QString::fromUtf8(query.value(2).toString()),
                QString::fromUtf8(query.value(3).toString()), 
                QString::fromUtf8(query.value(4).toString()),
                query.value(5).toString(), 
                query.value(6).toString());
        } while (query.prev());
    }
    
    // set selection to first item
    item = listView->firstChild();
    if (item)
        listView->setSelected(item, true);     
}

/*
---------------------------------------------------------------------
*/

SmartPlaylistDialog::SmartPlaylistDialog(MythMainWindow *parent, const char *name) 
                 :MythPopupBox(parent, name)
{
    bool keyboard_accelerators = gContext->GetNumSetting("KeyboardAccelerators", 1);

    // we have to create a parentless layout because otherwise MythPopupbox
    // complains about already having a layout
    vbox = new QVBoxLayout((QWidget *) 0, (int)(10 * hmult));

    QHBoxLayout *hbox = new QHBoxLayout(vbox, (int)(10 * wmult));
    
    // create the widgets
    
    caption = new QLabel(QString(tr("Smart Playlists")), this);
    QFont font = caption->font();
    font.setPointSize(int (font.pointSize() * 1.2));
    font.setBold(true);
    caption->setFont(font);
    caption->setPaletteForegroundColor(QColor("yellow"));
    caption->setBackgroundOrigin(ParentOrigin);
    caption->setAlignment(Qt::AlignCenter);
    caption->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    caption->setMinimumWidth((int)(600 * hmult));
    caption->setMaximumWidth((int)(600 * hmult));  
    hbox->addWidget(caption);
    
    // category
    hbox = new QHBoxLayout(vbox, (int)(10 * hmult));
    categoryCombo = new MythComboBox(false, this, "categoryCombo");
    categoryCombo->setFocus(); 
    connect(categoryCombo, SIGNAL(highlighted(int)), this, SLOT(categoryChanged(void)));
    connect(categoryCombo, SIGNAL(activated(int)), this, SLOT(categoryChanged(void)));
    hbox->addWidget(categoryCombo);    
    getSmartPlaylistCategories();
    
    // listbox
    hbox = new QHBoxLayout(vbox, (int)(5 * hmult));
    listbox = new MythListBox(this);
    listbox->setScrollBar(false);
    listbox->setBottomScrollBar(false);
    hbox->addWidget(listbox);
    
    hbox = new QHBoxLayout(vbox, (int)(5 * wmult));
    selectButton = new MythPushButton(this, "selectbutton");
    if (keyboard_accelerators)
        selectButton->setText(tr("1 Select"));
    else
        selectButton->setText(tr("Select"));
    hbox->addWidget(selectButton);

    newButton = new MythPushButton(this, "newbutton");
    if (keyboard_accelerators)
        newButton->setText(tr("2 New"));
    else
        newButton->setText(tr("New"));
    hbox->addWidget(newButton);
    
    hbox = new QHBoxLayout(vbox, (int)(5 * wmult));
    editButton = new MythPushButton(this, "editbutton");
    if (keyboard_accelerators)    
        editButton->setText(tr("3 Edit"));
    else
        editButton->setText(tr("Edit"));
    hbox->addWidget(editButton);

    deleteButton = new MythPushButton(this, "deletebutton");
    if (keyboard_accelerators)    
        deleteButton->setText(tr("4 Delete"));
    else
        deleteButton->setText(tr("Delete"));
    hbox->addWidget(deleteButton);
    
    addLayout(vbox);
   
    connect(newButton, SIGNAL(clicked()), this, SLOT(newPressed()));
    connect(editButton, SIGNAL(clicked()), this, SLOT(editPressed()));
    connect(deleteButton, SIGNAL(clicked()), this, SLOT(deletePressed()));
    connect(selectButton, SIGNAL(clicked()), this, SLOT(selectPressed()));
    
    categoryChanged();
}

void SmartPlaylistDialog::setSmartPlaylist(QString Category, QString Name)
{
    // try to set the current playlist
    for (int x = 0; x < categoryCombo->count(); x++)
    {
        if (categoryCombo->text(x) == Category)
        {
            categoryCombo->setCurrentItem(x);
            categoryChanged();
            listbox->setCurrentItem(Name);
            listbox->setFocus();
            return;
        }    
    }
    
    // can't find the smartplaylist just select the first item
    categoryCombo->setCurrentItem(0);
    listbox->setCurrentItem(0); 
}

SmartPlaylistDialog::~SmartPlaylistDialog(void)
{
    if (vbox)
    {
        delete vbox;
        vbox = NULL;
    }
}

void SmartPlaylistDialog::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            if (action == "ESCAPE")
            {
                handled = true;
                done(-1);        
            }
            else if (action == "LEFT")
            {
                handled = true;
                focusNextPrevChild(false);
            }
            else if (action == "RIGHT")
            {
                handled = true;
                focusNextPrevChild(true);
            }
            else if (action == "UP")
            {
                handled = true;
                focusNextPrevChild(false);
            }
            else if (action == "DOWN")
            {
                handled = true;
                focusNextPrevChild(true);
            }
            else if (action == "1")
            {
                handled = true;
                selectPressed();
            }
            else if (action == "2")
            {
                handled = true;
                newPressed();
            }
            else if (action == "3")
            {
                handled = true;
                editPressed();
            }
            else if (action == "4")
            {
                handled = true;
                deletePressed();
            }
            else if (action == "SELECT" && listbox->hasFocus())
            {
                handled = true;
                selectPressed();
            }
            
        }
    }
    if (!handled)
        MythPopupBox::keyPressEvent(e);
}

void SmartPlaylistDialog::newPressed(void)
{
    SmartPlaylistEditor* editor = new SmartPlaylistEditor(gContext->GetMainWindow(), "SmartPlaylistEditor");
    editor->newSmartPlaylist(categoryCombo->currentText());
    
    editor->exec();
    QString category;
    QString name;
    editor->getCategoryAndName(category, name);

    delete editor;
        
    getSmartPlaylistCategories();
    
    // try to select the correct category and name
    categoryCombo->setCurrentText(category);
    categoryChanged();
    listbox->setCurrentItem(name);
    listbox->setFocus();
}

void SmartPlaylistDialog::selectPressed(void)   
{
    accept();
}

void SmartPlaylistDialog::deletePressed(void)
{
    if (!listbox->selectedItem())
        return;
        
    QString category = categoryCombo->currentText();
    QString name = listbox->selectedItem()->text();
    
    if (!MythPopupBox::showOkCancelPopup(gContext->GetMainWindow(), 
            "Delete SmartPlaylist",
            tr("Are you sure you want to delete this SmartPlaylist?")
            + "\n\n\"" + name + "\"", false))
    {    
        deleteButton->setFocus();   
        return;    
    }    
     
    SmartPlaylistEditor::deleteSmartPlaylist(category, name); 
        
    //refresh lists
    getSmartPlaylistCategories();
    categoryCombo->setCurrentText(category);
    categoryChanged();
    
    if (listbox->count() > 0)
        deleteButton->setFocus();
    else
        newButton->setFocus();        
}

void SmartPlaylistDialog::editPressed(void)
{
    QString category = categoryCombo->currentText();
    QString name = listbox->currentText();
      
    SmartPlaylistEditor* editor = new SmartPlaylistEditor(gContext->GetMainWindow(), "SmartPlaylistEditor");
    editor->editSmartPlaylist(category, name);
    
    editor->exec();
    editor->getCategoryAndName(category, name);
    getSmartPlaylistCategories();
    categoryChanged();    
    
    delete editor;
    
    // try to select the correct category and name
    categoryCombo->setCurrentText(category);
    listbox->setCurrentItem(name);
    listbox->setFocus();
}

void SmartPlaylistDialog::categoryChanged(void)
{
    getSmartPlaylists(categoryCombo->currentText());
}

void SmartPlaylistDialog::getSmartPlaylistCategories(void)
{
    categoryCombo->clear();
    MSqlQuery query(MSqlQuery::InitCon());

    if (query.exec("SELECT name FROM smartplaylistcategory ORDER BY name;"))
    {
        if (query.isActive() && query.numRowsAffected() > 0)
        {
            while (query.next())
                categoryCombo->insertItem(
                               QString::fromUtf8(query.value(0).toString()));
        }
    }    
    else
    {
        MythContext::DBError("Load smartplaylist categories", query);
    }
}

void SmartPlaylistDialog::getSmartPlaylists(QString category)
{
    int categoryid = SmartPlaylistEditor::lookupCategoryID(category);

    listbox->clear();

    MSqlQuery query(MSqlQuery::InitCon());    
    query.prepare("SELECT name FROM smartplaylist WHERE categoryid = :CATEGORYID "
                  "ORDER BY name;");
    query.bindValue(":CATEGORYID", categoryid);
                   
    if (query.exec())
    {
        if (query.isActive() && query.numRowsAffected() > 0)
        {
            while (query.next())
            {
                listbox->insertItem(QString::fromUtf8(query.value(0).toString()));
            }
            
            listbox->setCurrentItem(0);
            listbox->setTopItem(0);
        }
    }
    else
        MythContext::DBError("Load smartplaylist names", query);

    
    deleteButton->setEnabled( (listbox->count() > 0) );
    selectButton->setEnabled( (listbox->count() > 0) );
    editButton->setEnabled( (listbox->count() > 0) );
}

void SmartPlaylistDialog::getSmartPlaylist(QString &category, QString &name)
{
   category = categoryCombo->currentText();
   name = listbox->currentText();
}

/*
---------------------------------------------------------------------
*/

SmartPLOrderByDialog::SmartPLOrderByDialog(MythMainWindow *parent, const char *name) 
                 :MythPopupBox(parent, name)
{
    bool keyboard_accelerators = gContext->GetNumSetting("KeyboardAccelerators", 1);
    
    // we have to create a parent less layout because otherwise MythPopupbox
    // complains about already having a layout
    vbox = new QVBoxLayout((QWidget *) 0, (int)(10 * hmult));
    QHBoxLayout *hbox = new QHBoxLayout(vbox, (int)(10 * wmult));
    
    // create the widgets
    
    caption = new QLabel(QString(tr("Order By Fields")), this);
    QFont font = caption->font();
    font.setPointSize(int (font.pointSize() * 1.2));
    font.setBold(true);
    caption->setFont(font);
    caption->setPaletteForegroundColor(QColor("yellow"));
    caption->setBackgroundOrigin(ParentOrigin);
    caption->setAlignment(Qt::AlignCenter);
    caption->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    caption->setMinimumWidth((int)(400 * hmult));
    caption->setMaximumWidth((int)(400 * hmult));  
    hbox->addWidget(caption);
    
    // listbox
    hbox = new QHBoxLayout(vbox, (int)(5 * hmult));
    listbox = new MythListBox(this);
    listbox->setScrollBar(false);
    listbox->setBottomScrollBar(false);
    hbox->addWidget(listbox);
    
    // fields
    hbox = new QHBoxLayout(vbox, (int)(10 * hmult));
    orderByCombo = new MythComboBox(false, this, "orderByCombo");
    orderByCombo->setFocus(); 
    connect(orderByCombo, SIGNAL(highlighted(int)), this, SLOT(orderByChanged(void)));
    connect(orderByCombo, SIGNAL(activated(int)), this, SLOT(orderByChanged(void)));
    hbox->addWidget(orderByCombo);    
    getOrderByFields();

    hbox = new QHBoxLayout(vbox, (int)(5 * wmult));
    addButton = new MythPushButton(this, "addbutton");
    if (keyboard_accelerators)
        addButton->setText(tr("1 Add"));
    else
        addButton->setText(tr("Add"));
    hbox->addWidget(addButton);

    deleteButton = new MythPushButton(this, "deletebutton");
    if (keyboard_accelerators)
        deleteButton->setText(tr("2 Delete"));
    else
        deleteButton->setText(tr("Delete"));
    hbox->addWidget(deleteButton);
    
    hbox = new QHBoxLayout(vbox, (int)(5 * wmult));
    moveUpButton = new MythPushButton(this, "moveupbutton");
    if (keyboard_accelerators)
        moveUpButton->setText(tr("3 Move Up"));
    else
        moveUpButton->setText(tr("Move Up"));

    hbox->addWidget(moveUpButton);

    moveDownButton = new MythPushButton(this, "movedownbutton");
    if (keyboard_accelerators)
        moveDownButton->setText(tr("4 Move Down"));
    else
        moveDownButton->setText(tr("Move Down"));
    hbox->addWidget(moveDownButton);

    hbox = new QHBoxLayout(vbox, (int)(5 * wmult));
    ascendingButton = new MythPushButton(this, "ascendingbutton");
    if (keyboard_accelerators)
        ascendingButton->setText(tr("5 Ascending"));
    else
        ascendingButton->setText(tr("Ascending"));
    hbox->addWidget(ascendingButton);

    descendingButton = new MythPushButton(this, "descendingbutton");
    if (keyboard_accelerators)
        descendingButton->setText(tr("6 Descending"));
    else    
        descendingButton->setText(tr("Descending"));
    hbox->addWidget(descendingButton);
   
    hbox = new QHBoxLayout(vbox, (int)(5 * wmult));
    okButton = new MythPushButton(this, "okbutton");
    if (keyboard_accelerators)
        okButton->setText(tr("7 OK"));
    else
        okButton->setText(tr("OK"));
    hbox->addWidget(okButton);

    addLayout(vbox);
    
    connect(addButton, SIGNAL(clicked()), this, SLOT(addPressed()));
    connect(deleteButton, SIGNAL(clicked()), this, SLOT(deletePressed()));
    connect(moveUpButton, SIGNAL(clicked()), this, SLOT(moveUpPressed()));
    connect(moveDownButton, SIGNAL(clicked()), this, SLOT(moveDownPressed()));
    connect(ascendingButton, SIGNAL(clicked()), this, SLOT(ascendingPressed()));
    connect(descendingButton, SIGNAL(clicked()), this, SLOT(descendingPressed()));
    connect(okButton, SIGNAL(clicked()), this, SLOT(okPressed()));
    
    connect(listbox, SIGNAL(selectionChanged(QListBoxItem*)), this, 
            SLOT(listBoxSelectionChanged(QListBoxItem*)));
    
    orderByChanged();
}

SmartPLOrderByDialog::~SmartPLOrderByDialog(void)
{
    if (vbox)
    {
        delete vbox;
        vbox = NULL;
    }
}

QString SmartPLOrderByDialog::getFieldList(void)
{
    QString result;
    bool bFirst = true;
    
    for (unsigned i = 0; i < listbox->count(); i++)
    {
        if (bFirst)
        {
            bFirst = false;
            result = listbox->text(i);
        }
        else
            result += ", " + listbox->text(i);
    }
    
    return result;    
}
    
void SmartPLOrderByDialog::setFieldList(QString fieldList)
{
    listbox->clear();
    QStringList list = QStringList::split(",", fieldList);
    
    for (uint x = 0; x < list.count(); x++)
        listbox->insertItem(list[x].stripWhiteSpace());
    
    orderByChanged();
}
        
void SmartPLOrderByDialog::listBoxSelectionChanged(QListBoxItem *item)
{
    if (!item) 
        return;
    
    orderByCombo->setCurrentText(item->text().left(item->text().length() - 4));
}

void SmartPLOrderByDialog::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            if (action == "ESCAPE")
            {
                handled = true;
                done(-1);        
            }
            else if (action == "LEFT")
            {
                handled = true;
                focusNextPrevChild(false);
            }
            else if (action == "RIGHT")
            {
                handled = true;
                focusNextPrevChild(true);
            }
            else if (action == "UP")
            {
                handled = true;
                focusNextPrevChild(false);
            }
            else if (action == "DOWN")
            {
                handled = true;
                focusNextPrevChild(true);
            }
            else if (action == "1")
            {
                handled = true;
                addPressed();
            }
            else if (action == "2")
            {
                handled = true;
                deletePressed();
            }
            else if (action == "3")
            {
                handled = true;
                moveUpPressed();
            }
            else if (action == "4")
            {
                handled = true;
                moveDownPressed();
            }
            else if (action == "5")
            {
                handled = true;
                ascendingPressed();
            }
            else if (action == "6")
            {
                handled = true;
                descendingPressed();
            }
            else if (action == "7")
            {
                handled = true;
                okPressed();
            }
        }
    }
    if (!handled)
        MythPopupBox::keyPressEvent(e);
}

void SmartPLOrderByDialog::ascendingPressed(void)   
{
    listbox->changeItem(orderByCombo->currentText() + " (A)", listbox->currentItem());
    orderByChanged();
    descendingButton->setFocus();
}

void SmartPLOrderByDialog::descendingPressed(void)   
{
    listbox->changeItem(orderByCombo->currentText() + " (D)", listbox->currentItem());
    orderByChanged();
    ascendingButton->setFocus();
}

void SmartPLOrderByDialog::addPressed(void)
{
    listbox->insertItem(orderByCombo->currentText() + " (A)");
    orderByChanged();
    orderByCombo->setFocus();
}

void SmartPLOrderByDialog::deletePressed(void)
{
    listbox->removeItem(listbox->currentItem());
    orderByChanged();
}

void SmartPLOrderByDialog::moveUpPressed(void)
{
    QString item1, item2;
    int currentItem = listbox->currentItem();    
    
    if (!listbox->selectedItem() || !listbox->selectedItem()->prev())
        return;
    
    item1 = listbox->selectedItem()->text();
    item2 = listbox->selectedItem()->prev()->text();
 
    listbox->changeItem(item1, currentItem - 1);
    listbox->changeItem(item2, currentItem);
    
    listbox->setSelected(listbox->selectedItem()->prev(), true);
}

void SmartPLOrderByDialog::moveDownPressed(void)
{
    QString item1, item2;
    int currentItem = listbox->currentItem();    
    
    if (!listbox->selectedItem() || !listbox->selectedItem()->next())
        return;
    
    item1 = listbox->selectedItem()->text();
    item2 = listbox->selectedItem()->next()->text();
 
    listbox->changeItem(item1, currentItem + 1);
    listbox->changeItem(item2, currentItem);
    
    listbox->setSelected(listbox->selectedItem()->next(), true);
}

void SmartPLOrderByDialog::okPressed(void)
{
    done(0);
}

void SmartPLOrderByDialog::orderByChanged(void)
{
    bool found = false;
    for (unsigned i = 0 ; i < listbox->count() ; ++i)
    {
        if (listbox->text(i).startsWith(orderByCombo->currentText()))
        {
            listbox->setSelected(i, true);
            found = true;
        }
    }

    if (found)
    {
        addButton->setEnabled(false);
        deleteButton->setEnabled(true);
        moveUpButton->setEnabled( (listbox->currentItem() != 0) );
        moveDownButton->setEnabled( (listbox->currentItem() != (int) listbox->count() - 1) );
        ascendingButton->setEnabled( (listbox->selectedItem()->text().right(3) == "(D)") );
        descendingButton->setEnabled((listbox->selectedItem()->text().right(3) == "(A)"));
    }
    else
    {
        addButton->setEnabled(true);
        deleteButton->setEnabled(false);
        moveUpButton->setEnabled(false);
        moveDownButton->setEnabled(false);
        ascendingButton->setEnabled(false);
        descendingButton->setEnabled(false);

        listbox->clearSelection();
    }
}

void SmartPLOrderByDialog::getOrderByFields(void)
{
    orderByCombo->clear();
    for (int x = 1; x < SmartPLFieldsCount; x++)
        orderByCombo->insertItem(SmartPLFields[x].name);
}

/*
---------------------------------------------------------------------
*/

SmartPLDateDialog::SmartPLDateDialog(MythMainWindow *parent, const char *name) 
                 :MythPopupBox(parent, name)
{
    // we have to create a parent less layout because otherwise MythPopupbox
    // complains about already having a layout
    vbox = new QVBoxLayout(NULL, 0, (int)(15 * hmult));
    QHBoxLayout *hbox = new QHBoxLayout(vbox, (int)(15 * wmult));
    
    // create the widgets
    
    caption = new QLabel(tr("Edit Date"), this);
    QFont font = caption->font();
    font.setPointSize(int (font.pointSize() * 1.2));
    font.setBold(true);
    caption->setFont(font);
    caption->setPaletteForegroundColor(QColor("yellow"));
    caption->setBackgroundOrigin(ParentOrigin);
    caption->setAlignment(Qt::AlignCenter);
    caption->setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred));
    caption->setMinimumWidth((int)(400 * hmult));
    caption->setMaximumWidth((int)(400 * hmult));  
    hbox->addWidget(caption);
    
    // fixed date widgets
    QDate date = QDate::currentDate();
    hbox = new QHBoxLayout(vbox, (int)(10 * hmult));
    fixedRadio = new MythRadioButton(this, "nopopsize");
    fixedRadio->setText(tr("Fixed Date"));
    fixedRadio->setBackgroundOrigin(ParentOrigin);
    fixedRadio->setChecked(true);
    fixedRadio->setFocus();
    hbox->addWidget(fixedRadio);
    
    hbox = new QHBoxLayout(vbox, (int)(10 * hmult));
    dayLabel = new QLabel(tr("Day"), this, "nopopsize");
    dayLabel->setBackgroundOrigin(ParentOrigin);
    dayLabel->setAlignment(Qt::AlignLeft);
    dayLabel->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(dayLabel);

    daySpinEdit = new MythSpinBox(this);
    daySpinEdit->setBackgroundOrigin(ParentOrigin);
    daySpinEdit->setMinValue(1);
    daySpinEdit->setMaxValue(31);
    daySpinEdit->setValue(date.day());
    hbox->addWidget(daySpinEdit);

    monthLabel = new QLabel(QString(tr("Month")), this, "nopopsize");
    monthLabel->setBackgroundOrigin(ParentOrigin);
    monthLabel->setAlignment(Qt::AlignLeft);
    monthLabel->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(monthLabel);

    monthSpinEdit = new MythSpinBox(this);
    monthSpinEdit->setBackgroundOrigin(ParentOrigin);
    monthSpinEdit->setMinValue(1);
    monthSpinEdit->setMaxValue(12);
    monthSpinEdit->setValue(date.month());
    hbox->addWidget(monthSpinEdit);

    yearLabel = new QLabel(QString(tr("Year")), this, "nopopsize");
    yearLabel->setBackgroundOrigin(ParentOrigin);
    yearLabel->setAlignment(Qt::AlignLeft);
    yearLabel->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(yearLabel);

    yearSpinEdit = new MythSpinBox(this);
    yearSpinEdit->setBackgroundOrigin(ParentOrigin);
    yearSpinEdit->setMinValue(1900);
    yearSpinEdit->setMaxValue(2099);
    yearSpinEdit->setValue(date.year());
    hbox->addWidget(yearSpinEdit);
    
    hbox = new QHBoxLayout(vbox, (int)(10 * hmult));
    QLabel *splitter = new QLabel("", this);
    splitter->setLineWidth(2);
    splitter->setFrameShape(QFrame::HLine);
    splitter->setFrameShadow(QFrame::Sunken);
    splitter->setMaximumHeight((int) (5 * hmult));
    splitter->setMaximumHeight((int) (5 * hmult));
    hbox->addWidget(splitter);

    // date now widgets
    hbox = new QHBoxLayout(vbox, (int)(10 * hmult));
    nowRadio = new MythRadioButton(this);
    nowRadio->setText(tr("Use Current Date"));
    nowRadio->setBackgroundOrigin(ParentOrigin);
    nowRadio->setChecked(false);
    hbox->addWidget(nowRadio);

    
    hbox = new QHBoxLayout(vbox, (int)(10 * hmult));
    splitter = new QLabel("          ", this, "nopopsize");
    splitter->setBackgroundOrigin(ParentOrigin);
    splitter->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(splitter);

    // add/take days widgets
    addDaysCheck = new MythCheckBox(this);
    addDaysCheck->setText(tr("+/- Days"));
    addDaysCheck->setBackgroundOrigin(ParentOrigin);
    hbox->addWidget(addDaysCheck);
  
    addDaysSpinEdit = new MythSpinBox(this, "nopopsize");
    addDaysSpinEdit->setBackgroundOrigin(WindowOrigin);
    addDaysSpinEdit->setMinValue(-9999);
    addDaysSpinEdit->setMaxValue(9999);
    hbox->addWidget(addDaysSpinEdit);
    
    // status label
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));
    statusLabel = new QLabel(QString(""), this);
    statusLabel->setLineWidth(2);
    statusLabel->setFrameShape(QFrame::Panel);
    statusLabel->setFrameShadow(QFrame::Sunken);
    statusLabel->setBackgroundOrigin(ParentOrigin);
    statusLabel->setAlignment(Qt::AlignLeft);
    statusLabel->setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed));
    statusLabel->setAlignment(Qt::AlignCenter);
    hbox->addWidget(statusLabel);

    // buttons
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));
    okButton = new MythPushButton(this);
    okButton->setText(tr("OK"));
    hbox->addWidget(okButton);
    
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));
    cancelButton = new MythPushButton(this);
    cancelButton->setText(tr("Cancel"));
    hbox->addWidget(cancelButton);

    addLayout(vbox, 0);
    
    connect(okButton, SIGNAL(clicked()), this, SLOT(okPressed()));
    connect(cancelButton, SIGNAL(clicked()), this, SLOT(cancelPressed()));

    connect(fixedRadio, SIGNAL(toggled(bool)), this, SLOT(fixedCheckToggled(bool)));
    connect(nowRadio, SIGNAL(toggled(bool)), this, SLOT(nowCheckToggled(bool)));
    connect(addDaysCheck, SIGNAL(toggled(bool)), this, SLOT(addDaysCheckToggled(bool)));
    connect(addDaysSpinEdit, SIGNAL(valueChanged(const QString &)), 
            this, SLOT(valueChanged(void)));
    connect(daySpinEdit, SIGNAL(valueChanged(const QString &)), 
            this, SLOT(valueChanged(void)));
    connect(monthSpinEdit, SIGNAL(valueChanged(const QString &)), 
            this, SLOT(valueChanged(void)));
    connect(yearSpinEdit, SIGNAL(valueChanged(const QString &)), 
            this, SLOT(valueChanged(void)));
    
    valueChanged();        
}


SmartPLDateDialog::~SmartPLDateDialog(void)
{
    if (vbox)
    {
        delete vbox;
        vbox = NULL;
    }
}

QString SmartPLDateDialog::getDate(void)
{
    QString sResult;
    
    if (fixedRadio->isChecked())
    {
        QString day = daySpinEdit->text();
        if (daySpinEdit->value() < 10)
            day = "0" + day;
            
        QString month = monthSpinEdit->text();
        if (monthSpinEdit->value() < 10)      
            month = "0" + month;
        
        sResult = yearSpinEdit->text() + "-" + month + "-" + day;
    
    }
    else
       sResult = statusLabel->text(); 
    
    return sResult;    
}
    
void SmartPLDateDialog::setDate(QString date)
{
    if (date.startsWith("$DATE"))
    {
        nowRadio->setChecked(true);
        
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
                
            addDaysCheck->setEnabled(true);
            addDaysCheck->setChecked(true);
            addDaysSpinEdit->setEnabled(true);
            addDaysSpinEdit->setValue(nDays);
        }
        else
        {
            addDaysCheck->setEnabled(false);
            addDaysSpinEdit->setEnabled(false);
            addDaysSpinEdit->setValue(0);
        }
        
        nowCheckToggled(true);
    }
    else
    {
        int nYear = date.mid(0, 4).toInt();
        int nMonth = date.mid(5, 2).toInt();
        int nDay = date.mid(8, 2).toInt();
        
        daySpinEdit->setValue(nDay);
        monthSpinEdit->setValue(nMonth);
        yearSpinEdit->setValue(nYear);
        
        fixedCheckToggled(true);
    }
}
        
void SmartPLDateDialog::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            if (action == "ESCAPE")
            {
                handled = true;
                done(-1);        
            }
            else if (action == "LEFT")
            {
                handled = true;
                focusNextPrevChild(false);
            }
            else if (action == "RIGHT")
            {
                handled = true;
                focusNextPrevChild(true);
            }
            else if (action == "UP")
            {
                handled = true;
                focusNextPrevChild(false);
            }
            else if (action == "DOWN")
            {
                handled = true;
                focusNextPrevChild(true);
            }
        }
    }
    if (!handled)
        MythPopupBox::keyPressEvent(e);
}

void SmartPLDateDialog::okPressed(void)
{
    done(0);
}

void SmartPLDateDialog::cancelPressed(void)
{
    done(-1);
}

void SmartPLDateDialog::fixedCheckToggled(bool on)
{
    daySpinEdit->setEnabled(on);
    monthSpinEdit->setEnabled(on);
    yearSpinEdit->setEnabled(on);
    dayLabel->setEnabled(on);
    monthLabel->setEnabled(on);
    yearLabel->setEnabled(on);
    
    nowRadio->setChecked(!on);
    addDaysCheck->setEnabled(!on);
    addDaysSpinEdit->setEnabled(!on && addDaysCheck->isChecked());    
    
    valueChanged();
}

void SmartPLDateDialog::nowCheckToggled(bool on)
{
    fixedRadio->setChecked(!on);
    daySpinEdit->setEnabled(!on);
    monthSpinEdit->setEnabled(!on);
    yearSpinEdit->setEnabled(!on);
    dayLabel->setEnabled(!on);
    monthLabel->setEnabled(!on);
    yearLabel->setEnabled(!on);
    
    nowRadio->setChecked(on);
    addDaysCheck->setEnabled(on);
    
    addDaysSpinEdit->setEnabled(on && addDaysCheck->isChecked());    
    
    valueChanged();
}

void SmartPLDateDialog::addDaysCheckToggled(bool on)
{
    addDaysSpinEdit->setEnabled(on);
    
    valueChanged();
}    

void SmartPLDateDialog::valueChanged(void)
{
    bool bValidDate = true;
    
    if (fixedRadio->isChecked())
    {
        QString day = daySpinEdit->text();
        if (daySpinEdit->value() < 10)
            day = "0" + day;
            
        QString month = monthSpinEdit->text();
        if (monthSpinEdit->value() < 10)      
            month = "0" + month;
        
        QString sDate = yearSpinEdit->text() + "-" + month + "-" + day;
        QDate date = QDate::fromString(sDate, Qt::ISODate);
        if (date.isValid())
            statusLabel->setText(date.toString("dddd, d MMMM yyyy"));
        else
        {    
            bValidDate = false;
            statusLabel->setText(tr("Invalid Date"));
        } 
    }
    else if (nowRadio->isChecked())
    {
        if (addDaysCheck->isChecked())
        {
            QString days;
            if (addDaysSpinEdit->value() > 0)
                days = QString("$DATE + %1 days").arg(addDaysSpinEdit->value());
            else if (addDaysSpinEdit->value() == 0)
                days = QString("$DATE");
            else
                days = QString("$DATE - %1 days").arg(
                    addDaysSpinEdit->text().right(addDaysSpinEdit->text().length() - 1));
                
            statusLabel->setText(days);
        }
        else
            statusLabel->setText("$DATE");
    }
    
    if (bValidDate)
        statusLabel->setPaletteForegroundColor(QColor("green"));
    else    
        statusLabel->setPaletteForegroundColor(QColor("red"));

    okButton->setEnabled(bValidDate);
}

