#ifndef EINK_READER_H
#define EINK_READER_H

#include <stddef.h>
#include <time.h>

#define READER_MAX_FILES 128
#define READER_PAGE_BYTES 2048
#define READER_LINE_UNITS 26
#define READER_PAGE_LINES 13

struct reader_file {
	char path[256];
	char name[96];
	long size;
	time_t mtime;
};

struct reader_state {
	struct reader_file files[READER_MAX_FILES];
	unsigned int file_count;
	unsigned int selection;
	unsigned int current_file;
	int opened;
	int action_menu;
	unsigned int action_selection;
	char *content;
	size_t content_size;
	size_t *page_offsets;
	unsigned int page_capacity;
	unsigned int page_index;
	unsigned int known_pages;
	size_t next_offset;
	size_t bookmark_offset;
	int bookmark_set;
	char page[READER_PAGE_BYTES + 1];
	char state_path[256];
};

void reader_init(struct reader_state *reader, const char *state_path);
void reader_scan(struct reader_state *reader);
int reader_open(struct reader_state *reader, unsigned int index);
int reader_next_page(struct reader_state *reader);
int reader_previous_page(struct reader_state *reader);
int reader_toggle_bookmark(struct reader_state *reader);
int reader_jump_bookmark(struct reader_state *reader);
int reader_save(struct reader_state *reader);
void reader_close_book(struct reader_state *reader);
void reader_destroy(struct reader_state *reader);

#endif
