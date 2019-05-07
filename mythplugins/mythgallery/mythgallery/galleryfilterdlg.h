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

// MythTV headers
#include <mythscreentype.h>
#include <mythuitext.h>
#include <mythuibutton.h>
#include <mythuibuttonlist.h>
#include <mythuiimage.h>
#include <mythdialogbox.h>
#include <mythmedia.h>

#include "galleryfilter.h"

using namespace std;

class GalleryFilter;
class GalleryFilterDialog : public MythScreenType
{
  Q_OBJECT

  public:
    GalleryFilterDialog(MythScreenStack *parent, const QString& name,
                        GalleryFilter *filter);
    ~GalleryFilterDialog();

    bool Create() override; // MythScreenType

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

    bool               m_scanning         {false};
    QString            m_photoDir;
    GalleryFilter     *m_settingsOriginal {nullptr};
    GalleryFilter     *m_settingsTemp     {nullptr};

    MythUITextEdit    *m_dirFilter        {nullptr};
    MythUIButtonList  *m_typeFilter       {nullptr};
    MythUIText        *m_numImagesText    {nullptr};
    MythUIButtonList  *m_sortList         {nullptr};
    MythUIButton      *m_checkButton      {nullptr};
    MythUIButton      *m_saveButton       {nullptr};
    MythUIButton      *m_doneButton       {nullptr};
};

#endif /* GALLERYFILTERDLG_H */

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
