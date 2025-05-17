// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//
#include "output.h"

const QEvent::Type OutputEvent::kPlaying =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type OutputEvent::kBuffering =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type OutputEvent::kInfo =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type OutputEvent::kPaused =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type OutputEvent::kStopped =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type OutputEvent::kError =
    (QEvent::Type) QEvent::registerEventType();

