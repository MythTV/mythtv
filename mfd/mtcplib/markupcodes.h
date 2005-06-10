#ifndef MTCP_MARKUPCODES_H
#define MTCP_MARKUPCODES_H
/*
	markupcodes.h

	(c) 2003, 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for mtcp markup codes

*/

#define MTCP_PROTOCOL_VERSION_MAJOR 1
#define MTCP_PROTOCOL_VERSION_MINOR 0

class MtcpMarkupCodes
{

  public:

    static MtcpMarkupCodes& theCodes(){static MtcpMarkupCodes mc; return mc;}

    //
    //  These are just a bunch of static const's that map from a human
    //  remember'able name into a short symbol
    //


    //
    //  Codes for containers that hold groups of items
    //

    static const char server_info_group       = 1;
    static const char job_info_group          = 2;
    static const char job_group               = 3;

    //
    //  Smaller items that implicitly imply groups (by virtue of being of
    //  variable length)
    //
    
    static const char name                    = 50;
    static const char job_major_description   = 51;
    static const char job_minor_description   = 52;

    //
    //  Simple types of fixed lengths
    //

    static const char protocol_version        = 75;
    static const char job_count               = 76;
    static const char job_id                  = 77;
    static const char job_major_progress      = 78;
    static const char job_minor_progress      = 79;

  private:

    MtcpMarkupCodes();
    MtcpMarkupCodes(const MtcpMarkupCodes& rhs);

};

#endif
