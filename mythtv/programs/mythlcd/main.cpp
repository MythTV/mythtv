/*


*/

#include <stream.h>
#include <qapplication.h>
#include "mythlcd.h"
#include "libmyth/mythcontext.h"

//
//	Temp hack as can't link with libmyth unless this exists
//

MythContext *gContext;

int main(int argc, char **argv)
{
	//
	//	Temp hack
	//
	
	QApplication a(argc, argv);
	MythLCD mythLCD;
	a.setMainWidget(&mythLCD);
	mythLCD.show();
	return a.exec();

    
}
