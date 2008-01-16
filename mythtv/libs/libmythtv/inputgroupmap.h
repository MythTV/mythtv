// -*- Mode: c++ -*-
#ifndef _INPUTGROUPMAP_H_
#define _INPUTGROUPMAP_H_

// C++ headers
#include <vector>
using namespace std;

// Qt headers
#include <qmap.h>

// MythTV headers
#include "mythexp.h"

typedef vector<uint> InputGroupList;

class MPUBLIC InputGroupMap
{
  public:
    InputGroupMap() { Build(); }

    bool Build(void);
    uint GetSharedInputGroup(uint input1, uint input2) const;

  private:
    QMap<uint, InputGroupList> inputgroupmap;
};

#endif // _INPUTGROUPMAP_H_
