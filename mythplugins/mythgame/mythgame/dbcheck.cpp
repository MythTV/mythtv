#include <qstring.h>

#include <iostream>
using namespace std;

#include "dbcheck.h"

#include "mythtv/mythcontext.h"
#include "mythtv/mythdbcon.h"

#include "gamesettings.h"

const QString currentDatabaseVersion = "1007";

static bool UpdateDBVersionNumber(const QString &newnumber)
{
    // delete old schema version
    MSqlQuery query(MSqlQuery::InitCon());

    QString thequery = "DELETE FROM settings WHERE value='GameDBSchemaVer';";
    query.prepare(thequery);
    query.exec();

    if (query.lastError().type() != QSqlError::None)
    {
        QString msg =
            QString("DB Error (Deleting old DB version number): \n"
                    "Query was: %1 \nError was: %2 \nnew version: %3")
            .arg(thequery)
            .arg(MythContext::DBErrorMessage(query.lastError()))
            .arg(newnumber);
        VERBOSE(VB_IMPORTANT, msg);
        return false;
    }

    // set new schema version
    thequery = QString("INSERT INTO settings (value, data, hostname) "
                          "VALUES ('GameDBSchemaVer', %1, NULL);")
                         .arg(newnumber);
    query.prepare(thequery);
    query.exec();

    if (query.lastError().type() != QSqlError::None)
    {
        QString msg =
            QString("DB Error (Setting new DB version number): \n"
                    "Query was: %1 \nError was: %2 \nnew version: %3")
            .arg(thequery)
            .arg(MythContext::DBErrorMessage(query.lastError()))
            .arg(newnumber);
        VERBOSE(VB_IMPORTANT, msg);
        return false;
    }

    return true;
}

static bool performActualUpdate(const QString updates[], QString version,
                                QString &dbver)
{
    MSqlQuery query(MSqlQuery::InitCon());

    VERBOSE(VB_ALL, QString("Upgrading to MythGame schema version ") + 
            version);

    int counter = 0;
    QString thequery = updates[counter];

    while (thequery != "")
    {
        query.prepare(thequery);
        query.exec();

        if (query.lastError().type() != QSqlError::None)
        {
            QString msg =
                QString("DB Error (Performing database upgrade): \n"
                        "Query was: %1 \nError was: %2 \nnew version: %3")
                .arg(thequery)
                .arg(MythContext::DBErrorMessage(query.lastError()))
                .arg(version);
            VERBOSE(VB_IMPORTANT, msg);
            return false;
        }

        counter++;
        thequery = updates[counter];
    }

    if (!UpdateDBVersionNumber(version))
        return false;

    dbver = version;
    return true;
}

bool InitializeDatabase(void)
{
    VERBOSE(VB_ALL, "Inserting MythGame initial database information.");

    const QString updates[] = {
"CREATE TABLE IF NOT EXISTS gamemetadata ("
"    system VARCHAR(128) NOT NULL,"
"    romname VARCHAR(128) NOT NULL,"
"    gamename VARCHAR(128) NOT NULL,"
"    genre VARCHAR(128) NOT NULL,"
"    year INT UNSIGNED NOT NULL,"
"    INDEX (system),"
"    INDEX (year),"
"    INDEX (romname),"
"    INDEX (gamename),"
"    INDEX (genre)"
");",
"CREATE TABLE IF NOT EXISTS mamemetadata ("
"    romname VARCHAR(128) NOT NULL,"
"    manu VARCHAR(128) NOT NULL,"
"    cloneof VARCHAR(128) NOT NULL,"
"    romof VARCHAR(128) NOT NULL,"
"    driver VARCHAR(128) NOT NULL,"
"    cpu1 VARCHAR(128) NOT NULL,"
"    cpu2 VARCHAR(128) NOT NULL,"
"    cpu3 VARCHAR(128) NOT NULL,"
"    cpu4 VARCHAR(128) NOT NULL,"
"    sound1 VARCHAR(128) NOT NULL,"
"    sound2 VARCHAR(128) NOT NULL,"
"    sound3 VARCHAR(128) NOT NULL,"
"    sound4 VARCHAR(128) NOT NULL,"
"    players INT UNSIGNED NOT NULL,"
"    buttons INT UNSIGNED NOT NULL,"
"    INDEX (romname)"
");",
"CREATE TABLE IF NOT EXISTS nestitle ("
"  id int(11) default NULL,"
"  description varchar(160) default NULL,"
"  designer int(11) default NULL,"
"  publisher int(11) default NULL,"
"  releasedate year(4) default NULL,"
"  screenshot int(11) default NULL,"
"  keywords varchar(40) default NULL,"
"  FULLTEXT KEY description (description)"
");",
"CREATE TABLE IF NOT EXISTS neskeyword ("
"  keyword varchar(8) default NULL,"
"  value varchar(80) default NULL"
");",
};
    QString dbver = "";
    if (!performActualUpdate(updates, "1000", dbver))
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.exec("SELECT * FROM nestitle;");

    if (!query.isActive() || query.size() <= 0)
    {
        const QString updates2[] = {
"INSERT INTO nestitle VALUES (1,'10 Yard Fight',2,2,1985,897,'1PL 2PX FTB SPT');",
"INSERT INTO nestitle VALUES (10,'The Adventures Of Dino Riki',18,18,1989,415,'1PL ACT');",
"INSERT INTO nestitle VALUES (100,'Caesar\\'s Palace',78,78,1990,452,'1PL GMB');",
"INSERT INTO nestitle VALUES (101,'California Games',108,25,1989,453,'1PL 8PA SPT');",
"INSERT INTO nestitle VALUES (102,'Captain America And The Avengers',22,22,1991,454,'1PL ARC CBK');",
"INSERT INTO nestitle VALUES (103,'Captain Comic',43,43,1989,792,'1PL ACT ADV UNL');",
"INSERT INTO nestitle VALUES (104,'Captain Planet And The Planeteers',114,24,1991,455,'1PL TVA');",
"INSERT INTO nestitle VALUES (105,'Captain Skyhawk',7,25,1989,456,'1PL ACT SHO');",
"INSERT INTO nestitle VALUES (106,'Tiny Toon Cartoon Workshop',4,4,2000,0,'1PL');",
"INSERT INTO nestitle VALUES (107,'Casino Kid',32,32,1989,457,'1PL GMB PSS');",
"INSERT INTO nestitle VALUES (108,'Casino Kid 2',32,32,1989,0,'1PL GMB');",
"INSERT INTO nestitle VALUES (109,'Castelian',135,79,1991,836,'1PL ACT');",
"INSERT INTO nestitle VALUES (11,'Adventure Island',18,18,1988,790,'1PL ACT');",
"INSERT INTO nestitle VALUES (110,'Castle Of Deceit',80,80,1990,475,'1PL UNL');",
"INSERT INTO nestitle VALUES (111,'Castle of Dragon',66,66,1990,458,'1PL ACT');",
"INSERT INTO nestitle VALUES (112,'Castlequest',41,81,1989,837,'1PL ACT ADV PUZ');",
"INSERT INTO nestitle VALUES (113,'Castlevania',4,4,1987,347,'1PL ACT VMP');",
"INSERT INTO nestitle VALUES (114,'Castlevania II: Simon\\'s Quest',4,4,1988,348,'1PL ACT ADV PSS VMP');",
"INSERT INTO nestitle VALUES (115,'Castlevania III: Dracula\\'s Curse',4,4,1990,838,'1PL ACT ADV PSS VMP');",
"INSERT INTO nestitle VALUES (116,'Caveman Games',55,22,1990,459,'1PL');",
"INSERT INTO nestitle VALUES (117,'Challenge Of The Dragon',43,43,1990,839,'1PL UNL');",
"INSERT INTO nestitle VALUES (118,'Championship Bowling',82,82,1990,840,'1PL 4PA SPT');",
"INSERT INTO nestitle VALUES (119,'Championship Pool',136,24,1993,841,'1PL POO SPT');",
"INSERT INTO nestitle VALUES (12,'Adventure Island 2',18,18,1990,433,'1PL ACT');",
"INSERT INTO nestitle VALUES (120,'Chessmaster',119,17,1990,842,'1PL 2PX CHS STR');",
"INSERT INTO nestitle VALUES (121,'Chiller',137,83,1986,461,'1PL ARC GUN UNL');",
"INSERT INTO nestitle VALUES (122,'Chubby Cherub',28,28,1986,463,'1PL ACT');",
"INSERT INTO nestitle VALUES (123,'Circus Caper',36,36,1990,464,'1PL');",
"INSERT INTO nestitle VALUES (124,'City Connection',23,23,1988,465,'1PL 2PA ACT ARC');",
"INSERT INTO nestitle VALUES (125,'Clash At Demonhead',30,30,1989,366,'1PL ACT ADV PSS');",
"INSERT INTO nestitle VALUES (126,'Classic Concentration',111,37,1990,758,'1PL TVA');",
"INSERT INTO nestitle VALUES (127,'Cliffhanger',84,84,1993,844,'1PL MOV');",
"INSERT INTO nestitle VALUES (128,'Clu Clu Land',2,2,1985,466,'1PL 2PC ACT');",
"INSERT INTO nestitle VALUES (129,'Cobra Command',22,22,1988,467,'1PL ACT');",
"INSERT INTO nestitle VALUES (13,'Adventure Island 3',18,18,1992,434,'1PL');",
"INSERT INTO nestitle VALUES (130,'Cobra Triangle',7,2,1989,468,'1PL 2PA ACT');",
"INSERT INTO nestitle VALUES (131,'Code Name: Viper',3,3,1990,476,'1PL ACT PSS SHO');",
"INSERT INTO nestitle VALUES (132,'Color A Dinosaur',78,78,1993,845,'1PL KID');",
"INSERT INTO nestitle VALUES (133,'Commando',3,3,1986,469,'1PL ACT SHO');",
"INSERT INTO nestitle VALUES (134,'Conan',109,24,1990,470,'1PL CBK');",
"INSERT INTO nestitle VALUES (135,'Conflict',30,30,1990,846,'1PL 2PX BTT SIM STR');",
"INSERT INTO nestitle VALUES (136,'Conquest Of The Crystal Palace',53,53,1990,349,'1PL ACT');",
"INSERT INTO nestitle VALUES (137,'Contra',4,4,1988,350,'1PL 2PC ACT SHO');",
"INSERT INTO nestitle VALUES (138,'Contra Force',4,4,1992,847,'1PL 2PC ACT SHO');",
"INSERT INTO nestitle VALUES (139,'Cool World',65,65,1992,848,'1PL MOV');",
"INSERT INTO nestitle VALUES (14,'The Adventures Of Lolo',19,19,1989,370,'1PL ACT PSS PUZ');",
"INSERT INTO nestitle VALUES (140,'Cowboy Kid',138,82,1991,849,'1PL');",
"INSERT INTO nestitle VALUES (141,'The Incredible Crash Dummies',35,35,2000,901,'1PL ACT');",
"INSERT INTO nestitle VALUES (142,'Crash \\'n\\' The Boys Street Challenge',44,110,1993,471,'1PL 2PX 4PX MAR SPT TRK');",
"INSERT INTO nestitle VALUES (143,'Crystal Mines',43,43,1989,472,'1PL UNL');",
"INSERT INTO nestitle VALUES (144,'Crystalis',1,1,1990,351,'1PL ACT ADV');",
"INSERT INTO nestitle VALUES (145,'Cyberball',20,23,1991,473,'1PL');",
"INSERT INTO nestitle VALUES (146,'Cybernoid',8,8,1989,474,'1PL ACT SHO');",
"INSERT INTO nestitle VALUES (147,'Dance Aerobics',28,2,1988,850,'1PL PPD SPT');",
"INSERT INTO nestitle VALUES (148,'Darkman',65,65,1991,478,'1PL ACT MOV');",
"INSERT INTO nestitle VALUES (149,'Disney\\'s Darkwing Duck',3,3,1993,479,'1PL ACT TVA');",
"INSERT INTO nestitle VALUES (15,'The Adventures Of Lolo 2',19,19,1990,371,'1PL ACT PSS PUZ');",
"INSERT INTO nestitle VALUES (150,'Dash Galaxy In The Alien Asylum',96,22,1990,852,'1PL ACT ADV');",
"INSERT INTO nestitle VALUES (151,'Day Dreamin\\' Davey',105,19,1990,480,'1PL');",
"INSERT INTO nestitle VALUES (152,'Days Of Thunder',24,24,1990,902,'1PL DRV MOV RAC SPT');",
"INSERT INTO nestitle VALUES (153,'Deadly Towers',12,21,1987,496,'1PL ACT ADV PSS');",
"INSERT INTO nestitle VALUES (154,'Death Race',83,83,2000,0,'1PL MOV UNL');",
"INSERT INTO nestitle VALUES (155,'Deathbots',139,72,1990,853,'1PL UNL');",
"INSERT INTO nestitle VALUES (156,'Defender 2',152,19,1988,483,'1PL ACT SHO');",
"INSERT INTO nestitle VALUES (157,'Defender of the Crown',103,58,1989,484,'1PL');",
"INSERT INTO nestitle VALUES (158,'Defenders Of Dynatron City',106,75,1991,482,'1PL TVA');",
"INSERT INTO nestitle VALUES (159,'D��V',56,16,1991,485,'1PL ADV MYS RPG');",
"INSERT INTO nestitle VALUES (16,'The Adventures Of Lolo 3',19,19,1991,895,'1PL ACT PUZ');",
"INSERT INTO nestitle VALUES (160,'Demon Sword',10,10,1989,486,'1PL');",
"INSERT INTO nestitle VALUES (161,'Desert Commander',77,77,1989,854,'1PL 2PX SIM STR');",
"INSERT INTO nestitle VALUES (162,'Destination: Earthstar',112,8,1990,855,'1PL ACT ADV SHO');",
"INSERT INTO nestitle VALUES (163,'Destiny Of An Emperor',3,3,1990,487,'1PL ADV RPG');",
"INSERT INTO nestitle VALUES (164,'Dick Tracy',28,28,1990,497,'1PL ACT ADV MOV');",
"INSERT INTO nestitle VALUES (165,'Die Hard',100,38,1991,777,'1PL ACT MOV');",
"INSERT INTO nestitle VALUES (166,'Dig Dug II',51,28,1989,856,'1PL 2PA ACT ARC');",
"INSERT INTO nestitle VALUES (167,'Digger T. Rock',7,25,1990,490,'1PL');",
"INSERT INTO nestitle VALUES (168,'Dirty Harry',140,24,1990,491,'1PL MOV');",
"INSERT INTO nestitle VALUES (169,'Dizzy The Adventurer',97,62,1993,857,'1PL ADE UNL');",
"INSERT INTO nestitle VALUES (17,'The Adventures Of Tom Sawyer',66,66,1989,722,'1PL 2PA ACT BOK');",
"INSERT INTO nestitle VALUES (170,'Donkey Kong',2,2,1986,493,'1PL 2PA ACT ARC');",
"INSERT INTO nestitle VALUES (171,'Donkey Kong 3',2,2,1986,492,'1PL 2PA ACT ARC');",
"INSERT INTO nestitle VALUES (172,'Donkey Kong Classics',2,2,1988,860,'1PL 2PA ACT');",
"INSERT INTO nestitle VALUES (173,'Donkey Kong Jr.',2,2,1986,858,'1PL 2PA ACT ARC');",
"INSERT INTO nestitle VALUES (174,'Donkey Kong Jr. Math',2,2,1986,859,'1PL 2PA 2PX KID EDU');",
"INSERT INTO nestitle VALUES (175,'Double Dare',7,37,1990,903,'1PL 2PX GSH');",
"INSERT INTO nestitle VALUES (176,'Double Dragon',44,6,1988,352,'1PL 2PX 2PC ACT ARC MAR');",
"INSERT INTO nestitle VALUES (177,'Double Dragon II',44,8,1990,353,'1PL 2PC ACT MAR');",
"INSERT INTO nestitle VALUES (178,'Double Dragon III',44,8,1990,354,'1PL 2PC ACT MAR');",
"INSERT INTO nestitle VALUES (179,'Double Dribble',4,4,1987,481,'1PL 2PX BSK SPT');",
"INSERT INTO nestitle VALUES (18,'Afterburner',130,20,1989,794,'1PL ARC UNL');",
"INSERT INTO nestitle VALUES (180,'Double Strike',141,72,1990,862,'1PL ACT UNL');",
"INSERT INTO nestitle VALUES (181,'Dr. Chaos',11,11,1988,863,'1PL PSS');",
"INSERT INTO nestitle VALUES (182,'Dr. Jekyll & Mr. Hyde',28,28,1989,864,'1PL ACT BOK');",
"INSERT INTO nestitle VALUES (183,'Dr. Mario',2,2,1990,357,'1PL 2PX PUZ');",
"INSERT INTO nestitle VALUES (184,'Bram Stoker\\'s Dracula',131,84,1993,865,'1PL MOV VMP');",
"INSERT INTO nestitle VALUES (185,'Dragon Fighter',32,32,1991,494,'1PL');",
"INSERT INTO nestitle VALUES (186,'Dragon Power',28,28,1988,896,'1PL ACT ADV');",
"INSERT INTO nestitle VALUES (187,'Dragon Spirit',51,28,1990,355,'1PL ACT SHO');",
"INSERT INTO nestitle VALUES (188,'Dragon Warrior',34,2,2000,495,'1PL ADV BTT RPG');",
"INSERT INTO nestitle VALUES (189,'Dragon Warrior II',34,2,1990,356,'1PL ADV RPG');",
"INSERT INTO nestitle VALUES (19,'Air Fortress',19,19,1989,795,'1PL ACT ADV PSS SHO');",
"INSERT INTO nestitle VALUES (190,'Dragon Warrior III',34,34,1991,867,'1PL ADV RPG');",
"INSERT INTO nestitle VALUES (191,'Dragon Warrior IV',34,34,1992,868,'1PL ADV RPG');",
"INSERT INTO nestitle VALUES (192,'Dragon\\'s Lair',142,84,1990,869,'1PL ARC');",
"INSERT INTO nestitle VALUES (193,'Advanced Dungeons & Dragons: Dragonstrike',143,11,1992,870,'1PL');",
"INSERT INTO nestitle VALUES (194,'Duck Hunt',2,2,1985,498,'1PL GUN SHO SPT');",
"INSERT INTO nestitle VALUES (195,'Disney\\'s Duck Tales',3,3,1989,358,'1PL ACT TVA');",
"INSERT INTO nestitle VALUES (196,'Disney\\'s Duck Tales 2',3,3,1993,359,'1PL ACT TVA');",
"INSERT INTO nestitle VALUES (197,'Dudes With Attitude',72,72,1990,871,'1PL PUZ UNL');",
"INSERT INTO nestitle VALUES (198,'Dungeon Magic',50,10,1989,488,'1PL ADV RPG');",
"INSERT INTO nestitle VALUES (199,'Dusty Diamond\\'s All-Star Softball',144,21,1990,477,'1PL 2PX BAS SPT');",
"INSERT INTO nestitle VALUES (2,'1942',3,3,1986,339,'1PL 2PA ACT SHO');",
"INSERT INTO nestitle VALUES (20,'Airwolf',8,8,1989,435,'1PL ACT TVA');",
"INSERT INTO nestitle VALUES (200,'Dynowarz',28,28,1990,872,'1PL ACT PSS');",
"INSERT INTO nestitle VALUES (201,'Elevator Action',10,10,1987,873,'1PL 2PA ACT ARC');",
"INSERT INTO nestitle VALUES (202,'Elimonator: Boat Duel',67,67,2000,507,'1PL BOA RAC SPT');",
"INSERT INTO nestitle VALUES (203,'The Empire Strikes Back',40,75,1991,508,'1PL ACT MOV');",
"INSERT INTO nestitle VALUES (204,'Everet & Lendel Top Players Tennis',53,53,2000,959,'1PL 2PX 2PC SPT TNS');",
"INSERT INTO nestitle VALUES (205,'Excitebike',2,2,1985,509,'1PL 2PA RAC SPT');",
"INSERT INTO nestitle VALUES (206,'Exodus',61,61,1991,874,'1PL BBL UNL');",
"INSERT INTO nestitle VALUES (207,'F-117 Stealth Fighter',85,85,1992,876,'1PL');",
"INSERT INTO nestitle VALUES (208,'F-15 City War',72,72,1990,877,'1PL UNL');",
"INSERT INTO nestitle VALUES (209,'F-15 Strike Eagle',85,85,1991,878,'1PL');",
"INSERT INTO nestitle VALUES (21,'Al Unser Jr\\'s Turbo Racing',22,22,1990,796,'1PL BTT RAC SPT');",
"INSERT INTO nestitle VALUES (210,'Family Feud',96,37,1990,879,'1PL GSH');",
"INSERT INTO nestitle VALUES (211,'Fantasy Zone',20,20,1989,881,'1PL ARC UNL');",
"INSERT INTO nestitle VALUES (212,'Faria',145,81,1990,882,'1PL ACT ADV');",
"INSERT INTO nestitle VALUES (213,'Faxanadu',18,2,1989,512,'1PL ACT ADV RPG');",
"INSERT INTO nestitle VALUES (214,'Felix The Cat',18,18,1992,513,'1PL TVA');",
"INSERT INTO nestitle VALUES (215,'Ferrari Grand Prix',109,8,1992,883,'1PL RAC');",
"INSERT INTO nestitle VALUES (216,'Fester\\'s Quest',5,5,1989,362,'1PL ACT ADV TVA');",
"INSERT INTO nestitle VALUES (217,'Final Fantasy',39,2,1990,884,'1PL ADV RPG');",
"INSERT INTO nestitle VALUES (218,'Fire Hawk',97,62,1992,885,'1PL UNL');",
"INSERT INTO nestitle VALUES (219,'Fisher Price: Firehouse Rescue',112,37,1990,761,'1PL KID');",
"INSERT INTO nestitle VALUES (22,'Alfred Chicken',98,24,1993,414,'1PL');",
"INSERT INTO nestitle VALUES (220,'Fire N Ice',13,13,1993,760,'1PL ACT PSS PUZ');",
"INSERT INTO nestitle VALUES (221,'Fist Of The North Star',146,9,1989,888,'1PL ACT ANM MAR');",
"INSERT INTO nestitle VALUES (222,'Flight Of The Intruder',112,24,1990,762,'1PL ACT FLY MIL MOV');",
"INSERT INTO nestitle VALUES (223,'The Flintstones: The Rescue Of Dino & Hoppy',10,10,1991,889,'1PL TVA');",
"INSERT INTO nestitle VALUES (224,'The Flintstones: Surprise At Dino Rock',10,10,2000,0,'1PL');",
"INSERT INTO nestitle VALUES (225,'Flying Dragon: The Secret Scroll',27,27,1989,890,'1PL ACT PSS');",
"INSERT INTO nestitle VALUES (226,'Flying Warriors',27,27,1990,891,'1PL');",
"INSERT INTO nestitle VALUES (227,'Formula One: Built To Win',66,66,1990,875,'1PL RAC SPT');",
"INSERT INTO nestitle VALUES (228,'Frankenstein',28,28,1990,892,'1PL ACT BOK');",
"INSERT INTO nestitle VALUES (229,'Freedom Force',5,5,1988,893,'1PL 2PA GUN');",
"INSERT INTO nestitle VALUES (23,'Alien 3',131,35,1992,797,'1PL MOV');",
"INSERT INTO nestitle VALUES (230,'Friday The 13th',35,35,1988,510,'1PL ACT ADV MOV');",
"INSERT INTO nestitle VALUES (231,'Fun House',17,17,1990,894,'1PL GSH');",
"INSERT INTO nestitle VALUES (232,'Galactic Crusader',141,80,1990,759,'1PL ACT SHO SPC UNL');",
"INSERT INTO nestitle VALUES (233,'Galaga',28,28,1988,518,'1PL 2PA ACT SHO');",
"INSERT INTO nestitle VALUES (234,'Galaxy 5000',38,38,1990,904,'1PL 2PX RAC SPC SPT');",
"INSERT INTO nestitle VALUES (235,'Gargoyle\\'s Quest II: The Demon Darkness',3,3,1992,519,'1PL ADV');",
"INSERT INTO nestitle VALUES (236,'Gauntlet',20,20,2000,520,'1PL 2PC ACT ADV UNL');",
"INSERT INTO nestitle VALUES (237,'Gauntlet 2',20,24,2000,0,'1PL 2PC 4PC ACT ADV');",
"INSERT INTO nestitle VALUES (238,'Gemfire',70,70,2000,0,'1PL');",
"INSERT INTO nestitle VALUES (239,'Genghis Khan',70,70,1990,0,'1PL 4PA BTT RPG');",
"INSERT INTO nestitle VALUES (24,'Alien Syndrome',130,20,1989,798,'1PL 2PC ACT ADV ARC UNL');",
"INSERT INTO nestitle VALUES (240,'George Foreman KO Boxing',8,8,1992,521,'1PL BOX SPT');",
"INSERT INTO nestitle VALUES (241,'Ghostbusters',38,38,1988,525,'1PL 2PA ACT ADV MOV');",
"INSERT INTO nestitle VALUES (242,'Ghostbusters 2',38,38,1990,524,'1PL 2PA ACT MOV');",
"INSERT INTO nestitle VALUES (243,'Legend Of The Ghost Lion',77,77,2000,522,'1PL ADV RPG');",
"INSERT INTO nestitle VALUES (244,'Ghosts & Goblins',3,3,1986,363,'1PL 2PA ACT');",
"INSERT INTO nestitle VALUES (245,'Ghoul School',112,67,1991,950,'1PL ACT');",
"INSERT INTO nestitle VALUES (246,'G.I. Joe',9,9,2000,526,'1PL');",
"INSERT INTO nestitle VALUES (247,'G.I. Joe: The Atlantis Factor',3,3,2000,527,'1PL');",
"INSERT INTO nestitle VALUES (248,'Gilligan\\'s Island',28,28,2000,528,'1PL TVA');",
"INSERT INTO nestitle VALUES (249,'Goal!',23,23,1989,0,'1PL 2PX PSS SOC SPT');",
"INSERT INTO nestitle VALUES (25,'All Pro Basketball',30,30,1989,437,'1PL 2PX BSK SPT');",
"INSERT INTO nestitle VALUES (250,'Goal! 2',23,23,2000,0,'1PL SOC SPT');",
"INSERT INTO nestitle VALUES (251,'Godzilla',36,36,1989,529,'1PL ACT');",
"INSERT INTO nestitle VALUES (252,'Godzilla 2',36,36,2000,530,'1PL');",
"INSERT INTO nestitle VALUES (253,'Capcom\\'s Gold Medal Challenge 92',3,3,2000,531,'1PL SPT');",
"INSERT INTO nestitle VALUES (254,'Golf',2,2,1985,532,'1PL 2PX GLF SPT');",
"INSERT INTO nestitle VALUES (255,'Golf Grand Slam',86,86,1991,905,'1PL GLF SPT');",
"INSERT INTO nestitle VALUES (256,'Bandai Golf: Challenge Pebble Beach',28,28,1988,807,'1PL 2PA GLF SPT');",
"INSERT INTO nestitle VALUES (257,'Golf Power',78,78,2000,0,'1PL GLF SPT');",
"INSERT INTO nestitle VALUES (258,'Golgo 13: Top Secret Episode',30,30,1988,364,'1PL ACT');",
"INSERT INTO nestitle VALUES (259,'Goonies 2',4,4,1987,533,'1PL ACT ADV MOV PSS');",
"INSERT INTO nestitle VALUES (26,'Alpha Mission',1,1,1987,799,'1PL 2PA ACT');",
"INSERT INTO nestitle VALUES (260,'Gotcha!',35,35,1987,534,'1PL ACT GUN SHO');",
"INSERT INTO nestitle VALUES (261,'Gradius',4,4,1986,535,'1PL 2PA ACT SHO');",
"INSERT INTO nestitle VALUES (262,'Grand Prix',0,0,2000,0,'1PL RAC SPT');",
"INSERT INTO nestitle VALUES (263,'The Great Waldo Search',68,68,2000,0,'1PL PUZ');",
"INSERT INTO nestitle VALUES (264,'Gremlins 2: The New Batch',5,5,1991,536,'1PL ACT ADV MOV');",
"INSERT INTO nestitle VALUES (265,'The Guardian Legend',12,21,1989,0,'1PL ACT ADV PSS SHO');",
"INSERT INTO nestitle VALUES (266,'Guerilla War',1,1,1989,537,'1PL 2PC ACT');",
"INSERT INTO nestitle VALUES (267,'Gum Shoe',2,2,1986,0,'1PL GUN');",
"INSERT INTO nestitle VALUES (268,'Gun Nac',81,81,1990,0,'1PL ACT SHO');",
"INSERT INTO nestitle VALUES (269,'Gunsmoke',3,3,1988,538,'1PL ACT SHO');",
"INSERT INTO nestitle VALUES (27,'Amagon',26,26,1989,438,'1PL ACT');",
"INSERT INTO nestitle VALUES (270,'Gyruss',58,58,1988,0,'1PL 2PA ACT SHO');",
"INSERT INTO nestitle VALUES (271,'The Harlem Globetrotters',111,37,1990,539,'1PL BSK SPT');",
"INSERT INTO nestitle VALUES (272,'Hatris',87,87,2000,540,'1PL PUZ');",
"INSERT INTO nestitle VALUES (273,'Heavy Barrel',22,22,1990,541,'1PL 2PC ACT SHO');",
"INSERT INTO nestitle VALUES (274,'Heavy Shreddin\\'',112,88,1990,542,'1PL SNO SPT');",
"INSERT INTO nestitle VALUES (275,'Advanced Dungeons & Dragons: Heroes Of The Lance',150,11,1990,544,'1PL ACT ADV');",
"INSERT INTO nestitle VALUES (276,'High Speed',6,6,2000,0,'1PL');",
"INSERT INTO nestitle VALUES (277,'Advanced Dungeons & Dragons: Hillsfar',151,11,1992,0,'1PL ADV');",
"INSERT INTO nestitle VALUES (278,'Hogan\\'s Alley',2,2,1985,545,'1PL ACT GUN SHO');",
"INSERT INTO nestitle VALUES (279,'Hollywood Squares',37,37,1989,0,'1PL 2PX GSH');",
"INSERT INTO nestitle VALUES (28,'American Gladiators',132,37,1991,409,'1PL SPT TVA');",
"INSERT INTO nestitle VALUES (280,'Home Alone',113,68,1991,546,'1PL ACT MOV');",
"INSERT INTO nestitle VALUES (281,'Home Alone 2: Lost In New York',112,68,1991,547,'1PL ACT MOV');",
"INSERT INTO nestitle VALUES (282,'Hook',65,0,1991,906,'1PL MOV');",
"INSERT INTO nestitle VALUES (283,'Hoops',23,23,1989,548,'1PL 2PX BSK PSS SPT');",
"INSERT INTO nestitle VALUES (284,'Hudson Hawk',84,84,1991,550,'1PL ACT MOV');",
"INSERT INTO nestitle VALUES (285,'The Hunt For Red October',17,17,2000,551,'1PL MOV');",
"INSERT INTO nestitle VALUES (286,'Hydlide',11,11,1989,0,'1PL ACT ADV PSS RPG');",
"INSERT INTO nestitle VALUES (287,'Fisher-Price: I Can Remember',96,37,1990,886,'1PL 2PX KID EDU');",
"INSERT INTO nestitle VALUES (288,'Ice Climber',2,2,1985,552,'1PL 2PX ACT');",
"INSERT INTO nestitle VALUES (289,'Ice Hockey',2,2,1988,553,'1PL 2PX HOC SPT');",
"INSERT INTO nestitle VALUES (29,'Anticipation',7,2,1988,410,'1PL 4PX');",
"INSERT INTO nestitle VALUES (290,'Ikari Warriors',1,1,1987,554,'1PL 2PC ACT');",
"INSERT INTO nestitle VALUES (291,'Ikari Warriors II: Victory Road',1,1,1988,555,'1PL 2PC ACT');",
"INSERT INTO nestitle VALUES (292,'Ikari Warriors III: The Rescue',1,1,1990,556,'1PL 2PC ACT');",
"INSERT INTO nestitle VALUES (293,'Image Fight',12,12,1990,365,'1PL 2PA ACT SHO');",
"INSERT INTO nestitle VALUES (294,'The Immortal',54,55,1990,367,'1PL ACT ADV');",
"INSERT INTO nestitle VALUES (295,'Impossible Mission 2',72,72,2000,0,'1PL UNL');",
"INSERT INTO nestitle VALUES (296,'Indiana Jones & The Temple Of Doom',20,24,1988,561,'1PL ACT MOV');",
"INSERT INTO nestitle VALUES (297,'Indiana Jones & The Last Crusade',10,10,1990,560,'1PL ACT MOV');",
"INSERT INTO nestitle VALUES (298,'Danny Sullivan\\'s Indy Heat',7,6,1992,851,'1PL RAC SPT');",
"INSERT INTO nestitle VALUES (299,'Infiltrator',114,24,1989,557,'1PL ACT');",
"INSERT INTO nestitle VALUES (3,'1943: The Battle Of Midway',3,3,1988,340,'1PL ACT PSS SHO');",
"INSERT INTO nestitle VALUES (30,'Arch Rivals',7,8,1989,800,'1PL ARC BSK SPT');",
"INSERT INTO nestitle VALUES (300,'Iron Tank',1,1,2000,558,'1PL ACT PSS');",
"INSERT INTO nestitle VALUES (301,'Isolated Warrior',115,89,1990,559,'1PL ACT PSS');",
"INSERT INTO nestitle VALUES (302,'Jack Niklaus\\'s Greatest 18 Holes Of Major Championship Golf',4,4,1990,562,'1PL 4PA GLF SPT');",
"INSERT INTO nestitle VALUES (303,'Jackal',4,4,1989,563,'1PL 2PC ACT SHO');",
"INSERT INTO nestitle VALUES (304,'Jackie Chan Kung Fu Heroes',18,18,2000,0,'1PL ACT MAR');",
"INSERT INTO nestitle VALUES (305,'James Bond Jr.',68,68,1992,907,'1PL ACT');",
"INSERT INTO nestitle VALUES (306,'Jaws',35,35,1987,564,'1PL ACT MOV');",
"INSERT INTO nestitle VALUES (307,'Jeopardy!',7,37,1988,565,'1PL 3PX GSH');",
"INSERT INTO nestitle VALUES (308,'Jeopardy! Junior Edition',7,37,1989,567,'1PL 3PX GSH');",
"INSERT INTO nestitle VALUES (309,'Jeopardy! 25th Anniversary',7,37,1990,566,'1PL 3PX GSH');",
"INSERT INTO nestitle VALUES (31,'Archon',55,38,1990,801,'1PL 2PX STR');",
"INSERT INTO nestitle VALUES (310,'The Jetsons: Cogswell\\'s Caper',10,10,1992,568,'1PL TVA');",
"INSERT INTO nestitle VALUES (311,'Jimmy Connors Tennis',148,90,1993,908,'1PL SPT TNS');",
"INSERT INTO nestitle VALUES (312,'Joe And Mac',22,22,1991,909,'1PL 2PC ACT');",
"INSERT INTO nestitle VALUES (313,'John Elway\\'s Quarterback',6,6,1989,569,'1PL 2PX FTB SPT');",
"INSERT INTO nestitle VALUES (314,'Jordan vs. Bird: One on One',7,25,1989,570,'1PL 2PX BSK SPT');",
"INSERT INTO nestitle VALUES (315,'Joshua',61,61,2000,0,'1PL BBL UNL');",
"INSERT INTO nestitle VALUES (316,'Journey To Silius',5,5,1990,368,'1PL ACT');",
"INSERT INTO nestitle VALUES (317,'Joust',19,19,1988,930,'1PL 2PX ACT');",
"INSERT INTO nestitle VALUES (318,'Disney\\'s The Jungle Book',78,78,1994,572,'1PL ACT MOV');",
"INSERT INTO nestitle VALUES (319,'Jurassic Park',65,65,1993,910,'1PL MOV');",
"INSERT INTO nestitle VALUES (32,'Arkanoid',10,10,1987,802,'1PL 2PA ARC PUZ');",
"INSERT INTO nestitle VALUES (320,'Kabuki Quantum Fighter',19,19,1990,573,'1PL ACT');",
"INSERT INTO nestitle VALUES (321,'Karate Champ',22,22,1986,0,'1PL 2PX ACT MAR');",
"INSERT INTO nestitle VALUES (322,'Karate Kid',35,35,1987,361,'1PL 2PA ACT MAR MOV');",
"INSERT INTO nestitle VALUES (323,'Karnov',22,22,1988,574,'1PL 2PA ACT');",
"INSERT INTO nestitle VALUES (324,'Kickle Cubicle',12,12,1990,0,'1PL ACT PUZ');",
"INSERT INTO nestitle VALUES (325,'Kickmaster',10,10,1992,911,'1PL ACT MAR');",
"INSERT INTO nestitle VALUES (326,'Kid Icarus',2,2,1987,575,'1PL ACT ADV PSS');",
"INSERT INTO nestitle VALUES (327,'Kid Klown',77,77,2000,0,'1PL');",
"INSERT INTO nestitle VALUES (328,'Kid Kool',30,30,1990,576,'1PL ACT');",
"INSERT INTO nestitle VALUES (329,'Kid Niki',12,22,1987,577,'1PL 2PA ACT');",
"INSERT INTO nestitle VALUES (33,'Arkista\\'s Ring',26,26,1989,803,'1PL ADV');",
"INSERT INTO nestitle VALUES (330,'King Neptune\\'s Adventure',43,43,2000,0,'1PL UNL');",
"INSERT INTO nestitle VALUES (331,'The King Of Kings',61,61,1991,912,'1PL BBL UNL');",
"INSERT INTO nestitle VALUES (332,'King\\'s Knight',39,39,1989,360,'1PL ACT SHO');",
"INSERT INTO nestitle VALUES (333,'Kings Of The Beach',58,58,1989,0,'1PL 4PX SPT');",
"INSERT INTO nestitle VALUES (334,'King\\'s Quest 5',4,4,1991,914,'1PL ADV');",
"INSERT INTO nestitle VALUES (335,'Kirby\\'s Adventure',19,2,1993,578,'1PL ACT ADV BTT');",
"INSERT INTO nestitle VALUES (336,'Kiwi Kraze',10,10,1991,579,'1PL ACT');",
"INSERT INTO nestitle VALUES (337,'Klash Ball',116,32,1991,580,'1PL SPT');",
"INSERT INTO nestitle VALUES (338,'Klax',20,20,2000,0,'1PL PUZ UNL');",
"INSERT INTO nestitle VALUES (339,'Knight Rider',100,8,1989,581,'1PL ACT DRV TVA');",
"INSERT INTO nestitle VALUES (34,'Astyanax',23,23,1990,343,'1PL ACT');",
"INSERT INTO nestitle VALUES (340,'Krazy Kreatures',72,72,2000,0,'1PL UNL');",
"INSERT INTO nestitle VALUES (341,'The Krion Conquest',30,30,1990,582,'1PL ACT');",
"INSERT INTO nestitle VALUES (342,'Krusty\\'s Fun House',8,8,2000,0,'1PL ACT TVA');",
"INSERT INTO nestitle VALUES (343,'Kung Fu',12,2,1985,583,'1PL 2PA ACT MAR');",
"INSERT INTO nestitle VALUES (344,'Kung Fu Heroes',27,27,1989,0,'1PL 2PC ACT MAR');",
"INSERT INTO nestitle VALUES (345,'L\\'Empereur',70,70,2000,0,'1PL');",
"INSERT INTO nestitle VALUES (346,'Laser Invasion',4,4,1991,0,'1PL ACT SHO');",
"INSERT INTO nestitle VALUES (347,'Last Action Hero',84,84,1993,764,'1PL ACT MOV');",
"INSERT INTO nestitle VALUES (348,'Last Ninja',23,23,2000,0,'1PL ACT ADV NNJ');",
"INSERT INTO nestitle VALUES (349,'The Last Starfighter',24,24,2000,0,'1PL ACT MOV');",
"INSERT INTO nestitle VALUES (35,'Athena',1,1,1987,412,'1PL ACT');",
"INSERT INTO nestitle VALUES (350,'Lee Trevino\\'s Fighting Golf',1,1,1988,584,'1PL 4PA GLF SPT');",
"INSERT INTO nestitle VALUES (351,'Legacy Of The Wizard',47,21,1989,585,'1PL ACT ADV PSS');",
"INSERT INTO nestitle VALUES (352,'The Legend Of Kage',10,10,1987,763,'1PL 2PA ACT NNJ');",
"INSERT INTO nestitle VALUES (353,'The Legend Of Zelda',2,2,2000,407,'1PL ACT ADV BTT');",
"INSERT INTO nestitle VALUES (354,'Legendary Wings',3,3,2000,586,'1PL 2PC ACT SHO');",
"INSERT INTO nestitle VALUES (355,'Legends Of The Diamond',28,28,1990,587,'1PL BAS SPT');",
"INSERT INTO nestitle VALUES (356,'Lemmings',5,5,2000,0,'1PL PUZ STR');",
"INSERT INTO nestitle VALUES (357,'Lethal Weapon 3',65,65,2000,0,'1PL MOV');",
"INSERT INTO nestitle VALUES (358,'Life Force',4,4,1988,369,'1PL 2PC ACT SHO');",
"INSERT INTO nestitle VALUES (359,'Linus Spacehead',97,62,2000,0,'1PL UNL');",
"INSERT INTO nestitle VALUES (36,'Athletic World',28,28,1989,804,'1PL 2PA PPD SPT');",
"INSERT INTO nestitle VALUES (360,'Little League Baseball',1,1,1990,590,'1PL BAS SPT');",
"INSERT INTO nestitle VALUES (361,'Disney\\'s The Little Mermaid',3,3,1991,915,'1PL ACT MOV');",
"INSERT INTO nestitle VALUES (362,'Little Nemo The Dream Master',3,3,1990,591,'1PL ACT');",
"INSERT INTO nestitle VALUES (363,'Little Ninja Brothers',27,27,1990,589,'1PL ACT ADV MAR NNJ RPG');",
"INSERT INTO nestitle VALUES (364,'Little Samson',10,10,1992,765,'1PL ACT PSS');",
"INSERT INTO nestitle VALUES (365,'Championship Lode Runner',21,21,1987,0,'1PL 2PA ACT');",
"INSERT INTO nestitle VALUES (366,'The Lone Ranger',4,4,1991,592,'1PL ACT ADV GUN WST');",
"INSERT INTO nestitle VALUES (367,'Loopz',117,24,1990,593,'1PL');",
"INSERT INTO nestitle VALUES (368,'Low \\'G\\' Man',9,9,1990,594,'1PL ACT');",
"INSERT INTO nestitle VALUES (369,'Lunar Pool',11,11,1987,766,'1PL 2PA POO SPT');",
"INSERT INTO nestitle VALUES (37,'Attack of the Killer Tomatoes',68,68,1991,416,'1PL MOV');",
"INSERT INTO nestitle VALUES (370,'Mach Rider',2,2,1985,597,'1PL ACT RAC');",
"INSERT INTO nestitle VALUES (371,'Mad Max',24,24,1990,931,'1PL ACT MOV');",
"INSERT INTO nestitle VALUES (372,'The Mafat Conspiracy',30,30,1990,598,'1PL ACT DRV MAR SHO');",
"INSERT INTO nestitle VALUES (373,'Magic Darts',82,82,1991,600,'1PL DRT SPT');",
"INSERT INTO nestitle VALUES (374,'Magic Johnson\\'s Fast Break',6,6,1990,601,'1PL 4PX BSK SPT');",
"INSERT INTO nestitle VALUES (375,'The Magic of Scheherazade',27,27,1990,916,'1PL ACT ADV PSS RPG');",
"INSERT INTO nestitle VALUES (376,'Magician',9,9,1990,602,'1PL ADV');",
"INSERT INTO nestitle VALUES (377,'Magmax',11,11,1988,599,'1PL 2PA ACT');",
"INSERT INTO nestitle VALUES (378,'Major League Baseball',35,35,1988,603,'1PL 2PX BAS SPT');",
"INSERT INTO nestitle VALUES (379,'Maniac Mansion',40,23,1990,373,'1PL ADV PUZ');",
"INSERT INTO nestitle VALUES (38,'Baby Boomer',43,43,1989,805,'1PL GUN UNL');",
"INSERT INTO nestitle VALUES (380,'Mappyland',9,9,1989,917,'1PL 2PA ACT');",
"INSERT INTO nestitle VALUES (381,'Marble Madness',25,25,1989,918,'1PL 2PX ACT');",
"INSERT INTO nestitle VALUES (382,'Mario Bros.',2,2,1986,605,'1PL 2PC ACT');",
"INSERT INTO nestitle VALUES (383,'Mario Is Missing',118,119,1993,606,'1PL ACT ADV');",
"INSERT INTO nestitle VALUES (384,'Mario\\'s Time Machine',118,24,1994,919,'1PL TIM');",
"INSERT INTO nestitle VALUES (385,'Master Chu And The Drunkard Hu',120,43,1989,607,'1PL ACT');",
"INSERT INTO nestitle VALUES (386,'M.C. Kids',78,24,1991,595,'1PL ACT');",
"INSERT INTO nestitle VALUES (387,'Mechanized Attack',1,1,1990,608,'1PL ACT GUN');",
"INSERT INTO nestitle VALUES (388,'Mega Man',3,3,1987,609,'1PL ACT');",
"INSERT INTO nestitle VALUES (389,'Mega Man II',3,3,1989,375,'1PL ACT PSS');",
"INSERT INTO nestitle VALUES (39,'Back to the Future',96,35,1989,806,'1PL ACT MOV TIM');",
"INSERT INTO nestitle VALUES (390,'Mega Man III',3,3,1990,932,'1PL ACT PSS');",
"INSERT INTO nestitle VALUES (391,'Mega Man IV',3,3,1991,933,'1PL ACT PSS');",
"INSERT INTO nestitle VALUES (392,'Mega Man V',3,3,1992,376,'1PL ACT PSS');",
"INSERT INTO nestitle VALUES (393,'Mega Man VI',3,3,1993,377,'1PL ACT PSS');",
"INSERT INTO nestitle VALUES (394,'Menace Beach',43,43,1990,0,'1PL ACT SKA UNL');",
"INSERT INTO nestitle VALUES (395,'Mendel Palace',18,18,1990,610,'1PL ACT PUZ');",
"INSERT INTO nestitle VALUES (396,'Mermaids Of Atlantis',72,72,2000,0,'1PL UNL');",
"INSERT INTO nestitle VALUES (397,'Metal Fighter',120,43,1989,611,'1PL ACT SHO UNL');",
"INSERT INTO nestitle VALUES (398,'Metal Gear',4,58,1988,612,'1PL ACT ADV PSS');",
"INSERT INTO nestitle VALUES (399,'Metal Mech',105,23,1990,920,'1PL ACT ADV');",
"INSERT INTO nestitle VALUES (4,'720',20,24,1989,898,'1PL ARC SKA SPT');",
"INSERT INTO nestitle VALUES (40,'Back To The Future 2 & 3',96,35,1989,756,'1PL ACT MOV TIM');",
"INSERT INTO nestitle VALUES (400,'Metal Storm',12,12,1990,613,'1PL ACT');",
"INSERT INTO nestitle VALUES (401,'Metroid',2,2,1986,378,'1PL ACT ADV PSS');",
"INSERT INTO nestitle VALUES (402,'Michael Andretti\\'s World Grand Prix',26,26,1990,757,'1PL RAC SPT');",
"INSERT INTO nestitle VALUES (403,'Mickey Mousecapade',3,3,1988,0,'1PL ACT');",
"INSERT INTO nestitle VALUES (404,'Mickey\\'s Adventure In Numberland',17,17,2000,0,'1PL KID EDU');",
"INSERT INTO nestitle VALUES (405,'Mickey\\'s Safari In Letterland',17,17,2000,0,'1PL');",
"INSERT INTO nestitle VALUES (406,'Micro Machines',97,62,1991,614,'1PL RAC UNL');",
"INSERT INTO nestitle VALUES (407,'MIG-29',62,62,2000,0,'1PL UNL');",
"INSERT INTO nestitle VALUES (408,'Might And Magic',121,26,1991,615,'1PL ADV RPG');",
"INSERT INTO nestitle VALUES (409,'Mighty Bomb Jack',13,13,2000,616,'1PL ACT');",
"INSERT INTO nestitle VALUES (41,'Bad Dudes',22,22,2000,432,'1PL 2PA ACT ARC MAR NNJ');",
"INSERT INTO nestitle VALUES (410,'Mighty Final Fight',3,3,1993,934,'1PL ACT MAR');",
"INSERT INTO nestitle VALUES (411,'Mike Tyson\\'s Punch Out!!',2,2,1987,0,'1PL BOX PSS SPT');",
"INSERT INTO nestitle VALUES (412,'Millipede',19,19,1988,617,'1PL 2PA ACT');",
"INSERT INTO nestitle VALUES (413,'Milon\\'s Secret Castle',18,18,1988,379,'1PL ACT ADV');",
"INSERT INTO nestitle VALUES (414,'Mission Cobra',80,80,1990,935,'1PL UNL');",
"INSERT INTO nestitle VALUES (415,'Mission: Impossible',4,58,1990,618,'1PL ACT ADV');",
"INSERT INTO nestitle VALUES (416,'Monopoly',105,88,1991,619,'1PL 2PX BGA');",
"INSERT INTO nestitle VALUES (417,'Monster In My Pocket',4,4,1991,620,'1PL ACT');",
"INSERT INTO nestitle VALUES (418,'Monster Party',28,28,1989,0,'1PL ACT PSS');",
"INSERT INTO nestitle VALUES (419,'Monster Truck Rally',91,91,2000,0,'1PL ACT RAC');",
"INSERT INTO nestitle VALUES (42,'Bad News Baseball',13,13,1990,418,'1PL BAS SPT');",
"INSERT INTO nestitle VALUES (420,'Moon Ranger',80,80,2000,0,'1PL UNL');",
"INSERT INTO nestitle VALUES (421,'Motor City Patrol',122,92,1991,621,'1PL ACT ADV DRV');",
"INSERT INTO nestitle VALUES (422,'Ms. Pac-Man',51,20,1990,622,'1PL ACT UNL');",
"INSERT INTO nestitle VALUES (423,'M.U.L.E.',24,24,1990,623,'1PL 4PX');",
"INSERT INTO nestitle VALUES (424,'Jim Henson\\'s Muppet Adventure: Chaos at the Carnival',123,17,1990,625,'1PL ACT BOA KID DRV FLY SPC');",
"INSERT INTO nestitle VALUES (425,'M.U.S.C.L.E.',28,28,1986,596,'1PL 2PX SPT WRE');",
"INSERT INTO nestitle VALUES (426,'Mutant Virus',93,93,2000,0,'1PL');",
"INSERT INTO nestitle VALUES (427,'Mystery Quest',9,9,1989,624,'1PL ACT');",
"INSERT INTO nestitle VALUES (428,'NARC',7,8,1989,0,'1PL ACT');",
"INSERT INTO nestitle VALUES (429,'Bill Elliott\\'s NASCAR Challenge',107,4,1991,449,'1PL RAC SPT');",
"INSERT INTO nestitle VALUES (43,'Bad Street Brawler',96,69,1989,417,'1PL 2PA ACT MAR');",
"INSERT INTO nestitle VALUES (430,'NES Open Tournament Golf',2,2,1991,0,'1PL GLF SPT');",
"INSERT INTO nestitle VALUES (431,'NFL Football',35,35,1989,627,'1PL 2PX FTB SPT');",
"INSERT INTO nestitle VALUES (432,'Nigel Mansell-World Class Racing',37,37,2000,0,'1PL RAC SPT');",
"INSERT INTO nestitle VALUES (433,'A Nightmare On Elm Street',7,35,1989,628,'1PL 2PC ACT MOV');",
"INSERT INTO nestitle VALUES (434,'Nightshade',96,58,1991,0,'1PL ACT ADV MYS');",
"INSERT INTO nestitle VALUES (435,'Ninja Crusaders',26,26,2000,0,'1PL NNJ');",
"INSERT INTO nestitle VALUES (436,'Ninja Gaiden',13,13,1989,380,'1PL ACT MAR NNJ');",
"INSERT INTO nestitle VALUES (437,'Ninja Gaiden II: The Dark Sword Of Chaos',13,13,1990,381,'1PL ACT MAR NNJ');",
"INSERT INTO nestitle VALUES (438,'Ninja Gaiden III: The Ancient Ship Of Doom',13,13,1991,382,'1PL ACT MAR NNJ');",
"INSERT INTO nestitle VALUES (439,'Ninja Kid',28,28,1986,0,'1PL ACT NNJ');",
"INSERT INTO nestitle VALUES (44,'Balloon Fight',2,2,1986,419,'1PL 2PX ACT');",
"INSERT INTO nestitle VALUES (440,'Nobunaga\\'s Ambition',70,70,1989,0,'1PL 8PX BTT SIM STR');",
"INSERT INTO nestitle VALUES (441,'Nobunaga\\'s Ambition 2',70,70,2000,0,'1PL SIM STR');",
"INSERT INTO nestitle VALUES (442,'North And South',77,77,2000,0,'1PL SIM STR');",
"INSERT INTO nestitle VALUES (443,'Operation Wolf',10,10,1989,629,'1PL ACT GUN SHO');",
"INSERT INTO nestitle VALUES (444,'Orb 3D',119,17,1990,630,'1PL PUZ');",
"INSERT INTO nestitle VALUES (445,'Othello',8,8,1988,631,'1PL 2PX BGA');",
"INSERT INTO nestitle VALUES (446,'Overlord',78,78,1992,921,'1PL');",
"INSERT INTO nestitle VALUES (447,'P\\'radikus Conflict',43,43,2000,768,'1PL ACT SPC UNL');",
"INSERT INTO nestitle VALUES (448,'Pac-Man',51,20,2000,922,'1PL ACT UNL');",
"INSERT INTO nestitle VALUES (449,'Pac-Mania',20,20,2000,0,'1PL ACT UNL');",
"INSERT INTO nestitle VALUES (45,'Bandit Kings Of Ancient China',70,70,1991,808,'1PL SIM STR');",
"INSERT INTO nestitle VALUES (450,'Palamedes',71,71,1990,767,'1PL');",
"INSERT INTO nestitle VALUES (451,'Panic Restaurant',10,10,2000,0,'1PL');",
"INSERT INTO nestitle VALUES (452,'Paperboy',24,24,1988,0,'1PL 2PA ACT');",
"INSERT INTO nestitle VALUES (453,'Paperboy 2',24,24,1991,923,'1PL ACT');",
"INSERT INTO nestitle VALUES (454,'Fisher-Price: Perfect Fit',96,37,1990,887,'1PL 2PX KID EDU');",
"INSERT INTO nestitle VALUES (455,'Pesterminator',43,43,2000,0,'1PL UNL');",
"INSERT INTO nestitle VALUES (456,'Peter Pan And The Pirates',124,68,1990,633,'1PL ACT');",
"INSERT INTO nestitle VALUES (457,'Phantom Fighter',11,11,1990,634,'1PL ACT MAR');",
"INSERT INTO nestitle VALUES (458,'Pictionary',125,35,1990,635,'1PL BGA');",
"INSERT INTO nestitle VALUES (459,'Pinball',2,2,1985,636,'1PL 2PA PIN');",
"INSERT INTO nestitle VALUES (46,'Barbie',112,17,1991,420,'1PL');",
"INSERT INTO nestitle VALUES (460,'Pinball Quest',23,23,2000,0,'1PL GLF PIN RPG SPT');",
"INSERT INTO nestitle VALUES (461,'Pinbot',2,2,1990,0,'1PL 4PA PIN');",
"INSERT INTO nestitle VALUES (462,'Pipe Dream',87,87,1990,924,'1PL ACT');",
"INSERT INTO nestitle VALUES (463,'Pirates!',7,58,1991,925,'1PL ACT ADV RPG');",
"INSERT INTO nestitle VALUES (464,'Platoon',5,5,1988,637,'1PL ACT MOV');",
"INSERT INTO nestitle VALUES (465,'NES Play Action Football',2,2,1990,626,'1PL FTB SPT');",
"INSERT INTO nestitle VALUES (466,'Advanced Dungeons & Dragons: Pool Of Radiance',151,11,1991,0,'1PL');",
"INSERT INTO nestitle VALUES (467,'Popeye',2,2,1986,639,'1PL 2PA ACT TVA');",
"INSERT INTO nestitle VALUES (468,'P.O.W.',1,1,1989,640,'1PL ACT');",
"INSERT INTO nestitle VALUES (469,'Power Punch 2',93,93,2000,0,'1PL');",
"INSERT INTO nestitle VALUES (47,'Bard\\'s Tale',55,11,1991,421,'1PL ADV RPG');",
"INSERT INTO nestitle VALUES (470,'Power Blade',10,10,1991,926,'1PL ACT PSS');",
"INSERT INTO nestitle VALUES (471,'Power Blade 2',10,10,1992,927,'1PL ACT PSS');",
"INSERT INTO nestitle VALUES (472,'Predator',38,38,1989,641,'1PL ACT MOV');",
"INSERT INTO nestitle VALUES (473,'Prince Of Persia',126,78,1992,638,'1PL ACT ADV');",
"INSERT INTO nestitle VALUES (474,'Princess Tomato In The Salad Kingdom',18,18,1990,399,'1PL ADV RPG');",
"INSERT INTO nestitle VALUES (475,'Pro Sport Hockey',23,23,2000,0,'1PL HOC SPT');",
"INSERT INTO nestitle VALUES (476,'Pro Wrestling',2,2,1987,642,'1PL 2PX SPT WRE');",
"INSERT INTO nestitle VALUES (477,'Pugsley\\'s Scavenger Hunt',65,65,2000,0,'1PL');",
"INSERT INTO nestitle VALUES (478,'Punch Out!!',2,2,2000,0,'1PL BOX SPT');",
"INSERT INTO nestitle VALUES (479,'Punisher',96,35,1990,0,'1PL ACT SHO');",
"INSERT INTO nestitle VALUES (48,'Baseball',2,2,1985,423,'1PL 2PX BAS SPT');",
"INSERT INTO nestitle VALUES (480,'Puss \\'N\\' Boots',67,67,1990,0,'1PL 2PA ACT');",
"INSERT INTO nestitle VALUES (481,'Puzzle',72,72,2000,643,'1PL PUZ UNL');",
"INSERT INTO nestitle VALUES (482,'Puzznic',10,10,1990,769,'1PL PUZ');",
"INSERT INTO nestitle VALUES (483,'Pyramid',141,72,1990,949,'1PL PUZ UNL');",
"INSERT INTO nestitle VALUES (484,'Q*Bert',4,58,1988,644,'1PL 2PA ACT');",
"INSERT INTO nestitle VALUES (485,'Qix',10,10,2000,0,'1PL ACT PUZ');",
"INSERT INTO nestitle VALUES (486,'Quattro Adventure',97,62,2000,0,'1PL ADV UNL');",
"INSERT INTO nestitle VALUES (487,'Quattro Arcade',62,62,2000,0,'1PL ACT UNL');",
"INSERT INTO nestitle VALUES (488,'Quattro Sports',62,62,2000,0,'1PL SPT UNL');",
"INSERT INTO nestitle VALUES (489,'Race America',112,0,1991,645,'1PL RAC SPT');",
"INSERT INTO nestitle VALUES (49,'Baseball Simulator 1.000',27,27,1990,413,'1PL 2PX BAS BTT SPT');",
"INSERT INTO nestitle VALUES (490,'Racket Attack',23,23,1988,656,'1PL 2PX PSS SPT TNS');",
"INSERT INTO nestitle VALUES (491,'The Adventures Of Rad Gravity',57,38,1990,646,'1PL ACT PSS');",
"INSERT INTO nestitle VALUES (492,'Rad Racer',39,2,1987,647,'1PL RAC SPT');",
"INSERT INTO nestitle VALUES (493,'Rad Racer 2',39,39,1990,384,'1PL RAC SPT');",
"INSERT INTO nestitle VALUES (494,'Rad Racket',72,72,2000,0,'1PL SPT TNS UNL');",
"INSERT INTO nestitle VALUES (495,'Raid 2020',43,43,1989,648,'1PL UNL');",
"INSERT INTO nestitle VALUES (496,'Raid On Bungeling Bay',21,21,1987,649,'1PL 2PA ACT SHO');",
"INSERT INTO nestitle VALUES (497,'Rainbow Islands: The Story Of Bubble Bobble 2',10,10,1991,779,'1PL ACT');",
"INSERT INTO nestitle VALUES (498,'Rally Bike',82,82,2000,0,'1PL');",
"INSERT INTO nestitle VALUES (499,'Rambo',100,8,1987,651,'1PL ACT MOV PSS');",
"INSERT INTO nestitle VALUES (5,'8 Eyes',66,9,1990,899,'1PL 2PC ACT ADV PSS');",
"INSERT INTO nestitle VALUES (50,'Baseball Stars',1,1,1989,443,'1PL 2PX BAS BTT SPT');",
"INSERT INTO nestitle VALUES (500,'Rampage',22,22,1988,652,'1PL 2PC ACT');",
"INSERT INTO nestitle VALUES (501,'Rampart',23,23,2000,782,'1PL ACT STR');",
"INSERT INTO nestitle VALUES (502,'R.B.I. Baseball',20,20,1987,653,'1PL BAS SPT UNL');",
"INSERT INTO nestitle VALUES (503,'R.B.I. Baseball 2',20,20,1990,654,'1PL BAS SPT UNL');",
"INSERT INTO nestitle VALUES (504,'R.B.I. Baseball 3',20,20,1991,0,'1PL BAS SPT UNL');",
"INSERT INTO nestitle VALUES (505,'R.C. Pro-Am Racing',7,2,1988,0,'1PL RAC SPT');",
"INSERT INTO nestitle VALUES (506,'R.C. Pro-Am Racing 2',7,6,1992,0,'1PL RAC SPT');",
"INSERT INTO nestitle VALUES (507,'Remote Control',127,17,1990,658,'1PL 2PX GSH');",
"INSERT INTO nestitle VALUES (508,'Ren And Stimpy: Buckaroos',68,68,1993,660,'1PL TVA');",
"INSERT INTO nestitle VALUES (509,'Renegade',10,10,1988,659,'1PL 2PA ACT MAR');",
"INSERT INTO nestitle VALUES (51,'Baseball Stars II',1,82,1992,425,'1PL BAS SPT');",
"INSERT INTO nestitle VALUES (510,'Rescue: The Embassy Mission',77,77,1990,661,'1PL ACT ADV');",
"INSERT INTO nestitle VALUES (511,'Disney\\'s Chip \\'n Dale Rescue Rangers',3,3,1990,462,'1PL 2PC ACT TVA');",
"INSERT INTO nestitle VALUES (512,'Disney\\'s Chip \\'n Dale Rescue Rangers 2',3,3,1993,928,'1PL ACT TVA');",
"INSERT INTO nestitle VALUES (513,'Ring King',22,22,1987,662,'1PL BOX PSS SPT');",
"INSERT INTO nestitle VALUES (514,'River City Ransom',44,44,1989,385,'1PL 2PX 2PC ACT ADV MAR PSS');",
"INSERT INTO nestitle VALUES (515,'Road Blasters',96,24,1990,663,'1PL ACT DRV SHO');",
"INSERT INTO nestitle VALUES (516,'Road Runner',20,20,2000,0,'1PL UNL');",
"INSERT INTO nestitle VALUES (517,'Robin Hood: Prince Of Thieves',105,78,1991,306,'1PL ACT ADV MOV');",
"INSERT INTO nestitle VALUES (518,'Robocop',65,22,1989,664,'1PL ACT MOV');",
"INSERT INTO nestitle VALUES (519,'Robocop 2',65,22,1990,975,'1PL ACT MOV');",
"INSERT INTO nestitle VALUES (52,'Bases Loaded',23,23,1988,810,'1PL 2PX BAS PSS SPT');",
"INSERT INTO nestitle VALUES (520,'Robocop 3',65,65,1992,666,'1PL ACT MOV');",
"INSERT INTO nestitle VALUES (521,'Robo Demons',43,43,1990,0,'1PL UNL');",
"INSERT INTO nestitle VALUES (522,'Robo Warrior',23,23,1988,977,'1PL ADV');",
"INSERT INTO nestitle VALUES (523,'Rock N Ball',115,89,1990,667,'1PL 6PA ACT');",
"INSERT INTO nestitle VALUES (524,'Rocket Ranger',77,77,2000,0,'1PL');",
"INSERT INTO nestitle VALUES (525,'Rocketeer',28,28,1991,657,'1PL ACT MOV SHO');",
"INSERT INTO nestitle VALUES (526,'Rockin\\' Kats',86,86,1991,668,'1PL ACT');",
"INSERT INTO nestitle VALUES (527,'The Adventures Of Rocky & Bullwinkle',118,68,1991,793,'1PL ACT TVA');",
"INSERT INTO nestitle VALUES (528,'Roger Clemens MVP Baseball',105,35,1991,655,'1PL BAS SPT');",
"INSERT INTO nestitle VALUES (529,'Who Framed Roger Rabbit?',35,35,1989,0,'1PL ADV MOV PSS');",
"INSERT INTO nestitle VALUES (53,'Bases Loaded 2',23,23,1989,811,'1PL BAS PSS SPT');",
"INSERT INTO nestitle VALUES (530,'Rollerball',19,19,1988,669,'1PL PIN');",
"INSERT INTO nestitle VALUES (531,'Rollergames',4,58,1990,670,'1PL ACT');",
"INSERT INTO nestitle VALUES (532,'Rollerblade Racer',17,17,2000,0,'1PL');",
"INSERT INTO nestitle VALUES (533,'Rolling Thunder',20,20,1989,929,'1PL ACT PSS UNL');",
"INSERT INTO nestitle VALUES (534,'Romance of The Three Kingdoms',70,70,1989,671,'1PL 8PA RPG SIM STR');",
"INSERT INTO nestitle VALUES (535,'Romance of The Three Kingdoms II',70,70,2000,0,'1PL');",
"INSERT INTO nestitle VALUES (536,'Roundball 2-On-2 Challenge',24,24,1992,672,'1PL BSK SPT');",
"INSERT INTO nestitle VALUES (537,'Rush N Attack',4,4,1987,673,'1PL 2PA ACT');",
"INSERT INTO nestitle VALUES (538,'Rygar',13,13,2000,386,'1PL ACT ADV');",
"INSERT INTO nestitle VALUES (539,'S.C.A.T.',50,50,1991,675,'1PL 2PC ACT');",
"INSERT INTO nestitle VALUES (54,'Bases Loaded 3',23,23,1991,812,'1PL BAS SPT');",
"INSERT INTO nestitle VALUES (540,'Secret Scout',43,43,2000,0,'1PL UNL');",
"INSERT INTO nestitle VALUES (541,'Operation Secret Storm',43,43,1991,0,'1PL ACT UNL');",
"INSERT INTO nestitle VALUES (542,'Section Z',3,3,2000,677,'1PL ACT SHO');",
"INSERT INTO nestitle VALUES (543,'Seicross',11,11,1988,678,'1PL 2PA ACT SHO');",
"INSERT INTO nestitle VALUES (544,'Sesame Street 1-2-3',17,17,1989,0,'1PL KID EDU');",
"INSERT INTO nestitle VALUES (545,'Sesame Street A-B-C',17,17,1989,0,'1PL KID EDU');",
"INSERT INTO nestitle VALUES (546,'Sesame Street 1-2-3/A-B-C',17,17,2000,0,'1PL KID EDU');",
"INSERT INTO nestitle VALUES (547,'Sesame Street Countdown',17,17,2000,0,'1PL');",
"INSERT INTO nestitle VALUES (548,'Shadow Of The Ninja',50,50,1990,679,'1PL 2PC ACT MAR NNJ');",
"INSERT INTO nestitle VALUES (549,'Shadowgate',56,16,1989,388,'1PL ADV BTT PUZ RPG');",
"INSERT INTO nestitle VALUES (55,'Bases Loaded 4',23,23,1993,813,'1PL BAS SPT');",
"INSERT INTO nestitle VALUES (550,'Shatterhand',50,23,1991,681,'1PL ACT');",
"INSERT INTO nestitle VALUES (551,'Shingen the Ruler',71,71,1990,680,'1PL RPG SIM');",
"INSERT INTO nestitle VALUES (552,'Shinobi',20,20,1989,0,'1PL ACT MAR UNL');",
"INSERT INTO nestitle VALUES (553,'Shockwave',83,83,2000,0,'1PL');",
"INSERT INTO nestitle VALUES (554,'Shooting Range',0,28,2000,771,'1PL ACT GUN');",
"INSERT INTO nestitle VALUES (555,'Short Order',2,2,1989,0,'1PL PPD');",
"INSERT INTO nestitle VALUES (556,'Side Pocket',22,22,1987,776,'1PL 2PX POO SPT');",
"INSERT INTO nestitle VALUES (557,'Silent Assault',43,43,1990,936,'1PL ACT MIL UNL');",
"INSERT INTO nestitle VALUES (558,'Silent Service',7,58,1990,0,'1PL ACT SUB');",
"INSERT INTO nestitle VALUES (559,'Silk Worm',26,26,1990,682,'1PL 2PC ACT SHO');",
"INSERT INTO nestitle VALUES (56,'Basewars',4,58,1991,424,'1PL BAS SPT');",
"INSERT INTO nestitle VALUES (560,'Silver Surfer',125,94,1990,683,'1PL ACT SHO');",
"INSERT INTO nestitle VALUES (561,'The Simpsons: Bart Meets Radioactive Man',8,8,2000,450,'1PL ACT TVA');",
"INSERT INTO nestitle VALUES (562,'The Simpsons: Bart Vs. The Space Mutants',8,8,2000,451,'1PL ACT');",
"INSERT INTO nestitle VALUES (563,'The Simpsons: Bart Vs. The World',8,8,2000,422,'1PL ACT');",
"INSERT INTO nestitle VALUES (564,'Skate Or Die',4,58,1988,684,'1PL 2PA 2PX SKA SPT');",
"INSERT INTO nestitle VALUES (565,'Skate Or Die 2: The Search For Double Trouble',55,55,2000,0,'1PL ACT SKA SPT');",
"INSERT INTO nestitle VALUES (566,'Ski Or Die',4,58,1988,974,'1PL SKI SPT');",
"INSERT INTO nestitle VALUES (567,'Skull & Crossbones',20,20,2000,0,'1PL UNL');",
"INSERT INTO nestitle VALUES (568,'Sky Kid',5,5,1987,685,'1PL 2PC ACT');",
"INSERT INTO nestitle VALUES (569,'Sky Shark',125,10,1989,686,'1PL 2PA ACT SHO');",
"INSERT INTO nestitle VALUES (57,'Batman',5,5,1990,344,'1PL ACT MOV');",
"INSERT INTO nestitle VALUES (570,'Slalom',7,2,1987,687,'1PL SKI SPT');",
"INSERT INTO nestitle VALUES (571,'Smash TV',96,8,1991,937,'1PL 2PC ACT SHO');",
"INSERT INTO nestitle VALUES (572,'Snake Rattle \\'n\\' Roll',7,2,2000,0,'1PL ACT');",
"INSERT INTO nestitle VALUES (573,'Metal Gear 2: Snake\\'s Revenge',4,58,1990,938,'1PL ACT ADV MIL PSS');",
"INSERT INTO nestitle VALUES (574,'Snoopy\\'s Silly Sports',77,77,1990,689,'1PL 2PX SPT');",
"INSERT INTO nestitle VALUES (575,'Snow Bros.',3,3,1991,690,'1PL 2PC ACT');",
"INSERT INTO nestitle VALUES (576,'Soccer',2,2,1987,691,'1PL 2PX SOC SPT');",
"INSERT INTO nestitle VALUES (578,'Solitaire',72,72,2000,0,'1PL UNL');",
"INSERT INTO nestitle VALUES (579,'Solomon\\'s Key',13,13,2000,0,'1PL PUZ');",
"INSERT INTO nestitle VALUES (58,'Batman Returns',4,4,1993,815,'1PL ACT MOV');",
"INSERT INTO nestitle VALUES (580,'Solstice',125,84,1989,693,'1PL ADV PUZ');",
"INSERT INTO nestitle VALUES (581,'Space Shuttle Project',29,29,2000,0,'1PL');",
"INSERT INTO nestitle VALUES (582,'Spelunker',12,21,1987,694,'1PL ACT ADV');",
"INSERT INTO nestitle VALUES (583,'Spiderman: Return of The Sinister Six',128,35,1992,695,'1PL ACT CBK');",
"INSERT INTO nestitle VALUES (584,'Spiritual Warfare',61,61,1992,939,'1PL ACT ADV BBL UNL');",
"INSERT INTO nestitle VALUES (585,'Spot The Game',94,94,2000,0,'1PL ACT');",
"INSERT INTO nestitle VALUES (586,'Spy Hunter',5,5,1987,940,'1PL ACT DRV');",
"INSERT INTO nestitle VALUES (587,'Spy vs. Spy',77,77,1988,770,'1PL 2PX ACT');",
"INSERT INTO nestitle VALUES (588,'Sqoon',12,12,1987,941,'1PL ACT');",
"INSERT INTO nestitle VALUES (589,'Stadium Events',28,28,1987,0,'1PL PPD SPT');",
"INSERT INTO nestitle VALUES (59,'Batman: Return Of The Joker',5,5,1991,814,'1PL ACT');",
"INSERT INTO nestitle VALUES (590,'Stanley',67,67,1992,0,'1PL ADV');",
"INSERT INTO nestitle VALUES (591,'Star Force',13,13,1987,701,'1PL ACT SHO');",
"INSERT INTO nestitle VALUES (592,'Starship Hector',18,18,1987,543,'1PL ACT SHO');",
"INSERT INTO nestitle VALUES (593,'Star Soldier',9,9,1989,942,'1PL ACT SHO');",
"INSERT INTO nestitle VALUES (594,'Star Trek: 25th Anniversary',58,58,1991,943,'1PL ADV TVA');",
"INSERT INTO nestitle VALUES (595,'Star Trek: The Next Generation',29,29,1993,702,'1PL ADV TVA');",
"INSERT INTO nestitle VALUES (596,'Startropics',2,2,1990,391,'1PL ACT ADV BTT');",
"INSERT INTO nestitle VALUES (597,'Startropics II: Zoda\\'s Revenge',2,2,1994,390,'1PL ACT ADV');",
"INSERT INTO nestitle VALUES (598,'Star Voyager',8,8,1987,0,'1PL ACT SHO');",
"INSERT INTO nestitle VALUES (599,'Star Wars',106,75,1991,0,'1PL ACT MOV');",
"INSERT INTO nestitle VALUES (6,'Abadox',50,25,1990,341,'1PL ACT SHO');",
"INSERT INTO nestitle VALUES (60,'Battle Chess',96,22,1990,444,'1PL 2PX CHS STR');",
"INSERT INTO nestitle VALUES (600,'Stealth ATF',38,38,1989,699,'1PL 2PA ACT');",
"INSERT INTO nestitle VALUES (601,'Stinger',4,4,1987,700,'1PL 2PC ACT');",
"INSERT INTO nestitle VALUES (602,'Street Cop',28,28,1989,944,'1PL ACT PPD');",
"INSERT INTO nestitle VALUES (603,'Street Fighter 2010',3,3,2000,0,'1PL ACT MAR');",
"INSERT INTO nestitle VALUES (604,'Strider',3,3,2000,392,'1PL ACT ADV PSS');",
"INSERT INTO nestitle VALUES (605,'Stunt Kids',62,62,2000,0,'1PL UNL');",
"INSERT INTO nestitle VALUES (606,'Sunday Funday- The Ride',61,61,1995,0,'1PL ACT BBL UNL');",
"INSERT INTO nestitle VALUES (607,'Superman',77,77,1988,704,'1PL ACT ADV PSS');",
"INSERT INTO nestitle VALUES (608,'Super Cars',67,67,2000,0,'1PL');",
"INSERT INTO nestitle VALUES (609,'Super C',4,4,1990,703,'1PL 2PC ACT');",
"INSERT INTO nestitle VALUES (61,'The Battle Of Olympus',112,21,1990,383,'1PL ACT ADV PSS');",
"INSERT INTO nestitle VALUES (610,'Super Dodge Ball',84,84,1989,676,'1PL 2PX DBL SPT');",
"INSERT INTO nestitle VALUES (611,'Super Glove Ball',7,69,1990,945,'1PL GLV');",
"INSERT INTO nestitle VALUES (612,'Super Jeopardy',37,37,2000,0,'1PL 3PX GSH');",
"INSERT INTO nestitle VALUES (613,'Super Mario Bros.',2,2,1985,389,'1PL 2PA ACT');",
"INSERT INTO nestitle VALUES (614,'Super Mario Bros. 2',2,2,1988,374,'1PL ACT');",
"INSERT INTO nestitle VALUES (615,'Super Mario Bros. 3',2,2,1990,688,'1PL ACT ADV');",
"INSERT INTO nestitle VALUES (616,'Ivan \"Ironman\" Stewart\\'s Super Off Road',7,6,1990,674,'1PL 2PX 4PX RAC SPT');",
"INSERT INTO nestitle VALUES (617,'Super Pitfall',38,38,1988,696,'1PL 2PA ACT');",
"INSERT INTO nestitle VALUES (618,'Super Spike V\\'Ball',101,2,1990,705,'1PL 2PX 4PX SPT VLB');",
"INSERT INTO nestitle VALUES (619,'Super Sprint',20,20,1988,697,'1PL ACT DRV RAC UNL');",
"INSERT INTO nestitle VALUES (62,'Battletank',29,29,1990,0,'1PL MIL');",
"INSERT INTO nestitle VALUES (620,'Super Spy Hunter',5,5,1991,706,'1PL ACT');",
"INSERT INTO nestitle VALUES (621,'Super Team Games',2,2,1988,0,'1PL 6PX PPD SPT');",
"INSERT INTO nestitle VALUES (622,'Swamp Thing',112,68,1992,946,'1PL ACT TVA');",
"INSERT INTO nestitle VALUES (623,'Swordmaster',38,38,1990,947,'1PL ACT');",
"INSERT INTO nestitle VALUES (624,'Swords & Serpents',57,8,1990,707,'1PL 2PC 4PC ADV RPG');",
"INSERT INTO nestitle VALUES (625,'Taboo, The 6th Sense',7,6,1989,951,'1PL');",
"INSERT INTO nestitle VALUES (626,'Tag Team Pro Wrestling',22,22,1986,0,'1PL 2PX SPT WRE');",
"INSERT INTO nestitle VALUES (627,'Tagin\\' Dragon',80,80,1990,710,'1PL 2PX 2PC ACT UNL');",
"INSERT INTO nestitle VALUES (628,'Tale Spin',3,3,2000,712,'1PL ACT');",
"INSERT INTO nestitle VALUES (629,'Target: Renegade',10,10,1990,0,'1PL ACT MAR');",
"INSERT INTO nestitle VALUES (63,'Battleship',24,24,1993,816,'1PL SIM STR');",
"INSERT INTO nestitle VALUES (630,'Tecmo Baseball',13,13,1989,714,'1PL 2PX BAS PSS SPT');",
"INSERT INTO nestitle VALUES (631,'Tecmo Bowl',13,13,1989,0,'1PL 2PX FTB PSS SPT');",
"INSERT INTO nestitle VALUES (632,'Tecmo Super Bowl',13,13,1991,953,'1PL FTB SPT');",
"INSERT INTO nestitle VALUES (633,'Tecmo Cup Soccer',13,13,1992,952,'1PL SOC SPT');",
"INSERT INTO nestitle VALUES (634,'Tecmo NBA Basketball',13,13,1992,713,'1PL BSK SPT');",
"INSERT INTO nestitle VALUES (635,'Tecmo World Wrestling',13,13,1990,954,'1PL 2PX SPT WRE');",
"INSERT INTO nestitle VALUES (636,'Teenage Mutant Ninja Turtles',4,58,1989,394,'1PL ACT ADV NNJ');",
"INSERT INTO nestitle VALUES (637,'Teenage Mutant Ninja Turtles 2: The Arcade Game',4,58,1990,395,'1PL 2PC ACT NNJ');",
"INSERT INTO nestitle VALUES (638,'Teenage Mutant Ninja Turtles 3: The Turtles Take Manhattan',4,58,1992,398,'1PL 2PC ACT NNJ');",
"INSERT INTO nestitle VALUES (639,'Teenage Mutant Ninja Turtles: Tournament Fighters',4,4,1993,772,'1PL 2PX ACT MAR NNJ');",
"INSERT INTO nestitle VALUES (64,'Battletoads',7,6,1991,345,'1PL 2PC ACT MAR');",
"INSERT INTO nestitle VALUES (640,'Tennis',2,2,1985,715,'1PL 2PX SPT TNS');",
"INSERT INTO nestitle VALUES (641,'The Terminator',24,24,2000,0,'1PL ACT MOV');",
"INSERT INTO nestitle VALUES (642,'Terminator 2: Judgement Day',35,35,1991,709,'1PL ACT MOV');",
"INSERT INTO nestitle VALUES (643,'Terra Cresta',30,30,1990,0,'1PL 2PA ACT SHO');",
"INSERT INTO nestitle VALUES (645,'Tetris 2',2,2,1993,717,'1PL 2PX PUZ');",
"INSERT INTO nestitle VALUES (646,'The Three Stooges',96,38,1989,955,'1PL 2PA ACT TVA');",
"INSERT INTO nestitle VALUES (647,'Thunderbirds',38,38,2000,0,'1PL');",
"INSERT INTO nestitle VALUES (648,'Thundercade',26,26,2000,0,'1PL 2PC ACT SHO');",
"INSERT INTO nestitle VALUES (649,'Thunder And Lightning',82,82,2000,0,'1PL');",
"INSERT INTO nestitle VALUES (65,'Battletoads & Double Dragon: The Ultimate Team',7,6,1993,818,'1PL 2PC ACT CRS MAR');",
"INSERT INTO nestitle VALUES (650,'Tiger Heli',8,8,1987,718,'1PL 2PA ACT SHO');",
"INSERT INTO nestitle VALUES (651,'Tiles Of Fate',72,72,1990,0,'1PL PUZ UNL');",
"INSERT INTO nestitle VALUES (652,'Time Lord',7,25,2000,956,'1PL ACT');",
"INSERT INTO nestitle VALUES (653,'Times Of Lore',36,36,2000,0,'1PL');",
"INSERT INTO nestitle VALUES (654,'Tiny Toon Adventures',4,4,1991,719,'1PL ACT TVA');",
"INSERT INTO nestitle VALUES (655,'Tiny Toon Adventures 2: Trouble In Wackyland',4,4,1993,721,'1PL ACT TVA');",
"INSERT INTO nestitle VALUES (656,'To The Earth',2,2,1990,0,'1PL GUN');",
"INSERT INTO nestitle VALUES (657,'Toki',10,10,1991,957,'1PL 2PA ACT');",
"INSERT INTO nestitle VALUES (658,'Tom And Jerry',125,17,1991,0,'1PL ACT TVA');",
"INSERT INTO nestitle VALUES (659,'Tombs And Treasure',47,48,1989,393,'1PL ADV PUZ RPG');",
"INSERT INTO nestitle VALUES (66,'Bee 52',97,62,1992,819,'1PL ACT UNL');",
"INSERT INTO nestitle VALUES (660,'Toobin\\'',20,20,1989,0,'1PL SPT UNL');",
"INSERT INTO nestitle VALUES (661,'Top Gun',4,4,1987,723,'1PL ACT MOV');",
"INSERT INTO nestitle VALUES (662,'Top Gun II: The Second Mission',4,4,1989,958,'1PL 2PC ACT FLY MOV');",
"INSERT INTO nestitle VALUES (663,'Total Recall',8,8,1990,0,'1PL ACT MOV');",
"INSERT INTO nestitle VALUES (664,'Totally Rad',23,23,1991,724,'1PL ACT');",
"INSERT INTO nestitle VALUES (665,'Touchdown Fever',1,1,1988,960,'1PL 2PX 2PC FTB SPT');",
"INSERT INTO nestitle VALUES (666,'T&C Surf Design',35,35,1988,708,'1PL 2PA SKA SPT SRF');",
"INSERT INTO nestitle VALUES (667,'T&C Surf Design 2: Thrilla\\'s Safari',105,35,1991,720,'1PL ACT SKA SPT SRF');",
"INSERT INTO nestitle VALUES (668,'Toxic Crusaders',28,28,1991,961,'1PL ACT PSS TVA');",
"INSERT INTO nestitle VALUES (669,'Track & Field',4,4,1987,0,'1PL 2PX SPT TRK');",
"INSERT INTO nestitle VALUES (67,'Beetlejuice',7,35,1991,820,'1PL MOV');",
"INSERT INTO nestitle VALUES (670,'Track & Field II',4,4,1989,726,'1PL 2PX PSS SPT TRK');",
"INSERT INTO nestitle VALUES (671,'Treasure Master',93,93,2000,0,'1PL ACT');",
"INSERT INTO nestitle VALUES (672,'Barker Bill\\'s Trick Shooting',2,2,2000,809,'1PL GUN');",
"INSERT INTO nestitle VALUES (673,'Trog',129,8,1990,962,'1PL ACT ARC');",
"INSERT INTO nestitle VALUES (674,'Trojan',3,3,1987,0,'1PL 2PA 2PX ACT');",
"INSERT INTO nestitle VALUES (675,'Trolls On Treasure Island',72,72,2000,0,'1PL UNL');",
"INSERT INTO nestitle VALUES (676,'Twin Cobra',26,26,1990,728,'1PL 2PC ACT SHO');",
"INSERT INTO nestitle VALUES (677,'Twin Eagle',82,82,1989,773,'1PL 2PC ACT SHO');",
"INSERT INTO nestitle VALUES (678,'Ultima: Exodus',49,11,1989,400,'1PL ADV BTT RPG');",
"INSERT INTO nestitle VALUES (679,'Ultima: Quest Of The Avatar',49,11,1990,401,'1PL ADV RPG');",
"INSERT INTO nestitle VALUES (68,'Best Of The Best',99,67,1992,427,'1PL');",
"INSERT INTO nestitle VALUES (680,'Ultima: Warriors Of Destiny',49,11,1992,729,'1PL ADV BTT RPG');",
"INSERT INTO nestitle VALUES (681,'Ultimate Air Combat',38,38,1991,963,'1PL ACT FLY PSS');",
"INSERT INTO nestitle VALUES (682,'Ultimate Basketball',26,26,1990,0,'1PL 2PX BSK SPT');",
"INSERT INTO nestitle VALUES (683,'Ultimate League Soccer',72,72,2000,0,'1PL SOC SPT UNL');",
"INSERT INTO nestitle VALUES (684,'Ultimate Stuntman',97,62,1991,730,'1PL ACT DRV UNL');",
"INSERT INTO nestitle VALUES (685,'Uncharted Waters',70,70,2000,0,'1PL');",
"INSERT INTO nestitle VALUES (686,'Uninvited',56,16,1991,402,'1PL ADV PUZ RPG');",
"INSERT INTO nestitle VALUES (687,'The Untouchables',65,65,1990,0,'1PL ACT MOV SHO');",
"INSERT INTO nestitle VALUES (688,'Urban Champion',2,2,1986,731,'1PL 2PC ACT MAR');",
"INSERT INTO nestitle VALUES (689,'Vegas Dream',19,19,1990,964,'1PL BTT GMB PSS');",
"INSERT INTO nestitle VALUES (69,'Bible Adventures',61,61,1991,821,'1PL BBL UNL');",
"INSERT INTO nestitle VALUES (690,'Venice Beach Volleyball',147,72,1991,0,'1PL SPT UNL VLB');",
"INSERT INTO nestitle VALUES (691,'Vice: Project Doom',26,26,1991,0,'1PL ACT DRV');",
"INSERT INTO nestitle VALUES (692,'Videomation',68,68,2000,0,'1PL');",
"INSERT INTO nestitle VALUES (693,'Vindicators',20,20,1988,0,'1PL 2PC ACT UNL');",
"INSERT INTO nestitle VALUES (694,'Volleyball',2,2,1987,732,'1PL 2PX SPT VLB');",
"INSERT INTO nestitle VALUES (695,'Wacky Races',86,86,1992,734,'1PL RAC SPT');",
"INSERT INTO nestitle VALUES (696,'Wall Street Kid',32,32,1990,0,'1PL PSS SIM');",
"INSERT INTO nestitle VALUES (697,'Wally Bear And The No! Gang',72,72,2000,976,'1PL ACT SKA UNL');",
"INSERT INTO nestitle VALUES (698,'Wario\\'s Woods',2,2,1994,736,'1PL ACT');",
"INSERT INTO nestitle VALUES (699,'Wayne Gretzky Hockey',113,68,1990,737,'1PL 2PX 2PC HOC SPT');",
"INSERT INTO nestitle VALUES (7,'The Addams Family',65,65,1991,789,'1PL');",
"INSERT INTO nestitle VALUES (70,'Bible Buffet',61,61,1993,948,'1PL 4PX BBL UNL');",
"INSERT INTO nestitle VALUES (700,'Wayne\\'s World',118,68,1993,965,'1PL ACT MOV');",
"INSERT INTO nestitle VALUES (701,'Werewolf: The Last Warrior',22,22,1990,966,'1PL 2PA ACT');",
"INSERT INTO nestitle VALUES (702,'Wheel Of Fortune',7,37,1988,739,'1PL 3PX GSH');",
"INSERT INTO nestitle VALUES (703,'Wheel Of Fortune: Family Edition',7,37,1990,740,'1PL 3PX GSH');",
"INSERT INTO nestitle VALUES (704,'Wheel Of Fortune: Junior Edition',7,37,1989,775,'1PL 3PX GSH');",
"INSERT INTO nestitle VALUES (705,'Wheel Of Fortune With Vanna White',7,37,1991,744,'1PL 3PX GSH');",
"INSERT INTO nestitle VALUES (706,'Where In Time Is Carmen Sandiego?',4,4,1989,733,'1PL EDU PUZ RPG TIM');",
"INSERT INTO nestitle VALUES (707,'Where\\'s Waldo?',113,68,1991,735,'1PL KID PUZ');",
"INSERT INTO nestitle VALUES (708,'Whomp\\'em',23,23,1991,0,'1PL ACT');",
"INSERT INTO nestitle VALUES (709,'Widget',86,86,2000,0,'1PL');",
"INSERT INTO nestitle VALUES (71,'Big Bird\\'s Hide And Speak',127,17,1990,698,'1PL KID EDU');",
"INSERT INTO nestitle VALUES (710,'Wild Gunman',2,2,1985,0,'1PL 2PA GUN');",
"INSERT INTO nestitle VALUES (711,'Willow',3,3,1989,403,'1PL ACT ADV PSS');",
"INSERT INTO nestitle VALUES (712,'Win, Lose, Or Draw',127,17,1990,741,'1PL 2PX GSH PSS');",
"INSERT INTO nestitle VALUES (713,'Winter Games',8,8,1987,742,'1PL 2PA SKI SPT');",
"INSERT INTO nestitle VALUES (714,'Wizardry',41,81,1990,0,'1PL ADV RPG');",
"INSERT INTO nestitle VALUES (715,'Wizardry 2: Knight Of Diamonds',149,41,1991,967,'1PL ADV RPG');",
"INSERT INTO nestitle VALUES (716,'Wizards & Warriors',7,8,1987,404,'1PL ACT ADV');",
"INSERT INTO nestitle VALUES (717,'Wizards & Warriors II: Ironsword',7,8,1990,405,'1PL ACT ADV PSS');",
"INSERT INTO nestitle VALUES (718,'Wizards & Warriors III-Kuros: Visions Of Power',7,8,1991,968,'1PL ACT ADV');",
"INSERT INTO nestitle VALUES (719,'Wolverine',125,35,1991,969,'1PL ACT CBK');",
"INSERT INTO nestitle VALUES (72,'Big Foot',96,8,1989,825,'1PL');",
"INSERT INTO nestitle VALUES (720,'World Champ',82,82,1990,774,'1PL 2PX BOX SPT');",
"INSERT INTO nestitle VALUES (721,'WCW: World Championship Wrestling',11,11,1990,738,'1PL 2PX PSS SPT WRE');",
"INSERT INTO nestitle VALUES (722,'World Class Track Meet',2,2,1988,0,'1PL 6PX PPD SPT');",
"INSERT INTO nestitle VALUES (723,'World Cup Soccer',44,2,1990,746,'1PL 4PX SOC SPT');",
"INSERT INTO nestitle VALUES (724,'World Games',7,25,1989,970,'1PL 8PA SPT');",
"INSERT INTO nestitle VALUES (725,'3-D World Runner',39,8,1987,0,'1PL ACT');",
"INSERT INTO nestitle VALUES (726,'Wrath Of The Black Manta',10,10,1990,747,'1PL ACT MAR NNJ');",
"INSERT INTO nestitle VALUES (727,'Wrecking Crew',2,2,1985,748,'1PL 2PA ACT');",
"INSERT INTO nestitle VALUES (728,'WWF Wrestlemania',8,8,1989,743,'1PL 2PX SPT WRE');",
"INSERT INTO nestitle VALUES (729,'WURM',53,53,1991,0,'1PL ACT ADV PSS SHO');",
"INSERT INTO nestitle VALUES (73,'Big Nose The Caveman',97,62,1991,824,'1PL UNL');",
"INSERT INTO nestitle VALUES (730,'WWF Wrestlemania Challenge',7,35,1990,0,'1PL SPT WRE');",
"INSERT INTO nestitle VALUES (731,'WWF King Of The Ring',35,35,1993,913,'1PL SPT WRE');",
"INSERT INTO nestitle VALUES (732,'WWF Steel Cage Challenge',105,35,1992,971,'1PL 2PX SPT WRE');",
"INSERT INTO nestitle VALUES (733,'Marvel\\'s X-Men',35,35,2000,406,'1PL 2PC ACT CBK');",
"INSERT INTO nestitle VALUES (734,'Xenophobe',5,5,1988,750,'1PL 2PC ACT ADV');",
"INSERT INTO nestitle VALUES (735,'Xevious',28,28,1988,751,'1PL 2PA ACT SHO');",
"INSERT INTO nestitle VALUES (736,'XEXYZ',18,18,1990,752,'1PL ACT ADV PSS');",
"INSERT INTO nestitle VALUES (737,'Yo! Noid',3,3,1990,753,'1PL ACT');",
"INSERT INTO nestitle VALUES (738,'Yoshi',2,2,1992,0,'1PL 2PX PUZ');",
"INSERT INTO nestitle VALUES (739,'Yoshi\\'s Cookie',2,2,1993,754,'1PL 2PX PUZ');",
"INSERT INTO nestitle VALUES (74,'Big Nose Freaks Out',97,62,1992,823,'1PL UNL');",
"INSERT INTO nestitle VALUES (740,'The Young Indy Chronicles',23,23,1992,972,'1PL ACT');",
"INSERT INTO nestitle VALUES (741,'Zanac',104,11,1988,755,'1PL ACT SHO');",
"INSERT INTO nestitle VALUES (742,'Zelda II: The Adventure Of Link',2,2,1988,408,'1PL ACT ADV BTT');",
"INSERT INTO nestitle VALUES (743,'Zen: Intergalactic Ninja',4,4,1993,973,'1PL ACT NNJ');",
"INSERT INTO nestitle VALUES (744,'Zombie Nation',95,95,2000,0,'1PL');",
"INSERT INTO nestitle VALUES (746,'Elite',45,45,1991,0,'1PL UNL');",
"INSERT INTO nestitle VALUES (747,'Gyromite',2,2,1985,786,'1PL 2PC ACT ROB');",
"INSERT INTO nestitle VALUES (748,'Eggsplode',2,2,1989,0,'1PL PPD');",
"INSERT INTO nestitle VALUES (75,'Bill & Ted\\'s Excellent Video Game Adventure',133,35,1990,439,'1PL ACT ADV MOV TIM');",
"INSERT INTO nestitle VALUES (751,'Tetris (I)',20,20,2000,0,'1PL 2PX PUZ UNL');",
"INSERT INTO nestitle VALUES (752,'Tetris (II)',0,2,1989,716,'1PL PUZ');",
"INSERT INTO nestitle VALUES (753,'Stack-Up',2,2,1985,783,'1PL ROB');",
"INSERT INTO nestitle VALUES (754,'Action 52',64,64,1991,788,'1PL ACT UNL');",
"INSERT INTO nestitle VALUES (755,'Bubble Bath Babes',76,76,2000,0,'1PL ADL UNL');",
"INSERT INTO nestitle VALUES (756,'Cheetahmen II',64,64,1993,0,'1PL ACT UNL');",
"INSERT INTO nestitle VALUES (757,'Asterix The Gaul',67,67,2000,0,'1PL');",
"INSERT INTO nestitle VALUES (758,'The Fantastic Adventures Of Dizzy',97,62,1991,880,'1PL UNL');",
"INSERT INTO nestitle VALUES (759,'Hot Slots',76,76,1991,549,'1PL ADL GMB UNL');",
"INSERT INTO nestitle VALUES (76,'Bionic Commando',3,3,1988,346,'1PL ACT ADV');",
"INSERT INTO nestitle VALUES (761,'Peek A Boo Poker',76,76,2000,0,'1PL ADL GMB UNL');",
"INSERT INTO nestitle VALUES (762,'Pyramids Of Ra',92,92,2000,0,'1PL');",
"INSERT INTO nestitle VALUES (763,'Maxi 15',72,72,1992,0,'1PL 2PC ACT PUZ SHO UNL');",
"INSERT INTO nestitle VALUES (764,'Scarabeus',0,0,2000,0,'1PL');",
"INSERT INTO nestitle VALUES (765,'Over Horizon',71,71,2000,0,'1PL ACT');",
"INSERT INTO nestitle VALUES (766,'Chase H.Q.',10,10,2000,0,'1PL ACT DRV');",
"INSERT INTO nestitle VALUES (767,'Miracle Keyboard Teaching System',0,0,2000,0,'1PL');",
"INSERT INTO nestitle VALUES (77,'The Black Bass',71,71,1989,430,'1PL FSH PSS SPT');",
"INSERT INTO nestitle VALUES (78,'Blackjack',139,72,1992,0,'1PL GMB UNL');",
"INSERT INTO nestitle VALUES (79,'Blades Of Steel',4,4,1988,826,'1PL 2PX HOC PSS SPT');",
"INSERT INTO nestitle VALUES (8,'Disney\\'s Adventures In The Magic Kingdom',3,3,1989,436,'1PL ACT ADV SHO');",
"INSERT INTO nestitle VALUES (80,'Blaster Master',5,5,1988,312,'1PL ACT ADV');",
"INSERT INTO nestitle VALUES (81,'The Blue Marlin',71,71,1991,827,'1PL FSH SPT');",
"INSERT INTO nestitle VALUES (82,'The Blues Brothers',73,73,1991,828,'1PL ACT MOV');",
"INSERT INTO nestitle VALUES (83,'Bo Jackson Baseball',96,22,1991,460,'1PL BAS SPT');",
"INSERT INTO nestitle VALUES (84,'Bomberman',18,18,1989,829,'1PL ACT PSS');",
"INSERT INTO nestitle VALUES (85,'Bomberman 2',18,18,1992,830,'1PL');",
"INSERT INTO nestitle VALUES (86,'Bonk\\'s Adventure',134,18,1993,831,'1PL');",
"INSERT INTO nestitle VALUES (87,'Boulder Dash',22,75,1990,442,'1PL');",
"INSERT INTO nestitle VALUES (88,'A Boy & His Blob',52,29,1990,342,'1PL ACT ADV PUZ');",
"INSERT INTO nestitle VALUES (89,'Break Time',49,11,1992,0,'1PL');",
"INSERT INTO nestitle VALUES (9,'The Adventures Of Bayou Billy',4,4,1989,791,'1PL ACT DRV GUN MAR SHO');",
"INSERT INTO nestitle VALUES (90,'Breakthru',22,22,1987,900,'1PL 2PA ACT');",
"INSERT INTO nestitle VALUES (91,'Bubble Bobble',10,10,1988,446,'1PL 2PC PSS PUZ');",
"INSERT INTO nestitle VALUES (92,'Bubble Bobble 2',10,10,1993,445,'1PL');",
"INSERT INTO nestitle VALUES (93,'Bucky O\\'Hare',4,4,1992,429,'1PL TVA');",
"INSERT INTO nestitle VALUES (94,'Bugs Bunny\\'s Birthday Blowout',77,77,1990,428,'1PL TVA');",
"INSERT INTO nestitle VALUES (95,'Bugs Bunny\\'s Crazy Castle',77,77,1989,833,'1PL ACT PSS TVA');",
"INSERT INTO nestitle VALUES (96,'Bump \\'N\\' Jump',22,30,1988,440,'1PL ACT');",
"INSERT INTO nestitle VALUES (97,'Burai Fighter',9,9,1990,447,'1PL ACT SHO');",
"INSERT INTO nestitle VALUES (98,'Burgertime',22,22,1987,834,'1PL 2PA ACT ARC');",
"INSERT INTO nestitle VALUES (99,'Cabal',7,25,1989,835,'1PL ACT ARC SHO');",
""
};
        dbver = "";
        if(!performActualUpdate(updates2, "1000", dbver))
            return false;
    }

    query.exec("SELECT * FROM neskeyword;");
    if (!query.isActive() || query.size() <= 0)
    {
        const QString updates2[] = {
"INSERT INTO neskeyword VALUES ('1PL','1 Player');",
"INSERT INTO neskeyword VALUES ('2PA','2 Player Alternating');",
"INSERT INTO neskeyword VALUES ('2PC','2 Player Cooperative');",
"INSERT INTO neskeyword VALUES ('2PX','2 Player Competitive');",
"INSERT INTO neskeyword VALUES ('3PX','3 Player Competitive');",
"INSERT INTO neskeyword VALUES ('4PA','4 Player Alternating');",
"INSERT INTO neskeyword VALUES ('4PC','4 Player Cooperative');",
"INSERT INTO neskeyword VALUES ('4PX','4 Player Competitive');",
"INSERT INTO neskeyword VALUES ('6PA','6 Player Alternating');",
"INSERT INTO neskeyword VALUES ('6PX','6 Player Competitive');",
"INSERT INTO neskeyword VALUES ('8PA','8 Player Alternating');",
"INSERT INTO neskeyword VALUES ('8PX','8 Player Competitive');",
"INSERT INTO neskeyword VALUES ('ACT','Action');",
"INSERT INTO neskeyword VALUES ('ADE','Aladdin Deck Enhancer');",
"INSERT INTO neskeyword VALUES ('ADL','Adult');",
"INSERT INTO neskeyword VALUES ('ADV','Adventure');",
"INSERT INTO neskeyword VALUES ('ANM','Anime Adaptation');",
"INSERT INTO neskeyword VALUES ('ARC','Arcade Adaptation');",
"INSERT INTO neskeyword VALUES ('BAS','Baseball');",
"INSERT INTO neskeyword VALUES ('BBL','Bible');",
"INSERT INTO neskeyword VALUES ('BGA','Board Game Adaptation');",
"INSERT INTO neskeyword VALUES ('BOA','Boating');",
"INSERT INTO neskeyword VALUES ('BOK','Book Adaptation');",
"INSERT INTO neskeyword VALUES ('BOX','Boxing');",
"INSERT INTO neskeyword VALUES ('BSK','Basketball');",
"INSERT INTO neskeyword VALUES ('BTT','Battery');",
"INSERT INTO neskeyword VALUES ('CBK','Comic Book');",
"INSERT INTO neskeyword VALUES ('CHS','Chess');",
"INSERT INTO neskeyword VALUES ('CRS','Crossover');",
"INSERT INTO neskeyword VALUES ('DBL','Dodge Ball');",
"INSERT INTO neskeyword VALUES ('DRT','Darts');",
"INSERT INTO neskeyword VALUES ('DRV','Driving');",
"INSERT INTO neskeyword VALUES ('EDU','Educational');",
"INSERT INTO neskeyword VALUES ('FLY','Flying');",
"INSERT INTO neskeyword VALUES ('FSH','Fishing');",
"INSERT INTO neskeyword VALUES ('FTB','Football');",
"INSERT INTO neskeyword VALUES ('GLF','Golf');",
"INSERT INTO neskeyword VALUES ('GLV','Power Glove');",
"INSERT INTO neskeyword VALUES ('GMB','Gambling');",
"INSERT INTO neskeyword VALUES ('GSH','Game Show Adaptation');",
"INSERT INTO neskeyword VALUES ('GUN','Light Gun');",
"INSERT INTO neskeyword VALUES ('HOC','Hockey');",
"INSERT INTO neskeyword VALUES ('KID','Children');",
"INSERT INTO neskeyword VALUES ('MAR','Martial Arts');",
"INSERT INTO neskeyword VALUES ('MIL','Military');",
"INSERT INTO neskeyword VALUES ('MOV','Movie Adaptation');",
"INSERT INTO neskeyword VALUES ('MYS','Mystery');",
"INSERT INTO neskeyword VALUES ('NNJ','Ninjas');",
"INSERT INTO neskeyword VALUES ('PIN','Pinball');",
"INSERT INTO neskeyword VALUES ('POO','Pool');",
"INSERT INTO neskeyword VALUES ('PPD','Power Pad');",
"INSERT INTO neskeyword VALUES ('PSS','Password');",
"INSERT INTO neskeyword VALUES ('PUZ','Puzzle');",
"INSERT INTO neskeyword VALUES ('RAC','Racing');",
"INSERT INTO neskeyword VALUES ('ROB','R.O.B.');",
"INSERT INTO neskeyword VALUES ('RPG','Role Playing Game');",
"INSERT INTO neskeyword VALUES ('SHO','Shooting');",
"INSERT INTO neskeyword VALUES ('SIM','Simulation');",
"INSERT INTO neskeyword VALUES ('SKA','Skateboarding');",
"INSERT INTO neskeyword VALUES ('SKI','Skiing');",
"INSERT INTO neskeyword VALUES ('SNO','Snowboarding');",
"INSERT INTO neskeyword VALUES ('SOC','Soccer');",
"INSERT INTO neskeyword VALUES ('SPC','Space');",
"INSERT INTO neskeyword VALUES ('SPT','Sports');",
"INSERT INTO neskeyword VALUES ('SRF','Surfing');",
"INSERT INTO neskeyword VALUES ('STR','Strategy');",
"INSERT INTO neskeyword VALUES ('SUB','Submarine');",
"INSERT INTO neskeyword VALUES ('TIM','Time Travel');",
"INSERT INTO neskeyword VALUES ('TNS','Tennis');",
"INSERT INTO neskeyword VALUES ('TRK','Track');",
"INSERT INTO neskeyword VALUES ('TVA','TV Adaptation');",
"INSERT INTO neskeyword VALUES ('UNL','Unlicensed');",
"INSERT INTO neskeyword VALUES ('VLB','Volleyball');",
"INSERT INTO neskeyword VALUES ('VMP','Vampires');",
"INSERT INTO neskeyword VALUES ('WRE','Wrestling');",
"INSERT INTO neskeyword VALUES ('WST','Old West');",
""
};
        dbver = "";
        if(!performActualUpdate(updates2, "1000", dbver))
            return false;
    }

    return true;
}

bool UpgradeGameDatabaseSchema(void)
{
    QString dbver = gContext->GetSetting("GameDBSchemaVer");
    MSqlQuery query(MSqlQuery::InitCon());
   
    if (dbver == currentDatabaseVersion)
        return true;

    if (dbver == "")
    {
        if (!InitializeDatabase())
            return false;
        dbver = "1000";
    }

    if (dbver == "1000")
    {
        const QString updates[] = {
"ALTER TABLE gamemetadata ADD COLUMN favorite BOOL NULL;",
""
};
        if (!performActualUpdate(updates, "1001", dbver))
            return false;
    }

    if ((((dbver == "1004") 
      || (dbver == "1003")) 
      || (dbver == "1002"))
      || (dbver == "1001"))
    {   
        const QString updates[] = {

"CREATE TABLE gameplayers ("
"  gameplayerid int(10) unsigned NOT NULL auto_increment,"
"  playername varchar(64) NOT NULL default '',"
"  workingpath varchar(255) NOT NULL default '',"
"  rompath varchar(255) NOT NULL default '',"
"  screenshots varchar(255) NOT NULL default '',"
" commandline varchar(255) NOT NULL default '',"
"  gametype varchar(64) NOT NULL default '',"
"  extensions varchar(128) NOT NULL default '',"
"  PRIMARY KEY (gameplayerid),"
"  UNIQUE KEY playername (playername)"
");",
"ALTER TABLE gamemetadata ADD COLUMN rompath varchar(255) NOT NULL default ''; ",
"ALTER TABLE gamemetadata ADD COLUMN gametype varchar(64) NOT NULL default ''; ",
""
};
        if (!performActualUpdate(updates, "1005", dbver))
            return false;
    }

    if (dbver == "1005")
    {   
        const QString updates[] = {
"ALTER TABLE gameplayers ADD COLUMN spandisks tinyint(1) NOT NULL default 0; ",
"ALTER TABLE gamemetadata ADD COLUMN diskcount tinyint(1) NOT NULL default 1; ",
""
};
        if (!performActualUpdate(updates, "1006", dbver))
            return false;
    }

    if (dbver == "1006")
    {   
        
        if (gContext->GetSetting("GameAllTreeLevels"))
            query.exec("UPDATE settings SET data = 'system gamename' WHERE value = 'GameAllTreeLevels'; ");

        QString updates[] = {
"ALTER TABLE gamemetadata ADD COLUMN country varchar(128) NOT NULL default ''; ",
"ALTER TABLE gamemetadata ADD COLUMN crc_value varchar(64) NOT NULL default ''; ",
"ALTER TABLE gamemetadata ADD COLUMN display tinyint(1) NOT NULL default 1; ",
""
};

        if (!performActualUpdate(updates, "1007", dbver))
            return false;
    }

    return true;
}
