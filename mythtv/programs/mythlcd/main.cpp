/*

	main.cpp
	
    (c) 2002, 2003 Thor Sigvaldason and Isaac Richards
	
	Part of the mythTV project
	
	Silly little program to demonstrate how the lcd
	device class works. 
	
	NB: Now works through MythContext *gContext

*/
	


#include <stream.h>
#include <qapplication.h>

using namespace std;

#include "mythlcd.h"
#include "libmyth/mythcontext.h"

MythContext *gContext;

int main(int argc, char **argv)
{
	QApplication a(argc, argv);
	gContext = new MythContext(FALSE);
	gContext->LCDconnectToHost("localhost", 13666);
	MythLCD mythLCD;
	a.setMainWidget(&mythLCD);
	mythLCD.show();
	return a.exec();
}
