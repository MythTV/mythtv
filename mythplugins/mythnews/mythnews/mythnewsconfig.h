/* ============================================================
 * File  : mythnewsconfig.h
 * Author: Renchi Raju <renchi@pooh.tam.uiuc.edu>
 * Date  : 2003-09-02
 * Description :
 *
 * Copyright 2003 by Renchi Raju

 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published bythe Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ============================================================ */

#ifndef MYTHNEWSCONFIG_H
#define MYTHNEWSCONFIG_H

#include <qsqldatabase.h>

#include <mythtv/uitypes.h>
#include <mythtv/xmlparse.h>
#include <mythtv/oldsettings.h>
#include <mythtv/mythwidgets.h>
#include <mythtv/mythdialogs.h>

class QTimer;
class MythNewsConfigPriv;
class NewsSiteItem;
class CMythListBox;

class CPopupBox : public QDialog
{
    Q_OBJECT

public:

    CPopupBox(QWidget *parent);
    ~CPopupBox();

signals:

    void finished(const QString& name,
                  const QString& url,
                  const QString& ico);

private slots:

    void slotOkClicked();

private:

    QLineEdit* m_nameEdit;
    QLineEdit* m_urlEdit;
    QLineEdit* m_icoEdit;
};

class MythNewsConfig : public MythDialog
{
    Q_OBJECT

public:

    MythNewsConfig(QSqlDatabase *db,
                   MythMainWindow *parent,
                   const char *name = 0);
    ~MythNewsConfig();

private:

    void populateSites();
    void setupView();

    bool findInDB(const QString& name);
    bool insertInDB(NewsSiteItem* site);
    bool removeFromDB(NewsSiteItem* site);

    QSqlDatabase       *m_db;
    MythNewsConfigPriv *m_priv;

    MythListView       *m_leftView;
    CMythListBox       *m_rightView;
    MythSpinBox        *m_updateFreqBox;
    QTimer             *m_updateFreqTimer;

private slots:

    void slotSitesViewExecuted(QListViewItem *item);
    void slotSelSitesViewExecuted(QListBoxItem *item);
    void slotAddCustomSite();
    void slotCustomSiteAdded(const QString& name,
                             const QString& url,
                             const QString& ico);
    void slotUpdateFreqChanged();
    void slotUpdateFreqTimerTimeout();
};

#endif /* MYTHNEWSCONFIG_H */
