/*

    lcddevice.h
    
    a MythTV project object to control an
    LCDproc server
    
    (c) 2002, 2003 Thor Sigvaldason and Isaac Richards

*/


#ifndef LCDDEVICE_H_
#define LCDDEVICE_H_

#include <iostream>
#include <qobject.h>
#include <qstringlist.h>
#include <qsocket.h>
#include <qtimer.h>
#include <qdatetime.h>
#include <math.h>

using namespace std;
#include "../../config.h"	// Or we never know if LCD_DEVICE is defined

class LCD : public QObject
{
	Q_OBJECT

    public:
    
        LCD();
		~LCD();
		
		//
		//	The following public methods are all you need to
		//	know to interface with LCD. 
		//
		
				//
				//	Used to actually connect to an LCD
				//	device (in early versions of this code,
				//	the constructor did the connecting).
				//
				
		void	connectToHost(QString hostname, unsigned int port);

				//
				//	When nothing else going on, show the time
				//

		void	switchToTime();
		
				//
				//	When playing music, switch to this and give
				//	artist and track name
				//
				//
				//	Note: the use of switchToMusic and setLevels
				//	is discouraged, because it has become obvious
				//	that most LCD devices cannot handle 
				//	communications fast enough to make them useful.
				//	
				
		void	switchToMusic(QString artist, QString track);

				//
				//	You can set 10 (or less) equalizer
				//	values here (between 0.0 and 1.0)
				//  (e.g. several times a second)
				//
				
		void	setLevels(int, float *);
	
				//
				//	For Live TV, supply the channel number,
				//	program title and subtitle
				//	
				//	Note that the "channel" screen can be used
				//	for any kind of progress meter (just put
				//	whatever you want in the strings, and update
				//	the progress as appropriate; see the demo app
				//	mythlcd for an example)
				//
				
		void	switchToChannel(QString channum = "", QString title = "", QString subtitle = "");

				//
				//	While watching Live/Recording/Pause Buffer, 
				//	occasionaly describe how much of the program
				//	has been seen (between 0.0 and 1.0)
				//  (  e.g. [current time - start time] / [end time - start time]  )
				//
				
		void	setChannelProgress(float percentViewed);
		
				//	
				//	If some other process should be getting all the
				//	LCDd screen time (e.g. mythMusic) we can use this
				//	to try and prevent and screens from showing up
				//	without having to actual destroy the LCD object
				//
				
		void	switchToNothing();
		
				//
				//	This briefly displaces a menu title and
				//	selection choice on the LCD. After a few 
				//	seconds, the previous display will reappear.
				//	This will allow users to navigate menus
				//	and execute commands without having to turn
				//	on their TV (when selecting a playlist in
				//	mythmusic, for example)
				//
				
		void	popMenu(QString menu_item, QString menu_title); 

                //
                //  If you want to be pleasant, call shutdown()
                //  before deleting your LCD device
                //
                
        void    shutdown();             //  close communications
    

	private slots:
	
		void	veryBadThings(int);		// 	Communication Errors
		void	serverSendingData();	//	Data coming back from LCDd

		void	outputTime();			//	Fire from a timer
		void	outputMusic();			//	Short timer (equalizer)
		void	outputChannel();		//	Longer timer (progress bar)

		void	scrollText();			//	Scroll the topline text
		void	beginScrollingText();	//	But only after a bit of time has gone by
		void	unPopMenu();			//	If no activity, remove the Pop Menu display
		
	private:
	
		void	sendToServer(QString);
		void	init();
		void	assignScrollingText(QString);
		void	outputCenteredTopText(QString, QString);
		void	outputLeftTopText(QString, QString);

		void	stopAll();
		void	startTime();
		void	startMusic(QString artist, QString track);
		void	startChannel(QString channum, QString title, QString subtitle);

		unsigned int		theMode;

		QSocket	*socket;
		QTimer	*timeTimer;
		QTimer	*musicTimer;
		QTimer	*channelTimer;
		QTimer	*scrollTimer;
		QTimer	*preScrollTimer;
		QTimer	*popMenuTimer;

		void	setWidth(unsigned int);
		void	setHeight(unsigned int);
		void	setCellWidth(unsigned int);
		void	setCellHeight(unsigned int);
		void	describeServer();

		unsigned int		lcdWidth;
		unsigned int		lcdHeight;
		unsigned int		cellWidth;
		unsigned int		cellHeight;
		
		float	EQlevels[10];
		float	progress;

		QString scrollingText;
		unsigned int	scrollPosition;
		
		bool	connected;
		
		QString send_buffer;

};

#endif
