#ifndef _HAD_ZIPINT_H
#define _HAD_ZIPINT_H

/*
  $NiH: zipint.h,v 1.22 2003/10/06 22:44:07 dillo Exp $

  zipint.h -- internal declarations.
  Copyright (C) 1999, 2003 Dieter Baron and Thomas Klausner

  This file is part of libzip, a library to manipulate ZIP archives.
  The authors can be contacted at <nih@giga.or.at>

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



#define MAXCOMLEN        65536
#define EOCDLEN             22
#define BUFSIZE       (MAXCOMLEN+EOCDLEN)
#define LOCAL_MAGIC   "PK\3\4"
#define CENTRAL_MAGIC "PK\1\2"
#define EOCD_MAGIC    "PK\5\6"
#define DATADES_MAGIC "PK\7\8"
#define CDENTRYSIZE         46
#define LENTRYSIZE          30



enum zip_state { ZIP_ST_UNCHANGED, ZIP_ST_DELETED, ZIP_ST_REPLACED,
		 ZIP_ST_ADDED, ZIP_ST_RENAMED };

struct zip_error {
    int zip_err;	/* libzip error code (ZERR_*) */
    int sys_err;	/* copy of errno (E*) or zlib error code */
    char *str;		/* string representation or NULL */
};

struct zip {
    char *zn;			/* file name */
    FILE *zp;			/* file */
    struct zip_error error;	/* error information */
    unsigned int cd_size;	/* size of central direcotry (?) */
    unsigned int cd_offset;	/* offset of central directory in file (?) */
    unsigned short comlen;	/* length of com */
    char *com;			/* zip archvie comment (?) */
    int nentry;			/* number of entries */
    int nentry_alloc;		/* number of entries allocated */
    struct zip_entry *entry;	/* entries */
    int nfile;			/* number of opened files within archvie */
    int nfile_alloc;		/* numbere of files allocated */
    struct zip_file **file;	/* opened files within archvie */
};

/* file in zip file */

struct zip_file {
    struct zip *zf;		/* zip archive containing this file */
    char *name;			/* name of file */
    struct zip_error error;	/* error information */
    int flags;			/* -1: eof, >0: error */

    int method;			/* compression method (?) */
    long fpos;			/* position within zip file (fread/fwrite) */
    unsigned long bytes_left;	/* number of bytes left to read */
    unsigned long cbytes_left;  /* number of bytes of compressed data left */
    
    unsigned long crc;		/* crc so far */
    unsigned long crc_orig;	/* CRC recorded in archive */
    
    char *buffer;
    z_stream *zstr;
};

/* meta info about zip entry */

struct zip_meta {
    unsigned short version_made, version_need, bitflags, comp_method,
	disknrstart, int_attr;
    time_t last_mod;
    unsigned int crc, comp_size, uncomp_size, ext_attr, local_offset;
    unsigned short ef_len, lef_len, fc_len;
    unsigned char *ef, *lef, *fc;
};

/* entry in zip archive directory */

struct zip_entry {
    struct zip_meta *meta;
    char *fn;			/* file name */
    char *fn_old;		/* original file name (NULL if unchanged) */
    unsigned int file_fnlen;

    enum zip_state state;
    zip_read_func ch_func;
    void *ch_data;
    int ch_flags;		/* 1: data returned by ch_func is compressed */
    time_t ch_mtime;
    int ch_comp_method;
};



extern const char * const _zip_err_str[];
extern const int _zip_nerr_str;
extern const int _zip_err_type[];



void _zip_entry_init(struct zip *, int);
void _zip_error_copy(struct zip_error *, struct zip_error *);
void _zip_error_fini(struct zip_error *);
void _zip_error_get(struct zip_error *, int *, int *);
void _zip_error_init(struct zip_error *);
void _zip_error_set(struct zip_error *, int, int);
const char *_zip_error_strerror(struct zip_error *);
int _zip_file_fillbuf(void *, size_t, struct zip_file *);
void _zip_free(struct zip *);
int _zip_free_entry(struct zip_entry *);
void _zip_free_meta(struct zip_meta *);
int _zip_local_header_read(struct zip *, int);
void *_zip_memdup(const void *, int);
int _zip_merge_meta(struct zip_meta *, struct zip_meta *);
int _zip_merge_meta_fix(struct zip_meta *, struct zip_meta *);
struct zip *_zip_new(int *);
struct zip_entry *_zip_new_entry(struct zip *);
struct zip_meta *_zip_new_meta(void);
int _zip_readcdentry(FILE *, struct zip_entry *, unsigned char **, 
		     int, int, int);
int _zip_replace(struct zip *, int, const char *,zip_read_func, void *, int);
int _zip_replace_data(struct zip *, int, const char *,
		      const void *, off_t, int);
int _zip_replace_file(struct zip *, int, const char *,
		      const char *, off_t, off_t);
int _zip_replace_filep(struct zip *, int, const char *, FILE *, off_t, off_t);
int _zip_replace_zip(struct zip *zf, int idx, const char *name,
		     struct zip *srczf, int srcidx, off_t start, off_t len);
int _zip_set_name(struct zip *, int, const char *);
int _zip_unchange(struct zip_entry *);
int _zip_unchange_data(struct zip_entry *);

#endif /* zipint.h */

