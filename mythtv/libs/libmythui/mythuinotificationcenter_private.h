//
//  mythuinotificationcenter_private.h
//  MythTV
//
//  Created by Jean-Yves Avenard on 30/06/13.
//  Copyright (c) 2013 Bubblestuff Pty Ltd. All rights reserved.
//

#ifndef MythTV_mythuinotificationcenter_private_h
#define MythTV_mythuinotificationcenter_private_h

#include <stdint.h>

#include "mythscreenstack.h"
#include "mythscreentype.h"
#include "mythuiimage.h"
#include "mythuitext.h"
#include "mythuiprogressbar.h"
#include "mythuinotificationcenter.h"

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
    void UpdatePlayback(float progress, const QString &text);

    void SetSingleShotTimer(int s);

    // UI methods
    void AdjustYPosition(int height);
    int  GetHeight(void);

    enum Content {
        kForce      = 0,
        kNone       = 0,
        kImage      = 1 << 0,
        kDuration   = 1 << 1,
        kMetaData   = 1 << 2,
    };

    MythUINotificationScreen &operator=(MythUINotificationScreen &s);

signals:
    void ScreenDeleted();

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
    float               m_progress;
    QString             m_progressText;
    bool                m_fullscreen;
    bool                m_added;
    bool                m_created;
    uint32_t            m_content;
    uint32_t            m_update;
    MythUIImage        *m_artworkImage;
    MythUIText         *m_titleText;
    MythUIText         *m_artistText;
    MythUIText         *m_albumText;
    MythUIText         *m_formatText;
    MythUIText         *m_timeText;
    MythUIProgressBar  *m_progressBar;

};

//// class MythScreenNotificationStack

class MythNotificationScreenStack : public MythScreenStack
{
public:
    MythNotificationScreenStack(MythMainWindow *parent, const QString& name,
                                MythUINotificationCenter *owner)
        : MythScreenStack(parent, name), m_owner(owner)
    {
    }

    virtual ~MythNotificationScreenStack()
    {
        m_owner->ScreenStackDeleted();
    }

    void CheckDeletes()
    {
        QVector<MythScreenType*>::const_iterator it;

        for (it = m_ToDelete.begin(); it != m_ToDelete.end(); ++it)
        {
            (*it)->SetAlpha(0);
            (*it)->SetVisible(false);
            (*it)->Close();
        }
        MythScreenStack::CheckDeletes();
    }

private:
    MythUINotificationCenter *m_owner;

};

#endif
