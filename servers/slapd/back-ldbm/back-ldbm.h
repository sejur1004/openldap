/* back-ldbm.h - ldap ldbm back-end header file */
/* $OpenLDAP$ */
/*
 * Copyright 1998-2002 The OpenLDAP Foundation, All Rights Reserved.
 * COPYING RESTRICTIONS APPLY, see COPYRIGHT file
 */

#ifndef _BACK_LDBM_H_
#define _BACK_LDBM_H_

#include "ldbm.h"

LDAP_BEGIN_DECL

#define DEFAULT_CACHE_SIZE	1000

#if defined(HAVE_BERKELEY_DB) && DB_VERSION_MAJOR >= 2
#	define DEFAULT_DBCACHE_SIZE (100 * DEFAULT_DB_PAGE_SIZE)
#else
#	define DEFAULT_DBCACHE_SIZE 100000
#endif

#define DEFAULT_DB_DIRECTORY	LDAP_RUNDIR LDAP_DIRSEP "openldap-ldbm"
#define DEFAULT_MODE		0600

#define DN_BASE_PREFIX		SLAP_INDEX_EQUALITY_PREFIX
#define DN_ONE_PREFIX	 	'%'
#define DN_SUBTREE_PREFIX 	'@'

/*
 * there is a single index for each attribute.  these prefixes ensure
 * that there is no collision among keys.
 */

/* allow PREFIX + byte for continuate number */
#define SLAP_INDEX_CONT_SIZE ( sizeof(SLAP_INDEX_CONT_PREFIX) + sizeof(unsigned char) )

#define DEFAULT_BLOCKSIZE	8192

/*
 * This structure represents an id block on disk and an id list
 * in core.
 *
 * The fields have the following meanings:
 *
 *	b_nmax	maximum number of ids in this block. if this is == ALLIDSBLOCK,
 *		then this block represents all ids.
 *	b_nids	current number of ids in use in this block.  if this
 *		is == INDBLOCK, then this block is an indirect block
 *		containing a list of other blocks containing actual ids.
 *		the list is terminated by an id of NOID.
 *	b_ids	a list of the actual ids themselves
 */

typedef ID ID_BLOCK;

#define ID_BLOCK_NMAX_OFFSET	0
#define ID_BLOCK_NIDS_OFFSET	1
#define ID_BLOCK_IDS_OFFSET		2

/* all ID_BLOCK macros operate on a pointer to a ID_BLOCK */

#define ID_BLOCK_NMAX(b)		((b)[ID_BLOCK_NMAX_OFFSET])

/* Use this macro to get the value, but not to set it.
 * By default this is identical to above.
 */
#define	ID_BLOCK_NMAXN(b)		ID_BLOCK_NMAX(b)
#define ID_BLOCK_NIDS(b)		((b)[ID_BLOCK_NIDS_OFFSET])
#define ID_BLOCK_ID(b, n)		((b)[ID_BLOCK_IDS_OFFSET+(n)])

#define ID_BLOCK_NOID(b, n)		(ID_BLOCK_ID((b),(n)) == NOID)

#define ID_BLOCK_ALLIDS_VALUE	0
#define ID_BLOCK_ALLIDS(b)		(ID_BLOCK_NMAX(b) == ID_BLOCK_ALLIDS_VALUE)

#define ID_BLOCK_INDIRECT_VALUE	0
#define ID_BLOCK_INDIRECT(b)	(ID_BLOCK_NIDS(b) == ID_BLOCK_INDIRECT_VALUE)

#ifdef USE_INDIRECT_NIDS
/*
 * Use the high bit of ID_BLOCK_NMAX to indicate an INDIRECT block, thus
 * freeing up the ID_BLOCK_NIDS to store an actual count. This allows us
 * to use binary search on INDIRECT blocks.
 */
#undef	ID_BLOCK_NMAXN
#define	ID_BLOCK_NMAXN(b)		((b)[ID_BLOCK_NMAX_OFFSET]&0x7fffffff)
#undef	ID_BLOCK_INDIRECT_VALUE
#define	ID_BLOCK_INDIRECT_VALUE	0x80000000
#undef	ID_BLOCK_INDIRECT
#define	ID_BLOCK_INDIRECT(b)	(ID_BLOCK_NMAX(b) & ID_BLOCK_INDIRECT_VALUE)

#endif	/* USE_INDIRECT_NIDS */

/* for the in-core cache of entries */
typedef struct ldbm_cache {
	int		c_maxsize;
	int		c_cursize;
	Avlnode		*c_dntree;
	Avlnode		*c_idtree;
	Entry		*c_lruhead;	/* lru - add accessed entries here */
	Entry		*c_lrutail;	/* lru - rem lru entries from here */
	ldap_pvt_thread_mutex_t	c_mutex;
} Cache;

#define CACHE_READ_LOCK		0
#define CACHE_WRITE_LOCK	1

/* for the cache of open index files */
typedef struct ldbm_dbcache {
	int		dbc_refcnt;
	int		dbc_maxids;
	int		dbc_maxindirect;
	int		dbc_dirty;
	int		dbc_flags;
	time_t	dbc_lastref;
	long	dbc_blksize;
	char	*dbc_name;
	LDBM	dbc_db;
	ldap_pvt_thread_mutex_t	dbc_write_mutex;
} DBCache;

#define MAXDBCACHE	128

struct ldbminfo {
	ID			li_nextid;
	ldap_pvt_thread_mutex_t		li_nextid_mutex;
	ldap_pvt_thread_mutex_t		li_root_mutex;
	ldap_pvt_thread_mutex_t		li_add_mutex;
	int			li_mode;
	slap_mask_t	li_defaultmask;
	char			*li_directory;
	Cache		li_cache;
	Avlnode			*li_attrs;
	int			li_dblocking;	/* lock databases */
	int			li_dbwritesync;	/* write sync */
	int			li_dbcachesize;
	DBCache		li_dbcache[MAXDBCACHE];
	ldap_pvt_thread_mutex_t		li_dbcache_mutex;
	ldap_pvt_thread_cond_t		li_dbcache_cv;
	DB_ENV			*li_dbenv;
	int			li_envdirok;
	int			li_dbsyncfreq;
	int			li_dbsyncwaitn;
	int			li_dbsyncwaitinterval;
	ldap_pvt_thread_t	li_dbsynctid;
	int			li_dbshutdown;
};

LDAP_END_DECL

#include "proto-back-ldbm.h"

#endif /* _back_ldbm_h_ */
