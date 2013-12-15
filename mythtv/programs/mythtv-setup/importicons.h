/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Author(s):
 *      John Pullan, Matthew Wire
 *
 * Description:
 */

#ifndef IMPORTICONS_H
#define IMPORTICONS_H

#include <QUrl>
#include <QDir>

#include "mythscreentype.h"

class MythUIButton;
class MythUIButtonList;
class MythUIButtonListItem;
class MythUITextEdit;
class MythUIText;
class MythUIProgressDialog;

class ImportIconsWizard : public MythScreenType
{
    Q_OBJECT

  public:
    ImportIconsWizard(MythScreenStack *parent, bool fRefresh,
                      const QString &channelname = "");
   ~ImportIconsWizard();

    bool Create(void);
    void Load(void);
//    bool keyPressEvent(QKeyEvent *);
    void customEvent(QEvent *event);

    struct SearchEntry               //! search entry results
    {
        QString strID;               //!< the remote channel id
        QString strName;             //!< the remote name
        QString strLogo;             //!< the actual logo
    };

  private:

    enum dialogState
    {
        STATE_NORMAL,
        STATE_SEARCHING,
        STATE_DISABLED
    };

    struct CSVEntry                  //! describes the TV channel name
    {
        QString strChanId;           //!< local channel id
        QString strName;             //!< channel name
        QString strXmlTvId;          //!< the xmltvid
        QString strCallsign;         //!< callsign
        QString strTransportId;      //!< transport id
        QString strAtscMajorChan;    //!< ATSC major number
        QString strAtscMinorChan;    //!< ATSC minor number
        QString strNetworkId;        //!< network id
        QString strServiceId;        //!< service id
        QString strIconCSV;          //!< icon name (csv form)
        QString strNameCSV;          //!< name (csv form)
    };
    //! List of CSV entries
    typedef QList<CSVEntry> ListEntries;
    //! iterator over list of CSV entries
    typedef QList<CSVEntry>::Iterator ListEntriesIter;

    ListEntries m_listEntries;       //!< list of TV channels to search for
    ListEntries m_missingEntries;    //!< list of TV channels with no unique icon
    ListEntriesIter m_iter;          //!< the current iterator
    ListEntriesIter m_missingIter;

    //! List of SearchEntry entries
    typedef QList<SearchEntry> ListSearchEntries;
    //! iterator over list of SearchEntry entries
    typedef QList<SearchEntry>::Iterator ListSearchEntriesIter;

    /*! \brief changes a string into csv format
     * \param str the string to change
     * \return the actual string
     */
    QString escape_csv(const QString& str);

    /*! \brief extracts the csv values out of a string
     * \param str the string to work on
     * \return the actual QStringList
     */
    QStringList extract_csv(const QString& strLine);

    /*! \brief use the equivalent of wget to fetch the POST command
     * \param url the url to send this to
     * \param strParam the string to send
     * \return the actual string
     */
    QString wget(QUrl& url,const QString& strParam);

    /*! \brief looks up the string to determine the caller/xmltvid
     * \param str the string to work on
     * \return true/false
     */
    bool lookup(const QString& strParam);

    /*! \brief search the remote db for icons etc
     * \param str the string to work on
     * \return true/false
     */
    bool search(const QString& strParam);

    /*! \brief submit the icon information back to the remote db
     * \return true/false
     */
    bool submit();

    /*! \brief retrieve the actual logo for the TV channel
     * \param str the string to work on
     * \return true/false
     */
    bool findmissing(const QString& strParam);

    /*! \brief checks and attempts to download the logo file to the appropriate
     *   place
     * \param url the icon url
     * \param localChanId the local ID number of the channel
     * \return true/false
     */
    bool checkAndDownload(const QString& url, const QString& localChanId);

    /*! \brief attempt the inital load of the TV channel information
     * \return true if successful
     */
    bool initialLoad(QString name="");

    /*! \brief attempts to move the iteration on one/more than one
     * \return true if we can go again or false if we cannot
     */
    bool doLoad();

  protected slots:
    void enableControls(dialogState state=STATE_NORMAL, bool selectEnabled=true);         //!< enable/disable the controls
    void manualSearch();           //!< process the manual search
    void menuSelection(MythUIButtonListItem *);//!< process the icon selection
    void skip();                   //!< skip this icon
    void askSubmit(const QString& strParam);
    void Close();

  private slots:
    void itemChanged(MythUIButtonListItem *item);

  protected:
    void Init(void);

  private:
    ListSearchEntries m_listSearch;  //!< the list of SearchEntry
    QString m_strMatches;            //!< the string for the submit() call

    QString m_strChannelDir;         //!< the location of the channel icon dir
    QString m_strChannelname;        //!< the channel name if searching for a single channel icon
    QString m_strParam;

    bool m_fRefresh;                 //!< are we doing a refresh or not
    int m_nMaxCount;                 //!< the maximum number of TV channels
    int m_nCount;                    //!< the current search point (0..m_nMaxCount)
    int m_missingMaxCount;           //!< the total number of missing icons
    int m_missingCount;              //!< the current search point (0..m_missingCount)

    const QString m_url;        //!< the default url
    QDir m_tmpDir;

    void startDialog();

    MythScreenStack    *m_popupStack;
    MythUIProgressDialog *m_progressDialog;

    MythUIButtonList *m_iconsList;    //!< list of potential icons
    MythUITextEdit   *m_manualEdit;  //!< manual edit field
    MythUIText       *m_nameText;    //!< name field for the icon
    MythUIButton     *m_manualButton;  //!< manual button field
    MythUIButton     *m_skipButton;    //!< button skip
    MythUIText       *m_statusText;

    MythUIImage      *m_preview;
    MythUIText       *m_previewtitle;

};

Q_DECLARE_METATYPE(ImportIconsWizard::SearchEntry)

#endif // IMPORTICONS_H
