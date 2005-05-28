#ifndef MDCAPOUTPUT_H
#define MDCAPOUTPUT_H
/*
	mdcapoutput.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for mdcap output object

*/

#include <stdint.h>

#include <qstring.h>
#include <qvaluevector.h>
#include <qvaluestack.h>

class MdcapOutput
{

  public:

    MdcapOutput();
    ~MdcapOutput();

    bool openGroups();
    QValueVector<char>* getContents(){return &contents;}

    //
    //  Debugging
    //
    
    void printContents();
    
    //
    //  Group functions
    //

    void addServerInfoGroup();
    void addLoginGroup();
    void addUpdateGroup();
    void addCollectionGroup();

    void addItemGroup();
    void addAddedItemsGroup();
    void addAddedItemGroup();
    void addDeletedItemsGroup();

    void addListGroup();
    void addAddedListsGroup();
    void addAddedListGroup();
    void addListItemGroup();
    void addListListGroup();
    void addDeletedListsGroup();

    void addCommitListGroup();
    void addCommitListGroupList();

    void endGroup();
    
    //
    //  Simple type, but imply groups (eg. strings)
    //

    void addServiceName(const QString &service_name);
    void addCollectionName(const QString &collection_name);
    void addListItemName(const QString &list_item_name);

    //
    //  "element" functions
    //

    void addStatus(uint16_t the_status);
    void addProtocolVersion();
    void addSessionId(uint32_t session_id);
    void addCollectionCount(int collection_count);
    void addCollectionId(int collection_id);
    void addCollectionType(int collection_type);
    void addCollectionEditable(bool yes_or_no);
    void addCollectionRipable(bool yes_or_no);
    void addCollectionBeingRipped(bool yes_or_no);
    void addCollectionGeneration(int collection_generation);    
    void addUpdateType(bool full_or_not);
    void addTotalItems(uint count);
    void addAddedItems(uint count);
    void addDeletedItems(uint count);
    void addDeletedItem(uint item_id);

    //
    //  "element" functions for items
    //

    void addItemType(int item_type);
    void addItemId(int item_id);
    void addItemUrl(const QString &item_url);
    void addItemRating(int item_rating);
    void addItemLastPlayed(uint item_last_played);
    void addItemPlayCount(int item_play_count);
    void addItemArtist(const QString &item_artist);
    void addItemAlbum(const QString &item_album);
    void addItemTitle(const QString &item_title);
    void addItemGenre(const QString &item_genre);
    void addItemYear(int item_year);
    void addItemTrack(int item_track);
    void addItemLength(int item_length); 
    void addItemDupFlag(bool true_or_false);

    //
    //  Stuff for commits
    //

    void addCommitListType(bool new_or_not);

    //
    //  "element" functions for lists
    //
    
    void addListId(int list_id);
    void addListName(const QString &list_name);
    void addListItem(int list_item);
    void addDeletedList(int list_id);
    void addListEditable(bool yes_or_no);

  private:

    //
    //  Basic ways to squeeze different variable types into our single
    //  vector of char's
    //

    void append(char     a_char);
    void append(const char *a_string, uint length);
    void append(uint8_t  a_u8);
    void append(uint16_t a_u16);
    void append(uint32_t a_u32);
    
  
    QValueVector<char> contents;
    QValueStack<uint32_t>   open_groups;

};

#endif
