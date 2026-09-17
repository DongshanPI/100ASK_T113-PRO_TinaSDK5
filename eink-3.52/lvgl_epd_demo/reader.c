#include "reader.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static const char *book_roots[] = {
	"/mnt/UDISK/books",
	"/mnt/SDCARD/books",
	"/mnt/exUDISK/books",
	"/mnt/UDISK",
	"/mnt/SDCARD",
	"/root/books",
	"/usr/share/eink-os/books",
};

static int has_txt_suffix(const char *name)
{
	size_t length = strlen(name);

	return length > 4 && !strcasecmp(name + length - 4, ".txt");
}

static int compare_files(const void *left, const void *right)
{
	const struct reader_file *a = left;
	const struct reader_file *b = right;

	return strcasecmp(a->name, b->name);
}

static void scan_directory(struct reader_state *reader, const char *root)
{
	struct dirent *entry;
	DIR *directory = opendir(root);

	if (!directory)
		return;
	while (reader->file_count < READER_MAX_FILES &&
	       (entry = readdir(directory)) != NULL) {
		struct reader_file *file;

		if (entry->d_name[0] == '.' || !has_txt_suffix(entry->d_name))
			continue;
		file = &reader->files[reader->file_count];
		if (snprintf(file->path, sizeof(file->path), "%s/%s", root,
			     entry->d_name) >= (int)sizeof(file->path))
			continue;
		snprintf(file->name, sizeof(file->name), "%.95s", entry->d_name);
		reader->file_count++;
	}
	closedir(directory);
}

static unsigned int utf8_character_size(unsigned char first)
{
	if (first < 0x80)
		return 1;
	if ((first & 0xe0) == 0xc0)
		return 2;
	if ((first & 0xf0) == 0xe0)
		return 3;
	if ((first & 0xf8) == 0xf0)
		return 4;
	return 1;
}

static int load_page(struct reader_state *reader, long offset)
{
	unsigned char raw[READER_PAGE_BYTES];
	size_t got, valid = 0, out = 0;
	FILE *file;

	if (reader->current_file >= reader->file_count)
		return -EINVAL;
	file = fopen(reader->files[reader->current_file].path, "rb");
	if (!file)
		return -errno;
	if (fseek(file, offset, SEEK_SET)) {
		int error = errno;
		fclose(file);
		return -error;
	}
	got = fread(raw, 1, sizeof(raw), file);
	fclose(file);

	while (valid < got) {
		unsigned int length = utf8_character_size(raw[valid]);
		unsigned int i;
		int complete = valid + length <= got;

		for (i = 1; complete && i < length; i++)
			if ((raw[valid + i] & 0xc0) != 0x80)
				complete = 0;
		if (!complete)
			break;
		valid += length;
	}
	for (got = 0; got < valid && out < READER_PAGE_BYTES; got++) {
		unsigned char value = raw[got];

		if (offset == 0 && got < 3 && raw[0] == 0xef && raw[1] == 0xbb &&
		    raw[2] == 0xbf)
			continue;
		if (value == '\r')
			continue;
		reader->page[out++] = value == '\t' ? ' ' : (char)value;
	}
	reader->page[out] = '\0';
	reader->next_offset = offset + (long)valid;
	return 0;
}

void reader_init(struct reader_state *reader)
{
	memset(reader, 0, sizeof(*reader));
	reader_scan(reader);
}

void reader_scan(struct reader_state *reader)
{
	const char *custom_root = getenv("EINK_BOOK_ROOT");
	unsigned int i;

	reader->file_count = 0;
	reader->opened = 0;
	if (custom_root && *custom_root)
		scan_directory(reader, custom_root);
	for (i = 0; i < sizeof(book_roots) / sizeof(book_roots[0]); i++)
		scan_directory(reader, book_roots[i]);
	qsort(reader->files, reader->file_count, sizeof(reader->files[0]), compare_files);
	if (reader->selection >= reader->file_count)
		reader->selection = reader->file_count ? reader->file_count - 1 : 0;
}

int reader_open(struct reader_state *reader, unsigned int index)
{
	FILE *file;

	if (index >= reader->file_count)
		return -EINVAL;
	file = fopen(reader->files[index].path, "rb");
	if (!file)
		return -errno;
	fseek(file, 0, SEEK_END);
	reader->file_size = ftell(file);
	fclose(file);
	reader->current_file = index;
	reader->selection = index;
	reader->page_index = 0;
	reader->known_pages = 1;
	reader->page_offsets[0] = 0;
	if (load_page(reader, 0))
		return -EIO;
	reader->opened = 1;
	return 0;
}

int reader_next_page(struct reader_state *reader)
{
	long offset;

	if (!reader->opened || reader->next_offset >= reader->file_size ||
	    reader->page_index + 1 >= READER_MAX_PAGES)
		return 0;
	offset = reader->next_offset;
	reader->page_index++;
	if (reader->page_index >= reader->known_pages) {
		reader->page_offsets[reader->page_index] = offset;
		reader->known_pages = reader->page_index + 1;
	}
	return load_page(reader, reader->page_offsets[reader->page_index]) ? -1 : 1;
}

int reader_previous_page(struct reader_state *reader)
{
	if (!reader->opened || !reader->page_index)
		return 0;
	reader->page_index--;
	return load_page(reader, reader->page_offsets[reader->page_index]) ? -1 : 1;
}

void reader_close_book(struct reader_state *reader)
{
	reader->opened = 0;
}
