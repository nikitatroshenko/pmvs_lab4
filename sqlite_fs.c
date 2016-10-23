#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sqlite3.h"

#define SQLFS_MAX_PATH 65

sqlite3 *sqlfs_init(const char *img_path)
{
	sqlite3 *db;
	const char *sql;
	sqlite3_stmt *stmt;

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

void sqlfs_destroy(sqlite3 *db)
{
	sqlite3_close(db);
}

int sqlfs_create(const char *path, int mode, sqlite3 *db)
{
	const char *sql = "INSERT INTO FILES(fname) VALUES('?');";
	sqlite3_stmt *stmt;
	int rc = 0;

	if (strlen(path) > SQLFS_MAX_PATH)
		return -ENAMETOOLONG;

	sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
	if (sqlite3_step(stmt) == SQLITE_ERROR)
		rc = -EEXIST;
	sqlite3_finalize(stmt);

	return rc;
}

int sqlfs_open(const char *path, int flags, sqlite3 *db)
{
	const char *sql = "SELECT fname FROM FILES WHERE fname='?';";
	sqlite3_stmt *stmt;
	int rc = 0;

	sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
	if (sqlite3_step(stmt) == SQLITE_ERROR)
		rc = -EEXIST;
	sqlite3_finalize(stmt);

	return rc;
}

int sqlfs_read(const char *path, char *buf, int size, int off, sqlite3 *db)
{
	const char *sql = "SELECT data FROM FILES WHERE fname='?';";
	sqlite3_stmt *stmt;
	int blob_size;
	char *blob;

	sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
	if (sqlite3_step(stmt) != SQLITE_ROW) {
		sqlite3_finalize(stmt);

		return -ENOENT;
	}

	blob = sqlite3_column_blob(stmt, 3);
	blob_size = sqlite3_column_bytes(stmt, 3);
	if (off >= blob_size)
		return 0;

	if (off + size > blob_size)
		size = blob_size - off;
	memcpy(buf, blob + off, size);

	sqlite3_finalize(stmt);

	return size;
}

int sqlfs_write(const char *path, char *buf, int size, int off, sqlite3 *db)
{
	const char *sql = "SELECT data FROM FILES WHERE fname='?'";
	sqlite3_stmt *stmt;
	int blob_size;
	char *blob;
	char *new_blob;
	int new_blob_size;

	sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	sqlite3_bind_text(stmt, 0, path, -1, SQLITE_STATIC);

	if (sqlite3_step(stmt) != SQLITE_ROW)
		return -ENOENT;

	blob = sqlite3_column_blob(stmt, 3);
	blob_size = sqlite3_column_bytes(stmt, 3);

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
	sqlite3_bind_text(stmt, 2, path, -1, SQLITE_STATIC);
	sqlite3_bind_blob(stmt, 1, new_blob, new_blob_size, SQLITE_TRANSIENT);
	sqlite3_step(stmt);

	sqlite3_finalize(stmt);
	free(new_blob);

	return 0;
}

int sqlfs_rename(const char *old, const char *new, int flags, sqlite3 *db)
{
	const char *sql = "UPDATE FILES SET fname='?' WHERE fname='?';";
	sqlite3_stmt *stmt;
	int rc = 0;

	if (strlen(new) > SQLFS_MAX_PATH)
		return -ENAMETOOLONG;

	sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, new, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 2, old, -1, SQLITE_STATIC);
	if (sqlite3_step(stmt) == SQLITE_ERROR)
		rc = -EEXIST;
	sqlite3_finalize(stmt);

	return 0;
}
