/*
	mtcpinput.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for mtcp input object

*/

#include <iostream>
using namespace std;

#include "markupcodes.h"
#include "mtcpinput.h"


MtcpInput::MtcpInput(std::vector<char> *raw_data)
{
    std::vector<char>::iterator it;
    for(it = raw_data->begin(); it != raw_data->end(); )
    {
        contents.append((*it));
        ++it;
    }
    pos = 0;
}

MtcpInput::MtcpInput(QValueVector<char> *raw_data)
{
    contents = QValueVector<char>(*raw_data);
    pos = 0;
}

char MtcpInput::peekAtNextCode()
{
    if(amountLeft() > 0)
    {
        return contents[pos];
    }
    cerr << "mtcpinput.o: asked to peek at next code, but there's nothing "
         << "left to peek at" 
         << endl;

    return 0;
}

uint MtcpInput::amountLeft()
{
    int amount_left = contents.size() - pos;
    if(amount_left < 0)
    {
        cerr << "mtcpinput.o: there is less than nothing left "
             << "of my contents"
             << endl;

        return 0;
    }
    
    return (uint) amount_left;
}

char MtcpInput::popGroup(QValueVector<char> *group_contents)
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
        cerr << "mtcpinput.o: asked to pop a group, but there are no "
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
        cerr << "mtcpinput.o: there are not enough bytes in the stream "
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

char MtcpInput::popByte()
{
    //
    //  Pull one char/byte of the front of the contents
    //
    
    if(amountLeft() < 1)
    {
        cerr << "mtcpinput.o: asked to do a popByte(), but the "
             << "content is empty"
             << endl;

        return 0;
    }

    char return_value = contents[pos];

    ++pos;
    return return_value;
    
}

uint16_t MtcpInput::popU16()
{
    uint8_t value_bigendian    = popByte();
    uint8_t value_littleendian = popByte();
    
    uint16_t result = (value_bigendian * 256) + value_littleendian;
    
    return result;
}


uint32_t MtcpInput::popU32()
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


QString MtcpInput::popName()
{
    QValueVector<char> name_string_vector;
    char content_code = popGroup(&name_string_vector);
    
    if(content_code != MtcpMarkupCodes::name)
    {
        cerr << "mtcpinput.o: asked to do popName(), but this "
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

void MtcpInput::popProtocol(int *major, int *minor)
{
    //
    //  Protocol is always 5 bytes
    //  1st byte - status markup code
    //    next 2 - 16 bit major
    //    next 2 - 16 bit minor
    //
    
    if(amountLeft() < 5)
    {
        cerr << "mtcpinput.o: asked to popProtocol(), but there are not "
             << "enough bytes left in the stream "
             << endl;
        *major = 0;
        *minor = 0;
        return;
    }

    char content_code = popByte();
    if(content_code != MtcpMarkupCodes::protocol_version)
    {
        cerr << "mtcpinput.o: asked to popProtocol(), but content code is "
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

uint32_t MtcpInput::popJobCount()
{
    //
    //  job count is always 5 bytes
    //  1st byte - job count markup code
    //    next 4 - 32 bit # of jobs
    //
    
    if(amountLeft() < 5)
    {
        cerr << "mtcpinput.o: asked to popJobCount(), but there are not "
             << "enough bytes left in the stream "
             << endl;
        return 0;
    }

    char content_code = popByte();
    if(content_code != MtcpMarkupCodes::job_count)
    {
        cerr << "mtcpinput.o: asked to popJobCount(), but content code is "
             << "not job_count "
             << endl;
        return 0;       
    }

    return popU32();    
}

QString MtcpInput::popMajorDescription()
{
    QValueVector<char> major_string_vector;
    char content_code = popGroup(&major_string_vector);
    
    if(content_code != MtcpMarkupCodes::job_major_description)
    {
        cerr << "mtcpinput.o: asked to do popMajorDescription(), but this "
             << "doesn't look like a job_major_description"
             << endl;
        return QString("");
    }
    
    
    char *utf8_major = new char [major_string_vector.size() + 1];
    for(uint i = 0; i < major_string_vector.size(); i++)
    {
        utf8_major[i] = major_string_vector[i];
    }
    utf8_major[major_string_vector.size()] = '\0';
    
    QString the_major = QString::fromUtf8(utf8_major);
    
    delete [] utf8_major;
    return the_major;
}

QString MtcpInput::popMinorDescription()
{
    QValueVector<char> minor_string_vector;
    char content_code = popGroup(&minor_string_vector);
    
    if(content_code != MtcpMarkupCodes::job_minor_description)
    {
        cerr << "mtcpinput.o: asked to do popMinorDescription(), but this "
             << "doesn't look like a job_minor_description"
             << endl;
        return QString("");
    }
    
    
    char *utf8_minor = new char [minor_string_vector.size() + 1];
    for(uint i = 0; i < minor_string_vector.size(); i++)
    {
        utf8_minor[i] = minor_string_vector[i];
    }
    utf8_minor[minor_string_vector.size()] = '\0';
    
    QString the_minor = QString::fromUtf8(utf8_minor);
    
    delete [] utf8_minor;
    return the_minor;
}
    
uint32_t MtcpInput::popMajorProgress()
{
    //
    //  major progress is always 5 bytes
    //  1st byte - major progress markup code
    //    next 4 - 32 bit number between 0 and 100
    //
    
    if(amountLeft() < 5)
    {
        cerr << "mtcpinput.o: asked to popMajorProgress(), but there are not "
             << "enough bytes left in the stream "
             << endl;
        return 0;
    }

    char content_code = popByte();
    if(content_code != MtcpMarkupCodes::job_major_progress)
    {
        cerr << "mtcpinput.o: asked to popMajorProgress(), but content code is "
             << "not job_major_progress "
             << endl;
        return 0;       
    }

    return popU32();    
}

uint32_t MtcpInput::popMinorProgress()
{
    //
    //  minor progress is always 5 bytes
    //  1st byte - minor progress markup code
    //    next 4 - 32 bit number between 0 and 100
    //
    
    if(amountLeft() < 5)
    {
        cerr << "mtcpinput.o: asked to popMinorProgress(), but there are not "
             << "enough bytes left in the stream "
             << endl;
        return 0;
    }

    char content_code = popByte();
    if(content_code != MtcpMarkupCodes::job_minor_progress)
    {
        cerr << "mtcpinput.o: asked to popMinorProgress(), but content code is "
             << "not job_minor_progress "
             << endl;
        return 0;       
    }

    return popU32();    
}

void MtcpInput::printContents()
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

MtcpInput::~MtcpInput()
{
}



