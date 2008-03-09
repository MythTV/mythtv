/*
 *  it2m.h
 *  
 *  
 *  Created by sonique on 28/10/06.
 *  Copyright 2006 Cedric FERRY. All rights reserved.
 *
 *  contact : sonique_irc(at)yahoo(dot)fr
 *  http://sonique54.free.fr/it2m/
 *  http://sonique54.free.fr/it2m/it2m-0.1.i386.tar.bz2
 *
 */

// Include Standard
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

// Include LibXML
#include <libxml/tree.h>
#include <libxml/xpath.h>

// Include MySQL
#include <mysql/mysql.h>
#include <mysql/errmsg.h>

// Globals Variables
MYSQL * mysql;


int scan_itms (const char *filename );
int is_file (const char *filename);
void usage();
int UpdateMythConverg(char * artname, char * albname, char * songname, char * rating, char * count);



