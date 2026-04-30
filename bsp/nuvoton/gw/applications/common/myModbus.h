#include "rtdef.h"
#include "rtthread.h"

#include "agile_modbus.h"
#include "agile_modbus_rtu.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

struct MB_REQ {
  uint16_t start_addr;
  uint16_t wrRegQuantity;
  uint16_t data[AGILE_MODBUS_MAX_WRITE_REGISTERS];
};

struct MB_RSP {
  uint16_t addr;
  uint16_t value;
  char msg[128];
};

extern agile_modbus_rtu_t g_ctx_rtu;
extern agile_modbus_t *g_ctx;

extern uint8_t ctx_send_buf[AGILE_MODBUS_MAX_ADU_LENGTH];

#define SCAN_READ_BYTES 500
extern uint8_t ctx_read_buf[SCAN_READ_BYTES];

extern rt_err_t modbus_read_regs(agile_modbus_t *ctx, uint16_t rdHead,
                                 uint16_t rdQuantity, uint16_t *buf,
                                 uint16_t buf_len);

extern rt_err_t modbus_write_regs(agile_modbus_t *ctx, uint16_t wrHead,
                                  uint16_t wrRegQuantity, uint16_t *buf);