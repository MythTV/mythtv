#ifndef CHANNELEDITOR_H
#define CHANNELEDITOR_H

#include <qsqldatabase.h>

#include "settings.h"


class ChannelListSetting;
class ChannelEditor: public VerticalConfigurationGroup,
                     public ConfigurationDialog {
    Q_OBJECT
public:
    ChannelEditor();
    virtual int exec(QSqlDatabase* db);

public slots:
    void menu(int);
    void del();
    void edit();
    void edit(int);
    void scan();
    void advanced();

private:
    ChannelListSetting* list;
    QSqlDatabase* db;
    int id;

    TransButtonSetting *buttonScan;
    TransButtonSetting *buttonAdvanced;
};

class ChannelID;

class ChannelWizard: public ConfigurationWizard {
    Q_OBJECT
public:
    ChannelWizard(int id, QSqlDatabase* _db);
    QString getCardtype();
    bool cardTypesInclude(const QString& cardtype); 
    int countCardtypes();

private:
    ChannelID *cid;
    QSqlDatabase* db;
};

class ChannelListSetting: public ListBoxSetting {
    Q_OBJECT
public:
    ChannelListSetting(): ListBoxSetting() {
        currentSourceID = "";
        currentSortMode = QObject::tr("Channel Name");
        currentHideMode = false;
    };

    void save(QSqlDatabase* _db) { (void)_db; };
    void load(QSqlDatabase* _db) {
        db = _db;
        fillSelections();
    };

    QString getSourceID() { return currentSourceID; };
    QString getSortMode() { return currentSortMode; };
    bool getHideMode() { return currentHideMode; };

public slots:
    void fillSelections(void);
    void setSortMode(const QString& sort) {
        if (currentSortMode != sort) {
            currentSortMode = sort;
            fillSelections();
        }
    };

    void setSourceID(const QString& sourceID) {
        if (currentSourceID != sourceID) {
            currentSourceID = sourceID;
            fillSelections();
        }
    };

    void setHideMode(bool hide) {
        if (currentHideMode != hide) {
            currentHideMode = hide;
            fillSelections();
        }
    };

private:
    QSqlDatabase* db;
    QString currentSourceID;
    QString currentSortMode;
    bool currentHideMode;
};

#endif
