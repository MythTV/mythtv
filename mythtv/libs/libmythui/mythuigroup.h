#ifndef MYTHUI_GROUP_H_
#define MYTHUI_GROUP_H_

#include "mythuitype.h"

/** \class MythUIGroup
 *
 * \brief Create a group of widgets.
 * \ingroup MythUI_Widgets
 */
class MPUBLIC MythUIGroup : public MythUIType
{

  public:
    MythUIGroup(MythUIType *parent, const QString &name);
   ~MythUIGroup();

    void Reset(void);

  protected:
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);
};

#endif
