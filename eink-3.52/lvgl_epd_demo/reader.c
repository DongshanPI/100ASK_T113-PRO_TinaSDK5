#include "reader.h"

#include <dirent.h>
#include <errno.h>
#include <iconv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *book_roots[] = {
	"/mnt/UDISK/books", "/mnt/SDCARD/books", "/mnt/exUDISK/books",
	"/mnt/UDISK", "/mnt/SDCARD", "/root/books", "/usr/share/eink-os/books",
};

static int has_txt_suffix(const char *name)
{
	size_t length = strlen(name);
	return length > 4 && !strcasecmp(name + length - 4, ".txt");
}

static int compare_files(const void *left, const void *right)
{
	return strcasecmp(((const struct reader_file *)left)->name,
			  ((const struct reader_file *)right)->name);
}

static void scan_directory(struct reader_state *reader, const char *root, int depth)
{
	struct dirent *entry;
	DIR *directory = opendir(root);
	if (!directory) return;
	while (reader->file_count < READER_MAX_FILES && (entry = readdir(directory))) {
		char path[256];
		struct stat info;
		struct reader_file *file;
		if (entry->d_name[0] == '.') continue;
		if (snprintf(path, sizeof(path), "%s/%s", root, entry->d_name) >= (int)sizeof(path)) continue;
		if (stat(path, &info)) continue;
		if (S_ISDIR(info.st_mode) && depth < 2) { scan_directory(reader, path, depth + 1); continue; }
		if (!S_ISREG(info.st_mode) || !has_txt_suffix(entry->d_name)) continue;
		file = &reader->files[reader->file_count++];
		snprintf(file->path, sizeof(file->path), "%s", path);
		snprintf(file->name, sizeof(file->name), "%.95s", entry->d_name);
		file->size = (long)info.st_size; file->mtime = info.st_mtime;
	}
	closedir(directory);
}

static unsigned int utf8_size(unsigned char first)
{
	if (first < 0x80) return 1;
	if ((first & 0xe0) == 0xc0) return 2;
	if ((first & 0xf0) == 0xe0) return 3;
	if ((first & 0xf8) == 0xf0) return 4;
	return 0;
}

static int valid_utf8(const unsigned char *data, size_t size)
{
	size_t i = 0;
	while (i < size) {
		unsigned int n = utf8_size(data[i]), j;
		if (!n || i + n > size) return 0;
		for (j = 1; j < n; j++) if ((data[i + j] & 0xc0) != 0x80) return 0;
		i += n;
	}
	return 1;
}

static int decode_file(const char *path, char **decoded, size_t *decoded_size)
{
	FILE *file = fopen(path, "rb");
	unsigned char *raw;
	long length;
	size_t got, skip = 0;
	if (!file) return -errno;
	if (fseek(file, 0, SEEK_END) || (length = ftell(file)) < 0 || fseek(file, 0, SEEK_SET)) { fclose(file); return -EIO; }
	if (length > 32 * 1024 * 1024L) { fclose(file); return -EFBIG; }
	raw = malloc((size_t)length + 1);
	if (!raw) { fclose(file); return -ENOMEM; }
	got = fread(raw, 1, (size_t)length, file); fclose(file); raw[got] = 0;
	if (got >= 3 && raw[0] == 0xef && raw[1] == 0xbb && raw[2] == 0xbf) skip = 3;
	if (valid_utf8(raw + skip, got - skip)) {
		*decoded_size = got - skip;
		*decoded = malloc(*decoded_size + 1);
		if (!*decoded) { free(raw); return -ENOMEM; }
		memcpy(*decoded, raw + skip, *decoded_size); (*decoded)[*decoded_size] = 0; free(raw); return 0;
	} else {
		iconv_t converter = iconv_open("UTF-8", "GB18030");
		char *input = (char *)raw, *output, *cursor;
		size_t input_left = got, output_left = got * 4 + 4, capacity = output_left;
		if (converter == (iconv_t)-1) { free(raw); return -EINVAL; }
		output = malloc(capacity); cursor = output;
		if (!output) { iconv_close(converter); free(raw); return -ENOMEM; }
		if (iconv(converter, &input, &input_left, &cursor, &output_left) == (size_t)-1) {
			free(output); iconv_close(converter); free(raw); return -EILSEQ;
		}
		iconv_close(converter); free(raw); *decoded_size = capacity - output_left; output[*decoded_size] = 0; *decoded = output; return 0;
	}
}

static int ensure_offset(struct reader_state *reader, unsigned int index)
{
	if (index < reader->page_capacity) return 0;
	{
		unsigned int capacity = reader->page_capacity ? reader->page_capacity * 2 : 64;
		size_t *offsets;
		while (capacity <= index) capacity *= 2;
		offsets = realloc(reader->page_offsets, capacity * sizeof(*offsets));
		if (!offsets) return -ENOMEM;
		reader->page_offsets = offsets; reader->page_capacity = capacity;
	}
	return 0;
}

static int load_page(struct reader_state *reader, size_t offset)
{
	size_t input = offset, output = 0;
	unsigned int line = 1, units = 0;
	while (input < reader->content_size && line <= READER_PAGE_LINES && output + 5 < sizeof(reader->page)) {
		unsigned int bytes = utf8_size((unsigned char)reader->content[input]);
		unsigned int width = bytes > 1 ? 2 : 1;
		char value = reader->content[input];
		if (!bytes || input + bytes > reader->content_size) { input++; continue; }
		if (value == '\r') { input++; continue; }
		if (value == '\n') {
			reader->page[output++] = '\n'; input++; line++; units = 0; continue;
		}
		if (value == '\t') width = 2;
		if (units + width > READER_LINE_UNITS) {
			reader->page[output++] = '\n'; line++; units = 0;
			if (line > READER_PAGE_LINES) break;
		}
		if (value == '\t') { reader->page[output++] = ' '; reader->page[output++] = ' '; input++; units += 2; }
		else { memcpy(reader->page + output, reader->content + input, bytes); output += bytes; input += bytes; units += width; }
	}
	while (output && reader->page[output - 1] == '\n') output--;
	reader->page[output] = 0; reader->next_offset = input;
	return 0;
}

static void read_saved(struct reader_state *reader, const struct reader_file *file, size_t *offset)
{
	FILE *state = fopen(reader->state_path, "r");
	char path[256] = {0}; long size = -1, mtime = -1; size_t saved = 0, bookmark = 0;
	if (!state) return;
	while (!feof(state)) {
		char line[320]; if (!fgets(line, sizeof(line), state)) break;
		line[strcspn(line, "\r\n")] = 0;
		if (!strncmp(line, "path=", 5)) snprintf(path, sizeof(path), "%s", line + 5);
		else if (sscanf(line, "size=%ld", &size) == 1) {}
		else if (sscanf(line, "mtime=%ld", &mtime) == 1) {}
		else if (sscanf(line, "offset=%zu", &saved) == 1) {}
		else if (sscanf(line, "bookmark=%zu", &bookmark) == 1) {}
	}
	fclose(state);
	if (!strcmp(path, file->path) && size == file->size && mtime == (long)file->mtime) {
		*offset = saved < reader->content_size ? saved : 0;
		reader->bookmark_offset = bookmark; reader->bookmark_set = bookmark < reader->content_size;
	}
}

void reader_init(struct reader_state *reader, const char *state_path)
{
	memset(reader, 0, sizeof(*reader));
	snprintf(reader->state_path, sizeof(reader->state_path), "%s", state_path ? state_path : "/etc/eink-os/reader.state");
	reader_scan(reader);
}

void reader_scan(struct reader_state *reader)
{
	const char *custom = getenv("EINK_BOOK_ROOT"); unsigned int i;
	reader->file_count = 0; reader_close_book(reader);
	if (custom && *custom) scan_directory(reader, custom, 0);
	else for (i = 0; i < sizeof(book_roots) / sizeof(book_roots[0]); i++) scan_directory(reader, book_roots[i], 0);
	qsort(reader->files, reader->file_count, sizeof(reader->files[0]), compare_files);
	if (reader->selection >= reader->file_count) reader->selection = reader->file_count ? reader->file_count - 1 : 0;
}

int reader_open(struct reader_state *reader, unsigned int index)
{
	size_t saved = 0;
	if (index >= reader->file_count) return -EINVAL;
	reader_close_book(reader);
	if (decode_file(reader->files[index].path, &reader->content, &reader->content_size)) return -EIO;
	reader->current_file = reader->selection = index; reader->bookmark_set = 0; read_saved(reader, &reader->files[index], &saved);
	if (ensure_offset(reader, 0)) return -ENOMEM;
	reader->page_offsets[0] = saved; reader->page_index = 0; reader->known_pages = 1;
	load_page(reader, saved); reader->opened = 1; return 0;
}

int reader_next_page(struct reader_state *reader)
{
	if (!reader->opened || reader->next_offset >= reader->content_size) return 0;
	if (ensure_offset(reader, reader->page_index + 1)) return -1;
	reader->page_index++;
	if (reader->page_index >= reader->known_pages) { reader->page_offsets[reader->page_index] = reader->next_offset; reader->known_pages++; }
	load_page(reader, reader->page_offsets[reader->page_index]); reader_save(reader); return 1;
}

int reader_previous_page(struct reader_state *reader)
{
	if (!reader->opened || !reader->page_index) return 0;
	reader->page_index--; load_page(reader, reader->page_offsets[reader->page_index]); reader_save(reader); return 1;
}

int reader_toggle_bookmark(struct reader_state *reader)
{
	if (!reader->opened) return -EINVAL;
	if (reader->bookmark_set && reader->bookmark_offset == reader->page_offsets[reader->page_index]) reader->bookmark_set = 0;
	else { reader->bookmark_offset = reader->page_offsets[reader->page_index]; reader->bookmark_set = 1; }
	return reader_save(reader);
}

int reader_jump_bookmark(struct reader_state *reader)
{
	if (!reader->opened || !reader->bookmark_set) return 0;
	reader->page_index = 0; reader->known_pages = 1; reader->page_offsets[0] = reader->bookmark_offset;
	load_page(reader, reader->bookmark_offset); reader_save(reader); return 1;
}

int reader_save(struct reader_state *reader)
{
	char temporary[300], directory[256]; char *slash; FILE *state; struct reader_file *file;
	if (!reader->opened) return 0;
	file = &reader->files[reader->current_file];
	snprintf(directory, sizeof(directory), "%s", reader->state_path); slash = strrchr(directory, '/'); if (slash && slash != directory) { *slash = 0; mkdir(directory, 0755); }
	if (snprintf(temporary, sizeof(temporary), "%s.tmp", reader->state_path) >= (int)sizeof(temporary)) return -ENAMETOOLONG;
	state = fopen(temporary, "w"); if (!state) return -errno;
	fprintf(state, "version=1\npath=%s\nsize=%ld\nmtime=%ld\noffset=%zu\nbookmark=%zu\n", file->path, file->size, (long)file->mtime, reader->page_offsets[reader->page_index], reader->bookmark_set ? reader->bookmark_offset : reader->content_size);
	if (fflush(state) || fsync(fileno(state))) { fclose(state); return -EIO; }
	if (fclose(state) || rename(temporary, reader->state_path)) return -errno;
	return 0;
}

void reader_close_book(struct reader_state *reader)
{
	if (reader->opened) reader_save(reader);
	free(reader->content); reader->content = NULL; reader->content_size = 0; reader->opened = 0; reader->action_menu = 0;
}

void reader_destroy(struct reader_state *reader)
{
	reader_close_book(reader); free(reader->page_offsets); reader->page_offsets = NULL; reader->page_capacity = 0;
}
