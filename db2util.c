#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sqlcli1.h>

#define DB2UTIL_VERSION "1.0.1 beta"

#define SQL_IS_INTEGER 0

#define DB2UTIL_MAX_ARGS 32
#define DB2UTIL_MAX_COLS 1024
#define DB2UTIL_MAX_ERR_MSG_LEN (SQL_MAX_MESSAGE_LENGTH + SQL_SQLSTATE_SIZE + 10)

#define DB2UTIL_CMD_HELP 1
#define DB2UTIL_CMD_QUERY 2

#define DB2UTIL_OUT_COMMA 10
#define DB2UTIL_OUT_JSON 11
#define DB2UTIL_OUT_SPACE 12

#define DB2UTIL_ARG_INPUT 20

int db2util_hash(char * str) {
  int key = DB2UTIL_CMD_HELP;
  if (strcmp(str,"-h") == 0) {
    key = DB2UTIL_CMD_HELP;
  } else if (strcmp(str,"json") == 0) {
    key = DB2UTIL_OUT_JSON;
  } else if (strcmp(str,"comma") == 0) {
    key = DB2UTIL_OUT_COMMA;
  } else if (strcmp(str,"space") == 0) {
    key = DB2UTIL_OUT_SPACE;
  } else if (strcmp(str,"-args") == 0) {
    key = DB2UTIL_ARG_INPUT;
  } else {
    key = DB2UTIL_CMD_QUERY;
  }
  return key;
}

int db2util_ccsid() {
  char * env_ccsid = getenv("CCSID");
  int ccsid = Qp2paseCCSID();
  if (env_ccsid) {
     ccsid = atoi(env_ccsid);
  }
  return ccsid;
}

void * db2util_new(int size) {
  void * buffer = malloc(size + 1);
  memset(buffer,0,size + 1);
  return buffer;
}

void db2util_free(char *buffer) {
  if (buffer) {
    free(buffer);
  }
}

void db2util_output_record_array_beg(int fmt) {
  switch (fmt) {
  case DB2UTIL_OUT_JSON:
    printf("{\"records\":[");
    break;
  case DB2UTIL_OUT_SPACE:
    break;
  case DB2UTIL_OUT_COMMA:
  default:
    break;
  }
}
void db2util_output_record_row_beg(int fmt, int flag) {
  switch (fmt) {
  case DB2UTIL_OUT_JSON:
    if (flag) {
      printf(",\n{");
    } else {
      printf("\n{");
    }
    break;
  case DB2UTIL_OUT_SPACE:
    break;
  case DB2UTIL_OUT_COMMA:
  default:
    break;
  }
}
void db2util_output_record_name_value(int fmt, int flag, char *name, char *value) {
  switch (fmt) {
  case DB2UTIL_OUT_JSON:
    if (flag) {
      printf(",");
    }
    printf("\"%s\":\"%s\"",name,value);
    break;
  case DB2UTIL_OUT_SPACE:
    if (flag) {
      printf(" ");
    }
    printf("\"%s\"",value);
    break;
  case DB2UTIL_OUT_COMMA:
  default:
    if (flag) {
      printf(",");
    }
    printf("\"%s\"",value);
    break;
  }
}
void db2util_output_record_row_end(int fmt) {
  switch (fmt) {
  case DB2UTIL_OUT_JSON:
    printf("}");
    break;
  case DB2UTIL_OUT_SPACE:
    printf("\n");
    break;
  case DB2UTIL_OUT_COMMA:
  default:
    printf("\n");
    break;
  }
}
void db2util_output_record_array_end(int fmt) {
  switch (fmt) {
  case DB2UTIL_OUT_JSON:
    printf("\n]}\n");
    break;
  case DB2UTIL_OUT_SPACE:
    printf("\n");
    break;
  case DB2UTIL_OUT_COMMA:
  default:
    printf("\n");
    break;
  }
}

/*
  db2util_check_sql_errors(henv, SQL_HANDLE_ENV,   rc);
  db2util_check_sql_errors(hdbc, SQL_HANDLE_DBC,   rc);
  db2util_check_sql_errors(hstmt, SQL_HANDLE_STMT, rc);
*/
void db2util_check_sql_errors( SQLHANDLE handle, SQLSMALLINT hType, int rc)
{
  SQLCHAR msg[SQL_MAX_MESSAGE_LENGTH + 1];
  SQLCHAR sqlstate[SQL_SQLSTATE_SIZE + 1];
  SQLCHAR errMsg[DB2UTIL_MAX_ERR_MSG_LEN];
  SQLINTEGER sqlcode = 0;
  SQLSMALLINT length = 0;
  SQLCHAR *p = NULL;
  SQLSMALLINT recno = 1;
  if (rc == SQL_ERROR) {
    memset(msg, '\0', SQL_MAX_MESSAGE_LENGTH + 1);
    memset(sqlstate, '\0', SQL_SQLSTATE_SIZE + 1);
    memset(errMsg, '\0', DB2UTIL_MAX_ERR_MSG_LEN);
    if ( SQLGetDiagRec(hType, handle, recno, sqlstate, &sqlcode, msg, SQL_MAX_MESSAGE_LENGTH + 1, &length)  == SQL_SUCCESS ) {
      if (msg[length-1] == '\n') {
        p = &msg[length-1];
        *p = '\0';
      }
      printf("Error %s SQLCODE=%d\n", msg, sqlcode);
    }
  }
}

int db2util_query(char * stmt, int fmt, int argc, char *argv[]) {
  int i = 0;
  int recs = 0;
  int rc = 0;
  int ccsid = db2util_ccsid();
  SQLHENV henv = 0;
  SQLHANDLE hdbc = 0;
  SQLHANDLE hstmt = 0;
  SQLINTEGER attr = SQL_TRUE;
  SQLSMALLINT nResultCols = 0;
  SQLSMALLINT name_length = 0;
  SQLCHAR *buff_name[DB2UTIL_MAX_COLS];
  SQLCHAR *buff_value[DB2UTIL_MAX_COLS];
  SQLSMALLINT type;
  SQLUINTEGER size;
  SQLSMALLINT scale;
  SQLSMALLINT nullable;
  SQLINTEGER lob_loc;
  SQLINTEGER loc_ind;
  SQLSMALLINT loc_type;
  SQLINTEGER fStrLen = SQL_NTS;

  /* ccsid */
  rc = SQLOverrideCCSID400(ccsid);

  /* env */
  rc = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);
  rc = SQLSetEnvAttr((SQLHENV)henv, SQL_ATTR_SERVER_MODE, (SQLPOINTER)&attr, SQL_IS_INTEGER);
  rc = SQLSetEnvAttr((SQLHENV)henv, SQL_ATTR_INCLUDE_NULL_IN_LEN, (SQLPOINTER)&attr, SQL_IS_INTEGER);
  if (ccsid == 1208) {
    rc = SQLSetEnvAttr((SQLHENV)henv, SQL_ATTR_UTF8, (SQLPOINTER)&attr, SQL_IS_INTEGER);
  }
  /* connect */
  rc = SQLAllocHandle( SQL_HANDLE_DBC, henv, &hdbc);
  db2util_check_sql_errors(hdbc, SQL_HANDLE_DBC,   rc);
  rc = SQLConnect( (SQLHDBC)hdbc, 
         (SQLCHAR *)NULL, (SQLSMALLINT)0,
         (SQLCHAR *)NULL, (SQLSMALLINT)0,
         (SQLCHAR *)NULL, (SQLSMALLINT)0);
  db2util_check_sql_errors(hdbc, SQL_HANDLE_DBC,   rc);
  rc = SQLSetConnectAttr((SQLHDBC)hdbc, SQL_ATTR_DBC_SYS_NAMING, (SQLPOINTER)&attr, SQL_IS_INTEGER);
  db2util_check_sql_errors(hdbc, SQL_HANDLE_DBC,   rc);
  /* statement */
  rc = SQLAllocHandle(SQL_HANDLE_STMT, (SQLHDBC) hdbc, &hstmt);
  db2util_check_sql_errors(hstmt, SQL_HANDLE_STMT, rc);
  /* TODO:
   * if we have args, switch to use prepare/excute
   * ("select * from QIWSs/QCUSTCDT where LSTNAM=? or LSTNAM=?")
   */
  /* query (no arguments) */
  rc = SQLExecDirect((SQLHSTMT)hstmt, (SQLPOINTER)stmt, SQL_NTS);
  db2util_check_sql_errors(hstmt, SQL_HANDLE_STMT, rc);
  /* result set */
  rc = SQLNumResultCols((SQLHSTMT)hstmt, &nResultCols);
  db2util_check_sql_errors(hstmt, SQL_HANDLE_STMT, rc);
  if (nResultCols > 0) {
    for (i=0;i < DB2UTIL_MAX_COLS;i++) {
      buff_name[i] = NULL;
      buff_value[i] = NULL;
    }
    for (i = 0 ; i < nResultCols; i++) {
      size = 128;
      buff_name[i] = db2util_new(size);
      buff_value[i] = NULL;
      rc = SQLDescribeCol((SQLHSTMT)hstmt, (SQLSMALLINT)(i + 1),
             (SQLCHAR *)buff_name[i], size, &name_length, 
             &type, &size, &scale, &nullable);
      db2util_check_sql_errors(hstmt, SQL_HANDLE_STMT, rc);
      /* dbcs expansion */
      switch (type) {
      case SQL_CHAR:
      case SQL_VARCHAR:
      case SQL_CLOB:
      case SQL_DBCLOB:
      case SQL_UTF8_CHAR:
      case SQL_WCHAR:
      case SQL_WVARCHAR:
      case SQL_GRAPHIC:
      case SQL_VARGRAPHIC:
      case SQL_XML:
        size = size * 6;
        buff_value[i] = db2util_new(size);
        rc = SQLBindCol((SQLHSTMT)hstmt, (i + 1), SQL_CHAR, buff_value[i], size, &fStrLen);
        break;
      case SQL_BINARY:
      case SQL_VARBINARY:
      case SQL_BLOB:
        size = size * 2;
        buff_value[i] = db2util_new(size);
        rc = SQLBindCol((SQLHSTMT)hstmt, (i + 1), SQL_CHAR, buff_value[i], size, &fStrLen);
        break;
      case SQL_TYPE_DATE:
      case SQL_TYPE_TIME:
      case SQL_TYPE_TIMESTAMP:
      case SQL_DATETIME:
      case SQL_BIGINT:
      case SQL_DECFLOAT:
      case SQL_SMALLINT:
      case SQL_INTEGER:
      case SQL_REAL:
      case SQL_FLOAT:
      case SQL_DOUBLE:
      case SQL_DECIMAL:
      case SQL_NUMERIC:
      default:
        size = 64;
        buff_value[i] = db2util_new(size);
        rc = SQLBindCol((SQLHSTMT)hstmt, (i + 1), SQL_CHAR, buff_value[i], size, &fStrLen);
        break;
      }
      db2util_check_sql_errors(hstmt, SQL_HANDLE_STMT, rc);
    }
    rc = SQL_SUCCESS;
    db2util_output_record_array_beg(fmt);
    while (rc == SQL_SUCCESS) {
      rc = SQLFetch(hstmt);
      db2util_output_record_row_beg(fmt, recs);
      recs += 1;
      for (i = 0 ; i < nResultCols; i++) {
        if (buff_value[i]) {
          db2util_output_record_name_value(fmt,i,buff_name[i],buff_value[i]);
        }
      }
      db2util_output_record_row_end(fmt);
    }
    db2util_output_record_array_end(fmt);
    for (i = 0 ; i < nResultCols; i++) {
      if (buff_value[i]) {
        db2util_free(buff_name[i]);
        buff_name[i] = NULL;
      }
      if (buff_name[i]) {
        db2util_free(buff_name[i]);
        buff_name[i] = NULL;
      }
    }
  }
  rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
  rc = SQLDisconnect((SQLHDBC)hdbc);
  rc = SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
  /* SQLFreeHandle(SQL_HANDLE_ENV, henv); */
  return rc;
}

void db2util_help() {
  printf("Syntax: db2util 'sql statement' [json|comma|space] -args parm1 parm2 ...\n");
  printf("  Control record(s) output:\n");
  printf("    json - {\"records\":[{\"name\"}:{\"value\"},{\"name\"}:{\"value\"},...]}\n");
  printf("    comma- \"value\",\"value\",...\n");
  printf("    space- \"value\" \"value\" ...\n");
  printf("Version: %s\n", DB2UTIL_VERSION);
}


int main(int argc, char *argv[]) {
  SQLRETURN rc = 0;
  int i = 0;
  int iargc = 0;
  char *iargv[DB2UTIL_MAX_ARGS];
  int command = DB2UTIL_CMD_HELP;
  int fmt = DB2UTIL_OUT_COMMA;
  int have = DB2UTIL_CMD_HELP;
  for (i=0; i < DB2UTIL_MAX_ARGS; i++) {
    iargv[i] = NULL;
  }
  if (argc > 1) {
    command = db2util_hash(argv[1]);
    if (argc > 2) {
      fmt = db2util_hash(argv[2]);
      if (fmt == DB2UTIL_ARG_INPUT) {
        have = DB2UTIL_ARG_INPUT;
        fmt = DB2UTIL_OUT_COMMA;
      }
      if (argc > 3) {
        have = db2util_hash(argv[3]);
      }
    }
  }
  switch(command) {
  case DB2UTIL_CMD_QUERY:
    return db2util_query(argv[1],fmt,iargc,iargv);
    break;
  case DB2UTIL_CMD_HELP:
  default:
    db2util_help();
    break;
  }
  return -1;
}