#ifndef SMARTPLAYLIST_H_
#define SMARTPLAYLIST_H_

// c/c++
#include <utility>
#include <vector>

class MythUIButton;

// qt
#include <QDateTime>
#include <QVariant>
#include <QKeyEvent>
#include <QCoreApplication>

// MythTV
#include <libmythui/mythscreentype.h>

struct SmartPLOperator;
struct SmartPLField;

enum SmartPLFieldType : std::uint8_t
{
    ftString = 1,
    ftNumeric,
    ftDate,
    ftBoolean
};

// used by playlist.cpp
QString getCriteriaSQL(const QString& fieldName, const QString &operatorName,
                       QString value1, QString value2);

QString getSQLFieldName(const QString &fieldName);
QString getOrderBySQL(const QString& orderByFields);

// used by playbackbox.cpp
QString formattedFieldValue(const QVariant &value);

/*
/////////////////////////////////////////////////////////////////////////////
*/

class SmartPLCriteriaRow
{
    Q_DECLARE_TR_FUNCTIONS(SmartPLCriteriaRow);

  public:

    SmartPLCriteriaRow(QString field, QString op,
                       QString value1, QString value2)
        : m_field(std::move(field)), m_operator(std::move(op)),
          m_value1(std::move(value1)), m_value2(std::move(value2)) {}
    SmartPLCriteriaRow(void) = default;
    ~SmartPLCriteriaRow(void) = default;

    QString getSQL(void) const;

    bool saveToDatabase(int smartPlaylistID) const;

    QString toString(void) const;

  public:
    QString m_field;
    QString m_operator;
    QString m_value1;
    QString m_value2;
};

Q_DECLARE_METATYPE(SmartPLCriteriaRow *);

class SmartPlaylistEditor : public MythScreenType
{
    Q_OBJECT
  public:

    explicit SmartPlaylistEditor(MythScreenStack *parent)
        : MythScreenType(parent, "smartplaylisteditor") {}
   ~SmartPlaylistEditor(void) override;

    bool Create(void) override; // MythScreenType

    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType
    void customEvent(QEvent *event) override; // MythUIType

    QString getSQL(const QString& fields);
    QString getWhereClause(void);
    QString getOrderByClause(void);
    void getCategoryAndName(QString &category, QString &name);
    void newSmartPlaylist(const QString& category);
    void editSmartPlaylist(const QString& category, const QString& name);
    static bool deleteSmartPlaylist(const QString &category, const QString& name);
    static bool deleteCategory(const QString& category);
    static int  lookupCategoryID(const QString& category);

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
    void orderByChanged(const QString& orderBy);

  private:
    void getSmartPlaylistCategories(void);
    void loadFromDatabase(const QString& category, const QString& name);

    QList<SmartPLCriteriaRow*> m_criteriaRows;
    SmartPLCriteriaRow* m_tempCriteriaRow     {nullptr};

    int     m_matchesCount                    {0};
    bool    m_newPlaylist                     {false};
    bool    m_playlistIsValid                 {false};
    QString m_originalCategory;
    QString m_originalName;

    // gui stuff
    MythUIButtonList *m_categorySelector      {nullptr};
    MythUIButton *m_categoryButton            {nullptr};
    MythUITextEdit *m_titleEdit               {nullptr};
    MythUIButtonList *m_matchSelector         {nullptr};
    MythUIButtonList *m_criteriaList          {nullptr};
    MythUIButtonList *m_orderBySelector       {nullptr};
    MythUIButton *m_orderByButton             {nullptr};
    MythUIText *m_matchesText                 {nullptr};
    MythUISpinBox *m_limitSpin                {nullptr};
    MythUIButton *m_cancelButton              {nullptr};
    MythUIButton *m_saveButton                {nullptr};
    MythUIButton *m_showResultsButton         {nullptr};
};

class CriteriaRowEditor : public MythScreenType
{
    Q_OBJECT
  public:

    CriteriaRowEditor(MythScreenStack *parent, SmartPLCriteriaRow *row)
        : MythScreenType(parent, "CriteriaRowEditor"),
          m_criteriaRow(row) {}
   ~CriteriaRowEditor(void) override = default;

    bool Create(void) override; // MythScreenType

  protected slots:
    void fieldChanged(void);
    void operatorChanged(void);
    void valueEditChanged(void);
    void valueButtonClicked(void);
    void setValue(const QString& value);
    void setDate(const QString& date);
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

    SmartPLCriteriaRow* m_criteriaRow    {nullptr};

    QStringList m_searchList;

    // gui stuff
    MythUIButtonList *m_fieldSelector    {nullptr};
    MythUIButtonList *m_operatorSelector {nullptr};

    MythUITextEdit *m_value1Edit         {nullptr};
    MythUITextEdit *m_value2Edit         {nullptr};

    MythUIButtonList *m_value1Selector   {nullptr};
    MythUIButtonList *m_value2Selector   {nullptr};

    MythUISpinBox *m_value1Spinbox       {nullptr};
    MythUISpinBox *m_value2Spinbox       {nullptr};

    MythUIButton *m_value1Button         {nullptr};
    MythUIButton *m_value2Button         {nullptr};

    MythUIText   *m_andText              {nullptr};

    MythUIButton *m_cancelButton         {nullptr};
    MythUIButton *m_saveButton           {nullptr};
};


class SmartPLResultViewer : public MythScreenType
{
  Q_OBJECT

  public:

    explicit SmartPLResultViewer(MythScreenStack *parent)
        : MythScreenType(parent, "SmartPLResultViewer") {}
   ~SmartPLResultViewer(void) override = default;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType
    void setSQL(const QString& sql);

  private slots:
    static void trackVisible(MythUIButtonListItem *item);
    void trackSelected(MythUIButtonListItem *item);

  private:
    void showTrackInfo(void);

    MythUIButtonList *m_trackList    {nullptr};
    MythUIText       *m_positionText {nullptr};
};


class SmartPLOrderByDialog: public MythScreenType
{
  Q_OBJECT

  public:

    explicit SmartPLOrderByDialog(MythScreenStack *parent)
        :MythScreenType(parent, "SmartPLOrderByDialog") {}
    ~SmartPLOrderByDialog() override = default;

    bool Create(void) override; // MythScreenType

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
    void orderByChanged(MythUIButtonListItem */*item*/);
    void fieldListSelectionChanged(MythUIButtonListItem *item);

  private:
    void getOrderByFields(void);

    MythUIButtonList *m_fieldList        {nullptr};
    MythUIButtonList *m_orderSelector    {nullptr};
    MythUIButton     *m_addButton        {nullptr};
    MythUIButton     *m_deleteButton     {nullptr};
    MythUIButton     *m_moveUpButton     {nullptr};
    MythUIButton     *m_moveDownButton   {nullptr};
    MythUIButton     *m_ascendingButton  {nullptr};
    MythUIButton     *m_descendingButton {nullptr};
    MythUIButton     *m_cancelButton     {nullptr};
    MythUIButton     *m_okButton         {nullptr};
};

class SmartPLDateDialog: public MythScreenType
{
  Q_OBJECT

  public:

    explicit SmartPLDateDialog(MythScreenStack *parent)
        :MythScreenType(parent, "SmartPLDateDialog") {}
    ~SmartPLDateDialog() override = default;

    bool Create(void) override; // MythScreenType

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

    bool              m_updating     {false};

    MythUICheckBox   *m_fixedRadio   {nullptr};
    MythUISpinBox    *m_daySpin      {nullptr};
    MythUISpinBox    *m_monthSpin    {nullptr};
    MythUISpinBox    *m_yearSpin     {nullptr};

    MythUICheckBox   *m_nowRadio     {nullptr};
    MythUISpinBox    *m_addDaysSpin  {nullptr};

    MythUIText       *m_statusText   {nullptr};

    MythUIButton     *m_cancelButton {nullptr};
    MythUIButton     *m_okButton     {nullptr};
};

#endif
