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

#include <mythtv/uitypes.h>

class Metadata;
class VideoList;

class VideoSelected : public MythDialog
{
    Q_OBJECT

  public:
    VideoSelected(const VideoList *video_list,
                  MythMainWindow *lparent, const QString &lname,
                  int index);
    ~VideoSelected();

    void processEvents();

  protected slots:
    void exitWin();

  protected:
    void paintEvent(QPaintEvent *e);
    void keyPressEvent(QKeyEvent *e);
    void customEvent(QCustomEvent *e);

  private:
    QPixmap getPixmap(QString &level);

    void LoadWindow(QDomElement &);
    void parseContainer(QDomElement &);

    void updateBackground();
    void updateInfo(QPainter *);
    void updatePlayWait(QPainter *);

    void grayOut(QPainter *);

    void startPlayItem();

  private:
    bool noUpdate;
    std::auto_ptr<XMLParse> theme;
    QDomElement xmldata;

    std::auto_ptr<QPixmap> bgTransBackup;
    const Metadata *m_item;

    QPainter backup;
    QPixmap myBackground;

    int m_state;

    QRect infoRect;
    QRect fullRect;

    bool allowselect;

    const VideoList *m_video_list;
};

#endif
