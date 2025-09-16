#include "myConfig.h"
#include "auto_version.h"
#include "cJSON.h"
#include "myWebnet.h"

#define LOG_TAG "myConfig"
#define LOG_LVL LOG_LVL_DBG
#include <ulog.h>

char g_FmtTimeStr[50];

void print_time() {
  time_t timep;
  struct tm *p;
  time(&timep);
  p = localtime(&timep);
  rt_sprintf(g_FmtTimeStr, "%d-%d-%d %d:%d:%d", (1900 + p->tm_year),
             (1 + p->tm_mon), p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);
  log_d("g_FmtTimeStr:%s", g_FmtTimeStr);
}

void printJSON(cJSON *root) // 以递归的方式打印json的最内层键值对
{
  log_d("打印：\n%s\n", cJSON_Print(root));

  for (int i = 0; i < cJSON_GetArraySize(root); i++) // 遍历最外层json键值对
  {
    cJSON *item = cJSON_GetArrayItem(root, i);
    if (cJSON_Object ==
        item->type) // 如果对应键的值仍为cJSON_Object就递归调用printJson
      printJSON(item);
    else // 值不为json对象就直接打印出键和值
    {
      printf("%s->", item->string);
      printf("%s\n", cJSON_Print(item));
    }
  }
}

cJSON *g_root = NULL;

// 具体参考 webnet/config.json
//

int baud_arr[7] = {2400, 4800, 9600, 19200, 38400, 57600, 115200};
// RT_SERIAL_CONFIG_DEFAULT BAUD_RATE_115200
struct CONFIG g_stConfig = {
    .mapEnable = 1,
    .transType= 1, //0:ascii 1: CHCT 
    .rtuSys =
        {
            .scanEnable = 1,
            .scanInv = 100,
            .rtuAddr = 1,
            .scanStAddr = 0,
            .scanRegCnt = 100,
        },
    .serPorts =
        {
            {0,"uart8", 0, 5, "rtu",{BAUD_RATE_9600,DATA_BITS_8,STOP_BITS_1,PARITY_NONE,BIT_ORDER_LSB,NRZ_NORMAL,RT_SERIAL_RB_BUFSZ,0}}, // RTU-RS485 master
            {1,"uart1", 1, 5, "ascii",{BAUD_RATE_9600,DATA_BITS_7,STOP_BITS_1,PARITY_EVEN,BIT_ORDER_LSB,NRZ_NORMAL,RT_SERIAL_RB_BUFSZ,0}}, // ASCII-RS232 slave
            {2,"uart6", 1, 5, "ascii",{BAUD_RATE_9600,DATA_BITS_7,STOP_BITS_1,PARITY_EVEN,BIT_ORDER_LSB,NRZ_NORMAL,RT_SERIAL_RB_BUFSZ,0}}, // ASCII-RS485 slave
            // {2,"uart6", 1, 5, "ascii",{BAUD_RATE_9600,DATA_BITS_8,STOP_BITS_1,PARITY_NONE,BIT_ORDER_LSB,NRZ_NORMAL,RT_SERIAL_RB_BUFSZ,0}}, // ASCII-RS232 slave
        },
    // 实际上只有一个 PLC,但是在软件上分成了 3 端
    .ascSys = {
        {
            .name = "sys1",
            .enable = 1,
            .slaveAddr = 1, // 需要根据 0090 地址来决定 ASCII 端地址,默认为 1
            .regAddr = 0,
            .cnt = 100,
            .offset = 0 // ASCII  0x100 -> 0
        },
        {.name = "sys2",
         .enable = 1,
         .slaveAddr = 2,
         .regAddr = 0,
         .cnt = 20,
         .offset = 40}, // ASCII 0x100 -> 40
        {.name = "sys3",
         .enable = 1,
         .slaveAddr = 3,
         .regAddr = 0,
         .cnt = 40,
         .offset = 60}, // ASCII 0x100 -> 60
    }
};


// 将结构体转换 JSON 序列化
void build_config_factory_json() {
  int i = 0;
  cJSON *Array_baud, *Array_ports,*port_obj;

  g_root = cJSON_CreateObject();

  cJSON_AddItemToObject(g_root, "mapEnable",
                        cJSON_CreateNumber(g_stConfig.mapEnable));

  cJSON *rtu_obj = cJSON_CreateObject();
  cJSON_AddItemToObject(rtu_obj, "scanEnable",
                        cJSON_CreateNumber(g_stConfig.rtuSys.scanEnable));
  cJSON_AddItemToObject(rtu_obj, "scanInv",
                        cJSON_CreateNumber(g_stConfig.rtuSys.scanInv));
  cJSON_AddItemToObject(rtu_obj, "rtuAddr",
                        cJSON_CreateNumber(g_stConfig.rtuSys.rtuAddr));
  cJSON_AddItemToObject(rtu_obj, "scanStAddr",
                        cJSON_CreateNumber(g_stConfig.rtuSys.scanStAddr));
  cJSON_AddItemToObject(rtu_obj, "scanRegCnt",
                        cJSON_CreateNumber(g_stConfig.rtuSys.scanRegCnt));
  cJSON_AddItemToObject(g_root, "rtuSys", rtu_obj);

  Array_ports = cJSON_CreateArray();
  for(i=0;i<3;i++)
  {
    port_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(port_obj, "device_id", g_stConfig.serPorts[i].device_id);// uart8
    cJSON_AddStringToObject(port_obj, "name", g_stConfig.serPorts[i].dev_name);// uart8
    cJSON_AddNumberToObject(port_obj, "baudrate", g_stConfig.serPorts[i].config.baud_rate);//9600 
    cJSON_AddNumberToObject(port_obj, "databits", g_stConfig.serPorts[i].config.data_bits);//8
    cJSON_AddNumberToObject(port_obj, "parity", g_stConfig.serPorts[i].config.parity); // 0: None 1:ODD 2:EVEN
    cJSON_AddNumberToObject(port_obj, "stopbits", g_stConfig.serPorts[i].config.stop_bits);//1 bug:
    cJSON_AddNumberToObject(port_obj, "bufsz", g_stConfig.serPorts[i].config.bufsz); // 128==>1024
    cJSON_AddItemToArray(Array_ports, port_obj);
  }
  cJSON_AddItemToObject(g_root, "serPorts", Array_ports);

  cJSON_AddItemToObject(g_root, "lastConfigAt", cJSON_CreateString(BUILDTIME));


  // cJSON *asc_obj = cJSON_CreateObject();
  // cJSON_AddItemToObject(asc_obj, "scanEnable",
  // cJSON_CreateNumber(g_stConfig.rtuSys.scanEnable));
  // cJSON_AddItemToObject(asc_obj, "rtuAddr",
  // cJSON_CreateNumber(g_stConfig.rtuSys.rtuAddr));
  // cJSON_AddItemToObject(asc_obj, "scanStAddr",
  // cJSON_CreateNumber(g_stConfig.rtuSys.scanStAddr));
  // cJSON_AddItemToObject(asc_obj, "scanRegCnt",
  // cJSON_CreateNumber(g_stConfig.rtuSys.scanRegCnt));
  // cJSON_AddItemToObject(g_root, "ascSys", asc_obj);

  Array_baud = cJSON_CreateArray();

  // 构建 波特率数组
  for (i = 0; i < 7; i++) {
    cJSON_AddItemToArray(Array_baud, cJSON_CreateNumber(baud_arr[i]));
  }

  cJSON_AddItemToObject(g_root, "baudrates", Array_baud);

  // 构建 scan

  // cJSON *scan_obj = cJSON_CreateObject();
  // cJSON_AddBoolToObject(scan_obj, "test", 1);                           //
  // RT_TRUE 是 0 ？ cJSON_AddItemToObject(scan_obj, "test1",
  // cJSON_CreateBool(1));        // RT_TRUE 是 0 ？
  // cJSON_AddItemToObject(scan_obj, "scanEnable", cJSON_CreateNumber(1)); //
  // RT_TRUE 是 0 ？ cJSON_AddItemToObject(scan_obj, "scanInv",
  // cJSON_CreateNumber(500));  // 扫描间隔 cJSON_AddItemToObject(scan_obj,
  // "scanStAddr", cJSON_CreateNumber(0)); cJSON_AddItemToObject(scan_obj,
  // "scanRegCnt", cJSON_CreateNumber(100)); cJSON_AddItemToObject(g_root,
  // "scan", scan_obj);

  // cJSON *Array_querys = cJSON_CreateArray();
  // cJSON *Query_obj = cJSON_CreateObject();
  // cJSON_AddNumberToObject(Query_obj, "id", 1);
  // cJSON_AddNumberToObject(Query_obj, "cmd", 3);
  // cJSON_AddNumberToObject(Query_obj, "start", 0);
  // cJSON_AddNumberToObject(Query_obj, "cnt", 10);
  // cJSON_AddItemToArray(Array_querys, Query_obj);

  // Query_obj = cJSON_CreateObject();
  // cJSON_AddNumberToObject(Query_obj, "id", 2);
  // cJSON_AddNumberToObject(Query_obj, "cmd", 3);
  // cJSON_AddNumberToObject(Query_obj, "start", 0);
  // cJSON_AddNumberToObject(Query_obj, "cnt", 10);
  // cJSON_AddItemToArray(Array_querys, Query_obj);

  // cJSON_AddBoolToObject(AutoQuery_obj,"enable",RT_TRUE);
  // cJSON_AddItemToObject(AutoQuery_obj, "items", Array_querys);
  // cJSON_AddItemToObject(g_root, "autoquery", AutoQuery_obj);

  //RS485A

  // LOG_D("构建的JSON:\n%s\n", cJSON_Print(g_root));
}

int load_config() {
  // V1 判断 ram 中是否有 config.json,如果有，就将 webnet 设置在 ram，否则设置在
  // flash V2 不再通过判断 config.json 是否存在，通过 flash 中的 webnet_in_ram
  // 来判断，是否将 flash 中的 webnet 复制到 ram

  char path[50];
  int iConfigJsonSize = 0;
  int fd = 0;

  int config_from = 0; // 1:flash 0:ram
  cJSON *item = NULL;

  // 给 WEB_ROOT 赋一个初值
  // /mnt/filesystem/webnet

  // 首先判断从存储（RAM/FLASH）中判断是否有配置文件
  // 如果有就以存储中的文件配置做为配置
  // 否则就用自动构建的配置，并保存到存储中

#ifdef WEBNET_INRAM
  rt_sprintf(WEB_ROOT, "%s", "/webnet");
#else
  rt_sprintf(WEB_ROOT, "%s", "/mnt/filesystem/webnet");
#endif

  rt_sprintf(path, "%s/config.json", WEB_ROOT);
  log_d("read config.json from %s", path);
  fd = open(path, O_RDONLY);
  if (fd <= 0 || 1) {
    log_w("在 FLASH 中没有找到配置文件,将采用默认配置文件");
    build_config_factory_json();
    // 保存配置文件，下次重启的时候将从文件中加载
    // save_config(path, g_root);
    config_from = 1;
  } else {
    // 如果存储中已经有了 config.json,那么就读取
    iConfigJsonSize = read(fd, g_BUF_CONFIG_JSON, MAX_CONFIG_JSON_SIZE);
    close(fd);
    // 如果配置文件有问题
    if (iConfigJsonSize > MAX_CONFIG_JSON_SIZE || iConfigJsonSize < 0) {
      log_e("Config file is too large %d > %d", iConfigJsonSize,
            MAX_CONFIG_JSON_SIZE);
      build_config_factory_json();
    }
    log_i("Read config.json success.(%d)", iConfigJsonSize);
    g_root = cJSON_Parse(g_BUF_CONFIG_JSON);
  }

  if (!g_root) {
    log_e("parse config.json error");
    build_config_factory_json();
  }
  log_i("formated config.json print:%s", cJSON_Print(g_root));

  // 首先判断 ram 中是否已有配置文件，如果有就以 RAM 中的为准，如果没有就从
  // flash 中拷贝 rt_sprintf(path, "%s", "/webnet/config.json"); fd = open(path,
  // O_RDONLY); if (fd > 0)
  // {
  //     log_i("从 RAM 中读取 config.json");
  //     config_from = 0;
  // }
  // else
  // {
  //     log_i("从 FLASH 中读取 config.json");
  //     rt_sprintf(path, "%s", "/mnt/filesystem/webnet/config.json");
  //     fd = open(path, O_RDONLY);
  //     if (fd < 0)
  //     {
  //         // 说明 flash 中没有 config 文件
  //         log_e("在 FLASH 中没有找到配置文件");
  //         return -1;
  //     }
  //     config_from = 1;
  // }

  // log_d("unformated config.json print:%s", cJSON_PrintUnformatted(g_root));
  // printJSON(g_root);

  // 如果是从 flash 中读取的 config.json 那么说明，是第一次上电，需要复制到 ram
  // if(config_from)
  // {
  //     // 获取到配置文件的 json 对象后
  //     item = cJSON_GetObjectItem(g_root, "webnet_in_ram");
  //     log_d("webnet_in_ram:%d", item->valueint);
  //     if (item->valueint)
  //     {
  //         webnet_in_ram = RT_TRUE;
  //         log_w("开始复制 flash 中的 webnet 到 ram");
  //         system("cp /mnt/filesystem/webnet/ /webnet/");
  //         rt_sprintf(WEB_ROOT, "%s", "/webnet");
  //     }
  //     else
  //     {
  //         webnet_in_ram = RT_FALSE;
  //         rt_sprintf(WEB_ROOT, "%s", "/mnt/filesystem/webnet");
  //     }
  // }
}

int save_config(char *path, cJSON *root) {

  // printJSON(root);
  char jsonString[1024];
  sprintf(jsonString, "%s", cJSON_Print(root));
  // log_d("准备保存 %s 到:%s", jsonString, path);

  DIR *pDir = opendir(WEB_ROOT);
  if (pDir == RT_NULL) {
    log_e("open dir WEB_ROOT %s faild", WEB_ROOT);
  } else {
    log_i("open dir WEB_ROOT %s success", WEB_ROOT);
  }

  int fd = open(path, O_WRONLY | O_CREAT);
  int iConfigJsonSize = 0;
  if (fd < 0) {
    log_e("Open %s failed", path);
    return -1;
  }
  iConfigJsonSize = write(fd, jsonString, rt_strlen(jsonString));
  close(fd);
  if (iConfigJsonSize > MAX_CONFIG_JSON_SIZE) {
    log_e("Config file is too large");
    return -1;
  } else if (iConfigJsonSize < 0) {
    log_e("Write config.json failed");
    return -1;
  } else {
    log_d("Write config.json success.(%d)", iConfigJsonSize);
    // rt_kprintf("%s\n", g_BUF_CONFIG_JSON);
  }
  closedir(pDir);
}

struct tftp_server *tftp_server;

int switch_root(char *path) {

  webnet_set_root(path);
  // 删除 tftp_thead

  rt_thread_t tftp_tid;
  tftp_tid = rt_thread_find("tftps");
  if (tftp_tid && tftp_tid == g_tftp_tid) {
    tftp_server_destroy(tftp_server);

    do {
      if (tftp_server != NULL) {
        LOG_D("tftp_server->is_stop:%d", tftp_server->is_stop);
      }
      rt_thread_delay(1000);
    } while (tftp_server);

    if (rt_thread_find("tftps")) {
      do {
        if (rt_thread_delete(g_tftp_tid) == RT_EOK) {
          log_i("tftps 删除成功");
        }
        rt_thread_delay(1000);
      } while (rt_thread_find("tftps"));
    } else {
      log_i("不需要删除 tftps,已经退出了");
    }
    init_tftps(path);
  } else {
    log_e("没有找到 tftps 线程"); // rt_thread_find 没有找到会返回 0
  }
}

int config_load(int argc, char **argv) {
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

int config_save(int argc, char **argv) {
  char path[50];

  if (!strcmp(argv[1], "ram")) {
    rt_sprintf(path, "%s", "/webnet/config.json");
  } else {
    rt_sprintf(path, "%s", "/mnt/filesystem/webnet/config.json");
  }

  save_config(path, g_root);

  printJSON(g_root);
}

MSH_CMD_EXPORT(config_save, save config from config.json);
