//
//  mythnotification.h
//  MythTV
//
//  Created by Jean-Yves Avenard on 25/06/13.
//  Copyright (c) 2013 Bubblestuff Pty Ltd. All rights reserved.
//

#ifndef MYTHTV_MYTHNOTIFICATION_H
#define MYTHTV_MYTHNOTIFICATION_H

// Std
#include <utility>

// Qt
#include <QImage>
#include <QMap>
#include <QMutex>

// MythTV
#include "libmythbase/mythevent.h"
#include "libmythui/mythuiexp.h"

using namespace std::chrono_literals;

using DMAP = QMap<QString,QString>;
using VNMask = unsigned int;

class MUI_PUBLIC MythNotification : public MythEvent
{
  public:
    static const Type kNew;
    static const Type kUpdate;
    static const Type kInfo;
    static const Type kError;
    static const Type kWarning;
    static const Type kCheck;
    static const Type kBusy;

    MythNotification(Type nType, void* Parent = nullptr);
    MythNotification(int Id, void* Parent);
    MythNotification(const QString& Title, const QString& Author,
                     const QString& Details = QString());
    MythNotification(Type nType, const QString& Title, const QString& Author,
                     const QString& Details = QString(), const QString& Extra   = QString());
    MythNotification(Type nType, DMAP Metadata);
    explicit MythNotification(const MythEvent& Event);
   ~MythNotification() override = default;
    MythEvent* clone() const override;
    // No implicit copying.
    MythNotification(MythNotification &&) = delete;
    MythNotification &operator=(MythNotification &&) = delete;

    /*! \enum Priority
     * A notification can be given a priority. Display order of notification
     * will be sorted according to the priority level
     */
    enum Priority : std::uint8_t
    {
        kDefault = 0,
        kLow,
        kMedium,
        kHigh,
        kHigher,
        kHighest,
    };

    /*! \enum Visibility
     * A notification can be given visibility mask allowing to not be visible
     * under some circumstances, like the screen currently being displayed.
     * This is used to prevent redundant information appearing more than once:
     * like in MythMusic, there's no need to show music notifications
     */
    enum Visibility : std::uint8_t
    {
        kNone       = 0,
        kAll        = 0xFF,
        kPlayback   = (1 << 0),
        kSettings   = (1 << 1),
        kWizard     = (1 << 2),
        kVideos     = (1 << 3),
        kMusic      = (1 << 4),
        kRecordings = (1 << 5),
    };

    void SetId(int Id);
    void SetParent(void* Parent);
    void SetFullScreen(bool FullScreen);
    void SetDescription(const QString& Description);
    void SetMetaData(const DMAP& MetaData);
    void SetDuration(std::chrono::seconds Duration);
    void SetStyle(const QString& sStyle);
    void SetVisibility(VNMask nVisibility);
    void SetPriority(Priority nPriority);
    static Type TypeFromString(const QString& Type);
    void ToStringList();
    bool FromStringList();

    // Getter
    int      GetId() const          { return m_id; }
    void*    GetParent() const      { return m_parent; }
    bool     GetFullScreen() const  { return m_fullScreen; }
    QString  GetDescription() const { return m_description; }
    DMAP     GetMetaData() const    { return m_metadata; }
    std::chrono::seconds GetDuration() const { return m_duration; }
    QString  GetStyle() const       { return m_style; }
    VNMask   GetVisibility() const  { return m_visibility; }
    Priority GetPriority() const    { return m_priority; }

  protected:
    MythNotification(const MythNotification& Notification);

#ifndef _MSC_VER
    MythNotification &operator=(const MythNotification&);
#else
    MythNotification &operator=(const MythNotification &other) = default;
#endif

  protected:
    int      m_id          { -1 };
    void*    m_parent      { nullptr };
    bool     m_fullScreen  { false };
    QString  m_description;
    std::chrono::seconds m_duration { 0s };
    DMAP     m_metadata;
    QString  m_style;
    VNMask   m_visibility  { static_cast<VNMask>(kAll) };
    Priority m_priority    { kDefault };
};

class MUI_PUBLIC MythImageNotification : public virtual MythNotification
{
  public:
    MythImageNotification(Type nType, QImage Image);
    MythImageNotification(Type nType, QString ImagePath);
    MythImageNotification(Type nType, QImage Image, const DMAP& Metadata);
    MythImageNotification(Type nType, QString ImagePath, const DMAP& Metadata);
    MythEvent *clone() const override;

    void    SetImage(const QImage& Image)      { m_image = Image; }
    void    SetImagePath(const QString& Image) { m_imagePath = Image; }
    QImage  GetImage() const                   { return m_image; }
    QString GetImagePath() const               { return m_imagePath; }

  protected:
    MythImageNotification(const MythImageNotification&) = default;

  protected:
    QImage  m_image;
    QString m_imagePath;
};

class MUI_PUBLIC MythPlaybackNotification : public virtual MythNotification
{
  public:
    MythPlaybackNotification(Type nType, float Progress, QString ProgressText);
    MythPlaybackNotification(Type nType, float Progress, QString ProgressText,
                             const DMAP& Metadata);
    MythPlaybackNotification(Type nType, std::chrono::seconds Duration, int Position);
    MythEvent* clone() const override;

    // Setter
    /*! \brief Set the current playback position to be displayed with the notification.
     * Value to be between 0 <= x <= 1.
     * Note: x < 0 means no progress bar to be displayed.
    */
    void    SetProgress(float progress)          { m_progress = progress; }
    void    SetProgressText(const QString& text) { m_progressText = text; }
    float   GetProgress() const                  { return m_progress; }
    QString GetProgressText() const              { return m_progressText; }
    static QString StringFromSeconds(std::chrono::seconds Time);

  protected:
    MythPlaybackNotification(const MythPlaybackNotification&) = default;

    float   m_progress { -1.0F };
    QString m_progressText;
};

class MUI_PUBLIC MythMediaNotification : public MythImageNotification,
                                         public MythPlaybackNotification
{
  public:
    MythMediaNotification(Type nType, const QImage& Image, const DMAP& Metadata,
                          float Progress, const QString& DurationText);
    MythMediaNotification(Type nType, const QImage& Image, const DMAP& Metadata,
                          std::chrono::seconds Duration, int Position);
    MythMediaNotification(Type nType, const QString& Image, const DMAP& Metadata,
                          float Progress, const QString& DurationText);
    MythMediaNotification(Type nType, const QString& Image, const DMAP& Metadata,
                          std::chrono::seconds Duration, int Position);
    MythEvent* clone() const override;

  protected:
    MythMediaNotification(const MythMediaNotification& Notification);
};

class MUI_PUBLIC MythErrorNotification : public MythNotification
{
  public:
    MythErrorNotification(const QString& Title, const QString& Author,
                          const QString& Details = QString());
};

class MUI_PUBLIC MythWarningNotification : public MythNotification
{
  public:
    MythWarningNotification(const QString& Title, const QString& Author,
                            const QString& Details = QString());
};

class MUI_PUBLIC MythCheckNotification : public MythNotification
{
  public:
    MythCheckNotification(const QString& Title, const QString& Author,
                          const QString& Details = QString());
};

class MUI_PUBLIC MythBusyNotification : public MythNotification
{
  public:
    MythBusyNotification(const QString& Title, const QString& Author,
                         const QString& Details = QString());
};

#endif
