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
    void endGroup();
    
    //
    //  "element" functions
    //

    void addStatus(uint16_t the_status);
    void addServiceName(const QString &service_name);
    void addProtocolVersion();
    void addSessionId(uint32_t session_id);


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
