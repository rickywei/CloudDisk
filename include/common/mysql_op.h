#ifndef _MYSQL_OP_H
#define _MYSQL_OP_H

#include <mysql/mysql.h>

#define SQL_MAX_LEN (512)

void print_error(MYSQL *conn, const char *title);

MYSQL *msql_conn(char *user_name, char *passwd, char *db_name);

void process_result_test(MYSQL *conn, MYSQL_RES *res_set);

int process_result_one(MYSQL *conn, char *sql_cmd, char *buf);

#endif
