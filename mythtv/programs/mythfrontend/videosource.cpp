#include "videosource.h"
#include <qsqldatabase.h>
#include <qcursor.h>
#include <qlayout.h>
#include <iostream>

QString VSSetting::whereClause(void) {
  return QString("sourceid = %1").arg(parent.getSourceID());
}

QString VSSetting::setClause(void) {
  return QString("sourceid = %1, %2 = '%3'")
    .arg(parent.getSourceID())
    .arg(getColumn())
    .arg(getValue());
}

QString CaptureCard::CCSetting::whereClause(void) {
  return QString("cardid = %1").arg(parent.getCardID());
}

QString CaptureCard::CCSetting::setClause(void) {
  return QString("cardid = %1, %2 = '%3'")
    .arg(parent.getCardID())
    .arg(getColumn())
    .arg(getValue());
}
