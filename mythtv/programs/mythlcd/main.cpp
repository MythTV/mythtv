/*

	main.cpp
	
    (c) 2002, 2003 Thor Sigvaldason and Isaac Richards
	
	Part of the mythTV project
	
	Silly little program to demonstrate how the lcd
	device class works. 
	
	NB: Now works through MythContext *gContext

*/
	

#include <iostream>
#include <qapplication.h>
#include <qsqldatabase.h>
#include <qsqlquery.h>

using namespace std;

#include "mythlcd.h"
#include "libmyth/mythcontext.h"

MythContext *gContext;
QSqlDatabase* db;

int main(int argc, char **argv)
{
    QString lcd_host;
    int	lcd_port;
    QApplication a(argc, argv);
    gContext = new MythContext(MYTH_BINARY_VERSION, FALSE);

    db = QSqlDatabase::addDatabase("QMYSQL3");
    if (!gContext->OpenDatabase(db))
    {
        cerr << "Unable to open database:\n"
             << "Driver error was:" << endl
             << db->lastError().driverText() << endl
             << "Database error was:" << endl
             << db->lastError().databaseText() << endl;

        return -1;
    }

    lcd_host = gContext->GetSetting("LCDHost");
    lcd_port = gContext->GetNumSetting("LCDPort");

    if (lcd_host.length() > 0 && lcd_port > 1024)
    {
        gContext->LCDconnectToHost("localhost", 13666);
        MythLCD mythLCD;
        a.setMainWidget(&mythLCD);
        mythLCD.show();
        return a.exec();
    }
    else
    {
        cout << "Could not get LCD host and port settings from database" << endl;
        cout << "Did you run cvs.sql in the database directory?" << endl;
    }
}

