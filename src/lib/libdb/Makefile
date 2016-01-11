/*
 * 4.4BSD db
 */

VERSION = 1.0

CCFLAGS = -O $(CC.DLL)

.SOURCE : hash btree db mpool recno

db $(VERSION) :LIBRARY: README \
	btree.h btreelib.h \
	bt_close.c bt_conv.c bt_debug.c bt_delete.c bt_get.c bt_open.c \
	bt_overflow.c bt_page.c bt_put.c bt_search.c bt_seq.c bt_split.c \
	bt_stack.c bt_utils.c \
	hash.h hashlib.h hsearch.h page.h \
	hash.c hash_bigkey.c hash_buf.c hash_func.c hash_log2.c hash_page.c \
	hsearch.c ndbm.c \
	db.c \
	mpool.c \
	recno.h recnolib.h \
	rec_close.c rec_delete.c rec_get.c rec_open.c rec_put.c rec_search.c \
	rec_seq.c rec_utils.c
