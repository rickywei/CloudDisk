#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "cfg.h"
#include "cgi_util.h"
#include "fcgi_stdio.h"
#include "make_log.h"
#include "mysql_op.h"

#define UPLOAD_LOG_MODULE "cgi"
#define UPLOAD_LOG_PROC "upload"

static char mysql_user[128] = {0};
static char mysql_pwd[128] = {0};
static char mysql_db[128] = {0};

// redis
// static char redis_ip[30] = {0};
// static char redis_port[10] = {0};

// read config
void read_cfg() {
    get_cfg_value(CFG_PATH, "mysql", "user", mysql_user);
    get_cfg_value(CFG_PATH, "mysql", "password", mysql_pwd);
    get_cfg_value(CFG_PATH, "mysql", "database", mysql_db);
    LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,
        "mysql:[user=%s,pwd=%s,database=%s]\n", mysql_user, mysql_pwd,
        mysql_db);

    // get_cfg_value(CFG_PATH, "redis", "ip", redis_ip);
    // get_cfg_value(CFG_PATH, "redis", "port", redis_port);
    // LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "redis:[ip=%s,port=%s]\n",
    // redis_ip, redis_port);
}

// get received data
int recv_save_file(long len, char *user, char *filename, char *md5,
                   long *p_size) {
    int ret = 0;
    char *file_buf = NULL;
    char *begin = NULL;
    char *p, *q;  //*k;

    char content_text[TEMP_BUF_MAX_LEN] = {0};
    char boundary[TEMP_BUF_MAX_LEN] = {0};

    file_buf = (char *)malloc(len);
    if (file_buf == NULL) {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,
            "malloc error! file size is to big!!!!\n");
        return -1;
    }

    int ret2 = fread(file_buf, 1, len, stdin);
    if (ret2 == 0) {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,
            "fread(file_buf, 1, len, stdin) err\n");
        ret = -1;
        goto END;
    }

    // ------WebKitFormBoundaryR5Jof8B9e8tv016y\r\n
    // Content-Disposition: form-data; name="info"\r\n
    // \r\n
    // 123|2020-08-04_19-06.png|ed9920aacb64a5ee3e7d05843d543e13|147171\r\n
    // ------WebKitFormBoundaryR5Jof8B9e8tv016y\r\n
    // Content-Disposition: form-data; name="file";\r\n
    // filename="2020-08-04_19-06.png" Content-Type: image/png\r\n
    // \r\n
    // data\r\n
    // ------WebKitFormBoundaryR5Jof8B9e8tv016y--

    begin = file_buf;
    p = begin;
    q = begin;

    p = strstr(begin, "\r\n");
    if (p == NULL) {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "wrong no boundary!\n");
        ret = -1;
        goto END;
    }

    strncpy(boundary, begin, p - begin);
    boundary[p - begin] = '\0';
    p += 2;  //\r\n after 1 boundary
    LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "boundary: [%s]\n", boundary);

    begin = p;
    p = strstr(begin, "\r\n");
    strncpy(content_text, begin, p - begin);
    p += 2;  // after 1 contet-text header
    LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "1st content-text [%s]\n",
        content_text);

    p += 2;
    begin = p;
    p = strstr(begin, "\r\n");
    if (p == NULL) {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,
            "ERROR: get user & file info error...\n");
        ret = -1;
        goto END;
    }
    strncpy(content_text, begin, p - begin);
    content_text[p - begin] = '\0';
    LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "info: [%s]\n", content_text);

    // user
    char *pch = strtok(content_text, "|");
    strncpy(user, pch, strlen(pch) + 1);
    LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "info: [%s]\n", user);
    // filename
    pch = strtok(NULL, "|");
    LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "info: [%s]\n", pch);
    strncpy(filename, pch, strlen(pch) + 1);
    LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "info: [%s]\n", filename);
    // md5
    pch = strtok(NULL, "|");
    strncpy(md5, pch, strlen(pch) + 1);
    LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "info: [%s]\n", md5);
    // size
    char sz[128] = {0};
    pch = strtok(NULL, "|");
    strncpy(sz, pch, strlen(pch) + 1);
    LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "info: [%s]\n", sz);

    LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,
        "get user & file info complete...\n");

    p += 2;  // -- start of 2nd boundary
    p = strstr(p, "\r\n");
    p += 2;  // start of content-disposition
    p = strstr(p, "\r\n");
    p += 2;  // start of contetn-type
    p = strstr(p, "\r\n");
    p += 2;
    p += 2;  // start of data
    begin = p;
    // find file's end
    p = memstr(begin, len - (q - p), boundary);
    LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "[%s]\n", p);
    if (p == NULL) {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,
            "memstr(begin, len, boundary) error\n");
        ret = -1;
        goto END;
    } else {
        p = p - 2;  //\r\n
    }

    int fd = 0;
    fd = open(filename, O_CREAT | O_WRONLY, 0644);
    if (fd < 0) {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "open %s error\n", filename);
        ret = -1;
        goto END;
    }

    ftruncate(fd, (p - begin));
    write(fd, begin, (p - begin));
    close(fd);

END:
    free(file_buf);
    return ret;
}

// to fdfs
int upload_to_dstorage(char *filename, char *fileid) {
    int ret = 0;

    pid_t pid;
    int fd[2];

    if (pipe(fd) < 0) {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "pip error\n");
        ret = -1;
        goto END;
    }

    pid = fork();
    if (pid < 0) {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "fork error\n");
        ret = -1;
        goto END;
    }

    if (pid == 0) {
        // close read
        close(fd[0]);

        dup2(fd[1], STDOUT_FILENO);  // dup2(fd[1], 1);

        char fdfs_cli_conf_path[256] = {0};
        get_cfg_value(CFG_PATH, "dfs_path", "client", fdfs_cli_conf_path);

        // fdfs_upload_file
        execlp("fdfs_upload_file", "fdfs_upload_file", fdfs_cli_conf_path,
               filename, NULL);

        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,
            "execlp fdfs_upload_file error\n");

        close(fd[1]);
    } else {
        // close write
        close(fd[1]);

        read(fd[0], fileid, TEMP_BUF_MAX_LEN);

        trim_space(fileid);

        if (strlen(fileid) == 0) {
            LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "[upload FAILED!]\n");
            ret = -1;
            goto END;
        }

        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "get [%s] succ!\n", fileid);

        wait(NULL);
        close(fd[0]);
    }

END:
    return ret;
}

// file url
int make_file_url(char *fileid, char *fdfs_file_url) {
    int ret = 0;

    char *p = NULL;
    char *q = NULL;
    char *k = NULL;

    char fdfs_file_stat_buf[TEMP_BUF_MAX_LEN] = {0};
    char fdfs_file_host_name[HOST_NAME_LEN] = {0};

    pid_t pid;
    int fd[2];

    if (pipe(fd) < 0) {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "pip error\n");
        ret = -1;
        goto END;
    }

    pid = fork();
    if (pid < 0) {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "fork error\n");
        ret = -1;
        goto END;
    }

    if (pid == 0) {
        // close read
        close(fd[0]);

        dup2(fd[1], STDOUT_FILENO);  // dup2(fd[1], 1);

        char fdfs_cli_conf_path[256] = {0};
        get_cfg_value(CFG_PATH, "dfs_path", "client", fdfs_cli_conf_path);

        execlp("fdfs_file_info", "fdfs_file_info", fdfs_cli_conf_path, fileid,
               NULL);

        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,
            "execlp fdfs_file_info error\n");

        close(fd[1]);
    } else {
        // close write
        close(fd[1]);

        read(fd[0], fdfs_file_stat_buf, TEMP_BUF_MAX_LEN);
        // LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "get file_ip [%s] succ\n",
        // fdfs_file_stat_buf);

        close(fd[0]);

        // url-->http://host_name/group1/M00/00/00/D12313123232312.png
        p = strstr(fdfs_file_stat_buf, "source ip address: ");

        q = p + strlen("source ip address: ");
        k = strstr(q, "\n");

        strncpy(fdfs_file_host_name, q, k - q);
        fdfs_file_host_name[k - q] = '\0';

        // printf("host_name:[%s]\n", fdfs_file_host_name);

        char storage_web_server_port[20] = {0};
        get_cfg_value(CFG_PATH, "storage_web_server", "port",
                      storage_web_server_port);
        strcat(fdfs_file_url, "http://");
        strcat(fdfs_file_url, fdfs_file_host_name);
        strcat(fdfs_file_url, ":");
        strcat(fdfs_file_url, storage_web_server_port);
        strcat(fdfs_file_url, "/");
        strcat(fdfs_file_url, fileid);

        // printf("[%s]\n", fdfs_file_url);
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "file url is: %s\n",
            fdfs_file_url);
    }

END:
    return ret;
}

// save file info to mysql
int store_fileinfo_to_mysql(char *user, char *filename, char *md5, long size,
                            char *fileid, char *fdfs_file_url) {
    int ret = 0;
    MYSQL *conn = NULL;

    time_t now;
    char create_time[TIME_STRING_LEN];
    char suffix[SUFFIX_LEN];
    char sql_cmd[SQL_MAX_LEN] = {0};

    conn = msql_conn(mysql_user, mysql_pwd, mysql_db);
    if (conn == NULL) {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "msql_conn connect err\n");
        ret = -1;
        goto END;
    }

    mysql_query(conn, "set names utf8");

    get_file_suffix(filename, suffix);

    // file_info
    sprintf(sql_cmd,
            "insert into file_info (md5, file_id, url, size, type, count) "
            "values ('%s', '%s', '%s', '%ld', '%s', %d)",
            md5, fileid, fdfs_file_url, size, suffix, 1);

    if (mysql_query(conn, sql_cmd) != 0) {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "%s 插入失败: %s\n", sql_cmd,
            mysql_error(conn));
        ret = -1;
        goto END;
    }

    LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "%s file info inster success\n",
        sql_cmd);

    now = time(NULL);
    strftime(create_time, TIME_STRING_LEN - 1, "%Y-%m-%d %H:%M:%S",
             localtime(&now));

    // user_file_list
    sprintf(sql_cmd,
            "insert into user_file_list(user, md5, createtime, filename, "
            "shared_status, pv) values ('%s', '%s', '%s', '%s', %d, %d)",
            user, md5, create_time, filename, 0, 0);
    if (mysql_query(conn, sql_cmd) != 0) {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "%s mysql op failed: %s\n",
            sql_cmd, mysql_error(conn));
        ret = -1;
        goto END;
    }

    sprintf(sql_cmd, "select count from user_file_count where user = '%s'",
            user);
    int ret2 = 0;
    char tmp[512] = {0};
    int count = 0;
    ret2 = process_result_one(conn, sql_cmd, tmp);
    if (ret2 == 1) {
        sprintf(sql_cmd,
                " insert into user_file_count (user, count) values('%s', %d)",
                user, 1);
    } else if (ret2 == 0) {
        // update count
        count = atoi(tmp);
        sprintf(sql_cmd,
                "update user_file_count set count = %d where user = '%s'",
                count + 1, user);
    }

    if (mysql_query(conn, sql_cmd) != 0) {
        LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "%s mysql op failed: %s\n",
            sql_cmd, mysql_error(conn));
        ret = -1;
        goto END;
    }

END:
    if (conn != NULL) {
        mysql_close(conn);
    }

    return ret;
}

int main() {
    char filename[FILE_NAME_LEN] = {0};
    char user[USER_NAME_LEN] = {0};
    char md5[MD5_LEN] = {0};
    long size;
    char fileid[TEMP_BUF_MAX_LEN] = {0};
    char fdfs_file_url[FILE_URL_LEN] = {0};

    read_cfg();

    while (FCGI_Accept() >= 0) {
        char *contentLength = getenv("CONTENT_LENGTH");
        long len;
        int ret = 0;

        printf("Content-type: text/html\r\n\r\n");

        if (contentLength != NULL) {
            len = strtol(contentLength, NULL, 10);
        } else {
            len = 0;
        }

        if (len <= 0) {
            printf("No data from standard input\n");
            LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,
                "len = 0, No data from standard input\n");
            ret = -1;
        } else {
            // receive file
            if (recv_save_file(len, user, filename, md5, &size) < 0) {
                ret = -1;
                goto END;
            }

            LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,
                "%s upload success [%s, size: %ld, md5: %s]\n", user, filename,
                size, md5);

            // to fdfs
            if (upload_to_dstorage(filename, fileid) < 0) {
                ret = -1;
                goto END;
            }

            // delete tmp file
            unlink(filename);

            // get url
            if (make_file_url(fileid, fdfs_file_url) < 0) {
                ret = -1;
                goto END;
            }

            // save file's fdfs info to mysql
            if (store_fileinfo_to_mysql(user, filename, md5, size, fileid,
                                        fdfs_file_url) < 0) {
                ret = -1;
                goto END;
            }

        END:
            memset(filename, 0, FILE_NAME_LEN);
            memset(user, 0, USER_NAME_LEN);
            memset(md5, 0, MD5_LEN);
            memset(fileid, 0, TEMP_BUF_MAX_LEN);
            memset(fdfs_file_url, 0, FILE_URL_LEN);

            char *out = NULL;
            // upload:
            //    success: {"code":"008"}
            //    failed: {"code":"009"}
            if (ret == 0) {
                out = return_status("008");
            } else {
                out = return_status("009");
            }

            if (out != NULL) {
                printf(out);
                free(out);
            }
        }
    }

    return 0;
}
