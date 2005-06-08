/*
	mtcpoutput.cpp

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for mtcp output object
	
*/

#include <iostream>
using namespace std;

#include "markupcodes.h"
#include "mtcpoutput.h"

MtcpOutput::MtcpOutput()
{
}

bool MtcpOutput::openGroups()
{
    if(open_groups.count() > 0)
    {
        return true;
    }
    return false;
}

void MtcpOutput::endGroup()
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

void MtcpOutput::addServerInfoGroup()
{
    //
    //  Add a server info markup code
    //

    append(MtcpMarkupCodes::server_info_group);
    
    //
    //  Leave four empty bytes where we'll store the length of this group
    //  once it is done (ie. when endGroup()) is called. We store where
    //  these 4 bytes begin by pushing an index value onto our open_groups
    //  stack.
    //
    
    open_groups.push(contents.size());
    append((uint32_t) 0);
    
}


void MtcpOutput::addServiceName(const QString &service_name)
{

    //
    //  Get the name of the service in utf8
    //

    QCString utf8_string = service_name.utf8();
    
    //
    //  Put in a content code for a name
    //

    append(MtcpMarkupCodes::name);
    
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

void MtcpOutput::addJobMajorDescription(const QString &major_description)
{

    //
    //  Get the description if utf8
    //

    QCString utf8_string = major_description.utf8();
    
    //
    //  Put in a content code for a major job description
    //

    append(MtcpMarkupCodes::job_major_description);
    
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

void MtcpOutput::addJobMinorDescription(const QString &minor_description)
{

    //
    //  Get the description if utf8
    //

    QCString utf8_string = minor_description.utf8();
    
    //
    //  Put in a content code for a minor job description
    //

    append(MtcpMarkupCodes::job_minor_description);
    
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

void MtcpOutput::addProtocolVersion()
{
    //
    //  protocol version is always 5 bytes long
    //
    
    append(MtcpMarkupCodes::protocol_version);
    append((uint16_t) MTCP_PROTOCOL_VERSION_MAJOR);
    append((uint16_t) MTCP_PROTOCOL_VERSION_MINOR);
}

void MtcpOutput::addJobCount(int count)
{
    //
    //  job count is always 5 bytes
    //
    
    append(MtcpMarkupCodes::job_count);
    append((uint32_t) count);
}
    
void MtcpOutput::addJobMajorProgress(int major_progress)
{
    //
    //  major progress is always 5 bytes
    //
    
    append(MtcpMarkupCodes::job_major_progress);
    append((uint32_t) major_progress);
}
    
void MtcpOutput::addJobMinorProgress(int minor_progress)
{
    //
    //  minor progress is always 5 bytes
    //
    
    append(MtcpMarkupCodes::job_minor_progress);
    append((uint32_t) minor_progress);
}
    
void MtcpOutput::printContents()
{
    for(uint i = 0; i < contents.size(); i++)
    {
        cout << "contents[" << i << "] is " << (int) ((uint8_t) contents[i]) << endl;
    }
}

void MtcpOutput::addJobInfoGroup()
{
    //
    //  Add a job info markup code
    //

    append(MtcpMarkupCodes::job_info_group);
    
    //
    //  Leave four empty bytes where we'll store the length of this group
    //  once it is done (ie. when endGroup()) is called. We store where
    //  these 4 bytes begin by pushing an index value onto our open_groups
    //  stack.
    //
    
    open_groups.push(contents.size());
    append((uint32_t) 0);
    
}

void MtcpOutput::addJobGroup()
{
    //
    //  Add a job info markup code
    //

    append(MtcpMarkupCodes::job_group);
    
    //
    //  Leave four empty bytes where we'll store the length of this group
    //  once it is done (ie. when endGroup()) is called. We store where
    //  these 4 bytes begin by pushing an index value onto our open_groups
    //  stack.
    //
    
    open_groups.push(contents.size());
    append((uint32_t) 0);
    
}



void MtcpOutput::append(char a_char)
{
    contents.push_back(a_char);
}

void MtcpOutput::append(const char *a_string, uint length)
{
    for(uint i = 0; i < length; i++)
    {
        append(a_string[i]);
    }
}

void MtcpOutput::append(uint8_t a_u8)
{
    contents.push_back(a_u8);
}

void MtcpOutput::append(uint16_t a_u16)
{
    contents.push_back(( a_u16 >>  8 ) &0xff );
    contents.push_back(( a_u16       ) &0xff );
}

void MtcpOutput::append(uint32_t a_u32)
{
    contents.push_back(( a_u32 >> 24 ) &0xff );
    contents.push_back(( a_u32 >> 16 ) &0xff );
    contents.push_back(( a_u32 >>  8 ) &0xff );
    contents.push_back(( a_u32       ) &0xff );
}

MtcpOutput::~MtcpOutput()
{

}


