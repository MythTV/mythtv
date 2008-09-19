/*
Copyright (c) 2004 Koninklijke Philips Electronics NV. All rights reserved.
Based on "videobrowser.cpp" of MythVideo.

This is free software; you can redistribute it and/or modify it under the
terms of version 2 of the GNU General Public License as published by the
Free Software Foundation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include <unistd.h>
#include <cstdlib>

#include <QApplication>
#include <QStringList>
#include <QPixmap>
#include <QPainter>
#include <QPaintEvent>
#include <QKeyEvent>

#include <mythtv/mythcontext.h>
#include <mythtv/xmlparse.h>
#include <mythtv/libmythui/mythuihelper.h>

#include "videoselected.h"

#include "metadata.h"
#include "videolist.h"
#include "videoutils.h"
#include "imagecache.h"

VideoSelected::VideoSelected(const VideoList *video_list,
                             MythMainWindow *lparent, const QString &lname,
                             int index) :
    MythDialog(lparent, lname), noUpdate(false), m_state(0),
    allowselect(false), m_video_list(video_list)
{
    m_item = m_video_list->getVideoListMetadata(index);

    fullRect = QRect(0, 0, size().width(), size().height());

    theme.reset(new XMLParse());
    theme->SetWMult(wmult);
    theme->SetHMult(hmult);
    theme->LoadTheme(xmldata, "selected", "video-");
    LoadWindow(xmldata);

    bgTransBackup.reset(GetMythUI()->LoadScalePixmap("trans-backup.png"));
    if (!bgTransBackup.get())
        bgTransBackup.reset(new QPixmap());

    updateBackground();

    setNoErase();
}

VideoSelected::~VideoSelected()
{
}

void VideoSelected::processEvents()
{
    qApp->processEvents();
}

void VideoSelected::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList lactions;
    gContext->GetMainWindow()->TranslateKeyPress("Video", e, lactions);

    for (QStringList::const_iterator p = lactions.begin();
         p != lactions.end() && !handled; ++p)
    {
        if (*p == "SELECT" && allowselect)
        {
            handled = true;
            startPlayItem();
            return;

        }
    }

    if (!handled)
    {
        gContext->GetMainWindow()->TranslateKeyPress("TV Frontend", e,
                                                     lactions);

        for (QStringList::const_iterator p = lactions.begin();
             p != lactions.end()&& !handled; ++p)
        {
            if (*p == "PLAYBACK")
            {
                handled = true;
                startPlayItem();
            }
        }
    }

    if (!handled)
        MythDialog::keyPressEvent(e);
}

void VideoSelected::updateBackground(void)
{
    QPixmap bground(size());
    bground.fill(this, 0, 0);

    QPainter tmp(&bground);

    LayerSet *container = theme->GetSet("background");
    if (container)
        container->Draw(&tmp, 0, 0);

    tmp.end();
    myBackground = bground;

    setPaletteBackgroundPixmap(myBackground);
}

void VideoSelected::paintEvent(QPaintEvent *e)
{
    QRect r = e->rect();
    QPainter p(this);

    if (m_state == 0)
    {
       if (r.intersects(infoRect) && noUpdate == false)
       {
           updateInfo(&p);
       }
    }
    else if (m_state > 0)
    {
        noUpdate = true;
        updatePlayWait(&p);
    }
}

namespace
{
    const QEvent::Type kMythVideoStartPlayEventType =
            static_cast<QEvent::Type>(QEvent::User + 1976);
}

void VideoSelected::customEvent(QEvent *e)
{
    if (e->type() == kMythVideoStartPlayEventType)
    {
        if (m_item)
            PlayVideo(m_item->Filename(), m_video_list->getListCache());
        ++m_state;
        update(fullRect);
    }
}

void VideoSelected::updatePlayWait(QPainter *p)
{
    if (m_state < 4)
    {

        LayerSet *container = theme->GetSet("playwait");
        if (container)
        {
            QRect area = container->GetAreaRect();
            if (area.isValid())
            {
                QPixmap pix(area.size());
                pix.fill(this, area.topLeft());
                QPainter tmp(&pix);

                for (int i = 0; i < 4; ++i)
                    container->Draw(&tmp, i, 0);

                tmp.end();

                p->drawPixmap(area.topLeft(), pix);
            }
            else
            {
                VERBOSE(VB_IMPORTANT,
                        QObject::tr("Theme Error: selected/playwait "
                                    "has an invalid area."));
            }
        }

        m_state++;
        update(fullRect);
    }
    else if (m_state == 4)
    {
        update(fullRect);
        // This is done so we don't lock the paint event (bad).
        ++m_state;
        QApplication::postEvent(this,
                                new QEvent(kMythVideoStartPlayEventType));
    }
    else if (m_state == 5)
    {
        // playing state
    }
    else if (m_state == 6)
    {
        noUpdate = false;

        gContext->GetMainWindow()->raise();
        gContext->GetMainWindow()->setActiveWindow();
        if (gContext->GetMainWindow()->currentWidget())
            gContext->GetMainWindow()->currentWidget()->setFocus();

        m_state = 0;
        update(fullRect);
    }
}

void VideoSelected::updateInfo(QPainter *p)
{
    QRect pr = infoRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    if (m_item)
    {
       LayerSet *container = theme->GetSet("info");
       if (container)
       {
           checkedSetText(container, "title", m_item->Title());
           checkedSetText(container, "filename", m_item->Filename());

           QString coverfile = m_item->CoverFile();
           UIImageType *itype = (UIImageType *)container->GetType("coverart");
           if (itype)
           {
               if (isDefaultCoverFile(coverfile))
               {
                   if (itype->isShown())
                       itype->hide();
               }
               else
               {
                   QSize img_size = itype->GetSize(true);
                   const QPixmap *img =
                           ImageCache::getImageCache().load(coverfile,
                                                            img_size.width(),
                                                            img_size.height(),
                                                            Qt::IgnoreAspectRatio);

                   if (img)
                   {
                       if (itype->GetImage().serialNumber() !=
                           img->serialNumber())
                       {
                           itype->SetImage(*img);
                       }
                       if (itype->isHidden())
                           itype->show();
                   }
                   else
                   {
                       if (itype->isShown())
                           itype->hide();
                   }
               }
           }

           checkedSetText(container, "video_player",
                          Metadata::getPlayer(m_item));
           checkedSetText(container, "director", m_item->Director());
           checkedSetText(container, "plot", m_item->Plot());
           checkedSetText(container, "cast", GetCast(*m_item));
           checkedSetText(container, "rating",
                          getDisplayRating(m_item->Rating()));
           checkedSetText(container, "inetref", m_item->InetRef());
           checkedSetText(container, "year", getDisplayYear(m_item->Year()));
           checkedSetText(container, "userrating",
                          getDisplayUserRating(m_item->UserRating()));
           checkedSetText(container, "length",
                          getDisplayLength(m_item->Length()));
           checkedSetText(container, "coverfile", m_item->CoverFile());
           checkedSetText(container, "child_id",
                          QString::number(m_item->ChildID()));
           checkedSetText(container, "browseable",
                          getDisplayBrowse(m_item->Browse()));
           checkedSetText(container, "category", m_item->Category());
           checkedSetText(container, "level",
                          QString::number(m_item->ShowLevel()));

           for (int i = 1; i < 9; ++i)
               container->Draw(&tmp, i, 0);
       }

       allowselect = true;
    }
    else
    {
       // TODO: This seems to be nonsense.
       LayerSet *norec = theme->GetSet("novideos_info");
       if (norec)
       {
           for (int i = 4; i < 9; ++i)
               norec->Draw(&tmp, i, 0);
       }

       allowselect = false;
    }
    tmp.end();
    p->drawPixmap(pr.topLeft(), pix);
}

void VideoSelected::LoadWindow(QDomElement &element)
{

    for (QDomNode lchild = element.firstChild(); !lchild.isNull();
         lchild = lchild.nextSibling())
    {
        QDomElement e = lchild.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "font")
            {
                theme->parseFont(e);
            }
            else if (e.tagName() == "container")
            {
                parseContainer(e);
            }
            else
            {
                VERBOSE(VB_IMPORTANT,
                        QString("Unknown element: ").arg(e.tagName()));
                exit(0);
            }
        }
    }
}

void VideoSelected::parseContainer(QDomElement &element)
{
    QRect area;
    QString container_name;
    int context;
    theme->parseContainer(element, container_name, context, area);

    if (container_name.lower() == "info")
        infoRect = area;
}

void VideoSelected::exitWin()
{
    emit accept();
}

void VideoSelected::startPlayItem()
{
    LayerSet *container = theme->GetSet("playwait");
    if (container)
    {
        checkedSetText(container, "title", m_item->Title());
    }
    m_state = 1;
    update(fullRect);
}
