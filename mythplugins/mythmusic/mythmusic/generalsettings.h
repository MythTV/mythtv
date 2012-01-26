#ifndef GENERALSETTINGS_H
#define GENERALSETTINGS_H

// MythTV
#include <mythscreentype.h>
#include <mythuispinbox.h>
#include <mythuitext.h>
#include <mythuitextedit.h>
#include <mythuibutton.h>
#include <mythuicheckbox.h>


class GeneralSettings : public MythScreenType
{
    Q_OBJECT
public:
    GeneralSettings(MythScreenStack *parent, const char *name = 0);
    ~GeneralSettings();

    bool Create(void);

private:
    MythUITextEdit     *m_musicLocation;
    MythUITextEdit     *m_musicAudioDevice;
    MythUICheckBox     *m_musicDefaultUpmix;
    MythUITextEdit     *m_musicCDDevice;
    MythUITextEdit     *m_nonID3FileNameFormat;
    MythUICheckBox     *m_ignoreID3Tags;
    MythUIButtonList   *m_tagEncoding;
    MythUICheckBox     *m_allowTagWriting;
    MythUIText         *m_helpText;
    MythUIButton       *m_saveButton;
    MythUIButton       *m_cancelButton;

private slots:
    void slotSave(void);
    void slotFocusChanged(void);

};

#endif // GENERALSETTINGS_H
