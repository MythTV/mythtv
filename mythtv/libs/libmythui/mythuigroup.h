#ifndef MYTHUI_GROUP_H_
#define MYTHUI_GROUP_H_

#include "mythuicomposite.h"

/** \class MythUIGroup
 *
 * \brief Create a group of widgets.
 * \ingroup MythUI_Widgets
 */
class MUI_PUBLIC MythUIGroup : public MythUIComposite
{
    Q_OBJECT
  public:
    MythUIGroup(MythUIType *parent, const QString &name)
        : MythUIComposite(parent, name) {}
   ~MythUIGroup() override = default;

    void Reset(void) override; // MythUIType

  protected:
    void CopyFrom(MythUIType *base) override; // MythUIType
    void CreateCopy(MythUIType *parent) override; // MythUIType
};

#endif
