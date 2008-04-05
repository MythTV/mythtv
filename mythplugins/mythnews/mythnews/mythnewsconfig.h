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

// MythTV headers
#include <mythtv/libmythui/mythscreentype.h>
#include <mythtv/libmythui/mythuitext.h>
#include <mythtv/libmythui/mythlistbutton.h>

// MythNews headers
#include "newsengine.h"

class MythNewsConfigPriv;

class MythNewsConfig : public MythScreenType
{
    Q_OBJECT

public:

    MythNewsConfig(MythScreenStack *parent, const char *name = 0);
    ~MythNewsConfig();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

private:
    void loadData();
    void populateSites();

    MythNewsConfigPriv *m_priv;

    MythListButton      *m_categoriesList;
    MythListButton      *m_siteList;

    MythUIText *m_helpText;
    MythUIText *m_contextText;

    //MythNewsSpinBox    *m_SpinBox;

    int                 m_updateFreq;

private slots:
    void slotCategoryChanged(MythListButtonItem* item);
    void toggleItem(MythListButtonItem* item);
};

#endif /* MYTHNEWSCONFIG_H */
