#include <errno.h>
#include <fcntl.h>
#include "epd_100ask.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define STRIDE (EPD_100ASK_WIDTH / 8)

static void set_pixel(unsigned char *frame, unsigned int x, unsigned int y, int white)
{
	unsigned char mask = 0x80U >> (x & 7);
	unsigned int offset = y * STRIDE + x / 8;

	if (white)
		frame[offset] |= mask;
	else
		frame[offset] &= (unsigned char)~mask;
}

static int make_pattern(unsigned char *frame, const char *name)
{
	unsigned int x, y;

	memset(frame, 0xff, EPD_100ASK_FRAME_SIZE);
	if (!strcmp(name, "white"))
		return 0;
	if (!strcmp(name, "black")) {
		memset(frame, 0, EPD_100ASK_FRAME_SIZE);
		return 0;
	}
	for (y = 0; y < EPD_100ASK_HEIGHT; y++) {
		for (x = 0; x < EPD_100ASK_WIDTH; x++) {
			int white;

			if (!strcmp(name, "vsplit"))
				white = x >= EPD_100ASK_WIDTH / 2;
			else if (!strcmp(name, "hsplit"))
				white = y >= EPD_100ASK_HEIGHT / 2;
			else if (!strcmp(name, "quadrants"))
				white = ((x >= EPD_100ASK_WIDTH / 2) == (y >= EPD_100ASK_HEIGHT / 2));
			else if (!strcmp(name, "checker"))
				white = ((x / 16) + (y / 16)) & 1;
			else if (!strcmp(name, "bars"))
				white = (x / 30) & 1;
			else if (!strcmp(name, "border"))
				white = !(x < 4 || y < 4 || x >= EPD_100ASK_WIDTH - 4 ||
					  y >= EPD_100ASK_HEIGHT - 4 || x == EPD_100ASK_WIDTH / 2 ||
					  y == EPD_100ASK_HEIGHT / 2);
			else
				return -1;
			set_pixel(frame, x, y, white);
		}
	}
	return 0;
}

static void print_info(int fd)
{
	struct epd_100ask_info info;

	if (ioctl(fd, EPD_IOC_GET_INFO, &info)) {
		perror("EPD_IOC_GET_INFO");
		return;
	}
	printf("%ux%u frame=%u mode=%u caps=0x%x full=%u partial=%u skipped=%u last_error=%d\n",
	       info.width, info.height, info.frame_size, info.refresh_mode,
	       info.capabilities, info.full_refreshes, info.partial_refreshes,
	       info.skipped_refreshes, info.last_error);
}

static int show_pattern(int fd, const char *pattern, int partial)
{
	unsigned char *frame = malloc(EPD_100ASK_FRAME_SIZE);
	ssize_t result;

	if (!frame)
		return -1;
	if (make_pattern(frame, pattern)) {
		fprintf(stderr, "Unknown pattern: %s\n", pattern);
		free(frame);
		return -1;
	}
	if (ioctl(fd, partial ? EPD_IOC_SET_LUT_DU : EPD_IOC_SET_LUT_GC)) {
		perror("set refresh mode");
		free(frame);
		return -1;
	}
	result = write(fd, frame, EPD_100ASK_FRAME_SIZE);
	free(frame);
	if (result != EPD_100ASK_FRAME_SIZE) {
		if (result < 0)
			perror("write frame");
		else
			fprintf(stderr, "short write: %ld\n", (long)result);
		return -1;
	}
	printf("displayed %s using %s refresh\n", pattern, partial ? "DU" : "GC");
	print_info(fd);
	return 0;
}

int main(int argc, char **argv)
{
	static const char *sequence[] = {
		"white", "black", "vsplit", "hsplit", "quadrants", "checker", "bars", "border"
	};
	int fd, partial = 0;
	unsigned int i;

	fd = open("/dev/epd_100ask", O_WRONLY | O_CLOEXEC);
	if (fd < 0) {
		perror("open /dev/epd_100ask");
		return 1;
	}
	if (argc == 2 && !strcmp(argv[1], "info")) {
		print_info(fd);
		close(fd);
		return 0;
	}
	if (argc > 1 && !strcmp(argv[1], "--partial")) {
		partial = 1;
		argv++;
		argc--;
	}
	if (argc != 2) {
		fprintf(stderr, "usage: %s [--partial] white|black|vsplit|hsplit|quadrants|checker|bars|border|sequence|info\n", argv[0]);
		close(fd);
		return 2;
	}
	if (!strcmp(argv[1], "sequence")) {
		for (i = 0; i < sizeof(sequence) / sizeof(sequence[0]); i++) {
			if (show_pattern(fd, sequence[i], partial)) {
				close(fd);
				return 1;
			}
			sleep(2);
		}
	} else if (show_pattern(fd, argv[1], partial)) {
		close(fd);
		return 1;
	}
	close(fd);
	return 0;
}
