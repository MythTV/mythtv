#ifndef RATINGSETTINGS_H
#define RATINGSETTINGS_H

#include <mythscreentype.h>
#include <mythuispinbox.h>
#include <mythuibutton.h>


class RatingSettings : public MythScreenType
{
    Q_OBJECT
public:
    RatingSettings(MythScreenStack *parent, const char *name = 0);
    ~RatingSettings();

    bool Create(void);

private:
    MythUISpinBox      *m_ratingWeight;
    MythUISpinBox      *m_playCountWeight;
    MythUISpinBox      *m_lastPlayWeight;
    MythUISpinBox      *m_randomWeight;
    MythUIText         *m_helpText;
    MythUIButton       *m_saveButton;
    MythUIButton       *m_cancelButton;

private slots:
    void slotSave(void);
    void slotFocusChanged(void);
};

#endif // RATINGSETTINGS_H
