// MythNews headers
#include "newsarticle.h"

NewsArticle::NewsArticle(QString title, QString desc, QString articleURL,
                         QString thumbnail, QString mediaURL, QString enclosure) :
    m_title(std::move(title)),
    m_desc(std::move(desc)),
    m_articleURL(std::move(articleURL)),
    m_thumbnail(std::move(thumbnail)),
    m_mediaURL(std::move(mediaURL)),
    m_enclosure(std::move(enclosure))
{
}

NewsArticle::NewsArticle(QString title, QString desc,
                         QString articleURL) :
    m_title(std::move(title)),
    m_desc(std::move(desc)),
    m_articleURL(std::move(articleURL))
{
}

NewsArticle::NewsArticle(QString title) :
    m_title(std::move(title))
{
}
