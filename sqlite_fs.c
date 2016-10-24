#include <fuse.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sqlite3.h"

#define SQLFS_MAX_PATH 65

static struct options {
	const char *img_path;
	int show_help;
} options;

#define OPTION(t, p)                           \
    { t, offsetof(struct options, p), 1 }
static const struct fuse_opt option_spec[] = {
	OPTION("--img_path=%s", img_path),
	OPTION("-h", show_help),
	OPTION("--help", show_help),
	FUSE_OPT_END
};

void *sqlfs_init(struct fuse_conn_info *conn)
{
	sqlite3 *db;
	const char *sql;
	sqlite3_stmt *stmt;
	char *img_path;

	img_path = fuse_get_context()->private_data;

	sqlite3_open(img_path, &db);

	sql = "CREATE TABLE FILES(id INTEGER PRIMARY KEY,"
			" fname TEXT NOT NULL UNIQUE,"
			" attr INTEGER NOT NULL,"
			" data BLOB NULL);";
	sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	return db;
}

void sqlfs_destroy(void *db)
{
	sqlite3_close(db);
}

int sqlfs_mknod(const char *path, mode_t mode, dev_t dev)
{
	const char *sql = "INSERT INTO FILES(fname) VALUES('?');";
	struct fuse_context *cxt;
	sqlite3 *db;
	sqlite3_stmt *stmt;
	int rc = 0;

	if (strlen(path) > SQLFS_MAX_PATH)
		return -ENAMETOOLONG;

	cxt = fuse_get_context();
	db = (sqlite3 *)cxt->private_data;
	
	sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, path, -1, SQLITE_TRANSIENT);
	if (sqlite3_step(stmt) == SQLITE_ERROR)
		rc = -EEXIST;
	sqlite3_finalize(stmt);

	return rc;
}

int sqlfs_open(const char *path, struct fuse_file_info *fi)
{
	const char *sql = "SELECT fname FROM FILES WHERE fname='?';";
	struct fuse_context *cxt;
	sqlite3 *db;
	sqlite3_stmt *stmt;
	int rc = 0;

	(void)fi;

	cxt = fuse_get_context();
	db = (sqlite3 *)cxt->private_data;
	
	sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, path, -1, SQLITE_TRANSIENT);
	if (sqlite3_step(stmt) == SQLITE_ERROR)
		rc = -EEXIST;
	sqlite3_finalize(stmt);

	return rc;
}

int sqlfs_unlink(const char *path)
{
	const char *sql = "SELECT id FROM FILES WHERE fname='?';";
	sqlite3 *db;
	sqlite3_stmt *stmt;
	int rc = 0;

	db = (sqlite3 *)fuse_get_context()->private_data;
	sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, path, -1, SQLITE_TRANSIENT);

	if (sqlite3_step(stmt) != SQLITE_ROW) {
		rc = -ENOENT;
	}
	sqlite3_finalize(stmt);

	if (rc)
		return rc;

	sql = "DELETE FROM FILES WHERE fname='?';";
	sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, path, -1, SQLITE_TRANSIENT);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	return 0;
}

int sqlfs_read(const char *path,
		char *buf,
		size_t size,
		off_t off,
		struct fuse_file_info *fi)
{
	const char *sql = "SELECT data FROM FILES WHERE fname='?';";
	struct fuse_context *cxt;
	sqlite3 *db;
	sqlite3_stmt *stmt;
	int blob_size;
	const char *blob;

	cxt = fuse_get_context();
	db = (sqlite3 *)cxt->private_data;
	
	sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, path, -1, SQLITE_TRANSIENT);
	if (sqlite3_step(stmt) != SQLITE_ROW) {
		sqlite3_finalize(stmt);

		return -ENOENT;
	}

	blob = sqlite3_column_blob(stmt, 0);
	blob_size = sqlite3_column_bytes(stmt, 0);
	if (off >= blob_size)
		return 0;

	if (off + size > blob_size)
		size = blob_size - off;
	memcpy(buf, blob + off, size);

	sqlite3_finalize(stmt);

	return size;
}

int sqlfs_write(const char *path,
		char *buf,
		size_t size,
		off_t off,
		struct fuse_file_info *fi)
{
	const char *sql = "SELECT data FROM FILES WHERE fname='?'";
	struct fuse_context *cxt;
	sqlite3 *db;
	sqlite3_stmt *stmt;
	int blob_size;
	const char *blob;
	char *new_blob;
	int new_blob_size;

	cxt = fuse_get_context();
	db = (sqlite3 *)cxt->private_data;

	sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	sqlite3_bind_text(stmt, 0, path, -1, SQLITE_TRANSIENT);

	if (sqlite3_step(stmt) != SQLITE_ROW)
		return -ENOENT;

	blob = sqlite3_column_blob(stmt, 0);
	blob_size = sqlite3_column_bytes(stmt, 0);

	if (off + size >= blob_size)
		new_blob_size = off + size;
	else
		new_blob_size = blob_size;

	new_blob = calloc(new_blob_size, sizeof *new_blob);
	memcpy(new_blob, blob, blob_size);
	memcpy(new_blob + off, buf, size);
	sqlite3_finalize(stmt);

	sql = "UPDATE FILES SET data=? WHERE fname='?'";
	sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	sqlite3_bind_text(stmt, 2, path, -1, SQLITE_TRANSIENT);
	sqlite3_bind_blob(stmt, 1, new_blob, new_blob_size, SQLITE_TRANSIENT);
	sqlite3_step(stmt);

	sqlite3_finalize(stmt);
	free(new_blob);

	return 0;
}

int sqlfs_rename(const char *old, const char *new, unsigned int flags)
{
	const char *sql = "UPDATE FILES SET fname='?' WHERE fname='?';";
	struct fuse_context *cxt;
	sqlite3 *db;
	sqlite3_stmt *stmt;
	int rc = 0;

	if (strlen(new) > SQLFS_MAX_PATH)
		return -ENAMETOOLONG;

	cxt = fuse_get_context();
	db = (sqlite3 *)cxt->private_data;

	sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, new, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, old, -1, SQLITE_TRANSIENT);
	if (sqlite3_step(stmt) == SQLITE_ERROR)
		rc = -EEXIST;
	sqlite3_finalize(stmt);

	return 0;
}

int sqlfs_getattr(const char *path, struct stat *st_buf, struct fuse_file_info *fi)
{
	const char *sql = "SELECT attr, data FROM FILES WHERE fname='?';";
	sqlite3_stmt *stmt;
	int attr;
	struct fuse_context *cxt;
	sqlite3 *db;

	memset(st_buf, 0, sizeof *st_buf);

	if (strcmp(path, "/") == 0) {
		st_buf->st_mode = S_IFDIR | 0755;
		st_buf->st_nlink = 2;

		return 0;
	}

	cxt = fuse_get_context();
	db = (sqlite3 *)cxt->private_data;

	sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, path, -1, SQLITE_TRANSIENT);
	if (sqlite3_step(stmt) != SQLITE_ROW)
		return -ENOENT;

	attr = sqlite3_column_int(stmt, 0);
	st_buf->st_mode = S_IFREG | 0777;
	st_buf->st_nlink = 1;
	sqlite3_column_blob(stmt, 1);
	st_buf->st_size = sqlite3_column_bytes(stmt, 1);

	sqlite3_finalize(stmt);

	return 0;
}

int sqlfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
	off_t offset, struct fuse_file_info *fi)
{
	const char *sql = "SELECT fname FROM FILES;";
	sqlite3 *db;
	sqlite3_stmt *stmt;
	char *fname;

	(void) offset;
	(void) fi;
	(void) flags;

	if (strcmp(path, "/") != 0)
		return -ENOENT;

	filler(buf, ".", NULL, 0, 0);
	filler(buf, "..", NULL, 0, 0);

	db = (sqlite3 *)fuse_get_context()->private_data;
	sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		fname = sqlite3_column_text(stmt, 0);
		filler(buf, fname, NULL, 0, 0);
	}
	sqlite3_finalize(stmt);

	return 0;
}

static struct fuse_operations sqlfs_oper = {
	.getattr	= sqlfs_getattr,
	.readdir	= sqlfs_readdir,
	.mknod		= sqlfs_mknod,
	.open		= sqlfs_open,
	.unlink		= sqlfs_unlink,
	.read		= sqlfs_read,
	.write		= sqlfs_write,
	.rename		= sqlfs_rename,
	.init		= sqlfs_init,
	.destroy	= sqlfs_destroy
};

static void show_help(const char *progname)
{
	printf("usage: %s [options] <mountpoint>\n\n", progname);
	printf("File-system specific options:\n"
	       "    --img_path=<s>      Image disk path\n"
	       "                        (default: \"test.sqlfs\")\n"
	       "\n");
}

int main(int argc, char *argv[])
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	/* Set defaults -- we have to use strdup so that
	   fuse_opt_parse can free the defaults if other
	   values are specified */
	options.img_path = strdup("test.sqlfs");

	/* Parse options */
	if (fuse_opt_parse(&args, &options, option_spec, NULL) == -1)
		return 1;

	/* When --help is specified, first print our own file-system
	   specific help text, then signal fuse_main to show
	   additional help (by adding `--help` to the options again)
	   without usage: line (by setting argv[0] to the empty
	   string) */
	if (options.show_help) {
		show_help(argv[0]);
		assert(fuse_opt_add_arg(&args, "--help") == 0);
		args.argv[0] = (char*) "";
	}

	return fuse_main(args.argc, args.argv, &sqlfs_oper, NULL);
}
