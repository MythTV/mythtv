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

#include <mythtv/uitypes.h>
#include <mythtv/uilistbtntype.h>
#include <mythtv/xmlparse.h>
#include <mythtv/mythdialogs.h>

class QTimer;
class QPixmap;

class MythNewsConfigPriv;
class NewsSiteItem;
class UIListBtnType;

class MythNewsSpinBox : public MythSpinBox
{
public:
    
    MythNewsSpinBox(QWidget* parent = 0, const char* widgetName = 0);

protected:

    virtual bool eventFilter(QObject* o, QEvent* e);
    
};

class MythNewsConfig : public MythDialog
{
    Q_OBJECT

public:

    MythNewsConfig(MythMainWindow *parent,
                   const char *name = 0);
    ~MythNewsConfig();

private:
    void changeContext();
    void paintEvent(QPaintEvent *e);
    void keyPressEvent(QKeyEvent *e);

    void populateSites();
    void loadTheme();


    void updateSites();
    void updateFreq();
    void updateBot();
    
    void cursorUp(bool page=false);
    void cursorDown(bool page=false);
    void cursorLeft();
    void cursorRight();
    
    void toggleItem(UIListBtnTypeItem* item);
    bool findInDB(const QString& name);
    bool insertInDB(NewsSiteItem* site);
    bool removeFromDB(NewsSiteItem* site);

    MythNewsConfigPriv *m_priv;

    XMLParse           *m_Theme;
    uint                m_Context;
    uint                m_InColumn;

    UIListBtnType      *m_UICategory;
    UIListBtnType      *m_UISite;

    MythNewsSpinBox    *m_SpinBox;


    QRect               m_SiteRect;
    QRect               m_FreqRect;
    QRect               m_BotRect;

    QTimer             *m_updateFreqTimer;
    int                 m_updateFreq;

private slots:

    void slotUpdateFreqChanged();
    void slotUpdateFreqTimerTimeout();
    void slotCategoryChanged(UIListBtnTypeItem* item);
};

#endif /* MYTHNEWSCONFIG_H */
