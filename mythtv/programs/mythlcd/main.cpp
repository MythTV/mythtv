/*

	main.cpp
	
    (c) 2002, 2003 Thor Sigvaldason and Isaac Richards
	
	Part of the mythTV project
	
	Silly little program to demonstrate how the lcd
	device class works. 
*/
	

#include <iostream>
#include <qapplication.h>
#include <qsqldatabase.h>
#include <qsqlquery.h>

using namespace std;

#include <lcddevice.h>
#include "mythlcd.h"
#include "libmyth/mythcontext.h"
#include "libmyth/mythdbcon.h"


int main(int argc, char **argv)
{
    QString lcd_host;
    int	lcd_port;
    QApplication a(argc, argv);

    gContext = NULL;
    gContext = new MythContext(MYTH_BINARY_VERSION);
    gContext->Init(false);

    if (!MSqlQuery::testDBConnection())
    {
        cerr << "Unable to open database.\n" << endl;
             //<< "Driver error was:" << endl
             //<< db->lastError().driverText() << endl
             //<< "Database error was:" << endl
             //<< db->lastError().databaseText() << endl;

        return -1;
    }

    lcd_host = gContext->GetSetting("LCDHost", "localhost");
    lcd_port = gContext->GetNumSetting("LCDPort", 13666);

    if (lcd_host.length() > 0 && lcd_port > 1024)
    {
        class LCD * lcd = LCD::Get();
        if (lcd->connectToHost(lcd_host, lcd_port) == false)
            return -1;

        MythLCD mythLCD;
        a.setMainWidget(&mythLCD);
        mythLCD.show();
        return a.exec();
    }
    else
    {
        cout << "Could not get LCD host and port settings from database." << endl;
        cout << "You do know that this is just a toy application that will " << endl;
        cout << "not suddenly make your MythTV box work with with an LCD device. " << endl;
    }
}

