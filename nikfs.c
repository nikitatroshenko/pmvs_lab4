#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#define NIKFS_MAX_PATH 255

static int *file_offset_end;
static char **file_name;
static int *file_size;
static int files_cnt = 0;

#define DATA_STORAGE_FILE "/home/nikita/Documents/PMVS4/data_file"
#define METADATA_FILE "/home/nikita/Documents/PMVS4/metadata_file"

struct file_info {
	char file_name[NIKFS_MAX_PATH];
	int file_size;
	int file_offset;
};

static int path_index(const char* path)
{
	int i  = 0;

	for (i = 0; i < files_cnt; i++) {
		if (strcmp(file_name[i], path)==0) {
			return i;
		}
	}

	return -1;
}

static int nikfs_getattr(const char *path, struct stat *stbuf) {
  memset(stbuf, 0, sizeof(struct stat));

  if (strcmp(path, "/") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
    return 0;
  }
  else {
	int index = path_index(path);
	if(index == -1){
		return -ENOENT;
	}
	stbuf->st_mode = S_IFREG | 0777;
	stbuf->st_nlink = 1;
	stbuf->st_size = file_size[index];
	return 0;
  }

  return -ENOENT;
}

static int nikfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
    off_t offset, struct fuse_file_info *fi)
{

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	int i;

	for (i = 0; i < files_cnt; i++) {
		if(strlen(file_name[i])!= 0) {
  			filler(buf, file_name[i]+1, NULL, 0);
		}
	}

  return 0;
}

static int nikfs_open(const char *path, struct fuse_file_info *fi)
{
	int index = path_index(path);
	if (index == -1)
		return -ENOENT;
  	return 0;
}

static int nikfs_read(const char *path, char *buf, size_t size, off_t offset,
    struct fuse_file_info *fi) {

	(void) fi;

	int index = path_index(path);
	FILE *file_in = fopen(DATA_STORAGE_FILE, "rb");
	int start = (index == 0) ? 0
			: file_offset_end[index-1];

	fseek(file_in, start + offset, SEEK_SET);
	fread(buf, size, 1, file_in);

	printf("%d\n",file_offset_end[index]-start);
	printf("%s\n", buf);

	fclose(file_in);

	return size;
}
static int nikfs_utimens (const char *v, const struct timespec tv[2])
{
	return 0;
}
static int nikfs_getxattr (const char *x, const char *y, char *z, size_t f)
{
	return 0;
}
static int nikfs_setxattr (const char *x,
				const char *y,
				const char *z,
				size_t l,
				int f)
{
	return 0;
}
static int nikfs_listxattr (const char *x, char *y, size_t z)
{
	return 0;
}
static int nikfs_write (const char *path,
			const char *buf,
			size_t size,
			off_t offset,
			struct fuse_file_info *fi)
{
	int index = path_index(path);
	if (index == -1) {
		return -ENOENT;
	}
	FILE *file_in = fopen(DATA_STORAGE_FILE, "rb+");
	int start = index == 0 ? 0 : file_offset_end[index-1];
	fseek(file_in, start+offset, SEEK_SET);
	fwrite(buf, size, 1, file_in);
	if (offset == 0) {
		file_size[index] = 0;
	}
	file_offset_end[index] = start + offset;
	file_size[index]+=size;
	fclose(file_in);
	return size;
}
static int nikfs_mknod (const char * path, mode_t mode, dev_t dev)
{
	int index = path_index(path);
	if (index != -1) {
		return -ENOENT;
	}
	else {
		files_cnt++;

		int i  = 0;
		int* buf = calloc(files_cnt, sizeof *buf);
		char **buf_name = calloc(files_cnt, sizeof *buf_name);
		int* size_buf = calloc(files_cnt, sizeof *size_buf);

		for (i = 0; i < files_cnt; i++) {
			buf_name[i] = calloc(NIKFS_MAX_PATH, sizeof *buf_name);
		}

		for (i  = 0; i < files_cnt-1; i++) {
			buf[i] = file_offset_end[i];
			size_buf[i] = file_size[i];
			memset(buf_name[i], 0, NIKFS_MAX_PATH);
			strcpy(buf_name[i], file_name[i]);
		}

		if (files_cnt != 1) {
			for(i = 0; i < files_cnt - 2; i++) {
				free(file_name[i]);
			}

			free(file_name);
			free(file_offset_end);
			free(file_size);
		}

		for (i = 0; i < files_cnt - 1; i++) {
			printf("%s\n", buf_name[i]);
		}

		buf[files_cnt - 1] = files_cnt == 1 ? 0 : buf[files_cnt - 2];
		size_buf[files_cnt - 1] = 0; 
		memset(buf_name[files_cnt - 1], 0, NIKFS_MAX_PATH);
		strcpy(buf_name[files_cnt - 1], path);

		for (i = 0; i < files_cnt; i++) {
			printf("%s\n", buf_name[i]);
			printf("%d\n", buf[i]);
		}

		file_name = buf_name;
		file_offset_end = buf;
		file_size = size_buf;
	}
	return 0;
}
static int nikfs_unlink (const char *path)
{
	int index = path_index(path);

	if (index == -1) {
		return -ENOENT;
	}

	memset(file_name[index], 0, NIKFS_MAX_PATH);

	return 0;
}
static int nikfs_truncate (const char * z, off_t v)
{
	return 0;
}
static struct fuse_operations fuse_example_operations = {
	.getattr	= nikfs_getattr,
	.mknod		= nikfs_mknod,
	.open		= nikfs_open,
	.read		= nikfs_read,
	.readdir	= nikfs_readdir,
	.utimens	= nikfs_utimens,
	.setxattr	= nikfs_setxattr,
	.getxattr	= nikfs_getxattr,
	.listxattr	= nikfs_listxattr,
	.write 		= nikfs_write,
	.truncate 	= nikfs_truncate,
	.unlink 	= nikfs_unlink
};


int main(int argc, char *argv[])
{
	FILE *file_in = fopen(METADATA_FILE, "rb");
	
	fseek(file_in, 0, SEEK_SET);
	files_cnt = 0;
	fread(&files_cnt, sizeof(int), 1, file_in);

	if (files_cnt != 0) {
		file_offset_end = calloc(files_cnt, sizeof *file_offset_end);
		file_name = calloc(files_cnt, sizeof *file_name);
		file_size = calloc(files_cnt, sizeof *file_size);

		int i = 0;

		for (i = 0; i< files_cnt; i++) {
			file_name[i] = calloc(NIKFS_MAX_PATH,
						sizeof *file_name);
		}

		for (i = 0; i < files_cnt; i++) {
			struct file_info info;

			memset(info.file_name, 0, NIKFS_MAX_PATH);

			fread(&info, sizeof info, 1, file_in);
			file_size[i] = info.file_size;
			file_offset_end[i] = info.file_offset;

			memset(file_name[i], 0, NIKFS_MAX_PATH);
			strcpy(file_name[i], info.file_name);
		}
	}

	fclose(file_in);

	int x = fuse_main(argc, argv, &fuse_example_operations, NULL);

	file_in = fopen(METADATA_FILE, "rb+");
	fseek(file_in, 0, SEEK_SET);
	fwrite(&files_cnt, sizeof(int), 1, file_in);

	int i = 0;

	for (i = 0; i < files_cnt; i++) {
		struct file_info info;

		memset(info.file_name, 0, NIKFS_MAX_PATH);

		strcpy(info.file_name, file_name[i]);
		info.file_size = file_size[i];
		info.file_offset = file_offset_end[i];

		fwrite(&info, sizeof(struct file_info), 1, file_in);
	}

	for (i = 0; i < files_cnt-2; i++) {
		free(file_name[i]);
	}

	free(file_name);
	free(file_offset_end);
	free(file_size);
	fclose(file_in);

	return x;
}

