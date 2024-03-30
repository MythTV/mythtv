#ifndef MYTHMEDIAOVERLAY_H
#define MYTHMEDIAOVERLAY_H

// Qt
#include <QMap>
#include <QRect>
#include <QHash>
#include <QObject>

// MythTV
#include "libmythui/mythscreentype.h"

class TV;
class MythMainWindow;
class MythPlayerUI;
class MythPainter;

class MythOverlayWindow : public MythScreenType
{
    Q_OBJECT

  public:
    MythOverlayWindow(MythScreenStack* Parent, MythPainter* Painter, const QString& Name, bool Themed);
    bool Create() override;

  private:
    bool m_themed { false };
};

class MythMediaOverlay : public QObject
{
  public:
    MythMediaOverlay(MythMainWindow* MainWindow, TV* Tv, MythPlayerUI* Player, MythPainter* Painter);
   ~MythMediaOverlay() override;

    void    SetPlayer(MythPlayerUI* Player);
    virtual bool Init(QRect Rect, float FontAspect);
    QRect   Bounds() const;
    int     GetFontStretch() const;
    bool    HasWindow(const QString& Window);
    virtual MythScreenType* GetWindow(const QString& Window);
    virtual void HideWindow(const QString& Window);

  protected:
    virtual void TearDown();
    void OverrideUIScale(bool Log = true);
    void RevertUIScale();
    MythScreenType* InitWindow(const QString& Window, MythScreenType* Screen);

    MythMainWindow* m_mainWindow       { nullptr };
    TV*             m_tv               { nullptr };
    MythPlayerUI*   m_player           { nullptr };
    MythPainter*    m_painter          { nullptr };
    QRect           m_rect;
    bool            m_uiScaleOverride  { false };
    float           m_savedWMult       { 1.0F };
    float           m_savedHMult       { 1.0F };
    QRect           m_savedUIRect;
    int             m_fontStretch      { 0 };
    int             m_savedFontStretch { 0 };
    QMap<QString, MythScreenType*> m_children;
};

#endif
