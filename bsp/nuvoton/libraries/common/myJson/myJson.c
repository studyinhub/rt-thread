#include "myJson.h"

#define LOG_TAG "myJson"
#define LOG_LVL LOG_LVL_DBG
// #define LOG_LVL LOG_LVL_INFO
#include <ulog.h>

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
      rt_kprintf("%s->", item->string);
      rt_kprintf("%s\n", cJSON_Print(item));
    }
  }
}