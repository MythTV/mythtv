/* ============================================================
 * File  : singleview.h
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

#ifndef SINGLEVIEW_H
#define SINGLEVIEW_H

#include "iconview.h"

class ThumbGenerator;

class SingleView : public MythDialog
{
public:

    SingleView(QSqlDatabase *db, const QPtrList<ThumbItem>& images,
               int pos, ThumbGenerator *thumbGen, 
               MythMainWindow *parent, const char *name=0);
    ~SingleView();

    void startShow();
    void stopShow();

protected:

    void paintEvent(QPaintEvent *e);
    void keyPressEvent(QKeyEvent *e);
    
private:

    void loadNext();
    void loadPrev();
    void loadImage();
    void rotate(int angle);
    void zoom();
    void createInfoBg();

    QSqlDatabase          *m_db;
    ThumbGenerator        *m_thumbGen;
    QPtrList<ThumbItem>    m_imageList;

    QPixmap               *m_pixmap;
    QImage                 m_image;
    int                    m_pos;
                          
    int                    m_rotateAngle;
    float                  m_zoom;
    int                    m_sx;
    int                    m_sy;
                          
    bool                   m_info;
    QPixmap               *m_infoBgPix;
};

#endif /* SINGLEVIEW_H */
