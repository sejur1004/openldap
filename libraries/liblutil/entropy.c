/* $OpenLDAP$ */
/*
 * Copyright 1998-2002 The OpenLDAP Foundation, All Rights Reserved.
 * COPYING RESTRICTIONS APPLY, see COPYRIGHT file
 */

#include "portable.h"

#include <ac/string.h>
#include <ac/time.h>
#include <ac/unistd.h>

#ifdef HAVE_PROCESS_H
#include <process.h>
#endif

#include <fcntl.h>

#include <lutil.h>
#include <lutil_md5.h>

/*
 * lutil_entropy() provides nbytes of entropy in buf.
 * Quality offerred is suitable for one-time uses, such as "once" keys.
 * Values may not be suitable for multi-time uses.
 *
 * Note:  Callers are encouraged to provide additional bytes of
 * of entropy in the buf argument.  This information is used in
 * fallback mode to improve the quality of bytes returned.
 *
 * This routinue should be extended to support additional sources
 * of entropy.
 */
int lutil_entropy( unsigned char *buf, ber_len_t nbytes )
{
	if( nbytes == 0 ) return 0;

#ifdef URANDOM_DEVICE
	/* Linux and *BSD offer a urandom device */
	{
		int rc, fd;

		fd = open( URANDOM_DEVICE, O_RDONLY );

		if( fd < 0 ) return -1;

		rc = read( fd, buf, nbytes );
		close(fd);

		/* should return nbytes */
		if( rc < nbytes ) return -1;

		return 0;
	}
#elif PROV_RSA_FULL
	{
		/* Not used since _WIN32_WINNT not set... */
		HCRYPTPROV hProv = 0;

		/* Get handle to user default provider */
		if(!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, 0)) {
		   return -1;
		}

		/* Generate random initialization vector */
		if(!CryptGenRandom(hProv, (DWORD) nbytes, (BYTE *) buf)) {
		   return -1;
		}

		/* Release provider handle */
		if(hProv != 0) CryptReleaseContext(hProv, 0);

		return 0;
	}
#else
	{
		/* based upon Phil Karn's "practical randomness" idea
		 * but implementation 100% OpenLDAP.  So don't blame Phil.
		 *
		 * Worse case is that this is a MD5 hash of a counter, if
		 * MD5 is a strong cryptographic hash, this should be fairly
		 * resistant to attack
		 */

		/*
		 * the caller may need to provide external synchronization OR
		 * provide entropy (in buf) to ensure quality results as
		 * access to this counter may not be atomic.
		 */
		static int counter = 0;
		ber_len_t n;

		struct rdata_s {
			int counter;

			unsigned char *buf;
			struct rdata_s *stack;

			pid_t	pid;

#ifdef HAVE_GETTIMEOFDAY
			struct timeval tv;
#else
			time_t	time;
#endif

			unsigned long	junk;	/* purposely not initialized */
		} rdata;

		/* make sure rdata differs for each process */
		rdata.pid = getpid();

		/* make sure rdata differs for each program */
		rdata.buf = buf;
		rdata.stack = &rdata;

		for( n = 0; n < nbytes; n += 16 ) {
			struct lutil_MD5Context ctx;
			char digest[16];

			/* poor resolution */
#ifdef HAVE_GETTIMEOFDAY
			(void) gettimeofday( &rdata.tv, NULL );
#else
			(void) time( &rdata.time );
#endif

			/* make sure rdata differs */
			rdata.counter = ++counter;
			rdata.pid++;
			rdata.junk++;

			lutil_MD5Init( &ctx );
			lutil_MD5Update( &ctx, (unsigned char *) &rdata, sizeof( rdata ) );

			/* allow caller to provided additional entropy */
			lutil_MD5Update( &ctx, buf, nbytes );

			lutil_MD5Final( digest, &ctx );

			AC_MEMCPY( &buf[n], digest,
				nbytes - n >= 16 ? 16 : nbytes - n );
		}

		return 0;
	}
#endif
	return -1;
}
