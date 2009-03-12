#ifndef SMARTPLAYLIST_H_
#define SMARTPLAYLIST_H_

#include <vector>
using namespace std;

#include <QDateTime>
#include <QVariant>
#include <QKeyEvent>
//Added by qt3to4:
#include <QLayout>
#include <QLabel>
#include <Q3HBox>
#include <Q3HBoxLayout>
#include <Q3VBoxLayout>

#include <mythtv/mythwidgets.h>
#include <mythtv/mythdialogs.h>

class Q3MythListView;
class Q3ListBoxItem;
class Q3MythListBox;
struct SmartPLOperator;
struct SmartPLField;

// why can't you forward declare enum's ??
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

class SmartPLCriteriaRow : public QObject
{
  Q_OBJECT

  public:
    
    SmartPLCriteriaRow(QWidget *parent, Q3HBoxLayout *hbox);
    ~SmartPLCriteriaRow(void);
    
    QString            getSQL(void);
    MythComboBox       *fieldCombo;
    MythComboBox       *operatorCombo;
    MythRemoteLineEdit *value1Edit;       
    MythRemoteLineEdit *value2Edit;       
    MythSpinBox        *value1SpinEdit;       
    MythSpinBox        *value2SpinEdit;
    MythPushButton     *value1Button;
    MythPushButton     *value2Button;
    MythComboBox       *value1Combo;
    MythComboBox       *value2Combo;
          
    bool saveToDatabase(int smartPlaylistID);
    void initValues(QString Field, QString Operator, QString Value1, QString Value2);
    
  signals:
    void criteriaChanged(void);
    
  protected slots:
    
    void fieldChanged(void);
    void operatorChanged(void);
    void valueChanged(void);
    void value1ButtonClicked(void);
    void value2ButtonClicked(void);
  
  private:
    bool showList(QString caption, QString &value);
    void searchGenre(MythRemoteLineEdit *editor);
    void searchArtist(MythRemoteLineEdit *editor);
    void searchCompilationArtist(MythRemoteLineEdit *editor);
    void searchAlbum(MythRemoteLineEdit *editor);
    void searchTitle(MythRemoteLineEdit *editor);
    void editDate(MythComboBox *combo);
    void getOperatorList(SmartPLFieldType fieldType);   
    
    QStringList searchList;
    bool bUpdating;             
};

class SmartPlaylistEditor : public MythDialog
{
    Q_OBJECT
  public:

    SmartPlaylistEditor(MythMainWindow *parent, const char *name = 0);
   ~SmartPlaylistEditor(void);
    
    QString getSQL(QString fields);
    QString getWhereClause(void);
    QString getOrderByClause(void);
    void    getCategoryAndName(QString &category, QString &name);
    void    newSmartPlaylist(QString category);
    void    editSmartPlaylist(QString category, QString name);
    static bool deleteSmartPlaylist(QString category, QString name);
    static bool deleteCategory(QString category);
    static int  lookupCategoryID(QString category);
  signals:
    
    void dismissWindow();

  protected slots:
    
    void titleChanged(void);
    void updateMatches(void);
    void categoryClicked(void);
    void saveClicked(void);
    void showResultsClicked(void);
    
    // category popup
    void newCategory(void);
    void deleteCategory(void);
    void renameCategory(void);
    void showCategoryPopup(void);
    void closeCategoryPopup(void);
    void categoryEditChanged(void);    
    void orderByClicked(void);
      
  private:
    void getSmartPlaylistCategories(void);  
    void loadFromDatabase(QString category, QString name);
  
//    QHBox *m_boxframe;
    MythComboBox       *categoryCombo;
    MythPushButton     *categoryButton;
    MythRemoteLineEdit *titleEdit;
    MythComboBox       *matchCombo;
    MythPushButton     *saveButton;
    MythPushButton     *cancelButton;
    MythPushButton     *showResultsButton;
    MythComboBox       *orderByCombo;
    MythSpinBox        *limitSpinEdit;
    QLabel             *matchesLabel;    
    MythPushButton     *orderByButton;

    // category popup
    MythPopupBox       *category_popup;
    MythRemoteLineEdit *categoryEdit; 
    QAbstractButton            *newCategoryButton;
    QAbstractButton            *renameCategoryButton;
    QAbstractButton            *deleteCategoryButton;
    
    vector<SmartPLCriteriaRow*> criteriaRows;
    int matchesCount;
    bool bNewPlaylist;
    bool bPlaylistIsValid;
    QString originalCategory, originalName;
};

class SmartPLResultViewer : public MythDialog
{
    Q_OBJECT
  public:

    SmartPLResultViewer(MythMainWindow *parent, const char *name = 0);
   ~SmartPLResultViewer(void);
    
    void setSQL(QString sql);
  signals:

  protected slots:
    void exitClicked(void);

  private:
      Q3MythListView *listView;
      MythPushButton *exitButton;  
};

class SmartPlaylistDialog: public MythPopupBox
{
  Q_OBJECT

  public:

    SmartPlaylistDialog(MythMainWindow *parent, const char *name = 0); 
    ~SmartPlaylistDialog();

    void getSmartPlaylist(QString &category, QString &name);
    void setSmartPlaylist(QString category, QString name);
    
 protected slots:
    void newPressed(void);
    void selectPressed(void);   
    void deletePressed(void);
    void editPressed(void);
    void categoryChanged(void);

 protected:
    void keyPressEvent(QKeyEvent *e);

  private:
    void getSmartPlaylistCategories(void);
    void getSmartPlaylists(QString category);
    
    Q3VBoxLayout    *vbox;
    QLabel          *caption;
    MythComboBox    *categoryCombo;
    Q3MythListBox   *listbox;  
    QAbstractButton *selectButton;
    QAbstractButton *editButton;
    QAbstractButton *deleteButton;
    QAbstractButton *newButton;
    
};

class SmartPLOrderByDialog: public MythPopupBox
{
  Q_OBJECT

  public:

    SmartPLOrderByDialog(MythMainWindow *parent, const char *name = 0); 
    ~SmartPLOrderByDialog();

    QString getFieldList(void);
    void setFieldList(QString fieldList);
        
 protected slots:
    void addPressed(void);
    void deletePressed(void);   
    void moveUpPressed(void);
    void moveDownPressed(void);
    void ascendingPressed(void);
    void descendingPressed(void);
    void orderByChanged(void);
    void listBoxSelectionChanged(Q3ListBoxItem *item);
    
 protected:
    void keyPressEvent(QKeyEvent *e);

 private:
    void getOrderByFields(void);
   
    Q3VBoxLayout        *vbox;
    QLabel              *caption;
    MythComboBox        *orderByCombo;
    Q3MythListBox       *listbox;  
    QAbstractButton     *addButton;
    QAbstractButton     *deleteButton;
    QAbstractButton     *moveUpButton;
    QAbstractButton     *moveDownButton;
    QAbstractButton     *ascendingButton;
    QAbstractButton     *descendingButton;
    QAbstractButton     *okButton;
};

class SmartPLDateDialog: public MythPopupBox
{
  Q_OBJECT

  public:

    SmartPLDateDialog(MythMainWindow *parent, const char *name = 0); 
    ~SmartPLDateDialog();

    QString getDate(void);
    void setDate(QString date);
        
 protected slots:
    void fixedCheckToggled(bool on);
    void nowCheckToggled(bool on);
    void addDaysCheckToggled(bool on);
    void valueChanged(void);
    
 protected:
    void keyPressEvent(QKeyEvent *e);

 private:
    QString             dateValue;
    
    Q3VBoxLayout         *vbox;
    QLabel              *caption;
    QLabel              *dayLabel;
    QLabel              *monthLabel; 
    QLabel              *yearLabel;
    MythRadioButton     *fixedRadio; 
    MythSpinBox         *daySpinEdit;
    MythSpinBox         *monthSpinEdit;
    MythSpinBox         *yearSpinEdit;  
    
    MythRadioButton     *nowRadio; 
    
    MythCheckBox        *addDaysCheck; 
    MythSpinBox         *addDaysSpinEdit;
    QLabel              *statusLabel;
    
    QAbstractButton     *cancelButton;
    QAbstractButton     *okButton;
};

#endif
