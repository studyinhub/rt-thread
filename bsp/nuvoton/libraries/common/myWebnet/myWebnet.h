#ifndef __MYWEBNET_H__
#define __MYWEBNET_H__

#include <string.h>
#include <stdlib.h>

#include "webnet.h"
#include <wn_module.h>

#include "myConfig.h"

#include "cgi.h"
// #include "mnt.h"
// #include "app_config.h"

extern char WEB_ROOT[128];
extern char buildtime[20];
extern rt_thread_t g_tftp_tid;


extern int make_root_dirs();

extern int init_tftps(char *root_path);
extern int init_webnet(char *root_path);

#endif