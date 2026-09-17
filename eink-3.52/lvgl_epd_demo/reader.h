#ifndef EINK_READER_H
#define EINK_READER_H

#include <stddef.h>

#define READER_MAX_FILES 24
#define READER_MAX_PAGES 256
#define READER_PAGE_BYTES 512

struct reader_file {
	char path[256];
	char name[96];
};

struct reader_state {
	struct reader_file files[READER_MAX_FILES];
	unsigned int file_count;
	unsigned int selection;
	unsigned int current_file;
	int opened;
	long file_size;
	long page_offsets[READER_MAX_PAGES];
	unsigned int page_index;
	unsigned int known_pages;
	long next_offset;
	char page[READER_PAGE_BYTES + 1];
};

void reader_init(struct reader_state *reader);
void reader_scan(struct reader_state *reader);
int reader_open(struct reader_state *reader, unsigned int index);
int reader_next_page(struct reader_state *reader);
int reader_previous_page(struct reader_state *reader);
void reader_close_book(struct reader_state *reader);

#endif
