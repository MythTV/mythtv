/*
	mythdigest.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for a little object to calculate persistent content id's
	(checksums)

*/

#include <iostream>
using namespace std;

#include <openssl/evp.h>

#include <qfile.h>

#include "mythdigest.h"

MythDigest::MythDigest(const QString &a_file_name)
{
    file_name = a_file_name;
};


QString MythDigest::calculate()
{
    //
    //  Bit of an odd algorithm. 
    //
    //  This code forms a "message" out the first 4096 bytes of the file + a
    //  string representation of the size of the file + the last 4096 bytes
    //  of the file. If the file is smaller than 8192 bytes, then the md5 is
    //  done on the whole file (and file length is ignored).
    //    
    //  In then uses standard ssl (openssl.org) routines to get the md5
    //  checksum of that 4096 + length + 4096 byte message.
    //

    QFile the_file(file_name);
    if(!the_file.exists())
    {
        return QString("");
    }    
    
    if(!the_file.open(IO_ReadOnly))
    {
        return QString("");
    }



    uint64_t file_size = the_file.size();
    if(file_size < 8192)
    {
        //
    }
    else
    {
        char first_four_kbytes[4096];
        char last_four_kbytes[4096];

        //
        //  Get the first 4096 bytes
        //

        if(!the_file.at(0))
        {
            the_file.close();
            return QString("");
        }
        if(the_file.readBlock(first_four_kbytes, 4096) != 4096)
        {
            the_file.close();
            return QString("");
        }
        
        //
        //  Get the last 4096 bytes
        //

        if(!the_file.at(file_size - 4096))
        {
            the_file.close();
            return QString("");
        }
        if(the_file.readBlock(last_four_kbytes, 4096) != 4096)
        {
            the_file.close();
            return QString("");
        }
        
        //
        //  Get string representatin of file size
        //
         
        QString file_size_string = QString("%1").arg(file_size);

        cout << "file_size_string is " << file_size_string.ascii() << endl;

        //
        //  Calculate the digest (md5, from openssl)
        //
        
        EVP_MD_CTX mdctx;
        const EVP_MD *md;
        unsigned char md_value[EVP_MAX_MD_SIZE];
        uint md_len;

        OpenSSL_add_all_digests();
        md = EVP_get_digestbyname("md5");
        EVP_MD_CTX_init(&mdctx);
        EVP_DigestInit_ex(&mdctx, md, NULL);
        EVP_DigestUpdate(&mdctx, first_four_kbytes, 4096);
        EVP_DigestUpdate(&mdctx, file_size_string.ascii(), strlen(file_size_string.ascii()));
        EVP_DigestUpdate(&mdctx, last_four_kbytes, 4096);
        EVP_DigestFinal_ex(&mdctx, md_value, &md_len);
        EVP_MD_CTX_cleanup(&mdctx);

        printf("Digest is: ");
        for(uint i = 0; i < md_len; i++) printf("%02x", md_value[i]);
        printf("\n");
            
    }

    
    

    the_file.close();
    return QString("");
}



MythDigest::~MythDigest()
{
}

