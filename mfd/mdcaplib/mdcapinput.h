#ifndef MDCAPINPUT_H
#define MDCAPINPUT_H
/*
	mdcapinput.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for mdcap input object

*/

#include <stdint.h>
#include <vector>
using namespace std;

#include <qvaluevector.h>


class MdcapInput
{

  public:

    MdcapInput(std::vector<char> *raw_data);
    MdcapInput(QValueVector<char> *raw_data);

    ~MdcapInput();

    char        peekAtNextCode();
    char        popGroup(QValueVector<char> *group_contents);
    char        popByte();
    uint16_t    popU16();
    uint32_t    popU32();
    uint        size(){return contents.size();}

    QString     popName();
    int         popStatus();
    void        popProtocol(int *major, int *minor);   
    uint32_t    popSessionId();

  private:

    QValueVector<char> contents;
};

#endif
