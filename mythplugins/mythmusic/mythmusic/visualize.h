/*

	visualize.h 

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Closely based on code from mq3 by Brad Hughes
	
	Part of the mythTV project
	
	music visualizers

*/



#ifndef VISUALIZE_H
#define VISUALIZE_H

#include "visual.h"
#include "mainvisual.h"
#include "constants.h"
#include "config.h"

#ifdef	FFTW_SUPPORT
#include <rfftw.h>
#include <fftw.h>
#endif		

#ifdef OPENGL_SUPPORT
#include <qgl.h>
#endif

class Spectrum : public VisualBase
{

	//
	//	This class draws bars (up and down)
	//	based on the magnitudes at various
	//	frequencies in the audio data.
	//	
		
	public:
		Spectrum();
		virtual ~Spectrum();

		//
		//	All mythmusic visualizers must implement
		//	these three functions
		//

		void resize(const QSize &size);
		bool process(VisualNode *node);
		bool draw(QPainter *p, const QColor &back = Qt::black);

	private:

		QColor startColor, targetColor;
		QMemArray<QRect> rects;
		QMemArray<double> magnitudes;
		QSize size;
		LogScale scale;
		double scaleFactor, falloff;
		int analyzerBarWidth, fps;

#ifdef FFTW_SUPPORT
		rfftw_plan plan;
		fftw_real lin[512], rin[512], lout[1024], rout[1024];
#endif
};


class Blank : public VisualBase
{

	//
	//	This draws ... well ... nothing
	//
	
	public:
		Blank();
		virtual ~Blank();

		void resize(const QSize &size);
		bool process(VisualNode *node = 0);
		bool draw(QPainter *p, const QColor &back = Qt::black);

	private:

		QSize 	size;
		int		fps;
};


#ifndef OPENGL_SUPPORT
#else

class Gears : public QGLWidget, public VisualBase
{


	//
	//	Draws some OpenGL gears and manipulates
	//	them based on audio data
	//
	
	
	public:
		Gears(QWidget *parent = 0, const char * = 0);
		virtual ~Gears();

		void resize(const QSize &size);
		bool process(VisualNode *node);
		bool draw(QPainter *p, const QColor &back);

	protected:
    	void initializeGL();
    	void resizeGL( int, int );
    	void paintGL();
		void drawTheGears();
		
	private:

		QColor startColor, targetColor;
		QMemArray<QRect> rects;
		QMemArray<double> magnitudes;
		QSize size;
		LogScale scale;
		double scaleFactor, falloff;
		int analyzerBarWidth, fps;
		GLfloat	angle, view_roty;

#ifdef FFTW_SUPPORT
		rfftw_plan plan;
		fftw_real lin[512], rin[512], lout[1024], rout[1024];
#endif

};


#endif // opengl_support	
#endif // __visualize_h
