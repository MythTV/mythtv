#ifndef MARKUPCODES_H
#define MARKUPCODES_H
/*
	markupcodes.h

	(c) 2003, 2004 Thor Sigvaldason and Isaac Richards
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

    static const char server_info_group       = 1;
    static const char login_group             = 2;
    static const char update_group            = 3;
    static const char collection_group        = 4;

    static const char item_group              = 5;
    static const char added_items_group       = 6;
    static const char added_item_group        = 7;
    static const char added_list_item_group   = 8;
    static const char added_list_list_group   = 9;
    static const char deleted_items_group     = 10;

    static const char list_group              = 11;
    static const char added_lists_group       = 12;
    static const char deleted_lists_group     = 13;
    static const char added_list_group        = 14;
    
    static const char commit_list_group       = 15;
    static const char commit_list_group_list  = 16;    
    //
    //  Smaller items that implicitly imply groups (by virtue of being of
    //  variable length)
    //
    
    static const char name                    = 50;
    static const char item_url                = 51;
    static const char item_artist             = 52;
    static const char item_album              = 53;
    static const char item_title              = 54;
    static const char item_genre              = 55;
    static const char list_name               = 56;
    static const char list_item_name          = 57;    
    
    //
    //  Simple types of fixed lengths
    //

    static const char status_code             = 75;
    static const char protocol_version        = 76;
    static const char session_id              = 77;
    static const char collection_count        = 78;
    static const char collection_id           = 79;
    static const char collection_type         = 80;
    static const char collection_is_editable  = 81;
    static const char collection_is_ripable   = 82;
    static const char collection_being_ripped = 83;
    static const char collection_generation   = 84;
    static const char update_type             = 85;
    static const char total_items             = 86;
    static const char added_items             = 87;
    static const char deleted_items           = 88;
    static const char deleted_item            = 89;
    static const char item_type               = 90;
    static const char item_id                 = 91;
    static const char item_rating             = 92;
    static const char item_last_played        = 93;
    static const char item_play_count         = 94;
    static const char item_year               = 95;
    static const char item_track              = 96;
    static const char item_length             = 97;
    static const char item_dup_flag           = 98;
    static const char list_id                 = 99;
    static const char list_item               = 100;
    static const char list_is_editable        = 101;
    static const char list_is_ripable         = 102;
    static const char list_being_ripped       = 103;
    static const char deleted_list            = 104;
    static const char commit_list_type        = 105;  // new or edit of existing

  private:

    MarkupCodes();
    MarkupCodes(const MarkupCodes& rhs);

};

#endif
