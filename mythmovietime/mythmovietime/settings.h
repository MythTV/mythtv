/* ============================================================
 * File  : settings.h
 * Author: J. Donavan Stanley <jdonavan@jdonavan.net>
 *
 * Abstract: This file includes the definition for the
 *           configuration wizard.
 *
 *
 * Copyright 2005 by J. Donavan Stanley
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ============================================================ */


#ifndef MMMTSETTINGS_H
#define MMMTSETTINGS_H

#include "mythtv/settings.h"
#include "mythtv/mythcontext.h"

class MMTGeneralSettings: virtual public ConfigurationWizard {
public:
    MMTGeneralSettings();
};


#endif

