#include "reader.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
	struct reader_state reader;
	long first_next;

	assert(argc == 2);
	assert(!setenv("EINK_BOOK_ROOT", argv[1], 1));
	reader_init(&reader);
	assert(reader.file_count == 1);
	assert(strstr(reader.files[0].name, "welcome.txt"));
	assert(!reader_open(&reader, 0));
	assert(reader.opened);
	assert(strstr(reader.page, "EINK OS"));
	assert(reader.next_offset > 0);
	first_next = reader.next_offset;
	assert(reader_next_page(&reader) == 1);
	assert(reader.page_index == 1);
	assert(reader.page_offsets[1] == first_next);
	assert(reader_previous_page(&reader) == 1);
	assert(reader.page_index == 0);
	assert(strstr(reader.page, "EINK OS"));
	reader_close_book(&reader);
	assert(!reader.opened);
	puts("reader_test: PASS");
	return 0;
}
