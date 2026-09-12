#ifndef BSP_RTC_IOCTL_H
#define BSP_RTC_IOCTL_H

#include <linux/ioctl.h>
#include <stdint.h>

/* 用户态和内核态必须使用同一组 ioctl 命令号和数据类型。 */
#define BSP_RTC_IOC_MAGIC 'r'
#define BSP_RTC_GET_COUNT _IOR(BSP_RTC_IOC_MAGIC, 0, uint32_t)
#define BSP_RTC_SET_ALARM _IOW(BSP_RTC_IOC_MAGIC, 1, uint32_t)

struct bsp_rtc_event {
	uint32_t sequence;    /* 驱动产生的递增事件序号。 */
	uint32_t rtc_seconds; /* 驱动读取到的 RTC 秒计数。 */
};

#endif
