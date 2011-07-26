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

#include "galleryfilter.h"

using namespace std;

// Qt headers
#include <QApplication>
#include <QEvent>
//#include <QDir>
//#include <QMatrix>
#include <QList>
//#include <QFileInfo>

// MythTV headers
#include <mythtv/util.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/libmythui/mythuibuttonlist.h>
//#include <mythtv/httpcomms.h>
#include <mythtv/mythcontext.h>
#include <mythtv/libmythdb/mythverbose.h>
//#include <mythtv/libmythui/mythmainwindow.h>
//#include <mythtv/libmythui/mythprogressdialog.h>
//#include <mythtv/mythmediamonitor.h>

// MythGallery headers
//#include "galleryutil.h"
//#include "gallerysettings.h"
#include "galleryfilterdlg.h"
#include "galleryfilter.h"
//#include "thumbgenerator.h"
//#include "iconview.h"
//#include "singleview.h"
//#include "glsingleview.h"

#define LOC QString("GalleryFilterDlg:")
#define LOC_ERR QString("GalleryFilterDlg, Error:")

class FilterScanThread : public QThread
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
                                   int *dirCount, int *imageCount, int *movieCount)
{
    m_dir = dir;
    m_filter = flt;
    m_dirCount = dirCount;
    m_imgCount = imageCount;
    m_movCount = movieCount;
}

void FilterScanThread::run()
{
    GalleryFilter::TestFilter(m_dir, m_filter, m_dirCount, m_imgCount, m_movCount);
}

GalleryFilterDialog::GalleryFilterDialog(MythScreenStack *parent, QString name, GalleryFilter *filter)
            : MythScreenType(parent, name)
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
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'filter'");
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
    // Directory filter
    m_dirFilter->SetText(m_settingsTemp->getDirFilter(), false);

    // Type Filter
    new MythUIButtonListItem(m_typeFilter, QObject::tr("All"),
                             kTypeFilterAll);
    new MythUIButtonListItem(m_typeFilter, QObject::tr("Images only"),
                             kTypeFilterImagesOnly);
    new MythUIButtonListItem(m_typeFilter, QObject::tr("Movies only"),
                             kTypeFilterMoviesOnly);
    m_typeFilter->SetValueByData(m_settingsTemp->getTypeFilter());
    m_numImagesText->SetText("Filter result : (unknown)");

    // Sort order
    new MythUIButtonListItem(m_sortList, QObject::tr("Unsorted"),
                             kSortOrderUnsorted);
    new MythUIButtonListItem(m_sortList, QObject::tr("Name (A-Z alpha)"),
                             kSortOrderNameAsc);
    new MythUIButtonListItem(m_sortList, QObject::tr("Reverse Name (Z-A alpha)"),
                             kSortOrderNameDesc);
    new MythUIButtonListItem(m_sortList, QObject::tr("Mod Time (oldest first)"),
                             kSortOrderModTimeAsc);
    new MythUIButtonListItem(m_sortList, QObject::tr("Reverse Mod Time (most recent first)"),
                             kSortOrderModTimeDesc);
    new MythUIButtonListItem(m_sortList, QObject::tr("Extension (A-Z alpha)"),
                             kSortOrderExtAsc);
    new MythUIButtonListItem(m_sortList, QObject::tr("Reverse Extension (Z-A alpha)"),
                             kSortOrderExtDesc);
    new MythUIButtonListItem(m_sortList, QObject::tr("Filesize (smallest first)"),
                             kSortOrderSizeAsc);
    new MythUIButtonListItem(m_sortList, QObject::tr("Reverse Filesize (largest first)"),
                             kSortOrderSizeDesc);
    m_sortList->SetValueByData(m_settingsTemp->getSort());
}

void GalleryFilterDialog::updateFilter()
{
    if (m_scanning)
    {
        m_numImagesText->SetText("-- please be patient --");
        return;
    }
    else
    {
        m_scanning = true;
    }

    int dir_count = 0;
    int img_count = 0;
    int mov_count = 0;

    m_numImagesText->SetText("-- scanning current filter --");

    FilterScanThread *fltScan = new FilterScanThread(m_photoDir, *m_settingsTemp, &dir_count, &img_count, &mov_count);
    fltScan->start();

    while (!fltScan->isFinished())
    {
        usleep(500);
        qApp->processEvents();
    }

    if (dir_count + img_count + mov_count == 0)
    {
        m_numImagesText->SetText(QString(tr("No files / directories found")));
    }
    else
    {
        if (dir_count > 0)
        {
            if (img_count + mov_count == 0)
                m_numImagesText->SetText(QString(tr("Filter result : %1 directories found but no files"))
                                         .arg(dir_count));
            else {
                if (img_count == 0)
                    m_numImagesText->SetText(QString(tr("Filter result : %1 directories, %2 movie(s) found"))
                                             .arg(dir_count)
                                             .arg(mov_count));
                else if (mov_count == 0)
                    m_numImagesText->SetText(QString(tr("Filter result : %1 directories, %2 image(s) found"))
                                             .arg(dir_count)
                                             .arg(img_count));
                else
                    m_numImagesText->SetText(QString(tr("Filter result : %1 directories, %2 image(s) and %3 movie(s) found"))
                                             .arg(dir_count)
                                             .arg(img_count)
                                             .arg(mov_count));
            }

        }
        else
        {
            if (img_count > 0 && mov_count > 0)
                m_numImagesText->SetText(QString(tr("Filter result : %1 image(s) and %2 movie(s) found"))
                                         .arg(img_count)
                                         .arg(mov_count));
            else if (mov_count == 0)
                m_numImagesText->SetText(QString(tr("Filter result : %1 image(s) found"))
                                         .arg(img_count));
            else
                m_numImagesText->SetText(QString(tr("Filter result : %1 movie(s) found"))
                                         .arg(mov_count));
        }
    }

    m_scanning = false;
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
    {
        emit filterChanged();
    }

    Close();
}
