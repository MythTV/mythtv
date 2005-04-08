/* ============================================================
 * File  : settings.cpp
 * Author: J. Donavan Stanley <jdonavan@jdonavan.net>
 *
 * Abstract: This file includes the implementation for the
 *           configuration wizard.
 *
 *
 * Copyright 2005 by J. Donavan Stanley
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published bythe Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ============================================================ */

#include "settings.h"
 

static GlobalLineEdit *UserName()
{
    GlobalLineEdit *ge = new GlobalLineEdit("MMTUser");
    ge->setLabel(QObject::tr("Username"));
    ge->setValue("");
    ge->setHelpText(QObject::tr("Enter the username you use to access movie information."));
    return ge;
}


static GlobalLineEdit *Password()
{
    GlobalLineEdit *ge = new GlobalLineEdit("MMTPass");
    ge->setLabel(QObject::tr("Password"));
    ge->setValue("");
    ge->setHelpText(QObject::tr("Enter the password you use to access movie information."));
    return ge;
}

static GlobalLineEdit *Postal()
{
    GlobalLineEdit *ge = new GlobalLineEdit("PostalCode");
    ge->setLabel(QObject::tr("Postal Code"));
    ge->setValue("");
    ge->setHelpText(QObject::tr("Enter your postal(zip) code."));
    return ge;
}

static GlobalSlider *DefaultRadius()
{
    GlobalSlider *gs = new GlobalSlider("MMTRadius", 1, 100, 1);
    gs->setLabel(QObject::tr("Radius"));
    gs->setValue(25);
    gs->setHelpText(QObject::tr("How far out from your postal code Myth should look for theatres."));
    return gs;
}

static GlobalSlider *DefaultDays()
{
    GlobalSlider *gs = new GlobalSlider("MMTDays", 1, 14, 1);
    gs->setLabel(QObject::tr("Days"));
    gs->setValue(7);
    gs->setHelpText(QObject::tr("How far many days of movie data Myth should fetch."));
    return gs;
}



MMTGeneralSettings::MMTGeneralSettings()
{
    VerticalConfigurationGroup* auth = new VerticalConfigurationGroup(false);
    auth->setLabel(QObject::tr("Authentication Settings"));
    auth->addChild(UserName());
    auth->addChild(Password());
    addChild(auth);
    
    VerticalConfigurationGroup* gen = new VerticalConfigurationGroup(false);
    gen->setLabel(QObject::tr("General Settings"));
    gen->addChild(Postal());
    gen->addChild(DefaultRadius());
    gen->addChild(DefaultDays());
    addChild(gen);
    
    gContext->SetSetting("MMT-Last-Fetch", "");
}



// EOF
