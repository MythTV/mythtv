/*
	mdcapoutput.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for mdcap output object
	
*/

#include <iostream>
using namespace std;

#include "markupcodes.h"
#include "mdcapoutput.h"

MdcapOutput::MdcapOutput()
{
}

bool MdcapOutput::openGroups()
{
    if(open_groups.count() > 0)
    {
        return true;
    }
    return false;
}

void MdcapOutput::printContents()
{
    for(uint i = 0; i < contents.size(); i++)
    {
        cout << "contents[" << i << "] is " << (int) ((uint8_t) contents[i]) << endl;
    }
}

void MdcapOutput::addServerInfoGroup()
{
    //
    //  Add a server info markup code
    //

    append(MarkupCodes::server_info_group);
    
    //
    //  Leave four empty bytes where we'll store the length of this group
    //  once it is done (ie. when endGroup()) is called. We store where
    //  these 4 bytes begin by pushing an index value onto our open_groups
    //  stack.
    //
    
    open_groups.push(contents.size());
    append((uint32_t) 0);
    
}

void MdcapOutput::addLoginGroup()
{
    //
    //  Add a login group markup code
    //

    append(MarkupCodes::login_group);
    
    //
    //  Leave four empty bytes where we'll store the length of this group
    //  once it is done (ie. when endGroup()) is called. We store where
    //  these 4 bytes begin by pushing an index value onto our open_groups
    //  stack.
    //
    
    open_groups.push(contents.size());
    append((uint32_t) 0);
    
}

void MdcapOutput::addUpdateGroup()
{
    append(MarkupCodes::update_group);
    open_groups.push(contents.size());
    append((uint32_t) 0);
}

void MdcapOutput::addCollectionGroup()
{
    append(MarkupCodes::collection_group);
    open_groups.push(contents.size());
    append((uint32_t) 0);
}

void MdcapOutput::addItemGroup()
{
    append(MarkupCodes::item_group);
    open_groups.push(contents.size());
    append((uint32_t) 0);
}

void MdcapOutput::addAddedItemsGroup()
{
    append(MarkupCodes::added_items_group);
    open_groups.push(contents.size());
    append((uint32_t) 0);
}

void MdcapOutput::addAddedItemGroup()
{
    append(MarkupCodes::added_item_group);
    open_groups.push(contents.size());
    append((uint32_t) 0);
}

void MdcapOutput::addDeletedItemsGroup()
{
    append(MarkupCodes::deleted_items_group);
    open_groups.push(contents.size());
    append((uint32_t) 0);
}

void MdcapOutput::addListGroup()
{
    append(MarkupCodes::list_group);
    open_groups.push(contents.size());
    append((uint32_t) 0);
}

void MdcapOutput::addAddedListsGroup()
{
    append(MarkupCodes::added_lists_group);
    open_groups.push(contents.size());
    append((uint32_t) 0);
}

void MdcapOutput::addListItemGroup()
{
    append(MarkupCodes::added_list_item_group);
    open_groups.push(contents.size());
    append((uint32_t) 0);
}

void MdcapOutput::addListListGroup()
{
    append(MarkupCodes::added_list_list_group);
    open_groups.push(contents.size());
    append((uint32_t) 0);
}

void MdcapOutput::addAddedListGroup()
{
    append(MarkupCodes::added_list_group);
    open_groups.push(contents.size());
    append((uint32_t) 0);
}

void MdcapOutput::addDeletedListsGroup()
{
    append(MarkupCodes::deleted_lists_group);
    open_groups.push(contents.size());
    append((uint32_t) 0);
}

void MdcapOutput::addCommitListGroup()
{
    append(MarkupCodes::commit_list_group);
    open_groups.push(contents.size());
    append((uint32_t) 0);
}

void MdcapOutput::addCommitListGroupList()
{
    append(MarkupCodes::commit_list_group_list);
    open_groups.push(contents.size());
    append((uint32_t) 0);
}

void MdcapOutput::endGroup()
{
    //
    //  Figure out how many bytes have been written since the last group was opened.
    //
    
    uint32_t group_size = ((contents.size() - open_groups.back()) - 4);
    
    //
    //  Mark the group tag in question with the proper size
    //

    contents[open_groups.back() + 0] = ( group_size >> 24) &0xff ;
    contents[open_groups.back() + 1] = ( group_size >> 16) &0xff ;
    contents[open_groups.back() + 2] = ( group_size >>  8) &0xff ;
    contents[open_groups.back() + 3] = ( group_size      ) &0xff ;
    
    //
    //  since we've closed the group, take it off our open stack
    //    

    open_groups.pop();
}
    
void MdcapOutput::addStatus(uint16_t the_status)
{
    append(MarkupCodes::status_code);
    append((uint16_t) the_status);
}

void MdcapOutput::addServiceName(const QString &service_name)
{

    //
    //  Get the name of the service in utf8
    //

    QCString utf8_string = service_name.utf8();
    
    //
    //  Put in a content code for a name
    //

    append(MarkupCodes::name);
    
    //
    //  This content is a string, it obviously requires a length specification
    //
    
    open_groups.push(contents.size());
    append((uint32_t) 0);
    
    //
    //  Add the name in utf8 format
    //
    
    append(utf8_string, utf8_string.length());

    //
    //  Now that the name has been inserted, close out this group
    //  
    
    endGroup();

    
}

void MdcapOutput::addCollectionName(const QString &collection_name)
{

    //
    //  Get the name of the service in utf8
    //

    QCString utf8_string = collection_name.utf8();
    
    //
    //  Put in a content code for a name
    //

    append(MarkupCodes::name);
    
    //
    //  This content is a string, it obviously requires a length specification
    //
    
    open_groups.push(contents.size());
    append((uint32_t) 0);
    
    //
    //  Add the name in utf8 format
    //
    
    append(utf8_string, utf8_string.length());

    //
    //  Now that the name has been inserted, close out this group
    //  
    
    endGroup();

    
}

void MdcapOutput::addListItemName(const QString &list_item_name)
{

    //
    //  A name of an entry on a list
    //

    QCString utf8_string = list_item_name.utf8();
    
    //
    //  Put in a content code
    //

    append(MarkupCodes::list_item_name);
    
    //
    //  This content is a string, it obviously requires a length specification
    //
    
    open_groups.push(contents.size());
    append((uint32_t) 0);
    
    //
    //  Add the name in utf8 format
    //
    
    append(utf8_string, utf8_string.length());

    //
    //  Now that the name has been inserted, close out this group
    //  
    
    endGroup();

    
}

void MdcapOutput::addProtocolVersion()
{
    //
    //  protocol version is always 5 bytes long
    //
    
    append(MarkupCodes::protocol_version);
    append((uint16_t) MDCAP_PROTOCOL_VERSION_MAJOR);
    append((uint16_t) MDCAP_PROTOCOL_VERSION_MINOR);
}

void MdcapOutput::addSessionId(uint32_t session_id)
{
    //
    //  session id is always 5 bytes long
    //
    
    append(MarkupCodes::session_id);
    append((uint32_t) session_id);
}

void MdcapOutput::addCollectionCount(int collection_count)
{
    //
    //  collection count is always 5 bytes long
    //
    
    append(MarkupCodes::collection_count);
    append((uint32_t) collection_count);
}

void MdcapOutput::addCollectionId(int collection_id)
{
    //
    //  collection id is always 5 bytes long
    //
    
    append(MarkupCodes::collection_id);
    append((uint32_t) collection_id);
}

void MdcapOutput::addCollectionType(int collection_type)
{
    //
    //  collection type is always 5 bytes long
    //
    
    append(MarkupCodes::collection_type);
    append((uint32_t) collection_type);
}

void MdcapOutput::addCollectionEditable(bool yes_or_no)
{
    //
    //  Editable is 2 bytes
    //

    append(MarkupCodes::collection_is_editable);
    if(yes_or_no)
    {
        append((uint8_t) 1);
    }    
    else
    {
        append((uint8_t) 0);
    }
}

void MdcapOutput::addCollectionRipable(bool yes_or_no)
{
    //
    //  Ripable is 2 bytes
    //

    append(MarkupCodes::collection_is_ripable);
    if(yes_or_no)
    {
        append((uint8_t) 1);
    }    
    else
    {
        append((uint8_t) 0);
    }
}

void MdcapOutput::addCollectionGeneration(int collection_generation)
{
    //
    //  collection generation is always 5 bytes long
    //
    
    append(MarkupCodes::collection_generation);
    append((uint32_t) collection_generation);
}

void MdcapOutput::addUpdateType(bool full_or_not)
{
    //
    //  Update type is 2 bytes
    //

    append(MarkupCodes::update_type);
    if(full_or_not)
    {
        append((uint8_t) 1);
    }    
    else
    {
        append((uint8_t) 0);
    }
}

void MdcapOutput::addTotalItems(uint count)
{
    //
    //  Total items is 5 bytes
    //  1 - markup code
    //  4 = 32 bit unsigned int
    
    append(MarkupCodes::total_items);
    append((uint32_t) count);
}

void MdcapOutput::addAddedItems(uint count)
{
    //
    //  Added items is 5 bytes
    //  1 - markup code
    //  4 = 32 bit unsigned int
    
    append(MarkupCodes::added_items);
    append((uint32_t) count);
}

void MdcapOutput::addDeletedItems(uint count)
{
    //
    //  Deleted items is 5 bytes
    //  1 - markup code
    //  4 = 32 bit unsigned int
    
    append(MarkupCodes::deleted_items);
    append((uint32_t) count);
}

void MdcapOutput::addDeletedItem(uint item_id)
{
    //
    //  Deleted items is 5 bytes
    //  1 - markup code
    //  4 = 32 bit id of deleted item
    
    append(MarkupCodes::deleted_item);
    append((uint32_t) item_id);
}

void MdcapOutput::addItemType(int item_type)
{
    //
    //  item type
    //  1 - markup code
    //  1 = 8 bit type
    
    if(item_type < 1)
    {
        cerr << "mdcapoutput.o: sending out a negative item id, not good" << endl;
    }
    
    append(MarkupCodes::item_type);
    append((uint8_t) item_type);
}



void MdcapOutput::addItemId(int item_id)
{
    //
    //  item id
    //  1 - markup code
    //  4 = 32 bit unsigned int
    
    if(item_id < 1)
    {
        cerr << "mdcapoutput.o: sending out a negative item id, not good" << endl;
    }
    
    append(MarkupCodes::item_id);
    append((uint32_t) item_id);
}

void MdcapOutput::addItemUrl(const QString &item_url)
{
    QCString utf8_string = item_url.utf8();
    append(MarkupCodes::item_url);
    open_groups.push(contents.size());
    append((uint32_t) 0);
    append(utf8_string, utf8_string.length());
    endGroup();
}

void MdcapOutput::addItemRating(int item_rating)
{
    append(MarkupCodes::item_rating);
    append((uint8_t) item_rating);
}

void MdcapOutput::addItemLastPlayed(uint item_last_played)
{
    append(MarkupCodes::item_last_played);
    append((uint32_t) item_last_played);
}

void MdcapOutput::addItemPlayCount(int item_play_count)
{
    append(MarkupCodes::item_play_count);
    append((uint32_t) item_play_count);
}

void MdcapOutput::addItemArtist(const QString &item_artist)
{
    QCString utf8_string = item_artist.utf8();
    append(MarkupCodes::item_artist);
    open_groups.push(contents.size());
    append((uint32_t) 0);
    append(utf8_string, utf8_string.length());
    endGroup();
}

void MdcapOutput::addItemAlbum(const QString &item_album)
{
    QCString utf8_string = item_album.utf8();
    append(MarkupCodes::item_album);
    open_groups.push(contents.size());
    append((uint32_t) 0);
    append(utf8_string, utf8_string.length());
    endGroup();
}

void MdcapOutput::addItemTitle(const QString &item_title)
{
    QCString utf8_string = item_title.utf8();
    append(MarkupCodes::item_title);
    open_groups.push(contents.size());
    append((uint32_t) 0);
    append(utf8_string, utf8_string.length());
    endGroup();
}

void MdcapOutput::addItemGenre(const QString &item_genre)
{
    QCString utf8_string = item_genre.utf8();
    append(MarkupCodes::item_genre);
    open_groups.push(contents.size());
    append((uint32_t) 0);
    append(utf8_string, utf8_string.length());
    endGroup();
}

void MdcapOutput::addItemYear(int item_year)
{
    append(MarkupCodes::item_year);
    append((uint32_t) item_year);
}

void MdcapOutput::addItemTrack(int item_track)
{
    append(MarkupCodes::item_track);
    append((uint32_t) item_track);
}

void MdcapOutput::addItemLength(int item_length)
{
    append(MarkupCodes::item_length);
    append((uint32_t) item_length);
}

void MdcapOutput::addItemDupFlag(bool true_or_false)
{
    append(MarkupCodes::item_dup_flag);
    if(true_or_false)
    {
        append((uint8_t) 1);
    }    
    else
    {
        append((uint8_t) 0);
    }
}

void MdcapOutput::addCommitListType(bool new_or_not)
{
    //
    //  Update type is 2 bytes
    //

    append(MarkupCodes::commit_list_type);
    if(new_or_not)
    {
        append((uint8_t) 1);
    }    
    else
    {
        append((uint8_t) 0);
    }
}


void MdcapOutput::addListId(int list_id)
{
    //
    //  list id
    //  1 - markup code
    //  4 = 32 bit unsigned int
    
    if(list_id < 1)
    {
        cerr << "mdcapoutput.o: sending out a negative list id, not good" << endl;
    }
    
    append(MarkupCodes::list_id);
    append((uint32_t) list_id);
}

void MdcapOutput::addListName(const QString &list_name)
{
    QCString utf8_string = list_name.utf8();
    append(MarkupCodes::list_name);
    open_groups.push(contents.size());
    append((uint32_t) 0);
    append(utf8_string, utf8_string.length());
    endGroup();
}

void MdcapOutput::addListItem(int list_item)
{
    //
    //  list item
    //  1 - markup code
    //  4 = 32 bit unsigned int
    
    append(MarkupCodes::list_item);
    append((uint32_t) list_item);
}

void MdcapOutput::addDeletedList(int list_id)
{
    //
    //  deleted list
    //  1 - markup code
    //  4 = 32 bit id of the list that has been deleted
    //
        
    append(MarkupCodes::deleted_list);
    append((uint32_t) list_id);
}

void MdcapOutput::addListEditable(bool yes_or_no)
{
    //
    //  Editable is 2 bytes
    //

    append(MarkupCodes::list_is_editable);
    if(yes_or_no)
    {
        append((uint8_t) 1);
    }    
    else
    {
        append((uint8_t) 0);
    }
}



void MdcapOutput::append(char a_char)
{
    contents.push_back(a_char);
}

void MdcapOutput::append(const char *a_string, uint length)
{
    for(uint i = 0; i < length; i++)
    {
        append(a_string[i]);
    }
}

void MdcapOutput::append(uint8_t a_u8)
{
    contents.push_back(a_u8);
}

void MdcapOutput::append(uint16_t a_u16)
{
    contents.push_back(( a_u16 >>  8 ) &0xff );
    contents.push_back(( a_u16       ) &0xff );
}

void MdcapOutput::append(uint32_t a_u32)
{
    contents.push_back(( a_u32 >> 24 ) &0xff );
    contents.push_back(( a_u32 >> 16 ) &0xff );
    contents.push_back(( a_u32 >>  8 ) &0xff );
    contents.push_back(( a_u32       ) &0xff );
}

MdcapOutput::~MdcapOutput()
{

}


