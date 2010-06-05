// Qt
#include <QCoreApplication>

// libmyth
#include "mythverbose.h"

// libmythui
#include "mythmainwindow.h"
#include "mythuihelper.h"
#include "mythpainter.h"
#include "mythuiimage.h"
#include "mythuiprogressbar.h"
#include "mythdialogbox.h"
#include "mythuitext.h"
#include "mythuibutton.h"
#include "mythuieditbar.h"

// libmythtv
#include "channelutil.h"
#include "teletextscreen.h"
#include "subtitlescreen.h"
#include "interactivescreen.h"
#include "osd.h"

#define LOC     QString("OSD: ")
#define LOC_ERR QString("OSD_ERR: ")

QEvent::Type OSDHideEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

ChannelEditor::ChannelEditor(const char *name)
  : MythScreenType((MythScreenType*)NULL, name)
{
    m_retObject    = NULL;
    m_callsignEdit = NULL;
    m_channumEdit  = NULL;
    m_channameEdit = NULL;
    m_xmltvidEdit  = NULL;
}

bool ChannelEditor::Create(void)
{
    if (!XMLParseBase::LoadWindowFromXML("osd.xml", "ChannelEditor", this))
        return false;

    MythUIText *messageText   = NULL;
    MythUIButton *probeButton = NULL;
    MythUIButton *okButton    = NULL;

    bool err = false;
    UIUtilE::Assign(this, messageText,    "message",  &err);
    UIUtilE::Assign(this, m_callsignEdit, "callsign", &err);
    UIUtilE::Assign(this, m_channumEdit,  "channum",  &err);
    UIUtilE::Assign(this, m_channameEdit, "channame", &err);
    UIUtilE::Assign(this, m_xmltvidEdit,  "XMLTV",    &err);
    UIUtilE::Assign(this, probeButton,    "probe",    &err);
    UIUtilE::Assign(this, okButton,       "ok",       &err);

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'ChannelEditor'");
        return false;
    }

    messageText->SetText(tr("Channel Editor"));
    BuildFocusList();
    connect(okButton,    SIGNAL(Clicked()), SLOT(Confirm()));
    connect(probeButton, SIGNAL(Clicked()), SLOT(Probe()));
    SetFocusWidget(okButton);

    return true;
}

void ChannelEditor::SetReturnEvent(QObject *retobject, const QString &resultid)
{
    (void)resultid;
    m_retObject = retobject;
}

void ChannelEditor::Confirm(void)
{
    sendResult(1, true);
}

void ChannelEditor::Probe(void)
{
    sendResult(1, false);
}

void ChannelEditor::SetText(QHash<QString,QString>&map)
{
    if (map.contains("callsign"))
        m_callsignEdit->SetText(map.value("callsign"));
    if (map.contains("channum"))
        m_channumEdit->SetText(map.value("channum"));
    if (map.contains("channame"))
        m_channameEdit->SetText(map.value("channame"));
    if (map.contains("XMLTV"))
        m_xmltvidEdit->SetText(map.value("XMLTV"));
}

void ChannelEditor::GetText(QHash<QString,QString>&map)
{
    map["callsign"] = m_callsignEdit->GetText();
    map["channum"]  = m_channumEdit->GetText();
    map["channame"] = m_channameEdit->GetText();
    map["XMLTV"]    = m_xmltvidEdit->GetText();
}

bool ChannelEditor::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("qt", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        if (action == "ESCAPE" )
        {
            sendResult(-1, false);
            Close();
            handled = true;
        }
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void ChannelEditor::sendResult(int result, bool ok)
{
    if (!m_retObject)
        return;

    QString  message = "";
    if (result < 0)
    {
    }
    else if (ok)
    {
        message = "DIALOG_EDITOR_OK_0";
    }
    else // probe
    {
        message = "DIALOG_EDITOR_PROBE_0";
    }

    DialogCompletionEvent *dce = new DialogCompletionEvent("", result,
                                                           "", message);
    QCoreApplication::postEvent(m_retObject, dce);
}

OSD::OSD(NuppelVideoPlayer *player, QObject *parent)
  : m_parent(player), m_ParentObject(parent), m_Rect(QRect()),
    m_Effects(true),  m_FadeTime(1000),       m_Dialog(NULL),
    m_PulsedDialogText(QString()),            m_NextPulseUpdate(QDateTime()),
    m_Refresh(false),   m_UIScaleOverride(false),
    m_SavedWMult(1.0f), m_SavedHMult(1.0f),   m_SavedUIRect(QRect()),
    m_fontStretch(100),
    m_FunctionalType(kOSDFunctionalType_Default), m_FunctionalWindow(QString())
{
}

OSD::~OSD()
{
    TearDown();
}

void OSD::TearDown(void)
{
    HideAll(false);
    foreach(MythScreenType* screen, m_Children)
        screen->deleteLater();
    m_Children.clear();
}

bool OSD::Init(const QRect &rect, float font_aspect)
{
    m_Rect = rect;
    m_fontStretch = (int)((font_aspect * 100) + 0.5f);
    OverrideUIScale();
    LoadWindows();
    RevertUIScale();

    if (!m_Children.size())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Failed to load any windows."));
        return false;
    }

    VERBOSE(VB_PLAYBACK, LOC + QString("Loaded OSD: size %1x%2 offset %3+%4")
                         .arg(m_Rect.width()).arg(m_Rect.height())
                         .arg(m_Rect.left()).arg(m_Rect.top()));
    HideAll(false);
    return true;
}

void OSD::OverrideUIScale(void)
{
    QRect uirect = GetMythMainWindow()->GetUIScreenRect();
    if (uirect == m_Rect)
        return;

    int width, height;
    MythUIHelper::getMythUI()->GetScreenSettings(width,  m_SavedWMult,
                                                 height, m_SavedHMult);
    QSize theme_size = MythUIHelper::getMythUI()->GetBaseSize();
    m_SavedUIRect = uirect;
    VERBOSE(VB_GENERAL, LOC + QString("Base theme size: %1x%2")
               .arg(theme_size.width()).arg(theme_size.height()));
    float tmp_wmult = (float)m_Rect.size().width() / (float)theme_size.width();
    float tmp_hmult = (float)m_Rect.size().height() / (float)theme_size.height();
    VERBOSE(VB_IMPORTANT, LOC + QString("Scaling factors: %1x%2")
                                .arg(tmp_wmult).arg(tmp_hmult));
    m_UIScaleOverride = true;
    GetMythMainWindow()->SetScalingFactors(tmp_wmult, tmp_hmult);
    GetMythMainWindow()->SetUIScreenRect(m_Rect);
}

void OSD::RevertUIScale(void)
{
    if (m_UIScaleOverride)
    {
        GetMythMainWindow()->SetScalingFactors(m_SavedWMult, m_SavedHMult);
        GetMythMainWindow()->SetUIScreenRect(m_SavedUIRect);
    }
    m_UIScaleOverride = false;
}

bool OSD::Reinit(const QRect &rect, float font_aspect)
{
    m_Refresh = true;
    int new_stretch = (int)((font_aspect * 100) + 0.5f);
    if ((rect == m_Rect) && (new_stretch == m_fontStretch))
        return true;

    TearDown();
    if (!Init(rect, font_aspect))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Failed to re-init OSD."));
        return false;
    }
    return true;
}

bool OSD::IsVisible(void)
{
    foreach(MythScreenType* child, m_Children)
        if (child->IsVisible() &&
            child->objectName() != OSD_WIN_SUBTITLE &&
            child->objectName() != OSD_WIN_TELETEXT &&
            child->objectName() != OSD_WIN_INTERACT)
            return true;

    return false;
}

void OSD::HideAll(bool keepsubs)
{
    QMutableHashIterator<QString, MythScreenType*> it(m_Children);
    while (it.hasNext())
    {
        it.next();
        if (!(keepsubs && (it.key() == OSD_WIN_SUBTITLE ||
            it.key() == OSD_WIN_TELETEXT)))
            HideWindow(it.key());
    }
}

void OSD::LoadWindows(void)
{
    static const char* default_windows[6] = {
        "osd_message", "osd_input", "program_info", "browse_info", "osd_status",
        "osd_program_editor"};

    for (int i = 0; i < 6; i++)
    {
        const char* window = default_windows[i];
        MythOSDWindow *win = new MythOSDWindow(NULL, window, true);
        if (win && win->Create())
        {
            PositionWindow(win);
            VERBOSE(VB_PLAYBACK, LOC + QString("Loaded window %1").arg(window));
            m_Children.insert(window, win);
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Failed to load window %1")
                .arg(window));
            delete win;
        }
    }
}

void OSD::SetValues(const QString &window, QHash<QString,int> &map,
                    bool set_expiry)
{
    MythScreenType *win = GetWindow(window);
    if (!win)
        return;

    bool found = false;
    if (map.contains("position"))
    {
        MythUIProgressBar *bar = dynamic_cast<MythUIProgressBar *> (win->GetChild("position"));
        if (bar)
        {
            bar->SetVisible(true);
            bar->SetStart(0);
            bar->SetTotal(1000);
            bar->SetUsed(map.value("position"));
            found = true;
        }
    }

    if (set_expiry && found)
        SetExpiry(win);
}

void OSD::SetValues(const QString &window, QHash<QString,float> &map,
                    bool set_expiry)
{
    MythScreenType *win = GetWindow(window);
    if (!win)
        return;

    bool found = false;
    if (map.contains("position"))
    {
        MythUIEditBar *edit = dynamic_cast<MythUIEditBar *> (win->GetChild("editbar"));
        if (edit)
            edit->SetPosition(map.value("position"));
    }

    if (set_expiry && found)
        SetExpiry(win);
}

void OSD::SetText(const QString &window, QHash<QString,QString> &map,
                  bool set_expiry)
{
    MythScreenType *win = GetWindow(window);
    if (!win)
        return;

    if (map.contains("chanid"))
    {
        MythUIImage *icon = dynamic_cast<MythUIImage *> (win->GetChild("iconpath"));
        if (icon)
        {
            uint chanid = map["chanid"].toUInt();
            QString iconpath;
            if (map.contains("iconpath"))
                iconpath = map["iconpath"];
            else
                iconpath = ChannelUtil::GetIcon(chanid);
            icon->SetFilename(iconpath);
            icon->Load(false);
        }
        MythUIImage *cover = dynamic_cast<MythUIImage *> (win->GetChild("coverart"));
        if (cover && map.contains("coverartpath"))
        {
            QString coverpath = map["coverartpath"];
            cover->SetFilename(coverpath);
            cover->Load(false);
        }
        MythUIImage *fanart = dynamic_cast<MythUIImage *> (win->GetChild("fanart"));
        if (fanart && map.contains("fanartpath"))
        {
            QString fanartpath = map["fanartpath"];
            fanart->SetFilename(fanartpath);
            fanart->Load(false);
        }
        MythUIImage *banner = dynamic_cast<MythUIImage *> (win->GetChild("banner"));
        if (banner && map.contains("bannerpath"))
        {
            QString bannerpath = map["bannerpath"];
            banner->SetFilename(bannerpath);
            banner->Load(false);
        }
        MythUIImage *screenshot = dynamic_cast<MythUIImage *> (win->GetChild("screenshot"));
        if (screenshot && map.contains("screenshotpath"))
        {
            QString screenshotpath = map["screenshotpath"];
            screenshot->SetFilename(screenshotpath);
            screenshot->Load(false);
        }
    }

    win->SetVisible(true);

    if (win == m_Dialog)
    {
        ChannelEditor *edit = dynamic_cast<ChannelEditor*>(m_Dialog);
        if (edit)
            edit->SetText(map);
    }
    else
        win->SetTextFromMap(map);

    if (set_expiry)
        SetExpiry(win);
}

void OSD::SetRegions(const QString &window, frm_dir_map_t &map,
                     long long total)
{
    MythScreenType *win = GetWindow(window);
    if (!win)
        return;

    MythUIEditBar *bar = dynamic_cast<MythUIEditBar*>(win->GetChild("editbar"));
    if (!bar)
        return;

    bar->ClearRegions();
    if (!map.size() || total < 1)
    {
        bar->Display();
        return;
    }

    long long start = -1;
    long long end   = -1;
    bool first = true;
    QMapIterator<uint64_t, MarkTypes> it(map);
    while (it.hasNext())
    {
        bool error = false;
        it.next();
        if (it.value() == MARK_CUT_START)
        {
            start = it.key();
            if (end > -1)
                error = true;
        }
        else if (it.value() == MARK_CUT_END)
        {
            if (first)
                start = 0;
            if (start < 0)
                error = true;
            end = it.key();
        }
        first = false;

        if (error)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "deleteMap discontinuity");
            start = -1;
            end   = -1;
        }

        if (start >=0 && end >= 0)
        {
            bar->AddRegion((float)((double)start/(double)total),
                           (float)((double)end/(double)total));
            start = -1;
            end   = -1;
        }
    }
    if (start > -1 && end < 0)
        bar->AddRegion((float)((double)start/(double)total), 1.0f);

    bar->Display();
}

bool OSD::DrawDirect(MythPainter* painter, QSize size, bool repaint)
{
    if (!painter)
        return false;

    bool visible = false;
    bool redraw  = m_Refresh;
    m_Refresh    = false;
    QTime now = QTime::currentTime();

    CheckExpiry();
    QHash<QString,MythScreenType*>::iterator it;
    for (it = m_Children.begin(); it != m_Children.end(); ++it)
    {
        if (it.value()->IsVisible())
        {
            visible = true;
            it.value()->Pulse();
            if (m_Effects && m_ExpireTimes.contains(it.value()))
            {
                QTime expires = m_ExpireTimes.value(it.value()).time();
                int left = now.msecsTo(expires);
                if (left < m_FadeTime)
                    (*it)->SetAlpha((255 * left) / m_FadeTime);
            }
            if (it.value()->NeedsRedraw())
                redraw = true;
        }
    }

    redraw |= repaint;

    if (redraw && visible)
    {
        QRect cliprect = QRect(QPoint(0, 0), size);
        painter->Begin(NULL);
        for (it = m_Children.begin(); it != m_Children.end(); ++it)
        {
            if (it.value()->IsVisible())
            {
                it.value()->Draw(painter, 0, 0, 255, cliprect);
                it.value()->SetAlpha(255);
                it.value()->ResetNeedsRedraw();
            }
        }
        painter->End();
    }

    return visible;
}

QRegion OSD::Draw(MythPainter* painter, QPaintDevice *device, QSize size,
                  QRegion &changed, int alignx, int aligny)
{
    bool redraw     = m_Refresh;
    QRegion visible = QRegion();
    QRegion dirty   = m_Refresh ? QRegion(QRect(QPoint(0,0), m_Rect.size())) :
                                  QRegion();
    m_Refresh       = false;

    if (!painter || !device)
        return visible;

    QTime now = QTime::currentTime();
    CheckExpiry();

    // first update for alpha pulse and fade
    QHash<QString,MythScreenType*>::iterator it;
    for (it = m_Children.begin(); it != m_Children.end(); ++it)
    {
        if (it.value()->IsVisible())
        {
            QRect vis = it.value()->GetArea().toQRect();
            if (visible.isEmpty())
                visible = QRegion(vis);
            else
                visible = visible.united(vis);

            it.value()->Pulse();
            if (m_Effects && m_ExpireTimes.contains(it.value()))
            {
                QTime expires = m_ExpireTimes.value(it.value()).time();
                int left = now.msecsTo(expires);
                if (left < m_FadeTime)
                    (*it)->SetAlpha((255 * left) / m_FadeTime);
            }
        }

        if (it.value()->NeedsRedraw())
        {
            QRegion area = it.value()->GetDirtyArea();
            dirty = dirty.united(area);
            redraw = true;
        }
    }

    if (redraw)
    {
        // clear the dirty area
        painter->Clear(device, dirty);

        // set redraw for any widgets that may now need a partial repaint
        for (it = m_Children.begin(); it != m_Children.end(); ++it)
        {
            if (it.value()->IsVisible() && !it.value()->NeedsRedraw() &&
                dirty.intersects(it.value()->GetArea().toQRect()))
            {
                it.value()->SetRedraw();
            }
        }

        // and finally draw
        QRect cliprect = dirty.boundingRect();
        painter->Begin(device);
        painter->SetClipRegion(dirty);
        // TODO painting in reverse may be more efficient...
        for (it = m_Children.begin(); it != m_Children.end(); ++it)
        {
            if (it.value()->NeedsRedraw())
            {
                if (it.value()->IsVisible())
                    it.value()->Draw(painter, 0, 0, 255, cliprect);
                it.value()->SetAlpha(255);
                it.value()->ResetNeedsRedraw();
            }
        }
        painter->End();
    }

    changed = dirty;

    if (visible.isEmpty() || (!alignx && !aligny))
        return visible;

    // assist yuv blending with some friendly alignments
    QRegion aligned;
    QVector<QRect> rects = visible.rects();
    for (int i = 0; i < rects.size(); i++)
    {
        QRect r   = rects[i];
        int left  = r.left() & ~(alignx - 1);
        int top   = r.top()  & ~(aligny - 1);
        int right = (r.left() + r.width());
        int bot   = (r.top() + r.height());
        if (right & (alignx - 1))
            right += alignx - (right & (alignx - 1));
        if (bot % aligny)
            bot += aligny - (bot % aligny);
        aligned = aligned.united(QRegion(left, top, right - left, bot - top));
    }

    return aligned.intersected(QRect(QPoint(0,0), size));
}

void OSD::CheckExpiry(void)
{
    QDateTime now = QDateTime::currentDateTime();
    QMutableHashIterator<MythScreenType*, QDateTime> it(m_ExpireTimes);
    while (it.hasNext())
    {
        it.next();
        if (it.value() < now)
        {
            if (it.key() == m_Dialog)
                DialogQuit();
            else
                HideWindow(m_Children.key(it.key()));
        }
        else if (it.key() == m_Dialog)
        {
            if (!m_PulsedDialogText.isEmpty() && now > m_NextPulseUpdate)
            {
                QString newtext = m_PulsedDialogText;
                newtext.replace("%d", QString::number(now.secsTo(it.value())));
                MythDialogBox *dialog = dynamic_cast<MythDialogBox*>(m_Dialog);
                if (dialog)
                    dialog->SetText(newtext);
                m_NextPulseUpdate = now.addSecs(1);
            }
        }
    }
}

void OSD::SetExpiry(MythScreenType *window, int time)
{
    if (time > 0)
    {
        QDateTime expires = QDateTime::currentDateTime().addSecs(time);
        m_ExpireTimes.insert(window, expires);
    }
    else if (time < 0)
    {
        if (m_ExpireTimes.contains(window))
            m_ExpireTimes.remove(window);
    }
}

bool OSD::IsWindowVisible(const QString &window)
{
    if (!m_Children.contains(window))
        return false;

    return m_Children.value(window)->IsVisible(/*true*/);
}

void OSD::ResetWindow(const QString &window)
{
    if (!m_Children.contains(window))
        return;

    m_Children.value(window)->DeleteAllChildren();
}

void OSD::PositionWindow(MythScreenType *window)
{
    if (!window)
        return;

    MythRect rect = window->GetArea();
    rect.translate(m_Rect.left(), m_Rect.top());
    window->SetArea(rect);
}

void OSD::RemoveWindow(const QString &window)
{
    if (!m_Children.contains(window))
        return;

    HideWindow(window);
    MythScreenType *child = m_Children.value(window);
    m_Children.remove(window);
    child->deleteLater();
}

MythScreenType *OSD::GetWindow(const QString &window)
{
    if (m_Children.contains(window))
        return m_Children.value(window);

    MythScreenType *new_window = NULL;

    if (window == OSD_WIN_INTERACT)
    {
        InteractiveScreen *screen = new InteractiveScreen(m_parent, window);
        new_window = (MythScreenType*) screen;
    }
    else
    {
        MythOSDWindow *screen = new MythOSDWindow(NULL, window, false);
        new_window = (MythScreenType*) screen;
    }

    if (new_window && new_window->Create())
    {
        m_Children.insert(window, new_window);
        VERBOSE(VB_PLAYBACK, LOC + QString("Created window %1").arg(window));
        return new_window;
    }

    VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Failed to create window %1")
            .arg(window));
    return NULL;
}

void OSD::DisableExpiry(const QString &window)
{
    if (m_Children.contains(window))
        SetExpiry(m_Children.value(window), -1);
}

void OSD::SetFunctionalWindow(const QString window, enum OSDFunctionalType type)
{
    if (m_FunctionalType != kOSDFunctionalType_Default &&
        m_FunctionalType != type)
        SendHideEvent();

    m_FunctionalWindow = window;
    m_FunctionalType   = type;
}

void OSD::HideWindow(const QString &window)
{
    if (!m_Children.contains(window))
        return;
    m_Children.value(window)->SetVisible(false);
    m_Children.value(window)->Close(); // for InteractiveScreen
    DisableExpiry(window);
    m_Refresh = true;

    if (m_FunctionalType != kOSDFunctionalType_Default)
    {
        bool valid   = m_Children.contains(m_FunctionalWindow);
        bool visible = valid && m_Children.value(m_FunctionalWindow)->IsVisible(false);
        if (!valid || !visible)
        {
            SendHideEvent();
            m_FunctionalType = kOSDFunctionalType_Default;
            m_FunctionalWindow = QString();
        }
    }
}

void OSD::SendHideEvent(void)
{
    OSDHideEvent *event = new OSDHideEvent(m_FunctionalType);
    QCoreApplication::postEvent(m_ParentObject, event);
}

bool OSD::HasWindow(const QString &window)
{
    return m_Children.contains(window);
}

bool OSD::DialogVisible(QString window)
{
    if (!m_Dialog || window.isEmpty())
        return m_Dialog;

    return m_Dialog->objectName() == window;
}

bool OSD::DialogHandleKeypress(QKeyEvent *e)
{
    if (!m_Dialog)
        return false;
    return m_Dialog->keyPressEvent(e);
}

void OSD::DialogQuit(void)
{
    if (!m_Dialog)
        return;

    RemoveWindow(m_Dialog->objectName());
    m_Dialog = NULL;
    m_PulsedDialogText = QString();
}

void OSD::DialogShow(const QString &window, const QString &text, int updatefor)
{
    if (m_Dialog)
    {
        QString current = m_Dialog->objectName();
        if (current != window)
        {
            DialogQuit();
        }
        else
        {
            MythDialogBox *dialog = dynamic_cast<MythDialogBox*>(m_Dialog);
            if (dialog)
                dialog->ResetButtons();
            DialogSetText(text);
        }
    }

    if (!m_Dialog)
    {
        OverrideUIScale();
        MythScreenType *dialog;
        if (window == OSD_DLG_EDITOR)
            dialog = new ChannelEditor(window.toLatin1());
        else
            dialog = new MythDialogBox(text, NULL, window.toLatin1(), false, true);

        if (dialog && dialog->Create())
        {
            PositionWindow(dialog);
            m_Dialog = dialog;
            MythDialogBox *dbox = dynamic_cast<MythDialogBox*>(m_Dialog);
            if (dbox)
                dbox->SetReturnEvent(m_ParentObject, window);
            m_Children.insert(window, m_Dialog);
        }
        else
        {
            if (dialog)
                delete dialog;
            return;
        }
        RevertUIScale();
    }

    if (updatefor)
    {
        m_NextPulseUpdate  = QDateTime::currentDateTime();
        m_PulsedDialogText = text;
        SetExpiry(m_Dialog, updatefor);
    }

    DialogBack();
    HideAll();
    m_Dialog->SetVisible(true);
}

void OSD::DialogSetText(const QString &text)
{
    MythDialogBox *dialog = dynamic_cast<MythDialogBox*>(m_Dialog);
    if (dialog)
        dialog->SetText(text);
}

void OSD::DialogBack(QString text, QVariant data, bool exit)
{
    MythDialogBox *dialog = dynamic_cast<MythDialogBox*>(m_Dialog);
    if (dialog)
    {
        dialog->SetBackAction(text, data);
        if (exit)
            dialog->SetExitAction(text, data);
    }
}

void OSD::DialogAddButton(QString text, QVariant data, bool menu, bool current)
{
    MythDialogBox *dialog = dynamic_cast<MythDialogBox*>(m_Dialog);
    if (dialog)
        dialog->AddButton(text, data, menu, current);
}

void OSD::DialogGetText(QHash<QString,QString> &map)
{
    ChannelEditor *edit = dynamic_cast<ChannelEditor*>(m_Dialog);
    if (edit)
        edit->GetText(map);
}

TeletextScreen* OSD::InitTeletext(void)
{
    TeletextScreen *tt = NULL;
    if (m_Children.contains(OSD_WIN_TELETEXT))
    {
        tt = (TeletextScreen*)m_Children.value(OSD_WIN_TELETEXT);
    }
    else
    {
        OverrideUIScale();
        tt = new TeletextScreen(m_parent, OSD_WIN_TELETEXT);
        if (tt)
        {
            if (tt->Create())
            {
                m_Children.insert(OSD_WIN_TELETEXT, tt);
            }
            else
            {
                delete tt;
                tt = NULL;
            }
        }
        RevertUIScale();
    }
    if (!tt)
    {
        VERBOSE(VB_IMPORTANT, LOC + QString("Failed to create Teletext window"));
        return NULL;
    }

    HideWindow(OSD_WIN_TELETEXT);
    tt->SetDisplaying(false);
    return tt;
}

void OSD::EnableTeletext(bool enable, int page)
{
    TeletextScreen *tt = InitTeletext();
    if (!enable || !tt)
    {
        VERBOSE(VB_PLAYBACK, LOC + "Disabled teletext");
        return;
    }

    tt->SetVisible(true);
    tt->SetDisplaying(true);
    tt->SetPage(page, -1);
    VERBOSE(VB_PLAYBACK, LOC + QString("Enabled teletext page %1").arg(page));
}

bool OSD::TeletextAction(const QString &action)
{
    if (!HasWindow(OSD_WIN_TELETEXT))
        return false;

    TeletextScreen* tt = (TeletextScreen*)m_Children.value(OSD_WIN_TELETEXT);
    if (!tt)
        return false;

    if (action == "NEXTPAGE")
        tt->KeyPress(TTKey::kNextPage);
    else if (action == "PREVPAGE")
        tt->KeyPress(TTKey::kPrevPage);
    else if (action == "NEXTSUBPAGE")
        tt->KeyPress(TTKey::kNextSubPage);
    else if (action == "PREVSUBPAGE")
        tt->KeyPress(TTKey::kPrevSubPage);
    else if (action == "TOGGLEBACKGROUND")
        tt->KeyPress(TTKey::kTransparent);
    else if (action == "MENURED")
        tt->KeyPress(TTKey::kFlofRed);
    else if (action == "MENUGREEN")
        tt->KeyPress(TTKey::kFlofGreen);
    else if (action == "MENUYELLOW")
        tt->KeyPress(TTKey::kFlofYellow);
    else if (action == "MENUBLUE")
        tt->KeyPress(TTKey::kFlofBlue);
    else if (action == "MENUWHITE")
        tt->KeyPress(TTKey::kFlofWhite);
    else if (action == "REVEAL")
        tt->KeyPress(TTKey::kRevealHidden);
    else if (action == "0" || action == "1" || action == "2" ||
             action == "3" || action == "4" || action == "5" ||
             action == "6" || action == "7" || action == "8" ||
             action == "9")
        tt->KeyPress(action.toInt());
    else
        return false;
    return true;
}

void OSD::TeletextReset(void)
{
    if (!HasWindow(OSD_WIN_TELETEXT))
        return;

    TeletextScreen* tt = InitTeletext();
    if (tt)
        tt->Reset();
}

SubtitleScreen* OSD::InitSubtitles(void)
{
    SubtitleScreen *sub = NULL;
    if (m_Children.contains(OSD_WIN_SUBTITLE))
    {
        sub = (SubtitleScreen*)m_Children.value(OSD_WIN_SUBTITLE);
    }
    else
    {
        OverrideUIScale();
        sub = new SubtitleScreen(m_parent, OSD_WIN_SUBTITLE);
        if (sub)
        {
            if (sub->Create())
            {
                m_Children.insert(OSD_WIN_SUBTITLE, sub);
            }
            else
            {
                delete sub;
                sub = NULL;
            }
        }
        RevertUIScale();
    }
    if (!sub)
    {
        VERBOSE(VB_IMPORTANT, LOC + QString("Failed to create subtitle window"));
        return NULL;
    }
    return sub;
}

void OSD::EnableSubtitles(int type)
{
    SubtitleScreen *sub = InitSubtitles();
    if (sub)
        sub->EnableSubtitles(type);
}

void OSD::ClearSubtitles(void)
{
    if (!HasWindow(OSD_WIN_SUBTITLE))
        return;

    SubtitleScreen* sub = InitSubtitles();
    if (sub)
        sub->ClearAllSubtitles();
}

void OSD::DisplayDVDButton(AVSubtitle* dvdButton, QRect &pos)
{
    SubtitleScreen* sub = InitSubtitles();
    if (sub && dvdButton)
    {
        EnableSubtitles(kDisplayDVDButton);
        sub->DisplayDVDButton(dvdButton, pos);
    }
}
