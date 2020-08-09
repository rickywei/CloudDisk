#ifndef _CFG_H
#define _CFG_H

#define CFG_PATH "./conf/cfg.json"

#define CFG_LOG_MODULE "cgi"
#define CFG_LOG_PROC "cfg"

extern int get_cfg_value(const char *profile, char *tile, char *key, char *value);

extern int get_mysql_info(char *mysql_user, char *mysql_pwd, char *mysql_db);

#endif
