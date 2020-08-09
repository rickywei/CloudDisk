#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "cJSON.h"
#include "cfg.h"
#include "cgi_util.h"
#include "fcgi_config.h"
#include "fcgi_stdio.h"
#include "make_log.h"
#include "mysql_op.h"

#define MYFILES_LOG_MODULE "cgi"
#define MYFILES_LOG_PROC "myfiles"

static char mysql_user[128] = {0};
static char mysql_pwd[128] = {0};
static char mysql_db[128] = {0};

// redis
// static char redis_ip[30] = {0};
// static char redis_port[10] = {0};

// read config
void read_cfg() {
    // mysql
    get_cfg_value(CFG_PATH, "mysql", "user", mysql_user);
    get_cfg_value(CFG_PATH, "mysql", "password", mysql_pwd);
    get_cfg_value(CFG_PATH, "mysql", "database", mysql_db);
    LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC,
        "mysql:[user=%s,pwd=%s,database=%s]", mysql_user, mysql_pwd, mysql_db);

    // redis
    // get_cfg_value(CFG_PATH, "redis", "ip", redis_ip);
    // get_cfg_value(CFG_PATH, "redis", "port", redis_port);
    // LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "redis:[ip=%s,port=%s]\n",
    // redis_ip, redis_port);
}

// get json info
int get_user_json_info(char *buf, char *user, char *token) {
    int ret = 0;

    // {
    //     "token": "9e894efc0b2a898a82765d0a7f2c94cb",
    //     user:xxxx
    // }

    cJSON *root = cJSON_Parse(buf);
    if (NULL == root) {
        LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "cJSON_Parse err\n");
        ret = -1;
        goto END;
    }

    // user
    cJSON *child1 = cJSON_GetObjectItem(root, "user");
    if (NULL == child1) {
        LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "cJSON_GetObjectItem err\n");
        ret = -1;
        goto END;
    }

    // LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "child1->valuestring = %s\n",
    // child1->valuestring);
    strcpy(user, child1->valuestring);

    // token
    cJSON *child2 = cJSON_GetObjectItem(root, "token");
    if (NULL == child2) {
        LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "cJSON_GetObjectItem err\n");
        ret = -1;
        goto END;
    }

    // LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "child2->valuestring = %s\n",
    // child2->valuestring);
    strcpy(token, child2->valuestring);

END:
    if (root != NULL) {
        cJSON_Delete(root);
        root = NULL;
    }

    return ret;
}

//
void return_login_status(long num, int token_flag) {
    char *out = NULL;
    char *token;
    char num_buf[128] = {0};

    if (token_flag == 0) {
        token = "110";
    } else {
        token = "111";
    }

    sprintf(num_buf, "%ld", num);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "num", num_buf);
    cJSON_AddStringToObject(root, "code", token);
    out = cJSON_Print(root);

    cJSON_Delete(root);

    if (out != NULL) {
        printf(out);
        free(out);
    }
}

// get file number of a user
long get_user_files_count(char *user, int ret) {
    char sql_cmd[SQL_MAX_LEN] = {0};
    MYSQL *conn = NULL;
    long line = 0;

    // connect the database
    conn = msql_conn(mysql_user, mysql_pwd, mysql_db);
    if (conn == NULL) {
        LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "msql_conn err\n");
        goto END;
    }

    mysql_query(conn, "set names utf8");

    sprintf(sql_cmd, "select count from user_file_count where user=\"%s\"",
            user);
    char tmp[512] = {0};
    int ret2 = process_result_one(conn, sql_cmd, tmp);
    if (ret2 != 0) {
        LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "%s mysql op failed\n",
            sql_cmd);
        goto END;
    }

    line = atol(tmp);

END:
    if (conn != NULL) {
        mysql_close(conn);
    }

    LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "line = %ld\n", line);

    // return_login_status(line, ret);
    return line;
}

//
int get_fileslist_json_info(char *buf, char *user, char *token, int *p_start,
                            int *p_count) {
    int ret = 0;

    // {
    //     "user": "xxx"
    //     "token": xxxx
    //     "start": 0
    //     "count": 10
    // }

    cJSON *root = cJSON_Parse(buf);
    if (NULL == root) {
        LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "cJSON_Parse err\n");
        ret = -1;
        goto END;
    }

    // user
    cJSON *child1 = cJSON_GetObjectItem(root, "user");
    if (NULL == child1) {
        LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "cJSON_GetObjectItem err\n");
        ret = -1;
        goto END;
    }

    // LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "child1->valuestring = %s\n",
    // child1->valuestring);
    strcpy(user, child1->valuestring);

    // token
    cJSON *child2 = cJSON_GetObjectItem(root, "token");
    if (NULL == child2) {
        LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "cJSON_GetObjectItem err\n");
        ret = -1;
        goto END;
    }

    strcpy(token, child2->valuestring);

    // start
    cJSON *child3 = cJSON_GetObjectItem(root, "start");
    if (NULL == child3) {
        LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "cJSON_GetObjectItem err\n");
        ret = -1;
        goto END;
    }

    *p_start = child3->valueint;

    // number
    cJSON *child4 = cJSON_GetObjectItem(root, "count");
    if (NULL == child4) {
        LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "cJSON_GetObjectItem err\n");
        ret = -1;
        goto END;
    }

    *p_count = child4->valueint;

END:
    if (root != NULL) {
        cJSON_Delete(root);
        root = NULL;
    }

    return ret;
}

int get_user_filelist(char *cmd, char *user, int start, int count) {
    int ret = 0;
    char sql_cmd[SQL_MAX_LEN] = {0};
    MYSQL *conn = NULL;
    cJSON *root = NULL;
    cJSON *array = NULL;
    char *out = NULL;
    char *out2 = NULL;
    MYSQL_RES *res_set = NULL;

    // connect the database
    conn = msql_conn(mysql_user, mysql_pwd, mysql_db);
    if (conn == NULL) {
        LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "msql_conn err\n");
        ret = -1;
        goto END;
    }

    mysql_query(conn, "set names utf8");

    sprintf(sql_cmd,
            "select user_file_list.*, file_info.url, file_info.size, "
            "file_info.type from file_info, user_file_list where user = "
            "'%s' and file_info.md5 = user_file_list.md5",
            user);

    LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "%s select files...\n", sql_cmd);

    if (mysql_query(conn, sql_cmd) != 0) {
        LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "%s mysql op failed：%s\n",
            sql_cmd, mysql_error(conn));
        ret = -1;
        goto END;
    }

    res_set = mysql_store_result(conn);
    if (res_set == NULL) {
        LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC,
            "smysql_store_result error: %s!\n", mysql_error(conn));
        ret = -1;
        goto END;
    }

    ulong line = 0;
    line = mysql_num_rows(res_set);
    if (line == 0) {
        LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC,
            "mysql_num_rows(res_set) failed：%s\n", mysql_error(conn));
        ret = -1;
        goto END;
    }

    MYSQL_ROW row;

    root = cJSON_CreateObject();
    array = cJSON_CreateArray();
    while ((row = mysql_fetch_row(res_set)) != NULL) {
        cJSON *item = cJSON_CreateObject();

        // {
        // "user": "yoyo",
        // "md5": "e8ea6031b779ac26c319ddf949ad9d8d",
        // "time": "2020-07-26 21:35:25",
        // "filename": "test.mp4",
        // "share_status": 0,
        // "pv": 0,
        // "url":
        // "http://192.168.31.109:80/group1/M00/00/00/wKgfbViy2Z2AJ-FTAaM3As-g3Z0782.mp4",
        // "size": 27473666,
        //  "type": "mp4"
        // }

        // user
        if (row[0] != NULL) {
            cJSON_AddStringToObject(item, "user", row[0]);
        }

        // md5
        if (row[1] != NULL) {
            cJSON_AddStringToObject(item, "md5", row[1]);
        }

        // createtime
        if (row[2] != NULL) {
            cJSON_AddStringToObject(item, "time", row[2]);
        }

        // filename
        if (row[3] != NULL) {
            cJSON_AddStringToObject(item, "filename", row[3]);
        }

        // shared_status
        if (row[4] != NULL) {
            cJSON_AddNumberToObject(item, "share_status", atoi(row[4]));
        }

        // pv
        if (row[5] != NULL) {
            cJSON_AddNumberToObject(item, "pv", atol(row[5]));
        }

        //-- url
        if (row[6] != NULL) {
            cJSON_AddStringToObject(item, "url", row[6]);
        }

        //-- size
        if (row[7] != NULL) {
            cJSON_AddNumberToObject(item, "size", atol(row[7]));
        }

        //-- type
        if (row[8] != NULL) {
            cJSON_AddStringToObject(item, "type", row[8]);
        }

        cJSON_AddItemToArray(array, item);
    }

    cJSON_AddItemToObject(root, "files", array);

    out = cJSON_Print(root);

    LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "%s\n", out);

END:
    if (ret == 0) {
        printf("%s", out);
    } else {
        // user file list:
        // success: json
        // failed: {"code": "015"}
        out2 = NULL;
        out2 = return_status("015");
    }
    if (out2 != NULL) {
        printf(out2);  // to web
        free(out2);
    }

    if (res_set != NULL) {
        mysql_free_result(res_set);
    }

    if (conn != NULL) {
        mysql_close(conn);
    }

    if (root != NULL) {
        cJSON_Delete(root);
    }

    if (out != NULL) {
        free(out);
    }

    return ret;
}

int main() {
    char cmd[20];
    char user[USER_NAME_LEN];
    char token[TOKEN_LEN];

    read_cfg();

    while (FCGI_Accept() >= 0) {
        // char *query = getenv("QUERY_STRING");
        // query_parse_key_value(query, "cmd", cmd, NULL);
        // LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "cmd = %s\n", cmd);

        char *contentLength = getenv("CONTENT_LENGTH");
        int len;

        printf("Content-type: text/html\r\n\r\n");

        if (contentLength == NULL) {
            len = 0;
        } else {
            len = atoi(contentLength);
        }

        if (len <= 0) {
            printf("No data from standard input.<p>\n");
            LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC,
                "len = 0, No data from standard input\n");
        } else {
            char buf[4 * 1024] = {0};
            int ret = 0;
            ret = fread(buf, 1, len, stdin);
            if (ret == 0) {
                LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC,
                    "fread(buf, 1, len, stdin) err\n");
                continue;
            }

            LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC, "buf = %s\n", buf);

            int start = 0;
            int count = 0;

            get_user_json_info(buf, user, token);

            ret = verify_token(user, token);

            count = get_user_files_count(user, ret);

            LOG(MYFILES_LOG_MODULE, MYFILES_LOG_PROC,
                "user = %s, token = %s, start = %d, count = %d\n", user, token,
                start, count);

            ret = verify_token(user, token);
            if (ret == 0) {
                get_user_filelist(cmd, user, start, count);
            } else {
                char *out = return_status("111");
                if (out != NULL) {
                    printf(out);
                    free(out);
                }
            }
        }
    }

    return 0;
}
