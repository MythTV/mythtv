#ifndef MYTHTV_MYTHUICOMPOSITE_H
#define MYTHTV_MYTHUICOMPOSITE_H

#include "mythuitype.h"
#include "mythtypes.h"

class MUI_PUBLIC MythUIComposite : public MythUIType
{
    Q_OBJECT
public:
    MythUIComposite(QObject *parent, const QString &name);
    ~MythUIComposite() override = default;

    virtual void SetTextFromMap(const InfoMap &infoMap);
    virtual void ResetMap(const InfoMap &infoMap);
};

#endif /* defined(MYTHTV_MYTHUICOMPOSITE_H) */
