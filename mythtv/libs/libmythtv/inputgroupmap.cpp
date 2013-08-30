// -*- Mode: c++ -*-
/*
 *   Copyright (c) Daniel Kristjansson 2007
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <algorithm>

#include "inputgroupmap.h"
#include "mythdb.h"

bool InputGroupMap::Build(void)
{
    bool ok = true;
    inputgroupmap.clear();
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT cardinputid, inputgroupid from inputgroup");
    if (!query.exec())
    {
        MythDB::DBError("InputGroupMap::Build 1", query);
        ok = false;
    }
    else
    {
        while (query.next())
        {
            uint inputid = query.value(0).toUInt();
            uint groupid = query.value(1).toUInt();
            inputgroupmap[inputid].push_back(groupid);
        }
    }

    query.prepare("SELECT cardinputid, cardid from cardinput");
    if (!query.exec())
    {
        MythDB::DBError("InputGroupMap::Build 2", query);
        ok = false;
    }
    else
    {
        while (query.next())
        {
            uint inputid = query.value(0).toUInt();
            uint groupid = query.value(1).toUInt() + 1000;
            if (inputgroupmap[inputid].empty())
                inputgroupmap[inputid].push_back(groupid);
        }
    }

    return ok;
}

uint InputGroupMap::GetSharedInputGroup(uint inputid1, uint inputid2) const
{
    const InputGroupList &input1 = inputgroupmap[inputid1];
    const InputGroupList &input2 = inputgroupmap[inputid2];
    if (input1.empty() || input2.empty())
        return 0;

    InputGroupList::const_iterator it;
    for (it = input1.begin(); it != input1.end(); ++it)
    {
        if (find(input2.begin(), input2.end(), *it) != input2.end())
            return *it;
    }

    return 0;
}
