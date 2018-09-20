// MythNews headers
#include "newsarticle.h"

NewsArticle::NewsArticle(const QString &title,
                         const QString &desc, const QString &articleURL,
                         const QString &thumbnail, const QString &mediaURL,
                         const QString &enclosure) :
    m_title(title),
    m_desc(desc),
    m_articleURL(articleURL),
    m_thumbnail(thumbnail),
    m_mediaURL(mediaURL),
    m_enclosure(enclosure)
{
}

NewsArticle::NewsArticle(const QString &title, const QString &desc,
                         const QString &articleURL) :
    m_title(title),
    m_desc(desc),
    m_articleURL(articleURL)
{
}

NewsArticle::NewsArticle(const QString &title) :
    m_title(title)
{
}
