/* ============================================================
 * File  : mythflixconfig.h
 * Author: John Petrocik <john@petrocik.net>
 * Date  : 2005-10-28
 * Description :
 *
 * Copyright 2005 by John Petrocik

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

#ifndef MYTHFLIXCONFIG_H
#define MYTHFLIXCONFIG_H

// MythTV headers
#include <mythtv/libmythui/mythscreentype.h>
#include <mythtv/libmythui/mythuitext.h>
#include <mythtv/libmythui/mythlistbutton.h>

//Added by qt3to4:
#include <QKeyEvent>

class MythFlixConfigPriv;
class NewsSiteItem;

class MythFlixConfig : public MythScreenType
{
    Q_OBJECT

  public:

    MythFlixConfig(MythScreenStack *parent, const char *name = 0);
    ~MythFlixConfig();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

  private:
    void loadData();
    void populateSites();

    bool findInDB(const QString& name);
    bool insertInDB(NewsSiteItem* site);
    bool removeFromDB(NewsSiteItem* site);

    MythFlixConfigPriv *m_priv;

    MythListButton      *m_genresList;
    MythListButton      *m_siteList;

private slots:
    void slotCategoryChanged(MythListButtonItem* item);
    void toggleItem(MythListButtonItem* item);
};

#endif /* MYTHFLIXCONFIG_H */
