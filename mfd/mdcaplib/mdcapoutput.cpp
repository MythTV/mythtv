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

void MdcapOutput::addProtocolVersion()
{
    //
    //  protocol version is always 4 bytes long
    //
    
    append(MarkupCodes::protocol_version);
    append((uint16_t) MDCAP_PROTOCOL_VERSION_MAJOR);
    append((uint16_t) MDCAP_PROTOCOL_VERSION_MINOR);
}

void MdcapOutput::addCollectionCount()
{
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


