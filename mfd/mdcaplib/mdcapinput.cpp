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
    // contents = QValueVector<char>(*raw_data);
}

MdcapInput::MdcapInput(QValueVector<char> *raw_data)
{
    contents = QValueVector<char>(*raw_data);
}

char MdcapInput::peekAtNextCode()
{
    if(contents.size() > 0)
    {
        return contents[0];
    }
    return 0;
}

char MdcapInput::popGroup(QValueVector<char> *group_contents)
{
    group_contents->clear();
    int return_value = 0;
    if(contents.size() < 1)
    {
        return return_value;
    }
    
    return_value = contents[0];
    
    //
    //  Need to make sure that we are actually positioned on a group
    //
    
    if(
        return_value != MarkupCodes::server_info_group &&
        return_value != MarkupCodes::name
      )
    {
        cerr << "mdcapinput.o: asked to pop a group, but first code "
             << "is not a group one!"
             << endl;
        return 0;
    }

    //
    //  find out how many bytes this group consists of
    //
    
    
    if(contents.size() < 5)
    {
        cerr << "mdcapinput.o: asked to pop a group, but there are no "
             << "size bytes in the stream " 
             << endl;
        return 0;
    }

    uint32_t group_size  = contents[4];
             group_size += contents[3] * 256;
             group_size += contents[2] * 256 * 256;
             group_size += contents[1] * 256 * 256 * 256;
             
    //
    //  Make sure there are at least as many bytes available as the group
    //  size calculation thinks there should be
    //

    if(contents.size() < group_size + 5)
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
        group_contents->append(contents[5 + i]);
    }

    //
    //  Now rip them out of our permanent content
    //

    QValueVector<char>::iterator contents_first = contents.begin();
    QValueVector<char>::iterator contents_last = contents.begin();
    contents_last += 5 + group_size;
    contents.erase(contents_first, contents_last);

    return return_value;
}

char MdcapInput::popByte()
{
    //
    //  Pull one char/byte of the front of the contents
    //
    
    if(contents.size() < 1)
    {
        cerr << "mdcapinput.o: asked to do a popByte(), but the "
             << "content is empty"
             << endl;

        return 0;
    }

    char return_value = contents[0];
    
    QValueVector<char>::iterator contents_first = contents.begin();
    QValueVector<char>::iterator contents_last = contents.begin();
    ++contents_last;

    contents.erase(contents_first, contents_last);

    return return_value;
    
}

uint16_t MdcapInput::popU16()
{
    uint8_t value_bigendian    = popByte();
    uint8_t value_littleendian = popByte();
    
    uint16_t result = (value_bigendian * 256) + value_littleendian;
    
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
    
    if(contents.size() < 3)
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
    
    if(contents.size() < 5)
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


MdcapInput::~MdcapInput()
{
}



