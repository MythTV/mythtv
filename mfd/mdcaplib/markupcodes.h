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

    static const char server_info_group     = 1;
    static const char login_group           = 2;
    static const char update_group          = 3;
    static const char collection_group      = 4;

    static const char item_group            = 5;
    static const char added_items_group     = 6;
    static const char added_item_group      = 7;
    static const char added_list_item_group = 8;
    static const char deleted_items_group   = 9;

    static const char list_group            = 10;
    static const char added_lists_group     = 11;
    static const char deleted_lists_group   = 12;
    static const char added_list_group      = 13;
    
    //
    //  Smaller items that implicitly imply groups (by virtue of being of
    //  variable length)
    //
    
    static const char name                  = 16;
    static const char item_url              = 17;
    static const char item_artist           = 18;
    static const char item_album            = 19;
    static const char item_title            = 20;
    static const char item_genre            = 21;
    static const char list_name             = 22;
    static const char list_item_name        = 23;    
    
    //
    //  Simple types of fixed lengths
    //

    static const char status_code           = 32;
    static const char protocol_version      = 33;
    static const char session_id            = 34;
    static const char collection_count      = 35;
    static const char collection_id         = 36;
    static const char collection_type       = 37;
    static const char collection_generation = 38;
    static const char update_type           = 39;
    static const char total_items           = 40;
    static const char added_items           = 41;
    static const char deleted_items         = 42;
    static const char deleted_item          = 43;
    static const char item_type             = 44;
    static const char item_id               = 45;
    static const char item_rating           = 46;
    static const char item_last_played      = 47;
    static const char item_play_count       = 48;
    static const char item_year             = 49;
    static const char item_track            = 50;
    static const char item_length           = 51;
    static const char list_id               = 52;
    static const char list_item             = 53;
    static const char deleted_list          = 54;    

  private:

    MarkupCodes();
    MarkupCodes(const MarkupCodes& rhs);

};

#endif
