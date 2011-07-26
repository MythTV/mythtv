/* ============================================================
 * File  : galleryfilterdlg.h
 * Description :
 *

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

#ifndef GALLERYFILTERDLG_H
#define GALLERYFILTERDLG_H

// Qt headers
#include <QThread>

// MythTV headers
#include <mythtv/libmythui/mythscreentype.h>
#include <mythtv/libmythui/mythuitext.h>
#include <mythtv/libmythui/mythuibutton.h>
#include <mythtv/libmythui/mythuibuttonlist.h>
#include <mythtv/libmythui/mythuiimage.h>
#include <mythtv/libmythui/mythdialogbox.h>
#include <mythtv/mythmedia.h>

#include "galleryfilter.h"

using namespace std;

class GalleryFilter;
class GalleryFilterDialog : public MythScreenType
{

  Q_OBJECT

  public:
    GalleryFilterDialog(MythScreenStack *parent, QString name, GalleryFilter *filter);
    ~GalleryFilterDialog();

    bool Create();

  signals:
    void filterChanged();

  public slots:
    void saveAndExit();
    void saveAsDefault();
    void updateFilter();
    void setDirFilter(void);
    void setTypeFilter(MythUIButtonListItem *item);
    void setSort(MythUIButtonListItem *item);

 private:
    void fillWidgets();

    bool               m_scanning;
    QString            m_photoDir;
    GalleryFilter     *m_settingsOriginal;
    GalleryFilter     *m_settingsTemp;

    MythUITextEdit    *m_dirFilter;
    MythUIButtonList  *m_typeFilter;
    MythUIText        *m_numImagesText;
    MythUIButtonList  *m_sortList;
    MythUIButton      *m_checkButton;
    MythUIButton      *m_saveButton;
    MythUIButton      *m_doneButton;
};

#endif /* GALLERYFILTERDLG_H */
