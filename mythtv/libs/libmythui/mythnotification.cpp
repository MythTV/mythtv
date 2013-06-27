//
//  mythnotification.cpp
//  MythTV
//
//  Created by Jean-Yves Avenard on 25/06/13.
//  Copyright (c) 2013 Bubblestuff Pty Ltd. All rights reserved.
//

#include "mythnotification.h"

#include <QCoreApplication>

QEvent::Type MythNotification::New =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type MythNotification::Update =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type MythNotification::Info =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type MythNotification::Error =
    (QEvent::Type) QEvent::registerEventType();

