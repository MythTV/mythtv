/*


*/

#include <stream.h>
#include <qapplication.h>
#include "mythlcd.h"
#include "settings.h"

//
//	Temp hack as can't link with libmyth unless this exists
//

Settings *globalsettings;
char installprefix[] = "/usr/local";


int main(int argc, char **argv)
{
	//
	//	Temp hack
	//
	
	globalsettings = new Settings;
    globalsettings->LoadSettingsFiles("theme.txt", installprefix);
    globalsettings->LoadSettingsFiles("mysql.txt", installprefix);
	

	QApplication a(argc, argv);
	MythLCD mythLCD;
	a.setMainWidget(&mythLCD);
	mythLCD.show();
	return a.exec();

    
}