#ifndef MARKUPCODES_H
#define MARKUPCODES_H
/*
	markupcodes.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for mdcap markup codes

*/

#define MDCAP_PROTOCOL_VERSION_MAJOR 1
#define MDCAP_PROTOCOL_VERSION_MINOR 0

class MarkupCodes
{

  public:

    static MarkupCodes& theCodes(){static MarkupCodes mc; return mc;}

    //
    //  These are just a bunch of static const's that map from a human
    //  remember'able name into a short symbol
    //


    //
    //  Codes for containers that hold groups of items
    //

    static const char server_info_group = 1;
    
    
    //
    //  Smaller items that implicitly imply groups (by virtue of being of
    //  variable length)
    //
    
    static const char name = 16;
    
    //
    //  Simple types of fixed lengths
    //

    static const char status_code = 32;
    static const char protocol_version = 33;
    

  private:

    MarkupCodes();
    MarkupCodes(const MarkupCodes& rhs);

};

#endif
