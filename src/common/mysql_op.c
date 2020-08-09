#include "mysql_op.h"

#include <mysql/mysql.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// log mysql error
void print_error(MYSQL *conn, const char *title) {
    fprintf(stderr, "%s:\nError %u (%s)\n", title, mysql_errno(conn),
            mysql_error(conn));
}

// connect to mysql
MYSQL *msql_conn(char *user_name, char *passwd, char *db_name) {
    MYSQL *conn = NULL;
    conn = mysql_init(NULL);
    if (conn == NULL) {
        fprintf(stderr, "mysql 初始化失败\n");
        return NULL;
    }

    // should init before call connect
    if (mysql_real_connect(conn, NULL, user_name, passwd, db_name, 0, NULL,
                           0) == NULL) {
        fprintf(stderr, "mysql_conn 失败:Error %u(%s)\n", mysql_errno(conn),
                mysql_error(conn));

        mysql_close(conn);
        return NULL;
    }

    return conn;
}

void process_result_test(MYSQL *conn, MYSQL_RES *res_set) {
    MYSQL_ROW row;
    uint i;

    // mysql_fetch_row get one result from mysql_store_result
    while ((row = mysql_fetch_row(res_set)) != NULL) {
        for (i = 0; i < mysql_num_fields(res_set); i++) {
            if (i > 0) {
                fputc('\t', stdout);
            }
            printf("%s", row[i] != NULL ? row[i] : "NULL");
        }
        fputc('\n', stdout);
    }

    if (mysql_errno(conn) != 0) {
        print_error(conn, "mysql_fetch_row() failed");
    } else {
        // return rows
        printf("%lu rows returned \n", (ulong)mysql_num_rows(res_set));
    }
}

// process one result from results set(buf)
int process_result_one(MYSQL *conn, char *sql_cmd, char *buf) {
    int ret = 0;
    MYSQL_RES *res_set = NULL;

    MYSQL_ROW row;
    ulong line = 0;

    if (mysql_query(conn, sql_cmd) != 0) {
        print_error(conn, "mysql_query error!\n");
        ret = -1;
        goto END;
    }

    res_set = mysql_store_result(conn);
    if (res_set == NULL) {
        print_error(conn, "smysql_store_result error!\n");
        ret = -1;
        goto END;
    }

    line = mysql_num_rows(res_set);
    if (line == 0) {
        ret = 1;  // 1: no record
        goto END;
    } else if (line > 0 && buf == NULL) {
        ret = 2;  // 2: has record without save
        goto END;
    }

    if ((row = mysql_fetch_row(res_set)) != NULL) {
        if (row[0] != NULL) {
            strcpy(buf, row[0]);
        }
    }

END:
    if (res_set != NULL) {
        mysql_free_result(res_set);
    }

    return ret;
}
