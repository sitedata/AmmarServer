#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>
#include <stdlib.h>

#include "../../../AmmServerlib/AmmServerlib.h"

struct SQLiteSession
{
 sqlite3 *db;
 sqlite3_stmt *res;
 char *err_msg;

 int rc;
};


struct SQLiteSession sqlserver={0};



int SQL_error(struct SQLiteSession * sqlserver,int rc,const char * msg , unsigned int line)
{
    if (rc != SQLITE_OK )
    {
        fprintf(stderr, "Error encountered while running SQL %s : %u\n",msg , line);
        fprintf(stderr, "SQL error: %s\n", sqlserver->err_msg);

        sqlite3_free(sqlserver->err_msg);
        sqlite3_close(sqlserver->db);
        return 1;
    }
  return 0;
}


int SQL_init(struct SQLiteSession * sqlserver , const char * dbFilename)
{
    if (sqlite3_config(SQLITE_CONFIG_SERIALIZED)!=SQLITE_OK)
    {
     fprintf(stderr,"Cannot set SQLite to serialized mode , cannot continue we are not going to be thread safe..!");
     return 0;
    }

    sqlserver->rc = sqlite3_open(dbFilename, &sqlserver->db);
    if (sqlserver->rc != SQLITE_OK)
    {
     fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(sqlserver->db));
     sqlite3_close(sqlserver->db);
     return 0;
    }

 return 1;
}

int SQL_close(struct SQLiteSession * sqlserver)
{
    sqlite3_close(sqlserver->db);
    return 1;
}

int SQL_getVersion(struct SQLiteSession * sqlserver)
{
    sqlserver->rc = sqlite3_prepare_v2(sqlserver->db, "SELECT SQLITE_VERSION()", -1, &sqlserver->res, 0);
    if (sqlserver->rc != SQLITE_OK)
    {
     fprintf(stderr, "Failed to fetch data: %s\n", sqlite3_errmsg(sqlserver->db));
     sqlite3_close(sqlserver->db);
     return 0;
    }

    sqlserver->rc = sqlite3_step(sqlserver->res);

    if (sqlserver->rc == SQLITE_ROW)
    {
        printf("%s\n", sqlite3_column_text(sqlserver->res, 0));
    }

    sqlite3_finalize(sqlserver->res);

  return 1;
}





/*
       =======================================================================================
       =======================================================================================
       =======================================================================================
       =======================================================================================
*/


int SQL_appendpost(struct SQLiteSession * sqlserver , const char * title , char * author, const char * data , unsigned int dataSize )
{

   unsigned int querySize = strlen(title)+strlen(author)+dataSize + 200;
   char * sql = (char *) malloc(sizeof(char) * querySize );


   if (sql!=0)
   {

    //sqlite3_snprintf(sql,querySize,"DROP TABLE IF EXISTS posts;\nCREATE TABLE posts(Id INTEGER PRIMARY KEY AUTOINCREMENT,title TEXT,date TEXT,author TEXT,content TEXT);INSERT INTO posts (title,date,author,content) VALUES(%q,%q,%q,%q);",title,"0/0/0",author,data  );
    /*Fucking historical accident*/
    sqlite3_snprintf(querySize,sql,"INSERT INTO posts (title,date,author,content) VALUES(%q,%q,%q,%q);",title,"0/0/0",author,data  );

    //snprintf(sql,querySize,"DROP TABLE IF EXISTS posts;\nCREATE TABLE posts(Id INTEGER PRIMARY KEY AUTOINCREMENT,title TEXT,date TEXT,author TEXT,content TEXT);INSERT INTO posts (title,date,author,content) VALUES(%s,%s,%s,%s);",title,"0/0/0",author,data  );

     fprintf(stderr,"Running query %s\n",sql);

    sqlserver->rc = sqlite3_exec(sqlserver->db, sql, 0, 0, &sqlserver->err_msg);
    free(sql);

    if ( SQL_error(sqlserver,sqlserver->rc, __FILE__, __LINE__) )
    {
        return 0;
    }

    return 1;
   }
  return 0;
}


int main(int argc, char *argv[])
{
    if (argc<3)
    {
        fprintf(stderr,"Usage : myblogTool filename \"Title\" \"Author\" ");
        return 0;
    }

    SQL_init(&sqlserver,"myblog.db");


    fprintf(stderr,"File To Add : %s  -  Title %s - Author %s \n",argv[1],argv[2],argv[3]);

    struct AmmServer_MemoryHandler *  tmp = AmmServer_ReadFileToMemoryHandler(argv[1]);

      SQL_appendpost(&sqlserver ,argv[2],argv[3] , tmp->content , tmp->contentCurrentLength );

    //-------------
    AmmServer_FreeMemoryHandler(&tmp);

    SQL_close(&sqlserver);
    return 0;
}





