#ifndef MTCPINPUT_H
#define MTCPINPUT_H
/*
	mtcpinput.h

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for mtcp input object

*/

#include <stdint.h>
#include <vector>
using namespace std;

#include <qvaluevector.h>


class MtcpInput
{

  public:

    MtcpInput(std::vector<char> *raw_data);
    MtcpInput(QValueVector<char> *raw_data);

    ~MtcpInput();

    char        peekAtNextCode();
    char        popGroup(QValueVector<char> *group_contents);
    char        popByte();
    uint16_t    popU16();
    uint32_t    popU32();
    uint        amountLeft();
    uint        size(){return amountLeft();}

    QString     popName();
    void        popProtocol(int *major, int *minor);   
    uint32_t    popJobCount();
    
    QString     popMajorDescription();
    QString     popMinorDescription();
    
    uint32_t    popJobId();
    uint32_t    popMajorProgress();
    uint32_t    popMinorProgress();

    void        printContents();    // Debugging
    
    
  private:

    QValueVector<char> contents;
    int pos;
};

#endif
