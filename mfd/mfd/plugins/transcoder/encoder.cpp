#include <iostream>

#include "metadata.h"
#include "encoder.h"

#include <mythtv/mythcontext.h> 

using namespace std;

Encoder::Encoder(const QString &outfile, int qualitylevel, AudioMetadata *metadata) 
{
    if (outfile) 
    {
        out = fopen(outfile.local8Bit(), "w");
        if (!out) 
            VERBOSE(VB_GENERAL, QString("Error opening output file: %1").arg(outfile.local8Bit()));
    } 
    else 
    {
        out = NULL;
    }

    this->outfile = &outfile;
    this->quality = qualitylevel;
    if(metadata)
    {
        this->metadata = metadata;
    }
    else
    {
        metadata = NULL;
    }
}

Encoder::~Encoder()
{
    if(out) 
       fclose(out);
}

