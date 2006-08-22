#ifndef SMARTPLAYLIST_H_
#define SMARTPLAYLIST_H_

#include <qdatetime.h>
#include <qlayout.h>
#include <qhbox.h>
#include <qvariant.h>

#include <mythtv/mythwidgets.h>
#include <mythtv/mythdialogs.h>


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
    
    SmartPLCriteriaRow(QWidget *parent, QHBoxLayout *hbox);
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
    void cancelClicked(void);
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
    QButton            *newCategoryButton;
    QButton            *renameCategoryButton;
    QButton            *deleteCategoryButton;
    
    QPtrList<SmartPLCriteriaRow> criteriaRows;
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
      MythListView   *listView;
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
    
    QVBoxLayout         *vbox;
    QLabel              *caption;
    MythComboBox        *categoryCombo;
    MythListBox         *listbox;  
    QButton             *selectButton;
    QButton             *editButton;
    QButton             *deleteButton;
    QButton             *newButton;
    
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
    void listBoxSelectionChanged(QListBoxItem *item);
    void okPressed(void);
    
 protected:
    void keyPressEvent(QKeyEvent *e);

 private:
    void getOrderByFields(void);
   
    QVBoxLayout         *vbox;
    QLabel              *caption;
    MythComboBox        *orderByCombo;
    MythListBox         *listbox;  
    QButton             *addButton;
    QButton             *deleteButton;
    QButton             *moveUpButton;
    QButton             *moveDownButton;
    QButton             *ascendingButton;
    QButton             *descendingButton;
    QButton             *okButton;
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
    
    void okPressed(void);
    void cancelPressed(void);
        
 protected:
    void keyPressEvent(QKeyEvent *e);

 private:
    QString             dateValue;
    
    QVBoxLayout         *vbox;
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
    
    QButton             *cancelButton;
    QButton             *okButton;
};

#endif
