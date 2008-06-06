#ifndef MYTHUI_GROUP_H_
#define MYTHUI_GROUP_H_

#include "mythuitype.h"

/** \class MythUIGroup
 *
 *  \brief Create a group of widgets.
 *
 */
class MythUIGroup : public MythUIType
{

 public:
   MythUIGroup(MythUIType *parent, const char *name);
  ~MythUIGroup();

  protected:
    virtual bool ParseElement(QDomElement &element);
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);

};

#endif
