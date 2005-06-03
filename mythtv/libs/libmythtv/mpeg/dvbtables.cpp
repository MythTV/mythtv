// -*- Mode: c++ -*-
// Copyright (c) 2005, Daniel Thor Kristjansson
#include "dvbtables.h"

void NetworkInformationTable::Parse(void) const
{
    _ptrs.clear();
}

QString NetworkInformationTable::toString(void) const
{
    return "NIT";
}

void ServiceDescriptionTable::Parse(void) const
{
    _ptrs.clear();
}

QString ServiceDescriptionTable::toString(void) const
{
    return "SDT";
}
