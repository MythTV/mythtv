#ifndef MTCPOUTPUT_H
#define MTCPOUTPUT_H
/*
	mtcpoutput.h

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for mdcap output object

*/

#include <stdint.h>

#include <qstring.h>
#include <qvaluevector.h>
#include <qvaluestack.h>

class MtcpOutput
{

  public:

    MtcpOutput();
    ~MtcpOutput();

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
    void addJobInfoGroup();
    void addJobGroup();
    void endGroup();
    

    //
    //  Simple type, but imply groups (eg. strings)
    //

    void addServiceName(const QString &service_name);
    void addJobMajorDescription(const QString &major_description);
    void addJobMinorDescription(const QString &minor_description);

    //
    //  "element" functions
    //


    void addProtocolVersion();
    void addJobCount(int count);
    void addJobId(int id);
    void addJobMajorProgress(int major_progress);
    void addJobMinorProgress(int minor_progress);

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
