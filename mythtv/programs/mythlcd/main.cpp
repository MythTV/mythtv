/*


*/

#include <stream.h>
#include <qapplication.h>
#include "mythlcd.h"

int main(int argc, char **argv)
{

	QApplication a(argc, argv);
	MythLCD mythLCD;
	a.setMainWidget(&mythLCD);
	mythLCD.show();
	return a.exec();

    
}