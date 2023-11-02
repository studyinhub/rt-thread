#include "myWebnet.h"

#include "UrlEncode.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "wb"
#define LOG_LVL LOG_LVL_DBG
#include <ulog.h>

// char *init_wb_dirs[] =
// {
//     WEB_ROOT,
//     WEB_ROOT"/css",
//     WEB_ROOT"/js",
//     WEB_ROOT"/fonts"
// };

#define NUMBER_OF_STRING 3
#define MAX_STRING_SIZE 40

char WEB_ROOT[128];

char buildtime[20];

rt_thread_t g_tftp_tid;

char init_wb_dirs[NUMBER_OF_STRING][MAX_STRING_SIZE] =
    {
        "/css",
        "/js",
        "/fonts"};

int make_root_dirs()
{
    char path[MAX_STRING_SIZE];
    for (int i = 0; i < NUMBER_OF_STRING; i++)
    {
        rt_sprintf(path, "%s%s", WEB_ROOT, init_wb_dirs[i]);
        LOG_D("dirs:%s", path);
    }

}

static void tftp_server_thread(void *param)
{
    tftp_server_run((struct tftp_server *)param);
    tftp_server = RT_NULL;
}

int init_tftps(char *root_path)
{
    LOG_I("启动 tftp server:%s", root_path);
    tftp_server = tftp_server_create(root_path, 69);
    if (!tftp_server)
    {
        log_e("创建 tftp_server 失败");
        return -1;
    }
    tftp_server_write_set(tftp_server, 1);

    g_tftp_tid = rt_thread_create("tftps", tftp_server_thread, tftp_server, 2048, 18, 20);
    if (g_tftp_tid == NULL)
    {
        LOG_E("create tftps thread faild");
    }
    if (rt_thread_startup(g_tftp_tid) == RT_EOK)
    {
        LOG_I("tftps Thread 启动成功");
    }
    LOG_D("tftp_server->is_stop:%d", tftp_server->is_stop);
}

void asp_var_version(struct webnet_session *session)
{
    RT_ASSERT(session != RT_NULL);

    static const char *version = "<html><body><font size=\"+2\">RT-Thread %d.%d.%d %s</font><br><br>"
                                 "<a href=\"javascript:history.go(-1);\">Go back to root</a></html></body>";

    webnet_session_printf(session, version, RT_VERSION, RT_SUBVERSION, RT_REVISION, buildtime);
}

void cgi_login_handler(struct webnet_session *session)
{
}

void cgi_calc_handler(struct webnet_session *session)
{
    int a, b;
    const char *mimetype;
    struct webnet_request *request;
    static const char *header = "<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; "
                                "charset=gb2312\" /><title> calc </title></head>";

    static const char *body = "<body><form method=\"post\" action=\"/cgi-bin/calc\"><input type=\"text\" name=\"a\" value=\"%d\"> "
                              "+ <input type=\"text\" name=\"b\" value=\"%d\"> = %d<br><input type=\"submit\" value=\"\xBC\xC6\xCB\xE3\"></form>"
                              "<br><a href=\"/index.html\">Go back to root</a></body></html>\r\n";

    RT_ASSERT(session != RT_NULL);
    request = session->request;
    RT_ASSERT(request != RT_NULL);

    /* get mimetype */
    mimetype = mime_get_type(".html");

    a = 1;
    b = 1;
    /* set http header */
    session->request->result_code = 200;
    webnet_session_set_header(session, mimetype, 200, "Ok", -1);

    webnet_session_write(session, (const rt_uint8_t *)header, rt_strlen(header));
    if (request->query_counter)
    {
        const char *a_value, *b_value;
        a_value = webnet_request_get_query(request, "a");
        b_value = webnet_request_get_query(request, "b");

        a = atoi(a_value);
        b = atoi(b_value);
    }

    webnet_session_printf(session, body, a, b, a + b);
}

void cgi_reset_handler(struct webnet_session *session)
{
    const char *mimetype;
    session->request->result_code = 200;
    static const char *status = "{\"data\":\"ok\"}";

    mimetype = mime_get_type(".json");

       /* set http header */
    session->request->result_code = 200;
    webnet_session_set_header(session, mimetype, 200, "Ok", strlen(status));

    webnet_session_write(session, (const rt_uint8_t *)status, rt_strlen(status));

    extern void rt_hw_cpu_reset(void);
    rt_hw_cpu_reset();

}

void cgi_hello_handler(struct webnet_session *session)
{
    const char *mimetype;
    static const char *status = "<html><head><title> hello </title>"
                                "</head><body><font size=\"+2\">hello world</font><br><br>"
                                "<a href=\"javascript:history.go(-1);\">Go back to root</a></body></html>\r\n";
    RT_ASSERT(session != RT_NULL);

    /* get mimetype */
    mimetype = mime_get_type(".html");

    /* set http header */
    session->request->result_code = 200;
    webnet_session_set_header(session, mimetype, 200, "Ok", strlen(status));

    webnet_session_write(session, (const rt_uint8_t *)status, rt_strlen(status));
}

void cgi_getconfig_handler(struct webnet_session *session)
{
    const char *mimetype;

    RT_ASSERT(session != RT_NULL);

    /* get mimetype */
    mimetype = mime_get_type(".html");
    load_config();

    // cJSON_AddItemToObject(g_root, "webnet_in_ram", cJSON_CreateBool(webnet_in_ram));
    // cJSON_AddItemToObject(g_root, "web_root", cJSON_CreateString(WEB_ROOT));
    rt_sprintf(g_BUF_CONFIG_JSON, "%s", cJSON_Print(g_root));

    /* set http header */
    session->request->result_code = 200;
    webnet_session_set_header(session, mimetype, 200, "Ok", strlen(g_BUF_CONFIG_JSON));
    webnet_session_write(session, (const rt_uint8_t *)g_BUF_CONFIG_JSON, rt_strlen(g_BUF_CONFIG_JSON));
}

void cgi_putconfig_handler(struct webnet_session *session)
{
    const char *mimetype;

    RT_ASSERT(session != RT_NULL);

    log_d("准备更新配置");

    // webnet_request_get_query();

    struct webnet_request *request;

    // /* get mimetype */
    mimetype = mime_get_type("json");

    request = session->request;
    log_d("query_counter:%d", request->query_counter);
    log_d("content_type:%s",request->content_type);
    log_d("session->buffer:%s", session->buffer);

    cJSON *root = cJSON_Parse(session->buffer);
    cJSON *item = cJSON_GetObjectItem(root, "lastConfigAt");
    log_d("lastConfigAt:%s",item->valuestring);



    // 对session->buffer其进行 url 解码操作

    // const char *port1_baudrate, *name;
    // port1_baudrate = webnet_request_get_query(request, "ports[0][baudrate]");
    // name = webnet_request_get_query(request, "name");
    // log_d("port1_baudrate:%s", port1_baudrate);

    // print_time();
    // cJSON_ReplaceItemInObject(g_root, "lastConfigAt", cJSON_CreateString(g_FmtTimeStr));

    // cJSON *pPorts;
    
    // pPorts = cJSON_GetObjectItem(g_root, "ports");
    // printJSON(pPorts);

    // cJSON *pPort1 = cJSON_GetArrayItem(pPorts, 1);
    // printJSON(pPort1);
    // cJSON_ReplaceItemInObject(pPort1, "baud_rate", cJSON_CreateNumber(atoi(port1_baudrate)));
    // printJSON(pPort1);
    // printJSON(g_root);

    char path[50];
    // rt_bool_t bSync2Flash = RT_FALSE;
    // cJSON *item = NULL;
    // item = cJSON_GetObjectItem(g_root, "sync2flash");
    // bSync2Flash = item->valueint;
    // log_w("bSync2Flash:%d", bSync2Flash);
    // if (bSync2Flash)
    // {
    //     log_i("保存到 flash");
    //     rt_sprintf(path, "%s%s", "/mnt/filesystem/webnet/", "/config.json");
    // }else
    // {   
    //     log_i("保存到 ram");
    //     rt_sprintf(path, "%s", "/webnet/config.json");
    // }

    rt_sprintf(path, "%s/config.json", WEB_ROOT);

    save_config(path, root);

    cJSON_Delete(root);

    static const char *status = "{\"ret\":\"ok\"}";
    session->request->result_code = 200;
    webnet_session_set_header(session, mimetype, 200, status, strlen(status));
    webnet_session_write(session, (const rt_uint8_t *)status, rt_strlen(status));
}

int chg_root(int argc, char **argv)
{
    char path[50];

    if (!strcmp(argv[1], "flash"))
    {
        rt_sprintf(path, "%s", "/mnt/filesystem/webnet");
    }
    else
    {
        rt_sprintf(path, "%s", "/webnet");
    }
    system("cp /mnt/filesystem/webnet/ /webnet/");

    switch_root(path);
}

MSH_CMD_EXPORT(chg_root, modify webnet root dir);

static void success(char *errMsg, char *data)
{
    // {
    //  "errMsg:":"ok",
    //  "data":"hello"
    // }

    cJSON *root;
}

void cgi_chgroot_handler(struct webnet_session *session)
{
    const char *mimetype;

    RT_ASSERT(session != RT_NULL);

    log_d("准备修改 web root");

    struct webnet_request *request;

    /* get mimetype */
    mimetype = mime_get_type(".html");

    request = session->request;

    log_d("session->buffer:%s", session->buffer);

    if (request->query_counter)
    {
        const char *path;
        path = webnet_request_get_query(request, "path");
        log_d("path:%s", path);
        system("chg_root flash");
    }
    static const char *status = "ok";
    session->request->result_code = 200;
    webnet_session_set_header(session, mimetype, 200, status, strlen(status));
    webnet_session_write(session, (const rt_uint8_t *)status, rt_strlen(status));
}

int init_webnet(char *root_path)
{

    log_d("init_webnet root_path:%s", root_path);
    make_root_dirs();
    webnet_set_root(root_path);

#ifdef WEBNET_USING_CGI
    webnet_cgi_register("reset", cgi_reset_handler);
    webnet_cgi_register("hello", cgi_hello_handler);
    webnet_cgi_register("get_config", cgi_getconfig_handler);
    webnet_cgi_register("put_config", cgi_putconfig_handler);
    webnet_cgi_register("chg_root", cgi_chgroot_handler);
    webnet_cgi_register("calc", cgi_calc_handler);
    webnet_cgi_register("calc", cgi_login_handler);
#endif

#ifdef WEBNET_USING_ASP
    webnet_asp_add_var("version", asp_var_version);
#endif

#ifdef WEBNET_USING_ALIAS
    webnet_alias_set("/config", "/admin");
#endif

#ifdef WEBNET_USING_AUTH
    webnet_auth_set("/admin", "admin:snahko");
#endif

// #ifdef WEBNET_USING_UPLOAD
//     extern const struct webnet_module_upload_entry upload_entry_upload;
//     webnet_upload_add(&upload_entry_upload);
// #endif
    LOG_I("启动 webnet");
    webnet_init();

    DIR *pDir = opendir(WEB_ROOT);
    if (pDir == RT_NULL)
    {
        log_e("open dir WEB_ROOT %s faild",WEB_ROOT);
    }else
    {
        log_i("open dir WEB_ROOT %s success",WEB_ROOT);
    }
}