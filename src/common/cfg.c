#include "cfg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "make_log.h"

// read config from file
int get_cfg_value(const char *profile, char *title, char *key, char *value) {
    int ret = 0;
    char *buf = NULL;
    FILE *fp = NULL;

    if (profile == NULL || title == NULL || key == NULL || value == NULL) {
        return -1;
    }

    // read-only
    fp = fopen(profile, "rb");
    if (fp == NULL) {
        perror("fopen");
        LOG(CFG_LOG_MODULE, CFG_LOG_PROC, "fopen err\n");
        ret = -1;
        goto END;
    }

    fseek(fp, 0, SEEK_END);  // cursor to end
    long size = ftell(fp);   // get file size
    fseek(fp, 0, SEEK_SET);  // to begin

    buf = (char *)calloc(1, size + 1);
    if (buf == NULL) {
        perror("calloc");
        LOG(CFG_LOG_MODULE, CFG_LOG_PROC, "calloc err\n");
        ret = -1;
        goto END;
    }

    fread(buf, 1, size, fp);

    // json string to cJson object
    cJSON *root = cJSON_Parse(buf);
    if (NULL == root) {
        LOG(CFG_LOG_MODULE, CFG_LOG_PROC, "root err\n");
        ret = -1;
        goto END;
    }

    cJSON *father = cJSON_GetObjectItem(root, title);
    if (NULL == father) {
        LOG(CFG_LOG_MODULE, CFG_LOG_PROC, "father err\n");
        ret = -1;
        goto END;
    }

    cJSON *son = cJSON_GetObjectItem(father, key);
    if (NULL == son) {
        LOG(CFG_LOG_MODULE, CFG_LOG_PROC, "son err\n");
        ret = -1;
        goto END;
    }

    strcpy(value, son->valuestring);

    cJSON_Delete(root);

END:
    if (fp != NULL) {
        fclose(fp);
    }

    if (buf != NULL) {
        free(buf);
    }

    return ret;
}

// get mysql user passwd
int get_mysql_info(char *mysql_user, char *mysql_pwd, char *mysql_db) {
    if (-1 == get_cfg_value(CFG_PATH, "mysql", "user", mysql_user)) {
        LOG(CFG_LOG_MODULE, CFG_LOG_PROC, "mysql_user err\n");
        return -1;
    }

    if (-1 == get_cfg_value(CFG_PATH, "mysql", "password", mysql_pwd)) {
        LOG(CFG_LOG_MODULE, CFG_LOG_PROC, "mysql_pwd err\n");
        return -1;
    }

    if (-1 == get_cfg_value(CFG_PATH, "mysql", "database", mysql_db)) {
        LOG(CFG_LOG_MODULE, CFG_LOG_PROC, "mysql_db err\n");
        return -1;
    }

    return 0;
}
