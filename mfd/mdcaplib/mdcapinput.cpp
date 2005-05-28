/*
	mdcapinput.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for mdcap input object

*/

#include <iostream>
using namespace std;

#include "markupcodes.h"
#include "mdcapinput.h"


MdcapInput::MdcapInput(std::vector<char> *raw_data)
{
    std::vector<char>::iterator it;
    for(it = raw_data->begin(); it != raw_data->end(); )
    {
        contents.append((*it));
        ++it;
    }
    pos = 0;
}

MdcapInput::MdcapInput(QValueVector<char> *raw_data)
{
    contents = QValueVector<char>(*raw_data);
    pos = 0;
}

char MdcapInput::peekAtNextCode()
{
    if(amountLeft() > 0)
    {
        return contents[pos];
    }
    cerr << "mdcapinput.o: asked to peek at next code, but there's nothing "
         << "left to peek at" 
         << endl;

    return 0;
}

uint MdcapInput::amountLeft()
{
    int amount_left = contents.size() - pos;
    if(amount_left < 0)
    {
        cerr << "mdcapinput.o: there is less than nothing left "
             << "of my contents"
             << endl;

        return 0;
    }
    
    return (uint) amount_left;
}

char MdcapInput::popGroup(QValueVector<char> *group_contents)
{
    group_contents->clear();
    int return_value = 0;
    if(amountLeft() < 1)
    {
        return return_value;
    }
    
    return_value = contents[pos];
    
    //
    //  find out how many bytes this group consists of
    //
    
    
    if(amountLeft() < 5)
    {
        cerr << "mdcapinput.o: asked to pop a group, but there are no "
             << "size bytes in the stream " 
             << endl;
        return 0;
    }

    uint32_t group_size  = ((int) ((uint8_t) contents[pos + 4]));
             group_size += ((int) ((uint8_t) contents[pos + 3])) * 256;
             group_size += ((int) ((uint8_t) contents[pos + 2])) * 256 * 256;
             group_size += ((int) ((uint8_t) contents[pos + 1])) * 256 * 256 * 256;
             
    //
    //  Make sure there are at least as many bytes available as the group
    //  size calculation thinks there should be
    //

    if(amountLeft() < group_size + 5)
    {
        cerr << "mdcapinput.o: there are not enough bytes in the stream "
             << "to get as many as the group size code says there should "
             << "be"
             << endl;
        return 0;
    }

    
    //
    //  reserve capacity in the destination vector to hold the bytes, then
    //  copy them in
    //

    group_contents->reserve(group_size);
    for(uint i = 0; i < group_size; i++)
    {
        group_contents->append(contents[5 + i + pos]);
    }

    pos = pos + 5 + group_size;
    return return_value;
}

char MdcapInput::popByte()
{
    //
    //  Pull one char/byte of the front of the contents
    //
    
    if(amountLeft() < 1)
    {
        cerr << "mdcapinput.o: asked to do a popByte(), but the "
             << "content is empty"
             << endl;

        return 0;
    }

    char return_value = contents[pos];

    ++pos;
    return return_value;
    
}

uint16_t MdcapInput::popU16()
{
    uint8_t value_bigendian    = popByte();
    uint8_t value_littleendian = popByte();
    
    uint16_t result = (value_bigendian * 256) + value_littleendian;
    
    return result;
}


uint32_t MdcapInput::popU32()
{
    uint8_t byte_one   = popByte();
    uint8_t byte_two   = popByte();
    uint8_t byte_three = popByte();
    uint8_t byte_four  = popByte();
    
    uint32_t result =   (byte_one   * 256 * 256 * 256) 
                      + (byte_two   * 256 * 256)
                      + (byte_three * 256)    
                      + (byte_four);
    return result;
}


QString MdcapInput::popName()
{
    QValueVector<char> name_string_vector;
    char content_code = popGroup(&name_string_vector);
    
    if(content_code != MarkupCodes::name)
    {
        cerr << "mdcapinput.o: asked to do popName(), but this "
             << "doesn't look like a name"
             << endl;
        return QString("");
    }
    
    
    char *utf8_name = new char [name_string_vector.size() + 1];
    for(uint i = 0; i < name_string_vector.size(); i++)
    {
        utf8_name[i] = name_string_vector[i];
    }
    utf8_name[name_string_vector.size()] = '\0';
    
    QString the_name = QString::fromUtf8(utf8_name);
    
    delete [] utf8_name;
    return the_name;
}

int MdcapInput::popStatus()
{
    //
    //  Status is always 3 bytes
    //  
    //  1st byte - status markup code
    //    next 2 - 16 bit unsigned integer
    //
    
    if(amountLeft() < 3)
    {
        cerr << "mdcapinput.o: asked to popStatus(), but there are not "
             << "enough bytes left in the stream "
             << endl;
        return 0;
    }

    char content_code = popByte();
    if(content_code != MarkupCodes::status_code)
    {
        cerr << "mdcapinput.o: asked to popStatus(), but content code is "
             << "not status_code "
             << endl;
        return 0;       
    }

    uint status = popU16();
    
    return (int) status;
}

void MdcapInput::popProtocol(int *major, int *minor)
{
    //
    //  Protocol is always 5 bytes
    //  1st byte - status markup code
    //    next 2 - 16 bit major
    //    next 2 - 16 bit minor
    //
    
    if(amountLeft() < 5)
    {
        cerr << "mdcapinput.o: asked to popProtocol(), but there are not "
             << "enough bytes left in the stream "
             << endl;
        *major = 0;
        *minor = 0;
        return;
    }

    char content_code = popByte();
    if(content_code != MarkupCodes::protocol_version)
    {
        cerr << "mdcapinput.o: asked to popProtocol(), but content code is "
             << "not protocol_version "
             << endl;
        *major = 0;
        *minor = 0;
        return;       
    }
    
    uint16_t v_major = popU16();
    uint16_t v_minor = popU16();
    
    *major = (int) v_major;
    *minor = (int) v_minor;
}


uint32_t MdcapInput::popSessionId()
{
    //
    //  Session id is always 5 bytes
    //  1st byte - session id markup code
    //    next 4 - 32 bit session id
    //
    
    if(amountLeft() < 5)
    {
        cerr << "mdcapinput.o: asked to popSessionId(), but there are not "
             << "enough bytes left in the stream "
             << endl;
        return 0;
    }

    char content_code = popByte();
    if(content_code != MarkupCodes::session_id)
    {
        cerr << "mdcapinput.o: asked to popSessionId(), but content code is "
             << "not session_id "
             << endl;
        return 0;       
    }

    return popU32();    
}


uint32_t MdcapInput::popCollectionCount()
{
    //
    //  Collection count is always 5 bytes
    //  1st byte - session id markup code
    //    next 4 - 32 bit integer = number of collections
    //
    
    if(amountLeft() < 5)
    {
        cerr << "mdcapinput.o: asked to popCollectionCount(), but "
             << "there are not enough bytes left in the stream "
             << endl;
        return 0;
    }

    char content_code = popByte();
    if(content_code != MarkupCodes::collection_count)
    {
        cerr << "mdcapinput.o: asked to popCollectionCount(), but "
             << "content code is not collection_count "
             << endl;
        return 0;       
    }

    return popU32();    
}


uint32_t MdcapInput::popCollectionId()
{
    //
    //  Collection id is always 5 bytes
    //  1st byte - session id markup code
    //    next 4 - 32 bit integer = collection id
    //
    
    if(amountLeft() < 5)
    {
        cerr << "mdcapinput.o: asked to popCollectionId(), but "
             << "there are not enough bytes left in the stream "
             << endl;
        return 0;
    }

    char content_code = popByte();
    if(content_code != MarkupCodes::collection_id)
    {
        cerr << "mdcapinput.o: asked to popCollectionId(), but "
             << "content code is not collection_id "
             << endl;
        return 0;       
    }

    return popU32();    
}


uint32_t MdcapInput::popCollectionType()
{
    //
    //  Collection type is always 5 bytes
    //  1st byte - session id markup code
    //    next 4 - 32 bit integer = collection id
    //
    
    if(amountLeft() < 5)
    {
        cerr << "mdcapinput.o: asked to popCollectionType(), but "
             << "there are not enough bytes left in the stream "
             << endl;
        return 0;
    }

    char content_code = popByte();
    if(content_code != MarkupCodes::collection_type)
    {
        cerr << "mdcapinput.o: asked to popCollectionType(), but "
             << "content code is not collection_type "
             << endl;
        return 0;       
    }

    return popU32();    
}


bool MdcapInput::popCollectionEditable()
{
    //
    //  Editable is 2 bytes
    //  1 - content markup code
    //  2 - int; 0 is false
    //           1 is true
    //
    
    if(amountLeft() < 2)
    {
        cerr << "mdcapinput.o: asked to popCollectionEditable(), but not enough "
             << "bytes left";
        return false;
    }
    
    char content_code = popByte();
    if(content_code != MarkupCodes::collection_is_editable)
    {
        cerr << "mdcapinput.o: asked to popCollectionEditable(), but "
             << "content code is not collection_is_editable "
             << endl;
        return false;
    }
    
    uint8_t result = popByte();
    if(result == 1)
    {
        return true;
    }
    else if(result == 0)
    {
        return false;
    }
    
    cerr << "mdcapinput.o: asked to popCollectionEditable(), but value was "
         << "neither true nor false"
         << endl;
    return false;
}


bool MdcapInput::popCollectionRipable()
{
    //
    //  Ripable is 2 bytes
    //  1 - content markup code
    //  2 - int; 0 is false
    //           1 is true
    //
    
    if(amountLeft() < 2)
    {
        cerr << "mdcapinput.o: asked to popCollectionRipable(), but not enough "
             << "bytes left";
        return false;
    }
    
    char content_code = popByte();
    if(content_code != MarkupCodes::collection_is_ripable)
    {
        cerr << "mdcapinput.o: asked to popCollectionRipable(), but "
             << "content code is not collection_is_ripable "
             << endl;
        return false;
    }
    
    uint8_t result = popByte();
    if(result == 1)
    {
        return true;
    }
    else if(result == 0)
    {
        return false;
    }
    
    cerr << "mdcapinput.o: asked to popCollectionRipable(), but value was "
         << "neither true nor false"
         << endl;
    return false;
}


bool MdcapInput::popCollectionBeingRipped()
{
    //
    //  Being Ripped is 2 bytes
    //  1 - content markup code
    //  2 - int; 0 is false
    //           1 is true
    //
    
    if(amountLeft() < 2)
    {
        cerr << "mdcapinput.o: asked to popCollectionBeingRipped(), but not enough "
             << "bytes left";
        return false;
    }
    
    char content_code = popByte();
    if(content_code != MarkupCodes::collection_being_ripped)
    {
        cerr << "mdcapinput.o: asked to popCollectionBeingRipped(), but "
             << "content code is not collection_being_ripped "
             << endl;
        return false;
    }
    
    uint8_t result = popByte();
    if(result == 1)
    {
        return true;
    }
    else if(result == 0)
    {
        return false;
    }
    
    cerr << "mdcapinput.o: asked to popCollectionBeingRipped(), but value was "
         << "neither true nor false"
         << endl;
    return false;
}


uint32_t MdcapInput::popCollectionGeneration()
{
    //
    //  Collection generation is always 5 bytes
    //  1st byte - session id markup code
    //    next 4 - 32 bit integer = generation number
    //
    
    if(amountLeft() < 5)
    {
        cerr << "mdcapinput.o: asked to popCollectionGeneration(), but "
             << "there are not enough bytes left in the stream "
             << endl;
        return 0;
    }

    char content_code = popByte();
    if(content_code != MarkupCodes::collection_generation)
    {
        cerr << "mdcapinput.o: asked to popCollectionGeneration(), but "
             << "content code is not collection_generation "
             << endl;
        return 0;       
    }

    return popU32();    
}

bool MdcapInput::popUpdateType()
{
    //
    //  Update type is 2 bytes
    //  1 - content markup code
    //  2 - int; 0 is false (partial update)
    //           1 is true  (full update)
    //
    
    if(amountLeft() < 2)
    {
        cerr << "mdcapinput.o: asked to popUpdateType(), but not enough "
             << "bytes left";
        return false;
    }
    
    char content_code = popByte();
    if(content_code != MarkupCodes::update_type)
    {
        cerr << "mdcapinput.o: asked to popUpdateType(), but "
             << "content code is not update_type "
             << endl;
        return false;
    }
    
    uint8_t result = popByte();
    if(result == 1)
    {
        return true;
    }
    else if(result == 0)
    {
        return false;
    }
    
    cerr << "mdcapinput.o: asked to popUpdateType(), but value was "
         << "neither true nor false"
         << endl;
    return false;
}


uint32_t MdcapInput::popTotalItems()
{
    //
    //  Total items is always 5 bytes
    //  1st byte - markup code
    //    next 4 - 32 bit integer = total items
    //
    
    if(amountLeft() < 5)
    {
        cerr << "mdcapinput.o: asked to popTotalItems(), but "
             << "there are not enough bytes left in the stream "
             << endl;
        return 0;
    }

    char content_code = popByte();
    if(content_code != MarkupCodes::total_items)
    {
        cerr << "mdcapinput.o: asked to popTotalItems(), but "
             << "content code is not total_items "
             << endl;
        return 0;       
    }

    return popU32();    
}

uint32_t MdcapInput::popAddedItems()
{
    //
    //  Added items is always 5 bytes
    //  1st byte - markup code
    //    next 4 - 32 bit integer = total items
    //
    
    if(amountLeft() < 5)
    {
        cerr << "mdcapinput.o: asked to popAddedItems(), but "
             << "there are not enough bytes left in the stream "
             << endl;
        return 0;
    }

    char content_code = popByte();
    if(content_code != MarkupCodes::added_items)
    {
        cerr << "mdcapinput.o: asked to popAddedItems(), but "
             << "content code is not added_items "
             << endl;
        return 0;       
    }

    return popU32();    
}

uint32_t MdcapInput::popDeletedItems()
{
    //
    //  Deleted items is always 5 bytes
    //  1st byte - markup code
    //    next 4 - 32 bit integer = total items
    //
    
    if(amountLeft() < 5)
    {
        cerr << "mdcapinput.o: asked to popDeletedItems(), but "
             << "there are not enough bytes left in the stream "
             << endl;
        return 0;
    }

    char content_code = popByte();

    if(content_code != MarkupCodes::deleted_items)
    {
        cerr << "mdcapinput.o: asked to popDeletedItems(), but "
             << "content code is not deleted_items "
             << endl;
        return 0;       
    }

    return popU32();    
}

uint8_t MdcapInput::popItemType()
{
    //
    //  item id is always 2 bytes
    //  1st byte - markup code
    //  2nd byte - item type
    //
    
    if(amountLeft() < 2)
    {
        cerr << "mdcapinput.o: asked to popItemType(), but "
             << "there are not enough bytes left in the stream "
             << endl;
        return 0;
    }

    char content_code = popByte();
    if(content_code != MarkupCodes::item_type)
    {
        cerr << "mdcapinput.o: asked to popItemType(), but "
             << "content code is not item_type "
             << endl;
        return 0;       
    }

    return popByte();    
}

uint32_t MdcapInput::popItemId()
{
    //
    //  item id is always 5 bytes
    //  1st byte - markup code
    //    next 4 - 32 bit integer = item id
    //
    
    if(amountLeft() < 5)
    {
        cerr << "mdcapinput.o: asked to popItemId(), but "
             << "there are not enough bytes left in the stream "
             << endl;
        return 0;
    }

    char content_code = popByte();
    if(content_code != MarkupCodes::item_id)
    {
        cerr << "mdcapinput.o: asked to popItemId(), but "
             << "content code is not item_id "
             << endl;
        return 0;       
    }

    return popU32();    
}

uint8_t MdcapInput::popItemRating()
{
    //
    //  item rating is always 2 bytes
    //  1st byte - markup code
    //  2nd byte - rating (0-10)
    //
    
    if(amountLeft() < 2)
    {
        cerr << "mdcapinput.o: asked to popItemRating(), but "
             << "there are not enough bytes left in the stream "
             << endl;
        return 0;
    }

    char content_code = popByte();
    if(content_code != MarkupCodes::item_rating)
    {
        cerr << "mdcapinput.o: asked to popItemRating(), but "
             << "content code is not item_rating "
             << endl;
        return 0;       
    }

    return popByte();
}

uint32_t MdcapInput::popItemLastPlayed()
{
    //
    //  last played is always 5 bytes
    //  1st byte - markup code
    //  next 4 - 32 bit integer = seconds since disco
    //
    
    if(amountLeft() < 5)
    {
        cerr << "mdcapinput.o: asked to popItemLastPlayed(), but "
             << "there are not enough bytes left in the stream "
             << endl;
        return 0;
    }

    char content_code = popByte();
    if(content_code != MarkupCodes::item_last_played)
    {
        cerr << "mdcapinput.o: asked to popItemLastPlayed(), but "
             << "content code is not item_last_played "
             << endl;
        return 0;       
    }

    return popU32();    
}

uint32_t MdcapInput::popItemPlayCount()
{
    //
    //  play count is always 5 bytes
    //  1st byte - markup code
    //  next 4 - 32 bit integer = number of times played
    //
    
    if(amountLeft() < 5)
    {
        cerr << "mdcapinput.o: asked to popItemPlayCount(), but "
             << "there are not enough bytes left in the stream "
             << endl;
        return 0;
    }

    char content_code = popByte();
    if(content_code != MarkupCodes::item_play_count)
    {
        cerr << "mdcapinput.o: asked to popItemPlayCount(), but "
             << "content code is not item_play_count "
             << endl;
        return 0;       
    }

    return popU32();    
}

uint32_t MdcapInput::popItemYear()
{
    //
    //  item year is always 5 bytes
    //  1st byte - markup code
    //  next 4 - 32 bit integer = year
    //
    
    if(amountLeft() < 5)
    {
        cerr << "mdcapinput.o: asked to popItemYear(), but "
             << "there are not enough bytes left in the stream "
             << endl;
        return 0;
    }

    char content_code = popByte();
    if(content_code != MarkupCodes::item_year)
    {
        cerr << "mdcapinput.o: asked to popItemYear(), but "
             << "content code is not item_year "
             << endl;
        return 0;       
    }

    return popU32();    
}

uint32_t MdcapInput::popItemTrack()
{
    //
    //  item track is always 5 bytes
    //  1st byte - markup code
    //  next 4 - 32 bit integer = track number
    //
    
    if(amountLeft() < 5)
    {
        cerr << "mdcapinput.o: asked to popTrack(), but "
             << "there are not enough bytes left in the stream "
             << endl;
        return 0;
    }

    char content_code = popByte();
    if(content_code != MarkupCodes::item_track)
    {
        cerr << "mdcapinput.o: asked to popItemTrack(), but "
             << "content code is not item_track "
             << endl;
        return 0;       
    }

    return popU32();    
}

uint32_t MdcapInput::popItemLength()
{
    //
    //  item length is always 5 bytes
    //  1st byte - markup code
    //  next 4 - 32 bit integer = length in seconds
    //
    
    if(amountLeft() < 5)
    {
        cerr << "mdcapinput.o: asked to popLength(), but "
             << "there are not enough bytes left in the stream "
             << endl;
        return 0;
    }

    char content_code = popByte();
    if(content_code != MarkupCodes::item_length)
    {
        cerr << "mdcapinput.o: asked to popItemLength(), but "
             << "content code is not item_length "
             << endl;
        return 0;       
    }

    return popU32();    
}

bool MdcapInput::popItemDupFlag()
{
    //
    //  item dup flag is always 2 bytes
    //  1st byte - markup code
    //  2nd byte - 1/0 true/false (item is/isn't a duplicate)
    //
    
    if(amountLeft() < 2)
    {
        cerr << "mdcapinput.o: asked to popItemDupFlag(), but "
             << "there are not enough bytes left in the stream "
             << endl;
        return 0;
    }

    char content_code = popByte();
    if(content_code != MarkupCodes::item_dup_flag)
    {
        cerr << "mdcapinput.o: asked to popItemDupFlag(), but "
             << "content code is not item_dup_flag "
             << endl;
        return 0;       
    }

    uint8_t flag_value = popByte();
    if(flag_value == 1)
    {
        return true;
    }
    else if(flag_value == 0)
    {
        return false;
    }
    
    cerr << "mdcapinput.o: asked to popItemDupFlag(), but value was "
         << "neither true nor false"
         << endl;
    return false;
}

QString MdcapInput::popItemUrl()
{
    QValueVector<char> url_string_vector;
    char content_code = popGroup(&url_string_vector);
    
    if(content_code != MarkupCodes::item_url)
    {
        cerr << "mdcapinput.o: asked to do popItemUrl(), but this "
             << "doesn't look like an item_url ("
             << (int) ((uint8_t )content_code)
             << ")"
             << endl;
        return QString("");
    }
    
    
    char *utf8_url = new char [url_string_vector.size() + 1];
    for(uint i = 0; i < url_string_vector.size(); i++)
    {
        utf8_url[i] = url_string_vector[i];
    }
    utf8_url[url_string_vector.size()] = '\0';
    
    QString the_url = QString::fromUtf8(utf8_url);
    
    delete [] utf8_url;
    
    return the_url;
}

QString MdcapInput::popItemArtist()
{
    QValueVector<char> artist_string_vector;
    char content_code = popGroup(&artist_string_vector);
    
    if(content_code != MarkupCodes::item_artist)
    {
        cerr << "mdcapinput.o: asked to do popItemArtist(), but this "
             << "doesn't look like an item_artist ("
             << (int) ((uint8_t )content_code)
             << ")"
             << endl;
        return QString("");
    }
    
    
    char *utf8_artist = new char [artist_string_vector.size() + 1];
    for(uint i = 0; i < artist_string_vector.size(); i++)
    {
        utf8_artist[i] = artist_string_vector[i];
    }
    utf8_artist[artist_string_vector.size()] = '\0';
    
    QString the_artist = QString::fromUtf8(utf8_artist);
    
    delete [] utf8_artist;
    
    return the_artist;
}

QString MdcapInput::popItemAlbum()
{
    QValueVector<char> album_string_vector;
    char content_code = popGroup(&album_string_vector);
    
    if(content_code != MarkupCodes::item_album)
    {
        cerr << "mdcapinput.o: asked to do popItemAlbum(), but this "
             << "doesn't look like an item_album ("
             << (int) ((uint8_t )content_code)
             << ")"
             << endl;
        return QString("");
    }
    
    
    char *utf8_album = new char [album_string_vector.size() + 1];
    for(uint i = 0; i < album_string_vector.size(); i++)
    {
        utf8_album[i] = album_string_vector[i];
    }
    utf8_album[album_string_vector.size()] = '\0';
    
    QString the_album = QString::fromUtf8(utf8_album);
    
    delete [] utf8_album;
    
    return the_album;
}

QString MdcapInput::popItemTitle()
{
    QValueVector<char> title_string_vector;
    char content_code = popGroup(&title_string_vector);
    
    if(content_code != MarkupCodes::item_title)
    {
        cerr << "mdcapinput.o: asked to do popItemTitle(), but this "
             << "doesn't look like an item_title ("
             << (int) ((uint8_t )content_code)
             << ")"
             << endl;
        return QString("");
    }
    
    
    char *utf8_title = new char [title_string_vector.size() + 1];
    for(uint i = 0; i < title_string_vector.size(); i++)
    {
        utf8_title[i] = title_string_vector[i];
    }
    utf8_title[title_string_vector.size()] = '\0';
    
    QString the_title = QString::fromUtf8(utf8_title);
    
    delete [] utf8_title;
    
    return the_title;
}

QString MdcapInput::popItemGenre()
{
    QValueVector<char> genre_string_vector;
    char content_code = popGroup(&genre_string_vector);
    
    if(content_code != MarkupCodes::item_genre)
    {
        cerr << "mdcapinput.o: asked to do popItemGenre(), but this "
             << "doesn't look like an item_genre ("
             << (int) ((uint8_t )content_code)
             << ")"
             << endl;
        return QString("");
    }
    
    
    char *utf8_genre = new char [genre_string_vector.size() + 1];
    for(uint i = 0; i < genre_string_vector.size(); i++)
    {
        utf8_genre[i] = genre_string_vector[i];
    }
    utf8_genre[genre_string_vector.size()] = '\0';
    
    QString the_genre = QString::fromUtf8(utf8_genre);
    
    delete [] utf8_genre;
    
    return the_genre;
}

int MdcapInput::popListId()
{
    //
    //  list id is always 5 bytes
    //  1st byte - markup code
    //  next 4 - 32 bit integer = list id
    //
    
    if(amountLeft() < 5)
    {
        cerr << "mdcapinput.o: asked to popListId(), but "
             << "there are not enough bytes left in the stream "
             << endl;
        return 0;
    }

    char content_code = popByte();
    if(content_code != MarkupCodes::list_id)
    {
        cerr << "mdcapinput.o: asked to popListId(), but "
             << "content code is not list_id "
             << endl;
        return 0;       
    }

    return popU32();    
}

QString MdcapInput::popListName()
{
    QValueVector<char> listname_string_vector;
    char content_code = popGroup(&listname_string_vector);
    
    if(content_code != MarkupCodes::list_name)
    {
        cerr << "mdcapinput.o: asked to do popListName(), but this "
             << "doesn't look like a list_name ("
             << (int) ((uint8_t )content_code)
             << ")"
             << endl;
        return QString("");
    }
    
    
    char *utf8_listname = new char [listname_string_vector.size() + 1];
    for(uint i = 0; i < listname_string_vector.size(); i++)
    {
        utf8_listname[i] = listname_string_vector[i];
    }
    utf8_listname[listname_string_vector.size()] = '\0';
    
    QString the_listname = QString::fromUtf8(utf8_listname);
    
    delete [] utf8_listname;
    
    return the_listname;
}

uint32_t MdcapInput::popListItem()
{
    //
    //  list item is always 5 bytes
    //  1st byte - markup code
    //  next 4 - 32 bit integer = id of the item
    //
    
    if(amountLeft() < 5)
    {
        cerr << "mdcapinput.o: asked to popListItem(), but "
             << "there are not enough bytes left in the stream "
             << endl;
        return 0;
    }

    char content_code = popByte();
    if(content_code != MarkupCodes::list_item)
    {
        cerr << "mdcapinput.o: asked to popListItem(), but "
             << "content code is not list_item "
             << endl;
        return 0;       
    }

    return popU32();    
}

uint32_t MdcapInput::popDeletedList()
{
    //
    //  deleted list is always 5 bytes
    //  1st byte - markup code
    //  next 4 - 32 bit integer = id of the list being deleted
    //
    
    if(amountLeft() < 5)
    {
        cerr << "mdcapinput.o: asked to popDeletedList(), but "
             << "there are not enough bytes left in the stream "
             << endl;
        return 0;
    }

    char content_code = popByte();
    if(content_code != MarkupCodes::deleted_list)
    {
        cerr << "mdcapinput.o: asked to popDeletedList(), but "
             << "content code is not deleted_list "
             << endl;
        return 0;       
    }

    return popU32();    
}

bool MdcapInput::popListEditable()
{
    //
    //  Editable is 2 bytes
    //  1 - content markup code
    //  2 - int; 0 is false
    //           1 is true
    //
    
    if(amountLeft() < 2)
    {
        cerr << "mdcapinput.o: asked to popListEditable(), but not enough "
             << "bytes left";
        return false;
    }
    
    char content_code = popByte();
    if(content_code != MarkupCodes::list_is_editable)
    {
        cerr << "mdcapinput.o: asked to popListEditable(), but "
             << "content code is not list_is_editable "
             << endl;
        return false;
    }
    
    uint8_t result = popByte();
    if(result == 1)
    {
        return true;
    }
    else if(result == 0)
    {
        return false;
    }
    
    cerr << "mdcapinput.o: asked to popListEditable(), but value was "
         << "neither true nor false"
         << endl;
    return false;
}



uint32_t MdcapInput::popDeletedItem()
{
    //
    //  deleted item is always 5 bytes
    //  1st byte - markup code
    //  next 4 - 32 bit integer = id of the item being deleted
    //
    
    if(amountLeft() < 5)
    {
        cerr << "mdcapinput.o: asked to popDeletedItem(), but "
             << "there are not enough bytes left in the stream "
             << endl;
        return 0;
    }

    char content_code = popByte();
    if(content_code != MarkupCodes::deleted_item)
    {
        cerr << "mdcapinput.o: asked to popDeletedItem(), but "
             << "content code is not deleted_item "
             << endl;
        return 0;       
    }

    return popU32();    
}

QString MdcapInput::popListItemName()
{
    QValueVector<char> list_item_string_vector;
    char content_code = popGroup(&list_item_string_vector);
    
    if(content_code != MarkupCodes::list_item_name)
    {
        cerr << "mdcapinput.o: asked to do popListItemName(), but this "
             << "doesn't look like an list_item_name ("
             << (int) ((uint8_t )content_code)
             << ")"
             << endl;
        return QString("");
    }
    
    
    char *utf8_list_item_name = new char [list_item_string_vector.size() + 1];
    for(uint i = 0; i < list_item_string_vector.size(); i++)
    {
        utf8_list_item_name[i] = list_item_string_vector[i];
    }
    utf8_list_item_name[list_item_string_vector.size()] = '\0';
    
    QString the_list_item_name = QString::fromUtf8(utf8_list_item_name);
    
    delete [] utf8_list_item_name;
    
    return the_list_item_name;
}

bool MdcapInput::popCommitListType()
{
    //
    //  Update type is 2 bytes
    //  1 - content markup code
    //  2 - int; 0 is false (partial update)
    //           1 is true  (full update)
    //
    
    if(amountLeft() < 2)
    {
        cerr << "mdcapinput.o: asked to popCommitListType(), but not enough "
             << "bytes left";
        return false;
    }
    
    char content_code = popByte();
    if(content_code != MarkupCodes::commit_list_type)
    {
        cerr << "mdcapinput.o: asked to popCommitListType(), but "
             << "content code is not commit_list_type "
             << endl;
        return false;
    }
    
    uint8_t result = popByte();
    if(result == 1)
    {
        return true;
    }
    else if(result == 0)
    {
        return false;
    }
    
    cerr << "mdcapinput.o: asked to popCommitListType(), but value was "
         << "neither true nor false"
         << endl;
    return false;
}




void MdcapInput::printContents()
{
    //
    //  For debugging
    //
    
    cout << "&&&&&&&&&&&&&&&&&&&& DEBUGGING OUTPUT &&&&&&&&&&&&&&&&&&&&"
         << endl
         << "contents of this MdcapInput object (size is "
         << contents.size()
         << ")"
         << endl;
    for(uint i = 0; i < contents.size(); i++)
    {
        cout << "[" 
             << i
             << "]="
             << (int) ((uint8_t) contents[i])
             << " ";
    }
    cout << endl;
}
MdcapInput::~MdcapInput()
{
}



