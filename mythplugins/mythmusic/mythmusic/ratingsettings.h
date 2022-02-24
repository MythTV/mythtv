#ifndef RATINGSETTINGS_H
#define RATINGSETTINGS_H

#include <libmythui/mythscreentype.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuispinbox.h>


class RatingSettings : public MythScreenType
{
    Q_OBJECT
public:
    explicit RatingSettings(MythScreenStack *parent, const char *name = nullptr)
        : MythScreenType(parent, name) {}
    ~RatingSettings() override = default;

    bool Create(void) override; // MythScreenType

private:
    MythUISpinBox      *m_ratingWeight    {nullptr};
    MythUISpinBox      *m_playCountWeight {nullptr};
    MythUISpinBox      *m_lastPlayWeight  {nullptr};
    MythUISpinBox      *m_randomWeight    {nullptr};
    MythUIButton       *m_saveButton      {nullptr};
    MythUIButton       *m_cancelButton    {nullptr};

private slots:
    void slotSave(void);
};

#endif // RATINGSETTINGS_H
