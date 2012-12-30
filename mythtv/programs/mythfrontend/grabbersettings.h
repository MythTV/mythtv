#ifndef GRABBERSETTINGS_H
#define GRABBERSETTINGS_H

#include "uitypes.h"
#include "mythwidgets.h"
#include "mythdialogs.h"

// libmythui
#include "mythuibutton.h"
#include "mythuibuttonlist.h"
#include "mythuicheckbox.h"
#include "mythscreentype.h"
#include "mythdialogbox.h"

class MetaGrabberScript;
class GrabberSettings : public MythScreenType
{
  Q_OBJECT

  public:

    GrabberSettings(MythScreenStack *parent, const char *name = 0);
    ~GrabberSettings();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

  private:
    virtual void Load(void);
    virtual void Init(void);

    QList<MetaGrabberScript*> m_movieGrabberList;
    QList<MetaGrabberScript*> m_tvGrabberList;
    QList<MetaGrabberScript*> m_gameGrabberList;

    QMap<QString, QString>  m_languageMap;
    QStringList             m_languageList;
    QString                 m_defaultLanguageCode;

    MythUIButtonList   *m_movieGrabberButtonList;
    MythUIButtonList   *m_tvGrabberButtonList;
    MythUIButtonList   *m_gameGrabberButtonList;

    MythUICheckBox     *m_dailyUpdatesCheck;

    MythUIButtonList   *m_movieLangButtonList;
    MythUIButtonList   *m_tvLangButtonList;
    MythUIButtonList   *m_gameLangButtonList;

    MythUIButton       *m_okButton;
    MythUIButton       *m_cancelButton;

  private slots:
    void slotSave(void);
};

#endif

