// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef DVBSTREAMDATA_H_
#define DVBSTREAMDATA_H_

#include "mpegstreamdata.h"

class PSIPTable;
class NetworkInformationTable;
class ServiceDescriptionTable;

class DVBStreamData : public MPEGStreamData
{
    Q_OBJECT
  public:
    DVBStreamData(bool cacheTables = false) :
        MPEGStreamData(-1, cacheTables) { ; }

    void HandleTables(const TSPacket* tspacket);
    bool IsRedundant(const PSIPTable&) const;

  signals:
    void UpdateNIT(const NetworkInformationTable*);
    void UpdateSDT(const ServiceDescriptionTable*);

  private slots:
    void PrintNIT(const NetworkInformationTable*) const;
    void PrintSDT(const ServiceDescriptionTable*) const;

  private:

};

#endif // DVBSTREAMDATA_H_
