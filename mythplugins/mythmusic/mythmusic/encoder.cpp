#include <iostream>

#include "metadata.h"
#include "encoder.h"

using namespace std;

Encoder::Encoder(const QString &outfile, int qualitylevel, Metadata *metadata) 
{
    if (outfile) 
    {
        out = fopen(outfile.ascii(), "w");
        if (!out) 
            cout << "ERROR opening output file " << outfile << "\n";
    } 
    else 
        out = NULL;

    this->outfile = &outfile;
    this->quality = qualitylevel;
    this->metadata = metadata;
}

Encoder::~Encoder()
{
    if(out) 
       fclose(out);
}

