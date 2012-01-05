#ifndef SMARTPLAYLIST_H_
#define SMARTPLAYLIST_H_

// c/c++
#include <vector>

class MythUIButton;using namespace std;

// qt
#include <QDateTime>
#include <QVariant>
#include <QKeyEvent>

// mythtv
#include <mythscreentype.h>

struct SmartPLOperator;
struct SmartPLField;

enum SmartPLFieldType
{
    ftString = 1,
    ftNumeric,
    ftDate,
    ftBoolean
};

// used by playlist.cpp
QString getCriteriaSQL(QString fieldName, QString operatorName, 
                       QString value1, QString value2);
 
QString getSQLFieldName(QString orderBy);
QString getOrderBySQL(QString orderByFields);

// used by playbackbox.cpp
QString formattedFieldValue(const QVariant &value);

/*
/////////////////////////////////////////////////////////////////////////////
*/

class SmartPLCriteriaRow
{
  public:

    SmartPLCriteriaRow(const QString &_Field, const QString &_Operator,
                       const QString &_Value1, const QString &_Value2);
    SmartPLCriteriaRow(void);

    ~SmartPLCriteriaRow(void);

    QString getSQL(void);

    bool saveToDatabase(int smartPlaylistID);

    QString toString(void);

  public:
    QString Field;
    QString Operator;
    QString Value1;
    QString Value2;
};

Q_DECLARE_METATYPE(SmartPLCriteriaRow *)

class SmartPlaylistEditor : public MythScreenType
{
    Q_OBJECT
  public:

    SmartPlaylistEditor(MythScreenStack *parent);
   ~SmartPlaylistEditor(void);

    bool Create(void);

    bool keyPressEvent(QKeyEvent *event);
    void customEvent(QEvent *event);

    QString getSQL(QString fields);
    QString getWhereClause(void);
    QString getOrderByClause(void);
    void getCategoryAndName(QString &category, QString &name);
    void newSmartPlaylist(QString category);
    void editSmartPlaylist(QString category, QString name);
    static bool deleteSmartPlaylist(QString category, QString name);
    static bool deleteCategory(QString category);
    static int  lookupCategoryID(QString category);

  signals:
    void smartPLChanged(const QString &category, const QString &name);

  protected slots:

    void titleChanged(void);
    void updateMatches(void);
//    void categoryClicked(void);
    void saveClicked(void);
    void showResultsClicked(void);

    void showCategoryMenu(void);
    void showCriteriaMenu(void);

    void newCategory(const QString &category);
    void startDeleteCategory(const QString &category);
    void renameCategory(const QString &category);

//    void categoryEditChanged(void);    
    void orderByClicked(void);

    void editCriteria(void);
    void addCriteria(void);
    void deleteCriteria(void);
    void doDeleteCriteria(bool doit);
    void criteriaChanged();
    void orderByChanged(QString orderBy);

  private:
    void getSmartPlaylistCategories(void);  
    void loadFromDatabase(QString category, QString name);

    QList<SmartPLCriteriaRow*> m_criteriaRows;
    SmartPLCriteriaRow* m_tempCriteriaRow;

    int matchesCount;
    bool bNewPlaylist;
    bool bPlaylistIsValid;
    QString originalCategory, originalName;

    // gui stuff
    MythUIButtonList *m_categorySelector;
    MythUIButton *m_categoryButton;
    MythUITextEdit *m_titleEdit;
    MythUIButtonList *m_matchSelector;
    MythUIButtonList *m_criteriaList;
    MythUIButtonList *m_orderBySelector;
    MythUIButton *m_orderByButton;
    MythUIText *m_matchesText;
    MythUISpinBox *m_limitSpin;
    MythUIButton *m_cancelButton;
    MythUIButton *m_saveButton;
    MythUIButton *m_showResultsButton;
};

class CriteriaRowEditor : public MythScreenType
{
    Q_OBJECT
  public:

    CriteriaRowEditor(MythScreenStack *parent, SmartPLCriteriaRow *row);
   ~CriteriaRowEditor(void);

    bool Create(void);

  protected slots:
    void fieldChanged(void);
    void operatorChanged(void);
    void valueEditChanged(void);
    void valueButtonClicked(void);
    void setValue(QString value);
    void setDate(QString date);
    void saveClicked(void);

  signals:
    void criteriaChanged();

  private:
    void updateFields(void);
    void updateOperators(void);
    void updateValues(void);
    void enableSaveButton(void);

    void getOperatorList(SmartPLFieldType fieldType);

    void editDate(void);

    SmartPLCriteriaRow* m_criteriaRow;

    QStringList m_searchList;

    // gui stuff
    MythUIButtonList *m_fieldSelector;
    MythUIButtonList *m_operatorSelector;

    MythUITextEdit *m_value1Edit;
    MythUITextEdit *m_value2Edit;

    MythUIButtonList *m_value1Selector;
    MythUIButtonList *m_value2Selector;

    MythUISpinBox *m_value1Spinbox;
    MythUISpinBox *m_value2Spinbox;

    MythUIButton *m_value1Button;
    MythUIButton *m_value2Button;

    MythUIText   *m_andText;

    MythUIButton *m_cancelButton;
    MythUIButton *m_saveButton;
};


class SmartPLResultViewer : public MythScreenType
{
  Q_OBJECT

  public:

    SmartPLResultViewer(MythScreenStack *parent);
   ~SmartPLResultViewer(void);

    bool Create(void);
    bool keyPressEvent(QKeyEvent *event);
    void setSQL(QString sql);

  private slots:
    void trackVisible(MythUIButtonListItem *item);
    void trackSelected(MythUIButtonListItem *item);

  private:
    void showTrackInfo(void);

    MythUIButtonList *m_trackList;
    MythUIText *m_positionText;
};


class SmartPLOrderByDialog: public MythScreenType
{
  Q_OBJECT

  public:

    SmartPLOrderByDialog(MythScreenStack *parent); 
    ~SmartPLOrderByDialog();

    bool Create(void);

    QString getFieldList(void);
    void setFieldList(const QString &fieldList);

  signals:
    void orderByChanged(QString orderBy);

  protected slots:
    void addPressed(void);
    void deletePressed(void);   
    void moveUpPressed(void);
    void moveDownPressed(void);
    void ascendingPressed(void);
    void descendingPressed(void);
    void okPressed(void);
    void orderByChanged(void);
    void fieldListSelectionChanged(MythUIButtonListItem *item);

  private:
    void getOrderByFields(void);

    MythUIButtonList *m_fieldList;  
    MythUIButtonList *m_orderSelector;
    MythUIButton     *m_addButton;
    MythUIButton     *m_deleteButton;
    MythUIButton     *m_moveUpButton;
    MythUIButton     *m_moveDownButton;
    MythUIButton     *m_ascendingButton;
    MythUIButton     *m_descendingButton;
    MythUIButton     *m_cancelButton;
    MythUIButton     *m_okButton;
};

class SmartPLDateDialog: public MythScreenType
{
  Q_OBJECT

  public:

    SmartPLDateDialog(MythScreenStack *parent); 
    ~SmartPLDateDialog();

    bool Create(void);

    QString getDate(void);
    void setDate(QString date);

  signals:
    void dateChanged(QString date);

  protected slots:
    void okPressed(void);
    void fixedCheckToggled(bool on);
    void nowCheckToggled(bool on);
    void valueChanged(void);

  private:

    bool              m_updating;

    MythUICheckBox   *m_fixedRadio;
    MythUISpinBox    *m_daySpin;
    MythUISpinBox    *m_monthSpin;
    MythUISpinBox    *m_yearSpin;

    MythUICheckBox   *m_nowRadio;
    MythUISpinBox    *m_addDaysSpin;

    MythUIText       *m_statusText;

    MythUIButton     *m_cancelButton;
    MythUIButton     *m_okButton;
};

#endif
