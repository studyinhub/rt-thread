#ifndef RT_CONFIG_H__
#define RT_CONFIG_H__

/* Automatically generated file; DO NOT EDIT. */
/* RT-Thread Configuration */

/* RT-Thread Kernel */

#define RT_NAME_MAX 16
#define RT_ALIGN_SIZE 4
#define RT_THREAD_PRIORITY_32
#define RT_THREAD_PRIORITY_MAX 32
#define RT_TICK_PER_SECOND 1000
#define RT_USING_OVERFLOW_CHECK
#define RT_USING_HOOK
#define RT_USING_IDLE_HOOK
#define RT_IDLE_HOOK_LIST_SIZE 4
#define IDLE_THREAD_STACK_SIZE 2048

/* kservice optimization */

#define RT_DEBUG
#define RT_DEBUG_COLOR

/* Inter-Thread communication */

#define RT_USING_SEMAPHORE
#define RT_USING_MUTEX
#define RT_USING_EVENT
#define RT_USING_MAILBOX
#define RT_USING_MESSAGEQUEUE
#define RT_USING_SIGNALS

/* Memory Management */

#define RT_USING_MEMPOOL
#define RT_USING_MEMHEAP
#define RT_USING_SMALL_MEM
#define RT_USING_MEMTRACE
#define RT_USING_HEAP

/* Kernel Device Object */

#define RT_USING_DEVICE
#define RT_USING_CONSOLE
#define RT_CONSOLEBUF_SIZE 256
#define RT_CONSOLE_DEVICE_NAME "uart0"
#define RT_VER_NUM 0x40004
#define ARCH_ARM
#define ARCH_ARM_ARM9

/* RT-Thread Components */

#define RT_USING_COMPONENTS_INIT
#define RT_USING_USER_MAIN
#define RT_MAIN_THREAD_STACK_SIZE 2048
#define RT_MAIN_THREAD_PRIORITY 10

/* C++ features */


/* Command shell */

#define RT_USING_FINSH
#define FINSH_THREAD_NAME "tshell"
#define FINSH_USING_HISTORY
#define FINSH_HISTORY_LINES 5
#define FINSH_USING_SYMTAB
#define FINSH_USING_DESCRIPTION
#define FINSH_THREAD_PRIORITY 20
#define FINSH_THREAD_STACK_SIZE 4096
#define FINSH_CMD_SIZE 80
#define FINSH_USING_MSH
#define FINSH_USING_MSH_DEFAULT
#define FINSH_ARG_MAX 10

/* Device virtual file system */

#define RT_USING_DFS
#define DFS_USING_WORKDIR
#define DFS_FILESYSTEMS_MAX 16
#define DFS_FILESYSTEM_TYPES_MAX 16
#define DFS_FD_MAX 64
#define RT_USING_DFS_MNTTABLE
#define RT_USING_DFS_ELMFAT

/* elm-chan's FatFs, Generic FAT Filesystem Module */

#define RT_DFS_ELM_CODE_PAGE 437
#define RT_DFS_ELM_WORD_ACCESS
#define RT_DFS_ELM_USE_LFN_3
#define RT_DFS_ELM_USE_LFN 3
#define RT_DFS_ELM_LFN_UNICODE_0
#define RT_DFS_ELM_LFN_UNICODE 0
#define RT_DFS_ELM_MAX_LFN 255
#define RT_DFS_ELM_DRIVES 8
#define RT_DFS_ELM_MAX_SECTOR_SIZE 4096
#define RT_DFS_ELM_REENTRANT
#define RT_USING_DFS_DEVFS

/* Device Drivers */

#define RT_USING_DEVICE_IPC
#define RT_PIPE_BUFSZ 512
#define RT_USING_SYSTEM_WORKQUEUE
#define RT_SYSTEM_WORKQUEUE_STACKSIZE 2048
#define RT_SYSTEM_WORKQUEUE_PRIORITY 23
#define RT_USING_SERIAL
#define RT_SERIAL_USING_DMA
#define RT_SERIAL_RB_BUFSZ 2048
#define RT_USING_CAN
#define RT_CAN_USING_HDR
#define RT_USING_HWTIMER
#define RT_USING_CPUTIME
#define RT_USING_I2C
#define RT_USING_I2C_BITOPS
#define RT_USING_PIN
#define RT_USING_ADC
#define RT_USING_PWM
#define RT_USING_MTD_NAND
#define RT_MTD_NAND_DEBUG
#define RT_USING_RTC
#define RT_USING_ALARM
#define RT_USING_SPI
#define RT_USING_QSPI
#define RT_USING_WDT
#define RT_USING_HWCRYPTO
#define RT_HWCRYPTO_DEFAULT_NAME "hwcryto"
#define RT_HWCRYPTO_IV_MAX_SIZE 16
#define RT_HWCRYPTO_KEYBIT_MAX_SIZE 256
#define RT_HWCRYPTO_USING_AES
#define RT_HWCRYPTO_USING_AES_ECB
#define RT_HWCRYPTO_USING_AES_CBC
#define RT_HWCRYPTO_USING_AES_CFB
#define RT_HWCRYPTO_USING_AES_CTR
#define RT_HWCRYPTO_USING_AES_OFB
#define RT_HWCRYPTO_USING_SHA1
#define RT_HWCRYPTO_USING_SHA2
#define RT_HWCRYPTO_USING_SHA2_224
#define RT_HWCRYPTO_USING_SHA2_256
#define RT_HWCRYPTO_USING_SHA2_384
#define RT_HWCRYPTO_USING_SHA2_512
#define RT_HWCRYPTO_USING_RNG

/* Using USB */

#define RT_USING_USB_HOST
#define RT_USBH_MSTORAGE
#define UDISK_MOUNTPOINT "/mnt/udisk"
#define RT_USING_USB_DEVICE
#define RT_USBD_THREAD_STACK_SZ 4096
#define USB_VENDOR_ID 0x0FFE
#define USB_PRODUCT_ID 0x0001
#define RT_USB_DEVICE_COMPOSITE
#define RT_USB_DEVICE_CDC
#define RT_USB_DEVICE_NONE
#define RT_USB_DEVICE_MSTORAGE
#define RT_VCOM_TASK_STK_SIZE 2048
#define RT_CDC_RX_BUFSIZE 128
#define RT_VCOM_SERNO "32021919830108"
#define RT_VCOM_SER_LEN 14
#define RT_VCOM_TX_TIMEOUT 1000
#define RT_USB_MSTORAGE_DISK_NAME "ramdisk1"

/* POSIX layer and C standard library */

#define RT_USING_LIBC
#define RT_USING_POSIX
#define RT_LIBC_FIXED_TIMEZONE 8

/* Network */

/* Socket abstraction layer */

#define RT_USING_SAL
#define SAL_INTERNET_CHECK

/* protocol stack implement */

#define SAL_USING_LWIP
#define SAL_USING_POSIX

/* Network interface device */

#define RT_USING_NETDEV
#define NETDEV_USING_IFCONFIG
#define NETDEV_USING_PING
#define NETDEV_USING_NETSTAT
#define NETDEV_USING_AUTO_DEFAULT
#define NETDEV_IPV4 1
#define NETDEV_IPV6 0

/* light weight TCP/IP stack */

#define RT_USING_LWIP
#define RT_USING_LWIP202
#define RT_LWIP_MEM_ALIGNMENT 4
#define RT_LWIP_IGMP
#define RT_LWIP_ICMP
#define RT_LWIP_DNS
#define RT_LWIP_DHCP
#define IP_SOF_BROADCAST 1
#define IP_SOF_BROADCAST_RECV 1

/* Static IPv4 Address */

#define RT_LWIP_IPADDR "192.168.1.30"
#define RT_LWIP_GWADDR "192.168.1.1"
#define RT_LWIP_MSKADDR "255.255.255.0"
#define RT_LWIP_UDP
#define RT_LWIP_TCP
#define RT_LWIP_RAW
#define RT_MEMP_NUM_NETCONN 32
#define RT_LWIP_PBUF_NUM 256
#define RT_LWIP_RAW_PCB_NUM 32
#define RT_LWIP_UDP_PCB_NUM 32
#define RT_LWIP_TCP_PCB_NUM 32
#define RT_LWIP_TCP_SEG_NUM 256
#define RT_LWIP_TCP_SND_BUF 32768
#define RT_LWIP_TCP_WND 10240
#define RT_LWIP_TCPTHREAD_PRIORITY 10
#define RT_LWIP_TCPTHREAD_MBOX_SIZE 32
#define RT_LWIP_TCPTHREAD_STACKSIZE 4096
#define RT_LWIP_ETHTHREAD_PRIORITY 12
#define RT_LWIP_ETHTHREAD_STACKSIZE 1024
#define RT_LWIP_ETHTHREAD_MBOX_SIZE 32
#define LWIP_NETIF_STATUS_CALLBACK 1
#define LWIP_NETIF_LINK_CALLBACK 1
#define SO_REUSE 1
#define LWIP_SO_RCVTIMEO 1
#define LWIP_SO_SNDTIMEO 1
#define LWIP_SO_RCVBUF 1
#define LWIP_SO_LINGER 0
#define RT_LWIP_NETIF_LOOPBACK
#define LWIP_NETIF_LOOPBACK 1
#define RT_LWIP_STATS
#define RT_LWIP_USING_PING

/* AT commands */


/* VBUS(Virtual Software BUS) */


/* Utilities */

#define RT_USING_RYM
#define RT_USING_UTEST
#define UTEST_THR_STACK_SIZE 4096
#define UTEST_THR_PRIORITY 20

/* RT-Thread Utestcases */


/* RT-Thread online packages */

/* IoT - internet of things */

#define PKG_USING_WEBCLIENT
#define WEBCLIENT_USING_SAMPLES
#define WEBCLIENT_USING_FILE_DOWMLOAD
#define WEBCLIENT_NOT_USE_TLS
#define PKG_USING_WEBCLIENT_V212
#define PKG_WEBCLIENT_VER_NUM 0x20102
#define PKG_USING_WEBNET
#define WEBNET_PORT 80
#define WEBNET_CONN_MAX 16
#define WEBNET_ROOT "/mnt/filesystem/webnet"

/* Select supported modules */

#define WEBNET_USING_LOG
#define WEBNET_USING_AUTH
#define WEBNET_USING_CGI
#define WEBNET_USING_ASP
#define WEBNET_USING_SSI
#define WEBNET_USING_INDEX
#define WEBNET_USING_ALIAS
#define WEBNET_USING_DAV
#define WEBNET_USING_UPLOAD
#define WEBNET_USING_GZIP
#define WEBNET_CACHE_LEVEL 0
#define WEBNET_USING_SAMPLES
#define PKG_USING_WEBNET_LATEST_VERSION
#define PKG_USING_FREEMODBUS
#define PKG_MODBUS_MASTER

/* advanced configuration */

#define RT_M_DISCRETE_INPUT_START 0
#define RT_M_DISCRETE_INPUT_NDISCRETES 16
#define RT_M_COIL_START 0
#define RT_M_COIL_NCOILS 64
#define RT_M_REG_INPUT_START 0
#define RT_M_REG_INPUT_NREGS 100
#define RT_M_REG_HOLDING_START 0
#define RT_M_REG_HOLDING_NREGS 100
#define RT_M_HD_RESERVE 0
#define RT_M_IN_RESERVE 0
#define RT_M_CO_RESERVE 0
#define RT_M_DI_RESERVE 0
#define PKG_MODBUS_MASTER_RTU
#define PKG_MODBUS_MASTER_SAMPLE
#define MB_SAMPLE_TEST_SLAVE_ADDR 1
#define MB_MASTER_USING_PORT_NUM 8
#define MB_MASTER_USING_PORT_BAUDRATE 9600
#define PKG_USING_FREEMODBUS_LATEST_VERSION

/* Wi-Fi */

/* Marvell WiFi */


/* Wiced WiFi */

#define PKG_USING_NETUTILS
#define PKG_NETUTILS_TFTP
#define PKG_NETUTILS_IPERF
#define PKG_NETUTILS_NTP
#define NTP_USING_AUTO_SYNC
#define NTP_AUTO_SYNC_THREAD_STACK_SIZE 1500
#define NTP_AUTO_SYNC_FIRST_DELAY 30
#define NTP_AUTO_SYNC_PERIOD 3600
#define NETUTILS_NTP_HOSTNAME "0.tw.pool.ntp.org"
#define NETUTILS_NTP_HOSTNAME2 "1.tw.pool.ntp.org"
#define NETUTILS_NTP_HOSTNAME3 "2.tw.pool.ntp.org"
#define PKG_USING_NETUTILS_V130
#define PKG_NETUTILS_VER_NUM 0x10300

/* IoT Cloud */


/* security packages */


/* language packages */


/* multimedia packages */


/* tools packages */

#define PKG_USING_MEM_SANDBOX
#define PKG_USING_MEM_SANDBOX_LATEST_VERSION

/* system packages */

#define PKG_USING_DFS_UFFS
#define RT_USING_DFS_UFFS
#define RT_UFFS_ECC_MODE_3
#define RT_UFFS_ECC_MODE 3
#define PKG_USING_DFS_UFFS_LATEST_VERSION
#define PKG_USING_RAMDISK
#define PKG_USING_RAMDISK_LATEST_VERSION

/* Micrium: Micrium software products porting for RT-Thread */


/* peripheral libraries and drivers */


/* AI packages */


/* miscellaneous packages */

#define PKG_USING_OPTPARSE
#define PKG_USING_OPTPARSE_LATEST_VERSION

/* samples: kernel and components samples */


/* entertainment: terminal games and other interesting software packages */


/* Nuvoton Packages Config */

#define NU_PKG_USING_UTILS
#define NU_PKG_USING_DEMO
#define NU_PKG_USING_SPINAND

/* Hardware Drivers Config */

/* On-chip Peripheral Drivers */

#define SOC_SERIES_NUC980
#define BSP_USING_MMU
#define BSP_USING_PDMA
#define NU_PDMA_MEMFUN_ACTOR_MAX 2
#define BSP_USING_GPIO
#define BSP_USING_EMAC
#define BSP_USING_EMAC0
#define BSP_USING_RTC
#define NU_RTC_SUPPORT_IO_RW
#define NU_RTC_SUPPORT_MSH_CMD
#define BSP_USING_TMR
#define BSP_USING_TIMER
#define BSP_USING_TMR0
#define BSP_USING_TIMER0
#define BSP_USING_TMR1
#define BSP_USING_TIMER1
#define BSP_USING_TMR2
#define BSP_USING_TIMER2
#define BSP_USING_TMR3
#define BSP_USING_TIMER3
#define BSP_USING_TMR4
#define BSP_USING_TIMER4
#define BSP_USING_UART
#define BSP_USING_UART0
#define BSP_USING_UART1
#define BSP_USING_UART1_TX_DMA
#define BSP_USING_UART1_RX_DMA
#define BSP_USING_UART6
#define BSP_USING_UART8
#define BSP_USING_I2C
#define BSP_USING_I2C0
#define BSP_USING_I2C2
#define BSP_USING_SDH
#define BSP_USING_SDH1
#define NU_SDH_USING_PDMA
#define NU_SDH_HOTPLUG
#define BSP_USING_SPI
#define BSP_USING_SPI_PDMA
#define BSP_USING_SPI0
#define BSP_USING_SPI0_PDMA
#define BSP_USING_SPI1_NONE
#define BSP_USING_QSPI
#define BSP_USING_QSPI_PDMA
#define BSP_USING_QSPI0
#define BSP_USING_QSPI0_PDMA
#define BSP_USING_CRYPTO
#define BSP_USING_WDT

/* On-board Peripheral Drivers */

#define BSP_USING_CONSOLE
#define BOARD_USING_UART6_RS485
#define BOARD_USING_UART8_RS485
#define BOARD_USING_IP101GR
#define BOARD_USING_STORAGE_SPINAND

/* Board extended module drivers */


#endif
