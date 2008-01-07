#ifndef UPnpMEDIA_H_
#define UPnpMEDIA_H_

#include "mainserver.h"
#include "upnpcds.h"
              
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

class UPnpMedia
{
    private:

	QStringMap	       m_mapTitleNames;
	QStringMap             m_mapCoverArt;
	//QString                sMediaType;

        void FillMetaMaps (void);
	int GetBaseCount(void);
	QString GetTitleName(QString fPath, QString fName);
	QString GetCoverArt(QString fPath);

        int buildFileList(QString directory, int itemID, MSqlQuery &query);

	void RunRebuildLoop(void);
        static void *doUPnpMediaThread(void *param);

    public:

	UPnpMedia(bool runthread, bool master);

	~UPnpMedia();


	void SetMediaType( QString mediatype) { sMediaType = mediatype; };

        void BuildMediaMap(void);
	QString sMediaType;
};

#endif
