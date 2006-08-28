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

#include <qapplication.h>
#include <qstringlist.h>
#include <qpixmap.h>

#include <unistd.h>

#include <mythtv/mythcontext.h>
#include <mythtv/xmlparse.h>

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

    bgTransBackup.reset(gContext->LoadScalePixmap("trans-backup.png"));
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
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Video", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];


        if (action == "SELECT" && allowselect)
        {
            handled = true;
            startPlayItem();
            return;

        }
    }

    if (!handled)
    {
        gContext->GetMainWindow()->TranslateKeyPress("TV Frontend", e, actions);

        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            if (actions[i] == "PLAYBACK")
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

void VideoSelected::grayOut(QPainter *tmp)
{
    tmp->fillRect(QRect(QPoint(0, 0), size()),
                  QBrush(QColor(10, 10, 10), Dense4Pattern));
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
    const int kMythVideoStartPlayEventType = 301976;
}

void VideoSelected::customEvent(QCustomEvent *e)
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
        backup.flush();
        backup.begin(this);
        if (m_state == 1)
            grayOut(&backup);
        backup.end();

        LayerSet *container = theme->GetSet("playwait");
        if (container)
        {
            for (int i = 0; i < 4; ++i)
                container->Draw(p, i, 0);
        }
        m_state++;
        update(fullRect);
    }
    else if (m_state == 4)
    {
        // This is done so we don't lock the paint event (bad).
        ++m_state;
        QApplication::postEvent(this,
                                new QCustomEvent(kMythVideoStartPlayEventType));
    }
    else if (m_state == 5)
    {
        // playing state
    }
    else if (m_state == 6)
    {
        backup.begin(this);
        backup.drawPixmap(0, 0, myBackground);
        backup.end();
        noUpdate = false;

        gContext->GetMainWindow()->raise();
        gContext->GetMainWindow()->setActiveWindow();
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
           checkedSetText((UITextType *)container->GetType("title"),
                          m_item->Title());
           checkedSetText((UITextType *)container->GetType("filename"),
                          m_item->Filename());

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
                                                            QImage::ScaleFree);

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

           checkedSetText((UITextType *)container->GetType("video_player"),
                          Metadata::getPlayer(m_item));
           checkedSetText((UITextType *)container->GetType("director"),
                          m_item->Director());
           checkedSetText((UITextType *)container->GetType("plot"),
                          m_item->Plot());
           checkedSetText((UITextType *)container->GetType("rating"),
                          getDisplayRating(m_item->Rating()));
           checkedSetText((UITextType *)container->GetType("inetref"),
                          m_item->InetRef());
           checkedSetText((UITextType *)container->GetType("year"),
                          getDisplayYear(m_item->Year()));
           checkedSetText((UITextType *)container->GetType("userrating"),
                          getDisplayUserRating(m_item->UserRating()));
           checkedSetText((UITextType *)container->GetType("length"),
                          getDisplayLength(m_item->Length()));
           checkedSetText((UITextType *)container->GetType("coverfile"),
                          m_item->CoverFile());
           checkedSetText((UITextType *)container->GetType("child_id"),
                          QString::number(m_item->ChildID()));
           checkedSetText((UITextType *)container->GetType("browseable"),
                          getDisplayBrowse(m_item->Browse()));
           checkedSetText((UITextType *)container->GetType("category"),
                          m_item->Category());
           checkedSetText((UITextType *)container->GetType("level"),
                          QString::number(m_item->ShowLevel()));

           for (int i = 1; i < 9; ++i)
               container->Draw(&tmp, i, 0);
       }

       allowselect = true;
    }
    else
    {
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
        checkedSetText((UITextType *)container->GetType("title"),
                       m_item->Title());
    }
    m_state = 1;
    update(fullRect);
}
