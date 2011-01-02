/*
 *  it2m.c
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

#include "iT2M.h"

// gcc `xml2-config --cflags --libs` -lmysqlclient -o it2m it2m.c && ./it2m



// this file exist ?
int is_file (const char *filename) {
	struct stat buf;
	if (stat (filename, &buf) == -1)
		return (0);
	if (S_ISREG (buf.st_mode))
		return (1);
	return (0);
}

// Usage of it2m
void usage(){
	printf("\n\t*************** it2m: Usage ***************\nit2m itunes.xml dbpassword user databasename port\nit2m ([itunes.xml]|default='iTunes Music Library.xml') dbpassword \n\t[ user [(database|default='mythconverg')] [(port|default=3306)] ]\n* VerySimple Use : YourPassWord\n* Simple : YourItunesMusicLin.xml YourPassWord\n* Custom : YourItunesMusicLin.xml YourPassWord YouUser\n* Default : \"iTunes Music Library.xml\" YourPass root  mythconverg 3306\n\ni2tm : writed by Sonique :: Cedric FERRY :: http://sonique54.free.fr.\n");
	return;
}


// Update MythTV database
int UpdateMythConverg(char * artname, char * albname, char * songname, char * rating, char * count){

	MYSQL_RES *res;
	MYSQL_ROW row;
	int t;
	int rate = atoi(rating)/10;
	int numplays = atoi(count);
	int num_rows = 0;
	char query [1024] ="";
	sprintf(query,"UPDATE music_songs SET rating = %d, numplays = %d WHERE music_songs.artist_id IN (SELECT artist_id FROM music_artists WHERE artist_name LIKE \"%s%%\") AND music_songs.album_id IN (SELECT album_id FROM music_albums WHERE album_name LIKE \"%s%%\") AND music_songs.name LIKE \"%s%%\" ",rate, numplays, artname, albname, songname);


	t=mysql_real_query(mysql, query, strlen(query));
	if (t) {
		printf("UpdateMythConverg request error : %s \n", mysql_error(mysql) );
	} else {
		if(mysql_affected_rows(mysql) < 1){
			// music cannot be found in database OR already usage();
			printf("\t\t%s - %s - %s cannot be found OR already up to date.\n", artname, albname, songname);
		} else {
			// Succefully Updated
			printf("%s - %s - %s Have been UPDATED.\n",artname, albname, songname);
		}
	}
}


// Scan itms XML file to find Play Count and Rating
int scan_itms (const char *filename ){
	
	// XML Var
	xmlDocPtr 		xmlperms_doc 	 = NULL;
	xmlXPathContextPtr 	xmlperms_context = NULL;
	xmlXPathObjectPtr 	xmlobject;

	const char path_template[] = "/plist/dict/dict/dict/*";

	char *path;

	if (!filename){
		fprintf (stderr, "%s:%d File not found\n", __FILE__, __LINE__);
		return (0);
	}

	xmlperms_doc = xmlParseFile (filename);
	
	if (!xmlperms_doc) {
		fprintf (stderr, "%s:%d Could not parse the document\n", __FILE__,__LINE__);
		return (0);
	}
	
	xmlXPathInit();
	
	xmlperms_context = xmlXPathNewContext (xmlperms_doc);

	xmlobject = xmlXPathEval (path_template, xmlperms_context);

	// Stockage Var
	char * albname	= NULL;
	char * artname	= NULL;
	char * rating	= NULL;
	char * songname	= NULL;
	char * count	= NULL;


	if (	(xmlobject->type == XPATH_NODESET) && 
		(xmlobject->nodesetval)		){
		// Is number of node > 0 ?
		if (xmlobject->nodesetval->nodeNr){
			xmlNodePtr node, node2;
			int i=0;
			// For each node found value
			for(i=0; i<xmlobject->nodesetval->nodeNr-1; i++){
				
				node = xmlobject->nodesetval->nodeTab[i];	// Key		ex: Album
				node2 = xmlobject->nodesetval->nodeTab[i+1];	// Value	ex: Album Name
				xmlChar *pathact1 = xmlGetNodePath (node);

				// Is it a node ? Is it a "key"
				if(	node->type == XML_ELEMENT_NODE 	&& 
					(xmlobject->nodesetval)  	&& 
					strcmp( node->name, "key" ) == 0 ){
					
					// New Track ID = New Songs -> free memory
					if( strcmp( (char*)node->children->content,"Track ID")==0 ){
						free(albname);
						free(artname);
						free(rating);
						free(songname);
						free(count);
						artname  = NULL;
						albname  = NULL;
						songname = NULL;
						rating 	 = NULL;
						count	 = NULL;
					}
					// Balise Name found, get Song Name
					if( strcmp( (char*)node->children->content,"Name")==0 ){
						if(strcmp( node2->name,"string") == 0) {
							songname = strdup(node2->children->content);
						}
					}
					// Balise Album found, get Album Name
					if(strcmp((char *) node->children->content,"Album")==0 ) {
						if(strcmp(node2->name,"string") == 0){
							albname = strdup(node2->children->content);
						}
					}
					// Balise Artist found, get Artist Name
					if(strcmp((char *) node->children->content,"Artist")==0 ){
						if(strcmp(node2->name,"string") == 0){
							artname = strdup(node2->children->content);
						}
					}
					// Balise Play Count found, get Num plays
					if(strcmp((char *) node->children->content,"Play Count")==0 ){
						if(strcmp(node2->name,"integer") == 0){
							count = strdup(node2->children->content);
						}
					}
					// Balise Rating found, get Rate
					if(strcmp((char *) node->children->content,"Rating")==0 ){
						if(strcmp(node2->name,"integer") == 0){
							rating = strdup(node2->children->content);
						}
					}
					
					// If evry data are there, fill database
					if(rating != NULL && artname != NULL && albname != NULL && songname != NULL ){
						
						if(count == NULL){ count = strdup("0"); }
						// Go ! Fill database
						UpdateMythConverg(artname, albname, songname, rating, count);
						// Free Memory else for each other balise next Rating, 
						free(albname);
						free(artname);
						free(rating);
						free(songname);
						free(count);
						artname  = NULL;
						albname  = NULL;
						songname = NULL;
						rating 	 = NULL;
						count	 = NULL;
					}
				}
			}
			// Free Memory
			free(albname);
			free(artname);
			free(rating);
			free(songname);
			free(count);
		}
	}
	// Free XML Memory
	xmlXPathFreeObject (xmlobject);
	xmlXPathFreeContext (xmlperms_context);
	return;
}



// Order of parameters
// it2m ituneslibraryfile.xml password user databasename port
// 0    1                     2        3    4            5

main(int argc, char **argv)
{
	char * 	file;
	char * 	pass;
	char * 	user;
	char * 	dbname;
	int 	port=0;
	int 	i = 1;

	mysql=mysql_init(NULL);

	// Test Itunes Library File
	if(!is_file (argv[i]) && !is_file ("iTunes Music Library.xml") ){
		printf("Error : No such file to analyse, %s or iTunes Music Library.xml not found.\n\n", argv[0]);
		usage();
		exit(-1);
	} else if(is_file (argv[i])){
		file=strdup(argv[i]);
		i++;
	} else {
		file=strdup("iTunes Music Library.xml");		
	}
	


	// Test Password
	if(argv[i] == NULL){
		printf("Error : You must specify the MySQL password.\n");
		usage();
		exit(-1);
	} else {
		pass=strdup(argv[i]);
		i++;
	}

	// Test User
	if(argv[i]==NULL){
		printf("* No user given in argument, root will be used.\n");
		user=strdup("root");
	} else {
		user=strdup(argv[i]);
		i++;
	}

	// Test DataBase name
	if(argv[i]==NULL){
		printf("* No dbname given in argument, mythconverg will be used.\n");
		dbname=strdup("mythconverg");
	} else {
		dbname=strdup(argv[i]);
		i++;
	}
	

	// Test DataBase Port
	if(argv[i]==NULL){
		printf("* No port given in argument, 3306 will be used.\n");
		port=3306;
	} else {
		port=atoi(argv[i]);
	}

	printf(" Itunes Music File: %s \n Password: %s \n User: %s \n dbname: %s \n port: %d \n", file, pass, user, dbname, port);
	


	if (!mysql_real_connect(mysql,"localhost",user,pass,dbname,port,NULL,0)) {
		printf("Connection error : %s \n", mysql_error(mysql) );
		exit(1) ;
	} else {
		printf("MySQL aviable ...\n");
	}


	xmlDocPtr xmldoc = NULL;
	xmldoc = xmlParseFile (file);
	if (!xmldoc){
		printf("%s isn't an XML file.\n",file);
		//fprintf (stderr, "%s:%d: %s n'est pas un fichier XML.\n",argv[0], __FILE__, __LINE__);
		exit (EXIT_FAILURE);
	}

	scan_itms(file);

	xmlFreeDoc (xmldoc);
	mysql_close(mysql);
	exit (EXIT_SUCCESS);
}
