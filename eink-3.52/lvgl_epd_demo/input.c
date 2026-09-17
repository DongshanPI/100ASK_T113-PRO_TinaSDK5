#include "input.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_INPUT_DEVICES 8
#define LONG_PRESS_MS 1200U
#define DUPLICATE_PRESS_MS 120U

struct dashboard_input {
	int fd[MAX_INPUT_DEVICES];
	unsigned int key_previous;
	unsigned int key_next;
	unsigned int key_next_alt;
	unsigned int key_confirm;
	unsigned int key_confirm_alt;
	uint64_t confirm_down_ms;
	uint64_t last_navigation_ms;
	enum dashboard_key_action last_navigation;
	int confirm_down;
};

static uint64_t monotonic_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000U + (uint64_t)ts.tv_nsec / 1000000U;
}

struct dashboard_input *dashboard_input_open(const struct dashboard_settings *settings)
{
	struct dashboard_input *input = calloc(1, sizeof(*input));
	unsigned int i;

	if (!input)
		return NULL;
	input->key_previous = settings->key_previous;
	input->key_next = settings->key_next;
	input->key_next_alt = settings->key_next_alt;
	input->key_confirm = settings->key_confirm;
	input->key_confirm_alt = settings->key_confirm_alt;
	for (i = 0; i < MAX_INPUT_DEVICES; i++) {
		char path[32];

		snprintf(path, sizeof(path), "/dev/input/event%u", i);
		input->fd[i] = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	}
	return input;
}

static int key_matches(unsigned int code, unsigned int primary, unsigned int alternate)
{
	return code == primary || code == alternate;
}

static int navigation_is_duplicate(struct dashboard_input *input,
				   enum dashboard_key_action action, uint64_t now)
{
	if (input->last_navigation == action &&
	    now - input->last_navigation_ms < DUPLICATE_PRESS_MS)
		return 1;
	input->last_navigation = action;
	input->last_navigation_ms = now;
	return 0;
}

enum dashboard_key_action dashboard_input_poll(struct dashboard_input *input)
{
	unsigned int i;

	if (!input)
		return DASH_KEY_NONE;
	for (i = 0; i < MAX_INPUT_DEVICES; i++) {
		struct input_event event;
		ssize_t size;

		if (input->fd[i] < 0)
			continue;
		while ((size = read(input->fd[i], &event, sizeof(event))) == sizeof(event)) {
			if (event.type != EV_KEY)
				continue;
			if (event.code == input->key_previous && event.value == 1) {
				uint64_t now = monotonic_ms();

				if (!navigation_is_duplicate(input, DASH_KEY_PREVIOUS, now))
					return DASH_KEY_PREVIOUS;
				continue;
			}
			if (key_matches(event.code, input->key_next, input->key_next_alt) &&
			    event.value == 1) {
				uint64_t now = monotonic_ms();

				if (!navigation_is_duplicate(input, DASH_KEY_NEXT, now))
					return DASH_KEY_NEXT;
				continue;
			}
			if (!key_matches(event.code, input->key_confirm, input->key_confirm_alt))
				continue;
			if (event.value == 1) {
				if (!input->confirm_down) {
					input->confirm_down = 1;
					input->confirm_down_ms = monotonic_ms();
				}
			} else if (event.value == 0 && input->confirm_down) {
				uint64_t duration = monotonic_ms() - input->confirm_down_ms;
				input->confirm_down = 0;
				return duration >= LONG_PRESS_MS ? DASH_KEY_CONFIRM_LONG : DASH_KEY_CONFIRM;
			}
		}
		if (size < 0 && errno != EAGAIN && errno != EINTR) {
			close(input->fd[i]);
			input->fd[i] = -1;
		}
	}
	return DASH_KEY_NONE;
}

void dashboard_input_close(struct dashboard_input *input)
{
	unsigned int i;

	if (!input)
		return;
	for (i = 0; i < MAX_INPUT_DEVICES; i++)
		if (input->fd[i] >= 0)
			close(input->fd[i]);
	free(input);
}
