//
//  mythuinotificationcenter_private.h
//  MythTV
//
//  Created by Jean-Yves Avenard on 30/06/13.
//  Copyright (c) 2013 Bubblestuff Pty Ltd. All rights reserved.
//

#ifndef MythTV_mythuinotificationcenter_private_h
#define MythTV_mythuinotificationcenter_private_h

#include "mythscreenstack.h"
#include "mythscreentype.h"
#include "mythuiimage.h"
#include "mythuitext.h"
#include "mythuiprogressbar.h"
#include "mythnotification.h"

class MythUINotificationScreen : public MythScreenType
{
    Q_OBJECT

public:
    MythUINotificationScreen(MythScreenStack *stack,
                             int id = -1);
    MythUINotificationScreen(MythScreenStack *stack,
                             MythNotification &notification);

    virtual ~MythUINotificationScreen();

    // These two methods are declared by MythScreenType and their signatures
    // should not be changed
    virtual bool Create(void);
    virtual void Init(void);

    void SetNotification(MythNotification &notification);

    void UpdateArtwork(const QImage &image);
    void UpdateArtwork(const QString &image);
    void UpdateMetaData(const DMAP &data);
    void UpdatePlayback(int duration, int position);

    // UI methods
    void AdjustYPosition(int height);
    int  GetHeight(void);

    // utility methods
    QString stringFromSeconds(int time);

    enum Update {
        kForce      = 0,
        kImage      = 1 << 0,
        kDuration   = 1 << 1,
        kMetaData   = 1 << 2,
    };

public slots:
    void ProcessTimer(void);

public:
    int                 m_id;
    QImage              m_image;
    QString             m_imagePath;
    QString             m_title;
    QString             m_artist;
    QString             m_album;
    QString             m_format;
    int                 m_duration;
    int                 m_position;
    bool                m_added;
    uint32_t            m_update;
    MythUIImage        *m_artworkImage;
    MythUIText         *m_titleText;
    MythUIText         *m_artistText;
    MythUIText         *m_albumText;
    MythUIText         *m_formatText;
    MythUIText         *m_timeText;
    MythUIProgressBar  *m_progressBar;
};

#endif
