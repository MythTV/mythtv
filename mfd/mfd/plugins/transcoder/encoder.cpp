#include <iostream>

#include "metadata.h"
#include "encoder.h"

#include <mythtv/mythcontext.h> 

using namespace std;

Encoder::Encoder(const QString &l_outfile, int qualitylevel, AudioMetadata *l_metadata) 
{
    if (l_outfile) 
    {
        out = fopen(l_outfile.local8Bit(), "w");
        if (!out) 
        {
            warning(QString("Error opening output file: %1").arg(l_outfile.local8Bit()));
        }
    } 
    else 
    {
        out = NULL;
    }

    outfile = l_outfile;
    quality = qualitylevel;
    if(l_metadata)
    {
        metadata = l_metadata;
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

