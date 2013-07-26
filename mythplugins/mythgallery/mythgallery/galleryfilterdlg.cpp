/* ============================================================
 * File  : galleryfilterdlg.cpp
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

// Qt headers
#include <QApplication>
#include <QEvent>
#include <QList>

// MythTV headers
#include <mythdate.h>
#include <mythdbcon.h>
#include <mythuibuttonlist.h>
#include <mythcontext.h>
#include <mythlogging.h>
#include <mthread.h>

// MythGallery headers
#include "galleryfilterdlg.h"
#include "galleryfilter.h"

#define LOC QString("GalleryFilterDlg:")

class FilterScanThread : public MThread
{
  public:
    FilterScanThread(const QString& dir, const GalleryFilter& flt,
                     int *dirCount, int *imageCount, int *movieCount);
    virtual void run();

  private:
    GalleryFilter m_filter;
    QString m_dir;
    int *m_dirCount;
    int *m_imgCount;
    int *m_movCount;
};

FilterScanThread::FilterScanThread(const QString& dir, const GalleryFilter& flt,
                                   int *dirCount, int *imageCount,
                                   int *movieCount) :
    MThread("FilterScan"), m_filter(flt), m_dir(dir), m_dirCount(dirCount),
    m_imgCount(imageCount), m_movCount(movieCount)
{
}

void FilterScanThread::run()
{
    RunProlog();

    GalleryFilter::TestFilter(m_dir, m_filter, m_dirCount, m_imgCount,
                              m_movCount);

    RunEpilog();
}

GalleryFilterDialog::GalleryFilterDialog(MythScreenStack *parent, QString name,
                                         GalleryFilter *filter)
            : MythScreenType(parent, name),
            m_dirFilter(NULL), m_typeFilter(NULL), m_numImagesText(NULL),
            m_sortList(NULL), m_checkButton(NULL), m_saveButton(NULL),
            m_doneButton(NULL)
{
    m_settingsOriginal = filter;
    m_settingsOriginal->dumpFilter("GalleryFilterDialog:ctor (original)");
    m_settingsTemp = new GalleryFilter();
    *m_settingsTemp = *filter;
    m_settingsTemp->dumpFilter("GalleryFilterDialog:ctor (temporary)");
    m_photoDir = gCoreContext->GetSetting("GalleryDir", "");
    m_scanning = false;
}

GalleryFilterDialog::~GalleryFilterDialog()
{
    delete m_settingsTemp;
}

bool GalleryFilterDialog::Create()
{
    if (!LoadWindowFromXML("gallery-ui.xml", "filter", this))
        return false;

    bool err = false;

    UIUtilE::Assign(this, m_dirFilter, "dirfilter_text", &err);
    UIUtilE::Assign(this, m_typeFilter, "typefilter_select", &err);
    UIUtilE::Assign(this, m_sortList, "sort_select", &err);
    UIUtilE::Assign(this, m_checkButton, "check_button", &err);
    UIUtilE::Assign(this, m_doneButton, "done_button", &err);
    UIUtilE::Assign(this, m_saveButton, "save_button", &err);
    UIUtilE::Assign(this, m_numImagesText, "numimages_text", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'filter'");
        return false;
    }

    BuildFocusList();

    fillWidgets();

    connect(m_dirFilter, SIGNAL(valueChanged()), SLOT(setDirFilter()));
    connect(m_typeFilter, SIGNAL(itemSelected(MythUIButtonListItem*)),
                          SLOT(setTypeFilter(MythUIButtonListItem*)));
    connect(m_sortList, SIGNAL(itemSelected(MythUIButtonListItem*)),
                        SLOT(setSort(MythUIButtonListItem*)));
    connect(m_checkButton, SIGNAL(Clicked()), SLOT(updateFilter()));
    connect(m_saveButton, SIGNAL(Clicked()), SLOT(saveAsDefault()));
    connect(m_doneButton, SIGNAL(Clicked()), SLOT(saveAndExit()));

    SetFocusWidget(m_dirFilter);

    return true;
}

void GalleryFilterDialog::fillWidgets()
{
    // Folder filter
    m_dirFilter->SetText(m_settingsTemp->getDirFilter(), false);

    // Type Filter
    new MythUIButtonListItem(m_typeFilter, tr("All"),
                             kTypeFilterAll);
    new MythUIButtonListItem(m_typeFilter, tr("Images only"),
                             kTypeFilterImagesOnly);
    new MythUIButtonListItem(m_typeFilter, tr("Movies only"),
                             kTypeFilterMoviesOnly);
    m_typeFilter->SetValueByData(m_settingsTemp->getTypeFilter());
    m_numImagesText->SetText(tr("Filter result : (unknown)"));

    // Sort order
    new MythUIButtonListItem(m_sortList, tr("Unsorted"),
                             kSortOrderUnsorted);
    new MythUIButtonListItem(m_sortList, tr("Name (A-Z alpha)"),
                             kSortOrderNameAsc);
    new MythUIButtonListItem(m_sortList,
                             tr("Reverse Name (Z-A alpha)"),
                             kSortOrderNameDesc);
    new MythUIButtonListItem(m_sortList, tr("Mod Time (oldest first)"),
                             kSortOrderModTimeAsc);
    new MythUIButtonListItem(m_sortList,
                             tr("Reverse Mod Time (newest first)"),
                             kSortOrderModTimeDesc);
    new MythUIButtonListItem(m_sortList, tr("Extension (A-Z alpha)"),
                             kSortOrderExtAsc);
    new MythUIButtonListItem(m_sortList,
                             tr("Reverse Extension (Z-A alpha)"),
                             kSortOrderExtDesc);
    new MythUIButtonListItem(m_sortList,
                             tr("Filesize (smallest first)"),
                             kSortOrderSizeAsc);
    new MythUIButtonListItem(m_sortList,
                             tr("Reverse Filesize (largest first)"),
                             kSortOrderSizeDesc);
    m_sortList->SetValueByData(m_settingsTemp->getSort());
}

void GalleryFilterDialog::updateFilter()
{
    if (m_scanning)
    {
        m_numImagesText->SetText(tr("-- please be patient --"));
        return;
    }
    else
    {
        m_scanning = true;
    }

    int dir_count = 0;
    int img_count = 0;
    int mov_count = 0;

    m_numImagesText->SetText(tr("-- scanning current filter --"));

    FilterScanThread fltScan(m_photoDir, *m_settingsTemp, &dir_count,
                             &img_count, &mov_count);
    fltScan.start();

    while (!fltScan.isFinished())
    {
        usleep(500);
        qApp->processEvents();
    }

    m_scanning = false;

    if (dir_count + img_count + mov_count == 0)
        m_numImagesText->SetText(tr("No files / folders found"));
    else if (dir_count > 0)
    {
        if (img_count + mov_count == 0)
            m_numImagesText->SetText(tr(
                "Filter result : %1 folder(s) found but no files")
                     .arg(dir_count));
        else if (img_count == 0)
            m_numImagesText->SetText(tr(
                "Filter result : %1 folder(s), %2 movie(s) found")
                     .arg(dir_count).arg(mov_count));
        else if (mov_count == 0)
            m_numImagesText->SetText(tr(
                "Filter result : %1 folder(s), %2 image(s) found")
                     .arg(dir_count).arg(img_count));
        else
            m_numImagesText->SetText(tr(
                "Filter result : %1 folder(s), %2 image(s) and %3 movie(s) "
                "found").arg(dir_count).arg(img_count).arg(mov_count));
    }
    else if (img_count > 0 && mov_count > 0)
        m_numImagesText->SetText(tr(
            "Filter result : %1 image(s) and %2 movie(s) found")
                 .arg(img_count).arg(mov_count));
    else if (mov_count == 0)
        m_numImagesText->SetText(tr(
            "Filter result : %1 image(s) found").arg(img_count));
    else
        m_numImagesText->SetText(tr(
            "Filter result : %1 movie(s) found").arg(mov_count));
}

void GalleryFilterDialog::setDirFilter(void)
{
    m_settingsTemp->setDirFilter(m_dirFilter->GetText());
}

void GalleryFilterDialog::setTypeFilter(MythUIButtonListItem *item)
{
    m_settingsTemp->setTypeFilter(item->GetData().toInt());
}

void GalleryFilterDialog::setSort(MythUIButtonListItem *item)
{
    m_settingsTemp->setSort(item->GetData().toInt());
}

void GalleryFilterDialog::saveAsDefault()
{
    // Save defaults from temp settings
    m_settingsTemp->saveAsDefault();
    saveAndExit();
}

void GalleryFilterDialog::saveAndExit()
{
    *m_settingsOriginal = *m_settingsTemp;

    m_settingsOriginal->dumpFilter("GalleryFilterDialog::saveAndExit()");

    if (m_settingsOriginal->getChangedState() > 0)
        emit filterChanged();

    Close();
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
