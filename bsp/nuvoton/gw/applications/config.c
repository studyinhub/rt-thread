#include "config.h"
#include "myWebnet.h"
#include "cJSON.h"

#define LOG_TAG "config"
#define LOG_LVL LOG_LVL_DBG
#include <ulog.h>

char g_BUF_CONFIG_JSON[MAX_CONFIG_JSON_SIZE];

rt_bool_t webnet_in_ram = RT_FALSE;

char g_FmtTimeStr[50];

void print_time()
{
    time_t timep;
    struct tm *p;
    time(&timep);
    p = localtime(&timep);
    rt_sprintf(g_FmtTimeStr, "%d-%d-%d %d:%d:%d", (1900 + p->tm_year), (1 + p->tm_mon), p->tm_mday,
               p->tm_hour, p->tm_min, p->tm_sec);
    rt_kprintf("g_FmtTimeStr:%s", g_FmtTimeStr);
}

void printJSON(cJSON *root) //以递归的方式打印json的最内层键值对
{
    log_d("打印：\n%s\n", cJSON_Print(root));

    for (int i = 0; i < cJSON_GetArraySize(root); i++) //遍历最外层json键值对
    {
        cJSON *item = cJSON_GetArrayItem(root, i);
        if (cJSON_Object == item->type) //如果对应键的值仍为cJSON_Object就递归调用printJson
            printJSON(item);
        else //值不为json对象就直接打印出键和值
        {
            printf("%s->", item->string);
            printf("%s\n", cJSON_Print(item));
        }
    }
}

cJSON *g_root = NULL;

void build_config_factory_json()
{
    g_root = cJSON_CreateObject(); //创建一个json对象

    cJSON_AddItemToObject(g_root, "lastConfigAt", cJSON_CreateString("2022-02-14 17:00:00"));
    cJSON_AddItemToObject(g_root, "masterID", cJSON_CreateNumber(1));

    cJSON *Array_baud;
    Array_baud = cJSON_CreateArray();

    cJSON_AddItemToArray(Array_baud, cJSON_CreateString("2400"));
    cJSON_AddItemToArray(Array_baud, cJSON_CreateString("4800"));
    cJSON_AddItemToArray(Array_baud, cJSON_CreateString("9600"));
    cJSON_AddItemToArray(Array_baud, cJSON_CreateString("19200"));
    cJSON_AddItemToArray(Array_baud, cJSON_CreateString("38400"));
    cJSON_AddItemToArray(Array_baud, cJSON_CreateString("57600"));
    cJSON_AddItemToArray(Array_baud, cJSON_CreateString("115200"));
    cJSON_AddItemToObject(g_root, "baudrates", Array_baud);

    cJSON *Array_querys = cJSON_CreateArray();
    cJSON *Query_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(Query_obj, "id", 1);
    cJSON_AddNumberToObject(Query_obj, "cmd", 3);
    cJSON_AddNumberToObject(Query_obj, "start", 0);
    cJSON_AddNumberToObject(Query_obj, "cnt", 10);
    cJSON_AddItemToArray(Array_querys, Query_obj);

    Query_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(Query_obj, "id", 2);
    cJSON_AddNumberToObject(Query_obj, "cmd", 3);
    cJSON_AddNumberToObject(Query_obj, "start", 0);
    cJSON_AddNumberToObject(Query_obj, "cnt", 10);
    cJSON_AddItemToArray(Array_querys, Query_obj);

    cJSON_AddItemToObject(g_root, "querys", Array_querys);

    cJSON *Array_ports = cJSON_CreateArray();

    Query_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(Query_obj, "name", "uart1");
    cJSON_AddNumberToObject(Query_obj, "baud_rate", 9600);
    cJSON_AddNumberToObject(Query_obj, "data_bits", 8);
    cJSON_AddNumberToObject(Query_obj, "parity", 0);
    cJSON_AddNumberToObject(Query_obj, "stop_bits", 1);
    cJSON_AddItemToArray(Array_ports, Query_obj);

    Query_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(Query_obj, "name", "uart6");
    cJSON_AddNumberToObject(Query_obj, "baud_rate", 9600);
    cJSON_AddNumberToObject(Query_obj, "data_bits", 8);
    cJSON_AddNumberToObject(Query_obj, "parity", 0);
    cJSON_AddNumberToObject(Query_obj, "stop_bits", 1);
    ;
    cJSON_AddItemToArray(Array_ports, Query_obj);

    Query_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(Query_obj, "name", "uart8");
    cJSON_AddNumberToObject(Query_obj, "baud_rate", 9600);
    cJSON_AddNumberToObject(Query_obj, "data_bits", 8);
    cJSON_AddNumberToObject(Query_obj, "parity", 0);
    cJSON_AddNumberToObject(Query_obj, "stop_bits", 1);
    ;
    cJSON_AddItemToArray(Array_ports, Query_obj);
    cJSON_AddItemToObject(g_root, "ports", Array_ports);

    // LOG_D("构建的JSON:\n%s\n", cJSON_Print(g_root));
    // cJSON_Delete(root);
}

int save_config(char *path, cJSON *root)
{
    int fd = open(path, O_WRONLY);

    log_d("准备保存如下 json 到路径:%s", path);
    printJSON(root);

    char jsonString[1024];

    sprintf(jsonString, "%s", cJSON_Print(root));

    int iConfigJsonSize = 0;
    if (fd < 0)
    {
        log_e("Open config.json failed");
        return -1;
    }
    iConfigJsonSize = write(fd, jsonString, rt_strlen(jsonString));
    close(fd);
    if (iConfigJsonSize > MAX_CONFIG_JSON_SIZE)
    {
        log_e("Config file is too large");
        return -1;
    }
    else if (iConfigJsonSize < 0)
    {
        log_e("Write config.json failed");
        return -1;
    }
    else
    {
        log_d("Write config.json success.(%d)", iConfigJsonSize);
        // rt_kprintf("%s\n", g_BUF_CONFIG_JSON);
    }
}

struct tftp_server *tftp_server;

int switch_root(char *path)
{

    webnet_set_root(path);
    // 删除 tftp_thead

    rt_thread_t tftp_tid;
    tftp_tid = rt_thread_find("tftps");
    if (tftp_tid && tftp_tid == g_tftp_tid)
    {
        tftp_server_destroy(tftp_server);

        do
        {
            if (tftp_server != NULL)
            {
                LOG_D("tftp_server->is_stop:%d", tftp_server->is_stop);
            }
            rt_thread_delay(1000);
        } while (tftp_server);

        if (rt_thread_find("tftps"))
        {
            do
            {
                if (rt_thread_delete(g_tftp_tid) == RT_EOK)
                {
                    log_i("tftps 删除成功");
                }
                rt_thread_delay(1000);
            } while (rt_thread_find("tftps"));
        }
        else
        {
            log_i("不需要删除 tftps,已经退出了");
        }
        init_tftps(path);
    }
    else
    {
        log_e("没有找到 tftps 线程"); // rt_thread_find 没有找到会返回 0
    }
}

int load_config()
{
    // V1 判断 ram 中是否有 config.json,如果有，就将 webnet 设置在 ram，否则设置在 flash
    // V2 不再通过判断 config.json 是否存在，通过 flash 中的 webnet_in_ram 来判断，是否将 flash 中的 webnet 复制到 ram

    char path[50];
    int iConfigJsonSize = 0;
    int fd = 0;

    int config_from =0; // 1:flash 0:ram
    cJSON *item = NULL;

    // 给 WEB_ROOT 赋一个初值
    rt_sprintf(WEB_ROOT, "%s", "/mnt/filesystem/webnet");

    // build_config_factory_json();

    // 首先判断 ram 中是否已有配置文件，如果有就以 RAM 中的为准，如果没有就从 flash 中拷贝
    rt_sprintf(path, "%s", "/webnet/config.json");
    fd = open(path, O_RDONLY);
    if (fd > 0)
    {
        log_i("从 RAM 中读取 config.json");
        config_from = 0;
    }
    else
    {
        log_i("从 FLASH 中读取 config.json");
        rt_sprintf(path, "%s", "/mnt/filesystem/webnet/config.json");
        fd = open(path, O_RDONLY);
        if (fd < 0)
        {
            // 说明 flash 中没有 config 文件
            log_e("在 FLASH 中没有找到配置文件");
            return -1;
        }
        config_from = 1;
        
    }

    iConfigJsonSize = read(fd, g_BUF_CONFIG_JSON, MAX_CONFIG_JSON_SIZE);
    close(fd);

    if (iConfigJsonSize > MAX_CONFIG_JSON_SIZE)
    {
        log_e("Config file is too large");
        return -1;
    }
    else if (iConfigJsonSize < 0)
    {
        log_e("Read config.json failed");
        return -1;
    }
    else
    {
        log_d("Read config.json success.(%d)", iConfigJsonSize);
        // rt_kprintf("%s\n", g_BUF_CONFIG_JSON);
    }
    g_root = cJSON_Parse(g_BUF_CONFIG_JSON);

    if (!g_root)
    {
        log_e("parse config.json error");
        return -1;
    }
    log_d("formated config.json print:%s", cJSON_Print(g_root));

    // log_d("unformated config.json print:%s", cJSON_PrintUnformatted(g_root));
    // printJSON(g_root);

    // 如果是从 flash 中读取的 config.json 那么说明，是第一次上电，需要复制到 ram
    if(config_from)
    {
        // 获取到配置文件的 json 对象后
        item = cJSON_GetObjectItem(g_root, "webnet_in_ram");
        log_d("webnet_in_ram:%d", item->valueint);
        if (item->valueint)
        {
            webnet_in_ram = RT_TRUE;
            log_w("开始复制 flash 中的 webnet 到 ram");
            system("cp /mnt/filesystem/webnet/ /webnet/");
            rt_sprintf(WEB_ROOT, "%s", "/webnet");
        }
        else
        {
            webnet_in_ram = RT_FALSE;
            rt_sprintf(WEB_ROOT, "%s", "/mnt/filesystem/webnet");
        }
    }
}

int config_load(int argc, char **argv)
{
    load_config();
    // if (!strcmp(argv[1], "ram"))
    // {
    //     load_config_ram();
    // }
    // else
    // {
    //     char path[50] = "/mnt/filesystem/webnet/config.json";

    //     load_config_flash(path);
    // }
    // printJSON(g_root);
}

MSH_CMD_EXPORT(config_load, load config from config.json);

int config_save(int argc, char **argv)
{
    char path[50];

    if (!strcmp(argv[1], "ram"))
    {
        rt_sprintf(path, "%s", "/webnet/config.json");
    }
    else
    {
        rt_sprintf(path, "%s", "/mnt/filesystem/webnet/config.json");
    }

    save_config(path, g_root);

    printJSON(g_root);
}

MSH_CMD_EXPORT(config_save, save config from config.json);
