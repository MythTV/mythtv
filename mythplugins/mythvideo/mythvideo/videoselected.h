/*
Copyright (c) 2004 Koninklijke Philips Electronics NV. All rights reserved.
Based on "videobrowser.h" of MythVideo.

This is free software; you can redistribute it and/or modify it under the
terms of version 2 of the GNU General Public License as published by the
Free Software Foundation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#ifndef VIDEOSELECTED_H_
#define VIDEOSELECTED_H_

#include <qwidget.h>
#include <qdialog.h>
#include <qapplication.h>
#include <qstringlist.h>

#include "metadata.h"
#include "videolist.h"
#include <mythtv/mythwidgets.h>
#include <qdom.h>
#include <mythtv/uitypes.h>
#include <mythtv/xmlparse.h>

typedef QValueList<Metadata>  ValueMetadata;

class VideoSelected : public MythDialog
{
    Q_OBJECT
  public:
    VideoSelected(VideoList *lvideolist,
                 MythMainWindow *parent, const char *name = 0, int index = 0);
    ~VideoSelected();
    void processEvents() { qApp->processEvents(); }
    

  protected slots:
    void selected(Metadata *someItem);
    void exitWin();

  protected:
    void paintEvent(QPaintEvent *);
    void keyPressEvent(QKeyEvent *e);

  private:
    bool noUpdate;

    QPixmap getPixmap(QString &level);

    void LoadWindow(QDomElement &);
    void parseContainer(QDomElement &);
    XMLParse *theme;
    QDomElement xmldata;

    void updateBackground(void);
    void updateInfo(QPainter *);
    void updatePlayWait(QPainter *);

    void grayOut(QPainter *);

    QPixmap *bgTransBackup;
    Metadata *curitem;

    QPainter backup;
    QPixmap myBackground;
 
    int m_state;
    QString m_title;
    QString m_cmd;

    QRect infoRect;
    QRect fullRect;

    bool allowselect;

    VideoList *video_list;
};

#endif
