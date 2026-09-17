#include "reader.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void write_bytes(const char *path, const void *data, size_t size)
{
	FILE *file = fopen(path, "wb"); assert(file); assert(fwrite(data, 1, size, file) == size); fclose(file);
}

int main(void)
{
	char root[] = "/tmp/eink-reader-XXXXXX", nested[256], utf8[512], gb[512], state[512];
	struct reader_state reader, restored; long first;
	static const unsigned char gb_text[] = {0xd6,0xd0,0xce,0xc4,' ',0xb2,0xe2,0xca,0xd4,'\n'};
	assert(mkdtemp(root)); snprintf(nested, sizeof(nested), "%s/level1", root); assert(!mkdir(nested, 0700));
	snprintf(utf8, sizeof(utf8), "%s/long.txt", nested); snprintf(gb, sizeof(gb), "%s/gb.txt", root); snprintf(state, sizeof(state), "%s/state", root);
	{
		FILE *file = fopen(utf8, "w"); int i; assert(file); fputs("EINK OS 中文阅读\n", file); for (i = 0; i < 1000; i++) fputs("0123456789 中文分页测试 ", file); fclose(file);
	}
	write_bytes(gb, gb_text, sizeof(gb_text)); setenv("EINK_BOOK_ROOT", root, 1);
	reader_init(&reader, state); assert(reader.file_count == 2);
	assert(!reader_open(&reader, reader.files[0].name[0] == 'g' ? 0 : 1)); assert(strstr(reader.page, "中文")); reader_close_book(&reader);
	assert(!reader_open(&reader, reader.files[0].name[0] == 'l' ? 0 : 1)); first = (long)reader.next_offset; assert(first > 0); assert(reader_next_page(&reader) == 1); assert(reader.page_index == 1); assert(!reader_toggle_bookmark(&reader)); assert(reader.bookmark_set); reader_destroy(&reader);
	reader_init(&restored, state); assert(!reader_open(&restored, restored.files[0].name[0] == 'l' ? 0 : 1)); assert(restored.bookmark_set); assert(restored.page_offsets[0] > 0); assert(strstr(restored.page, "分页")); reader_destroy(&restored);
	puts("reader_test: PASS"); return 0;
}
