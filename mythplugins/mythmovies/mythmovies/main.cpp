#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/mythpluginapi.h>
#include <mythtv/mythversion.h>

#include "moviesui.h"
#include "moviessettings.h"

void runMovies(void);
int setupDatabase();
MythPopupBox *configPopup;
const QString dbVersion = "4";

void setupKeys(void)
{
    //REG_JUMP("MythMovies", "Movie Times Screen", "", runMovies);
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythmovies", libversion,
                                    MYTH_BINARY_VERSION))
    {
        VERBOSE(VB_IMPORTANT,
                QString("libmythmovies.so/main.o: binary version mismatch"));
        return -1;
    }
    int dbSetup = setupDatabase();
    if (dbSetup == -1)
    {
        VERBOSE(VB_IMPORTANT, "MythMovies cannot continue without"
                "a proper database setup.");
        return -1;
    }
    setupKeys();
    return 0;
}

void runMovies(void)
{
    gContext->addCurrentLocation("mythmovies");
    MoviesUI moviesui(gContext->GetMainWindow(), "moviesui", "movies-");
    moviesui.exec();
    gContext->removeCurrentLocation();
}

void runConfig()
{
    MoviesSettings settings;
    settings.exec();
}

int mythplugin_run(void)
{
    gContext->ActivateSettingsCache(false);
    if (gContext->GetSetting("MythMovies.ZipCode") == "" ||
        gContext->GetSetting("MythMovies.Radius") == "" ||
        gContext->GetSetting("MythMovies.Grabber") == "")
    {
        runConfig();
    }
    if (gContext->GetSetting("MythMovies.ZipCode") == "" ||
        gContext->GetSetting("MythMovies.Radius") == "" ||
        gContext->GetSetting("MythMovies.Grabber") == "")
    {
        VERBOSE(VB_IMPORTANT,
                QString("Invalid configuration options supplied."));
        gContext->ActivateSettingsCache(true);
        return 0;
    }
    gContext->ActivateSettingsCache(true);
    runMovies();
    return 0;
}

int mythplugin_config(void)
{
    runConfig();
    return 0;
}

int setupDatabase()
{
    //we just throw away the old tables rather than worry about a database upgrade since movie times data
    //is highly transient and losing it isn't harmful
    if (dbVersion == gContext->GetSetting("MythMovies.DatabaseVersion"))
        return 0;

    gContext->SaveSetting("MythMovies.LastGrabDate", "");

    VERBOSE(VB_GENERAL, "Setting Up MythMovies Database Tables");

    MSqlQuery query(MSqlQuery::InitCon());
    if (query.exec("DROP TABLE IF EXISTS movies_showtimes, movies_theaters, movies_movies"))
    {
        bool a = query.exec("CREATE TABLE movies_theaters ("
                "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,"
                "theatername VARCHAR(100),"
                "theateraddress VARCHAR(100)"
                ");");

        bool b = query.exec("CREATE TABLE movies_movies ("
                "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,"
                "moviename VARCHAR(255),"
                "rating VARCHAR(10),"
                "runningtime VARCHAR(50)"
                ");");

        bool c = query.exec("CREATE TABLE movies_showtimes ("
                "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,"
                "theaterid INT NOT NULL,"
                "movieid INT NOT NULL,"
                "showtimes VARCHAR(255)"
                ");");

        if (!a || !b || !c)
        {
            VERBOSE(VB_IMPORTANT, "Failed to create MythMovies Tables");
            return -1;
        }
        else
        {
            gContext->SaveSetting("MythMovies.DatabaseVersion", dbVersion);
            VERBOSE(VB_GENERAL, "MythMovies Database Setup Complete");
            return 0;
        }
    }
    else
    {
        VERBOSE(VB_IMPORTANT, "Failed to delete old MythMovies Tables");
        return -1;
    }
}

