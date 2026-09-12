#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>

#include "bsp_rtc_ioctl.h"

int main(int argc, char **argv)
{
	/* 参数格式：bsp_rtc_demo [设备节点] [相对秒数]。 */
	struct bsp_rtc_event event;
	struct timeval timeout = { .tv_sec = 5 };
	uint32_t count;
	int fd;
	int ready;
	unsigned long seconds = 2;

	if (argc > 2)
		seconds = strtoul(argv[2], NULL, 0);
	/* 读端保持打开，用于接收事件；写端单独打开，用于设置 alarm。 */
	fd = open(argc > 1 ? argv[1] : "/dev/bsp_rtc0", O_RDONLY);
	if (fd < 0) {
		perror("open");
		return 1;
	}
	{
		char alarm[24];
		int length = snprintf(alarm, sizeof(alarm), "%lu", seconds);
		int write_fd = open(argc > 1 ? argv[1] : "/dev/bsp_rtc0", O_WRONLY);
		if (write_fd < 0 || write(write_fd, alarm, length) != length) {
			perror("write");
			return 1;
		}
		close(write_fd);
	}
	{
		fd_set read_fds;
		/* select() 会调用驱动 poll()，事件到达前这里会阻塞。 */
		FD_ZERO(&read_fds);
		FD_SET(fd, &read_fds);
		ready = select(fd + 1, &read_fds, NULL, NULL, &timeout);
	}
	if (ready < 0) {
		perror("select");
		return 1;
	}
	if (!ready) {
		fprintf(stderr, "select: timeout\n");
		return 1;
	}
	printf("select: ready\n");
	/* select() 返回可读后，read() 取出一个固定大小的事件结构。 */
	if (read(fd, &event, sizeof(event)) != sizeof(event)) {
		perror("read");
		return 1;
	}
	printf("read: sequence=%u rtc_seconds=%u\n", event.sequence,
		 event.rtc_seconds);
	/* 通过 ioctl 从内核复制出已处理的 alarm 总数。 */
	if (ioctl(fd, BSP_RTC_GET_COUNT, &count) < 0) {
		perror("ioctl");
		return 1;
	}
	printf("ioctl: count=%u\n", count);
	close(fd);
	return 0;
}
