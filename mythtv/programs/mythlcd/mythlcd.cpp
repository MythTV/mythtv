/*
	
	mythlcd.cpp
	
    (c) 2002 Thor Sigvaldason and Isaac Richards
	
	Part of the mythTV project
	
*/


#include "mythlcd.h"

MythLCD::MythLCD(QWidget *parent, const char *name )
	: QWidget(parent,name)
{

	//
	//	Create the LCD device
	//
	
	LCDdisplay = new LCD("localhost", 13666);

	resize(200, 210);

	//
	//	Create the buttons
	//
	
	QPushButton	*quitButton = new QPushButton("Quit", this);
	quitButton->resize(180,30);
	quitButton->move(10,170);
	connect(quitButton, SIGNAL(clicked()), qApp, SLOT(quit()));

	QPushButton *timeButton= new QPushButton("Time", this);
	timeButton->resize(180,30);
	timeButton->move(10,10);
	connect(timeButton, SIGNAL(clicked()), this, SLOT(doTime()));

	QPushButton *channelButton= new QPushButton("Channel", this);
	channelButton->resize(180,30);
	channelButton->move(10,50);
	connect(channelButton, SIGNAL(clicked()), this, SLOT(doChannel()));

	QPushButton *musicButton= new QPushButton("Music", this);
	musicButton->resize(180,30);
	musicButton->move(10,90);
	connect(musicButton, SIGNAL(clicked()), this, SLOT(doMusic()));

	QPushButton *progressButton= new QPushButton("Recording/Progress", this);
	progressButton->resize(180,30);
	progressButton->move(10,130);
	connect(progressButton, SIGNAL(clicked()), this, SLOT(doProgress()));
	
	
	//
	//	Add a timer for fake music data
	//
	
	fakeMusicTimer = new QTimer;
	connect(fakeMusicTimer, SIGNAL(timeout()), this, SLOT(generateFakeMusicData()));

	//
	//	Pregenerate some fake music data;
	//
	
	valuesStep = 0;
	for(int i = 0; i < 10; i++)
	{
		//
		//	Sin curve between -1.5 and 4.8 with min=0 and max=1
		//
		fakeValues[i] = (sin((float) (((i) / 1.746) - 1.5)) + 1.0) / 2.0; 
	}
	
	//
	//	And a fake progress generator
	//
	
	fakeProgressTimer = new QTimer;
	connect(fakeProgressTimer, SIGNAL(timeout()), this, SLOT(generateFakeProgressData()));
	fakeProgress = 0.0;	

}

void MythLCD::doTime()
{
	fakeMusicTimer->stop();
	fakeProgressTimer->stop();
	LCDdisplay->switchToTime();

}

void MythLCD::doChannel()
{
	fakeMusicTimer->stop();
	switch(rand() % 5)
	{
		case 0 :	
				LCDdisplay->switchToChannel("2", "Seinfeld", "");
				break;

		case 1 :	
				LCDdisplay->switchToChannel("3", "Star Trek", "Q Who?");
				break;

		case 2 :	
				LCDdisplay->switchToChannel("172", "Uzbekistan Today", "");
				break;

		case 3 :	
				LCDdisplay->switchToChannel("TELESAT", "Feiken Gehrman", "Zis ees Wooonky");
				break;

		case 4 :	
				LCDdisplay->switchToChannel("59 (Cartoon)", "Bugs and Daffy", "");
				break;
	}
	fakeProgress = 0.0;
	fakeProgressTimer->start(800, FALSE);
}

void MythLCD::doMusic()
{
	switch(rand() % 5)
	{
		case 0 :
					LCDdisplay->switchToMusic("2U", "1-1/4");
					break;
					
		case 1 :
					LCDdisplay->switchToMusic("Bernie", "Concrete Ducky");
					break;
		case 2 :
					LCDdisplay->switchToMusic("Vid Sicous", "Their Way");
					break;
		case 3 :
					LCDdisplay->switchToMusic("Idle Eric", "Edward the Quarter Bee");
					break;
		case 4 :
					LCDdisplay->switchToMusic("Dave Brebuck", "Take Half a Dozen");
					break;
	}
	
	//
	//	Start a timer to call
	//	generateFakeMusicLevels()
	//	every 100 milliseconds
	//
	
	fakeMusicTimer->start(100, FALSE);
}

void MythLCD::doProgress()
{
	fakeMusicTimer->stop();
	switch(rand() % 5)
	{
		case 0 :	
				LCDdisplay->switchToChannel("Pause Buffer", "", "");
				break;

		case 1 :	
				LCDdisplay->switchToChannel("Percent of CD Ripped", "", "");
				break;

		case 2 :	
				LCDdisplay->switchToChannel("Updating Program Data", "", "");
				break;

		case 3 :	
				LCDdisplay->switchToChannel("Game Level Completed", "", "");
				break;

		case 4 :	
				LCDdisplay->switchToChannel("This MythTV will self destruct in ...", "", "");
				break;
	}
	fakeProgress = 0.0;
	fakeProgressTimer->start(200, FALSE);
}

void MythLCD::generateFakeMusicData()
{
	int i, j;
	float tempValues[10];
	
	j = valuesStep;
	for(i =0; i < 10; i++)
	{
		tempValues[i] = fakeValues[j];
		j++;
		if(j >= 10)
		{
			j = 0;
		}			
	}
	valuesStep++;
	if(valuesStep >= 10)
	{
		valuesStep = 0;
	}
	LCDdisplay->setLevels(10, tempValues);
}

void MythLCD::generateFakeProgressData()
{

	fakeProgress = fakeProgress + 0.01;
	LCDdisplay->setChannelProgress(fakeProgress);
	if(fakeProgress >= 1.0)
	{
		fakeProgress = 0.0;
	}
}

MythLCD::~MythLCD()
{
	// bye bye
	delete LCDdisplay;
}

