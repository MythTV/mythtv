/*
	
	mythlcd.h
	
    (c) 2002, 2003 Thor Sigvaldason and Isaac Richards
	
	Part of the mythTV project
	
	This header file just defines a demo widget that
	will let you see what the lcddevice is capable 
	of doing. 
	
*/


#ifndef MYTHLCD_H
#define MYTHLCD_H

#include <qwidget.h>
#include <qpushbutton.h>
#include <qapplication.h>
#include <qtimer.h>

#include "libmyth/lcddevice.h"
#include "libmyth/mythcontext.h"

class MythLCD : public QWidget
{
	Q_OBJECT
	
	public:
	
		MythLCD(QWidget *parent=0, const char *name=0);

	public slots:
	
		void	doTime();
		void	doChannel();
		void	doMusic();
		void	doProgress();
		void	doMenuPop();
		
		void	generateFakeMusicData();
		void	generateFakeProgressData();
		
	private:
	
		QTimer		*fakeMusicTimer;
		QTimer		*fakeProgressTimer;

		float		fakeValues[10];
		float		fakeProgress;
		int			valuesStep;
	
};

#endif	

