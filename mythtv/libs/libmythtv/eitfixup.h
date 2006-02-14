/*
 *  Copyright 2004 - Taylor Jacob (rtjacob at earthlink.net)
 */

#ifndef EITFIXUP_H
#define EITFIXUP_H

#include "eit.h"

typedef QMap<uint,uint> QMap_uint_t;

/// EIT Fix Up Functions
class EITFixUp
{
  public:
    void Fix(Event &event, int fix_type);

    void clearSubtitleServiceIDs(void)
        { parseSubtitleServiceIDs.clear(); }
    void addSubtitleServiceID(uint st_pid)
        { parseSubtitleServiceIDs[st_pid] = 1; }

  private:
    void FixStyle1(Event &event);
    void FixStyle2(Event &event);
    void FixStyle3(Event &event);
    void FixStyle4(Event &event);

    /** List of ServiceID's for which to parse out subtitle
     *  from the description. Used in EITFixUpStyle4().
     */
    QMap_uint_t parseSubtitleServiceIDs;
};

#endif // EITFIXUP_H
