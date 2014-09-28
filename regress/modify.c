/*
  modify.c -- test tool for modifying zip archive in multiple ways
  Copyright (C) 2012-2014 Dieter Baron and Thomas Klausner

  This file is part of libzip, a library to manipulate ZIP archives.
  The authors can be contacted at <libzip@nih.at>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.
  3. The names of the authors may not be used to endorse or promote
     products derived from this software without specific prior
     written permission.
 
  THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS
  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
  IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "config.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef HAVE_GETOPT
#include "getopt.h"
#endif

#include "zip.h"

typedef struct dispatch_table_s {
    const char *cmdline_name;
    int argument_count;
    const char *arg_names;
    const char *description;
    int (*function)(int argc, char *argv[]);
} dispatch_table_t;

static zip_flags_t get_flags(const char *arg);
static zip_int32_t get_compression_method(const char *arg);
static void hexdump(const zip_uint8_t *data, zip_uint16_t len);
static zip_t *read_to_memory(const char *archive, int flags, int *err, zip_source_t **srcp);

zip_t *za, *z_in;
zip_flags_t stat_flags;

static int
add(int argc, char *argv[]) {
    zip_source_t *zs;

    if ((zs=zip_source_buffer(za, argv[1], strlen(argv[1]), 0)) == NULL) {
	fprintf(stderr, "can't create zip_source from buffer: %s\n", zip_strerror(za));
	return -1;
    }

    if (zip_add(za, argv[0], zs) == -1) {
	zip_source_free(zs);
	fprintf(stderr, "can't add file '%s': %s\n", argv[0], zip_strerror(za));
	return -1;
    }
    return 0;
}

static int
add_dir(int argc, char *argv[]) {
    /* add directory */
    if (zip_add_dir(za, argv[0]) < 0) {
	fprintf(stderr, "can't add directory '%s': %s\n", argv[0], zip_strerror(za));
	return -1;
    }
    return 0;
}

static int
add_file(int argc, char *argv[]) {
    zip_source_t *zs;
    zip_uint64_t start = strtoull(argv[2], NULL, 10);
    zip_int64_t len = strtoll(argv[3], NULL, 10);

    if (strcmp(argv[1], "/dev/stdin") == 0) {
	if ((zs=zip_source_filep(za, stdin, start, len)) == NULL) {
	    fprintf(stderr, "can't create zip_source from stdin: %s\n", zip_strerror(za));
	    return -1;
	}
    } else {
	if ((zs=zip_source_file(za, argv[1], start, len)) == NULL) {
	    fprintf(stderr, "can't create zip_source from file: %s\n", zip_strerror(za));
	    return -1;
	}
    }

    if (zip_add(za, argv[0], zs) == -1) {
	zip_source_free(zs);
	fprintf(stderr, "can't add file '%s': %s\n", argv[0], zip_strerror(za));
	return -1;
    }
    return 0;
}

static int
add_from_zip(int argc, char *argv[]) {
    zip_uint64_t idx;
    int err;
    char buf[100];
    zip_source_t *zs;
    /* add from another zip file */
    idx = strtoull(argv[2], NULL, 10);
    zip_uint64_t start = strtoull(argv[3], NULL, 10);
    zip_int64_t len = strtoll(argv[4], NULL, 10);
    if ((z_in=zip_open(argv[1], ZIP_CHECKCONS, &err)) == NULL) {
	zip_error_to_str(buf, sizeof(buf), err, errno);
	fprintf(stderr, "can't open source zip archive '%s': %s\n", argv[1], buf);
	return -1;
    }
    if ((zs=zip_source_zip(za, z_in, idx, 0, start, len)) == NULL) {
	fprintf(stderr, "error creating file source from '%s' index '%" PRIu64 "': %s\n", argv[1], idx, zip_strerror(za));
	zip_close(z_in);
	return -1;
    }
    if (zip_add(za, argv[0], zs) == -1) {
	fprintf(stderr, "can't add file '%s': %s\n", argv[0], zip_strerror(za));
	zip_source_free(zs);
	zip_close(z_in);
	return -1;
    }
    return 0;
}

static int
count_extra(int argc, char *argv[]) {
    zip_int16_t count;
    zip_uint64_t idx;
    zip_flags_t ceflags = 0;
    idx = strtoull(argv[0], NULL, 10);
    ceflags = get_flags(argv[1]);
    if ((count=zip_file_extra_fields_count(za, idx, ceflags)) < 0) {
	fprintf(stderr, "can't get extra field count for file at index '%" PRIu64 "': %s\n", idx, zip_strerror(za));
	return -1;
    } else {
	printf("Extra field count: %d\n", count);
    }
    return 0;
}

static int
count_extra_by_id(int argc, char *argv[]) {
    zip_int16_t count;
    zip_uint16_t eid;
    zip_flags_t ceflags = 0;
    zip_uint64_t idx;
    idx = strtoull(argv[0], NULL, 10);
    eid = (zip_uint16_t)strtoull(argv[1], NULL, 10);
    ceflags = get_flags(argv[2]);
    if ((count=zip_file_extra_fields_count_by_id(za, idx, eid, ceflags)) < 0) {
	fprintf(stderr, "can't get extra field count for file at index '%" PRIu64 "' and for id `%d': %s\n", idx, eid, zip_strerror(za));
	return -1;
    } else {
	printf("Extra field count: %d\n", count);
    }
    return 0;
}

static int
delete(int argc, char *argv[]) {
    zip_uint64_t idx;
    idx = strtoull(argv[0], NULL, 10);
    if (zip_delete(za, idx) < 0) {
	fprintf(stderr, "can't delete file at index '%" PRIu64 "': %s\n", idx, zip_strerror(za));
	return -1;
    }
    return 0;
}

static int
delete_extra(int argc, char *argv[]) {
    zip_flags_t geflags;
    zip_uint16_t eid;
    zip_uint64_t idx;
    idx = strtoull(argv[0], NULL, 10);
    eid = (zip_uint16_t)strtoull(argv[1], NULL, 10);
    geflags = get_flags(argv[2]);
    if ((zip_file_extra_field_delete(za, idx, eid, geflags)) < 0) {
	fprintf(stderr, "can't delete extra field data for file at index '%" PRIu64 "', extra field id `%d': %s\n", idx, eid, zip_strerror(za));
	return -1;
    }
    return 0;
}

static int
delete_extra_by_id(int argc, char *argv[]) {
    zip_flags_t geflags;
    zip_uint16_t eid, eidx;
    zip_uint64_t idx;
    idx = strtoull(argv[0], NULL, 10);
    eid = (zip_uint16_t)strtoull(argv[1], NULL, 10);
    eidx = (zip_uint16_t)strtoull(argv[2], NULL, 10);
    geflags = get_flags(argv[3]);
    if ((zip_file_extra_field_delete_by_id(za, idx, eid, eidx, geflags)) < 0) {
	fprintf(stderr, "can't delete extra field data for file at index '%" PRIu64 "', extra field id `%d', extra field idx `%d': %s\n", idx, eid, eidx, zip_strerror(za));
	return -1;
    }
    return 0;
}

static int
get_archive_comment(int argc, char *argv[]) {
    const char *comment;
    int len;
    /* get archive comment */
    if ((comment=zip_get_archive_comment(za, &len, 0)) == NULL)
	printf("No archive comment\n");
    else
	printf("Archive comment: %.*s\n", len, comment);
    return 0;
}

static int
get_extra(int argc, char *argv[]) {
    zip_flags_t geflags;
    zip_uint16_t id, eidx, eflen;
    const zip_uint8_t *efdata;
    zip_uint64_t idx;
    /* get extra field data */
    idx = strtoull(argv[0], NULL, 10);
    eidx = (zip_uint16_t)strtoull(argv[1], NULL, 10);
    geflags = get_flags(argv[2]);
    if ((efdata=zip_file_extra_field_get(za, idx, eidx, &id, &eflen, geflags)) == NULL) {
	fprintf(stderr, "can't get extra field data for file at index %" PRIu64 ", extra field %d, flags %u: %s\n", idx, eidx, geflags, zip_strerror(za));
	return -1;
    }
    printf("Extra field 0x%04x: len %d", id, eflen);
    if (eflen > 0) {
	printf(", data ");
	hexdump(efdata, eflen);
    }
    printf("\n");
    return 0;
}

static int
get_extra_by_id(int argc, char *argv[]) {
    zip_flags_t geflags;
    zip_uint16_t eid, eidx, eflen;
    const zip_uint8_t *efdata;
    zip_uint64_t idx;
    idx = strtoull(argv[0], NULL, 10);
    eid = (zip_uint16_t)strtoull(argv[1], NULL, 10);
    eidx = (zip_uint16_t)strtoull(argv[2], NULL, 10);
    geflags = get_flags(argv[3]);
    if ((efdata=zip_file_extra_field_get_by_id(za, idx, eid, eidx, &eflen, geflags)) == NULL) {
	fprintf(stderr, "can't get extra field data for file at index %" PRIu64 ", extra field id %d, ef index %d, flags %u: %s\n", idx, eid, eidx, geflags, zip_strerror(za));
	return -1;
    }
    printf("Extra field 0x%04x: len %d", eid, eflen);
    if (eflen > 0) {
	printf(", data ");
	hexdump(efdata, eflen);
    }
    printf("\n");
    return 0;
}

static int
get_file_comment(int argc, char *argv[]) {
    const char *comment;
    int len;
    zip_uint64_t idx;
    /* get file comment */
    idx = strtoull(argv[0], NULL, 10);
    if ((comment=zip_get_file_comment(za, idx, &len, 0)) == NULL) {
	fprintf(stderr, "can't get comment for '%s': %s\n", zip_get_name(za, idx, 0), zip_strerror(za));
	return -1;
    } else if (len == 0)
	printf("No comment for '%s'\n", zip_get_name(za, idx, 0));
    else
	printf("File comment for '%s': %.*s\n", zip_get_name(za, idx, 0), len, comment);
    return 0;
}

static int
zrename(int argc, char *argv[]) {
    zip_uint64_t idx;
    idx = strtoull(argv[0], NULL, 10);
    if (zip_rename(za, idx, argv[1]) < 0) {
	fprintf(stderr, "can't rename file at index '%" PRIu64 "' to `%s': %s\n", idx, argv[1], zip_strerror(za));
	return -1;
    }
    return 0;
}

static int
replace_file_contents(int argc, char *argv[]) {
    /* replace file contents with data from command line */
    const char *content;
    zip_source_t *s;
    zip_uint64_t idx;
    idx = strtoull(argv[0], NULL, 10);
    content = argv[1];
    if ((s=zip_source_buffer(za, content, strlen(content), 0)) == NULL ||
	zip_file_replace(za, idx, s, 0) < 0) {
	zip_source_free(s);
	fprintf(stderr, "error replacing file data: %s\n", zip_strerror(za));
	return -1;
    }
    return 0;
}

static int
set_extra(int argc, char *argv[]) {
    zip_flags_t geflags;
    zip_uint16_t eid, eidx;
    const zip_uint8_t *efdata;
    zip_uint64_t idx;
    idx = strtoull(argv[0], NULL, 10);
    eid = (zip_uint16_t)strtoull(argv[1], NULL, 10);
    eidx = (zip_uint16_t)strtoull(argv[2], NULL, 10);
    geflags = get_flags(argv[3]);
    efdata = (zip_uint8_t *)argv[4];
    if ((zip_file_extra_field_set(za, idx, eid, eidx, efdata, (zip_uint16_t)strlen((const char *)efdata), geflags)) < 0) {
	fprintf(stderr, "can't set extra field data for file at index '%" PRIu64 "', extra field id `%d', index `%d': %s\n", idx, eid, eidx, zip_strerror(za));
	return -1;
    }
    return 0;
}

static int
set_file_comment(int argc, char *argv[]) {
    zip_uint64_t idx;
    idx = strtoull(argv[0], NULL, 10);
    if (zip_file_set_comment(za, idx, argv[1], (zip_uint16_t)strlen(argv[1]), 0) < 0) {
	fprintf(stderr, "can't set file comment at index '%" PRIu64 "' to `%s': %s\n", idx, argv[1], zip_strerror(za));
	return -1;
    }
    return 0;
}

static int
set_file_compression(int argc, char *argv[]) {
    zip_int32_t method;
    zip_uint32_t flags;
    zip_uint64_t idx;
    idx = strtoull(argv[0], NULL, 10);
    method = get_compression_method(argv[1]);
    flags = (zip_uint32_t)strtoull(argv[2], NULL, 10);
    if (zip_set_file_compression(za, idx, method, flags) < 0) {
	fprintf(stderr, "can't set file compression method at index '%" PRIu64 "' to `%s', flags `%d': %s\n", idx, argv[1], flags, zip_strerror(za));
	return -1;
    }
    return 0;
}

static int
set_file_mtime(int argc, char *argv[]) {
    /* set file last modification time (mtime) */
    time_t mtime;
    zip_uint64_t idx;
    idx = strtoull(argv[0], NULL, 10);
    mtime = (time_t)strtoull(argv[1], NULL, 10);
    if (zip_file_set_mtime(za, idx, mtime, 0) < 0) {
	fprintf(stderr, "can't set file mtime at index '%" PRIu64 "' to `%ld': %s\n", idx, mtime, zip_strerror(za));
	return -1;
    }
    return 0;
}

static int
zstat(int argc, char *argv[]) {
    zip_uint64_t idx;
    char buf[100];
    struct zip_stat sb;
    idx = strtoull(argv[0], NULL, 10);

    if (zip_stat_index(za, idx, stat_flags, &sb) < 0) {
	fprintf(stderr, "zip_stat_index failed on '%" PRIu64 "' failed: %s\n", idx, zip_strerror(za));
	return -1;
    }

    if (sb.valid & ZIP_STAT_NAME)
	printf("name: '%s'\n", sb.name);
    if (sb.valid & ZIP_STAT_INDEX)
	printf("index: '%"PRIu64"'\n", sb.index);
    if (sb.valid & ZIP_STAT_SIZE)
	printf("size: '%"PRIu64"'\n", sb.size);
    if (sb.valid & ZIP_STAT_COMP_SIZE)
	printf("compressed size: '%"PRIu64"'\n", sb.comp_size);
    if (sb.valid & ZIP_STAT_MTIME) {
	struct tm *tpm;
	tpm = localtime(&sb.mtime);
	strftime(buf, sizeof(buf), "%a %b %d %Y %H:%M:%S", tpm);
	printf("mtime: '%s'\n", buf);
    }
    if (sb.valid & ZIP_STAT_CRC)
	printf("crc: '%0x'\n", sb.crc);
    if (sb.valid & ZIP_STAT_COMP_METHOD)
	printf("compression method: '%d'\n", sb.comp_method);
    if (sb.valid & ZIP_STAT_ENCRYPTION_METHOD)
	printf("encryption method: '%d'\n", sb.encryption_method);
    if (sb.valid & ZIP_STAT_FLAGS)
	printf("flags: '%ld'\n", (long)sb.flags);
    printf("\n");

    return 0;
}

static int
zin_close(int argc, char *argv[]) {
    if (zip_close(z_in) < 0) {
	fprintf(stderr, "can't close source archive: %s\n", zip_strerror(z_in));
	return -1;
    }
    return 0;
}

static zip_flags_t
get_flags(const char *arg)
{
    zip_flags_t flags = 0;
    if (strchr(arg, 'c') != NULL)
	flags |= ZIP_FL_CENTRAL;
    if (strchr(arg, 'l') != NULL)
	flags |= ZIP_FL_LOCAL;
    if (strchr(arg, 'u') != NULL)
	flags |= ZIP_FL_UNCHANGED;
    return flags;
}

static zip_int32_t
get_compression_method(const char *arg)
{
    if (strcmp(arg, "default") == 0)
        return ZIP_CM_DEFAULT;
    else if (strcmp(arg, "store") == 0)
        return ZIP_CM_STORE;
    else if (strcmp(arg, "deflate") ==0)
        return ZIP_CM_DEFLATE;
    return 0; /* TODO: error handling */
}

static void
hexdump(const zip_uint8_t *data, zip_uint16_t len)
{
    zip_uint16_t i;

    if (len <= 0)
	return;

    printf("0x");
	
    for (i=0; i<len; i++)
	printf("%02x", data[i]);

    return;    
}

static zip_t *
read_to_memory(const char *archive, int flags, int *err, zip_source_t **srcp)
{
    struct stat st;
    zip_source_t *src;
    zip_t *zb;
    zip_error_t error;

    if (stat(archive, &st) < 0) {
	if (errno == ENOENT) {
	    src = zip_source_buffer_create(NULL, 0, 0, &error);
	}
	else {
	    *err = ZIP_ER_OPEN;
	    return NULL;
	}
    }
    else {
	char *buf;
	FILE *fp;
	if ((buf=malloc((size_t)st.st_size)) == NULL) {
	    *err = ZIP_ER_MEMORY;
	    return NULL;
	}
	if ((fp=fopen(archive, "r")) == NULL) {
	    free(buf);
	    *err = ZIP_ER_READ;
	    return NULL;
	}
	if (fread(buf, (size_t)st.st_size, 1, fp) < 1) {
	    free(buf);
	    *err = ZIP_ER_READ;
	    return NULL;
	}
	fclose(fp);
	src = zip_source_buffer_create(buf, (zip_uint64_t)st.st_size, 1, &error);
	if (src == NULL) {
	    free(buf);
	}
    }
    if (src == NULL) {
	*err = zip_error_code_zip(&error);
	errno = zip_error_code_system(&error);
	return NULL;
    }
    zb = zip_open_from_source(src, flags, &error);
    if (zb == NULL) {
	*err = zip_error_code_zip(&error);
	errno = zip_error_code_system(&error);
	zip_source_free(src);
	return NULL;
    }
    zip_source_keep(src);
    *srcp = src;
    return zb;
}

static int
write_memory_src_to_file(const char *archive, zip_source_t *src)
{
    zip_stat_t zst;
    char *buf;
    FILE *fp;

    if (zip_source_stat(src, &zst) < 0) {
	fprintf(stderr, "zip_source_stat on buffer failed: %s\n", zip_error_strerror(zip_source_error(src)));
	return -1;
    }
    if (zip_source_open(src) < 0) {
	if (zip_error_code_zip(zip_source_error(src)) == ZIP_ER_DELETED) {
	    if (remove(archive) < 0 && errno != ENOENT) {
		fprintf(stderr, "remove failed: %s\n", strerror(errno));
		return -1;
	    }
	    return 0;
	}
	fprintf(stderr, "zip_source_open on buffer failed: %s\n", zip_error_strerror(zip_source_error(src)));
	return -1;
    }
    if ((buf=malloc(zst.size)) == NULL) {
	fprintf(stderr, "malloc failed: %s\n", strerror(errno));
	zip_source_close(src);
	return -1;
    }
    if (zip_source_read(src, buf, zst.size) < (zip_int64_t)zst.size) {
	fprintf(stderr, "zip_source_read on buffer failed: %s\n", zip_error_strerror(zip_source_error(src)));
	zip_source_close(src);
	free(buf);
	return -1;
    }
    zip_source_close(src);
    if ((fp=fopen(archive, "wb")) == NULL) {
	fprintf(stderr, "fopen failed: %s\n", strerror(errno));
	free(buf);
	return -1;
    }
    if (fwrite(buf, zst.size, 1, fp) < 1) {
	fprintf(stderr, "fwrite failed: %s\n", strerror(errno));
	free(buf);
	return -1;
    }
    free(buf);
    if (fclose(fp) != 0) {
	fprintf(stderr, "fclose failed: %s\n", strerror(errno));
	return -1;
    }
    return 0;
}

dispatch_table_t dispatch_table[] = {
    { "add", 2, "name content", "add file called name using content", add },
    { "add_dir", 1, "name", "add directory", add_dir },
    { "add_file", 4, "name file_to_add offset len", "add file to archive, len bytes starting from offset", add_file },
    { "add_from_zip", 5, "name archivename index offset len", "add file from another archive, len bytes starting from offset", add_from_zip },
    { "count_extra", 2, "index flags", "show number of extra fields for archive entry", count_extra },
    { "count_extra_by_id", 3, "index extra_id flags", "show number of extra fields of type extra_id for archive entry", count_extra_by_id },
    { "delete", 1, "index", "remove entry", delete },
    { "delete_extra", 3, "index extra_idx flags", "remove extra field", delete_extra },
    { "delete_extra_by_id", 4, "index extra_id extra_index flags", "remove extra field of type extra_id", delete_extra_by_id },
    { "get_archive_comment", 0, "", "show archive comment", get_archive_comment },
    { "get_extra", 3, "index extra_index flags", "show extra field", get_extra },
    { "get_extra_by_id", 4, "index extra_id extra_index flags", "show extra field of type extra_id", get_extra_by_id },
    { "get_file_comment", 1, "index", "get file comment", get_file_comment },
    { "rename", 2, "index name", "rename entry", zrename },
    { "replace_file_contents", 2, "index data", "replace entry with data", replace_file_contents },
    { "set_extra", 5, "index extra_id extra_index flags value", "set extra field", set_extra },
    { "set_file_comment", 2, "index comment", "set file comment", set_file_comment },
    { "set_file_compression", 3, "index method flags", "set file compression method", set_file_compression },
    { "set_file_mtime", 2, "index timestamp", "set file modification time", set_file_mtime },
    { "stat", 1, "index", "print information about entry", zstat },
    { "zin_close", 0, "", "close input zip_source (for internal tests)", zin_close }
};

int
dispatch(int argc, char *argv[])
{
    unsigned int i;
    for (i=0; i<sizeof(dispatch_table)/sizeof(dispatch_table_t); i++) {
	if (strcmp(dispatch_table[i].cmdline_name, argv[0]) == 0) {
	    argc--;
	    argv++;
	    /* 1 for the command, argument_count for the arguments */
	    if (argc < dispatch_table[i].argument_count) {
		fprintf(stderr, "not enough arguments for command '%s': %d available, %d needed\n", dispatch_table[i].cmdline_name, argc, dispatch_table[i].argument_count);
		return -1;
	    }
	    if (dispatch_table[i].function(argc, argv) == 0)
		return 1 + dispatch_table[i].argument_count;
	    return -1;
	}
    }

    fprintf(stderr, "unknown command '%s'\n", argv[0]);
    return -1;
}


void
usage(const char *progname)
{
    unsigned int i;
    fprintf(stderr, "usage: %s [-cemnt] archive command1 [args] [command2 [args] ...]\n\n"
	    "Supported options are:\n"
	    "\t-c\tcheck consistency\n"
	    "\t-e\terror if archive already exists (only useful with -n)\n"
	    "\t-g\tguess file name encoding (for stat)\n"
	    "\t-m\tread archive into memory, and modify there; write out at end\n"
	    "\t-n\tcreate archive if it doesn't exist (default)\n"
	    "\t-r\tprint raw file name encoding without translation (for stat)\n"
	    "\t-s\tfollow file name convention strictly (for stat)\n"
	    "\t-t\tdisregard current archive contents, if any\n", progname);
    fprintf(stderr, "\nSupported commands and arguments are:\n");
    for (i=0; i<sizeof(dispatch_table)/sizeof(dispatch_table_t); i++) {
	fprintf(stderr, "\t%s %s -- %s\n", dispatch_table[i].cmdline_name, dispatch_table[i].arg_names, dispatch_table[i].description);
    }
    fprintf(stderr, "\nThe index is zero-based.\n");
    exit(1);
}

int
main(int argc, char *argv[])
{
    const char *archive;
    zip_source_t *memory_src;
    char buf[100];
    int c, arg, err, flags, in_memory;
    const char *prg;

    flags = 0;
    in_memory = 0;
    prg = argv[0];

    if (argc < 2)
	usage(prg);

    while ((c=getopt(argc, argv, "cegmnrst")) != -1) {
	switch (c) {
	case 'c':
	    flags |= ZIP_CHECKCONS;
	    break;
	case 'e':
	    flags |= ZIP_EXCL;
	    break;
	case 'g':
	    stat_flags = ZIP_FL_ENC_GUESS;
	    break;
	case 'm':
	    in_memory = 1;
	    break;
	case 'n':
	    flags |= ZIP_CREATE;
	    break;
	case 'r':
	    stat_flags = ZIP_FL_ENC_RAW;
	    break;
	case 's':
	    stat_flags = ZIP_FL_ENC_STRICT;
	    break;
	case 't':
	    flags |= ZIP_TRUNCATE;
	    break;

	default:
	    usage(prg);
	}
    }
    
    arg = optind;

    archive = argv[arg++];

    if (flags == 0)
	flags = ZIP_CREATE;

    if (in_memory) {
	za = read_to_memory(archive, flags, &err, &memory_src);
    } else {
	za = zip_open(archive, flags, &err);
    }
    if (za == NULL) {
	zip_error_to_str(buf, sizeof(buf), err, errno);
	fprintf(stderr, "can't open zip archive '%s': %s\n", archive, buf);
	return 1;
    }

    err = 0;
    while (arg < argc) {
	int ret;
	ret = dispatch(argc-arg, argv+arg);
	if (ret > 0) {
	    arg += ret;
	} else {
	    err = 1;
	    break;
	}
    }

    if (zip_close(za) == -1) {
	fprintf(stderr, "can't close zip archive '%s': %s\n", archive, zip_strerror(za));
	return 1;
    }
    if (in_memory) {
	if (write_memory_src_to_file(archive, memory_src) < 0) {
	    err = 1;
	}
	zip_source_free(memory_src);
    }

    return err;
}
