/* $OpenLDAP$ */
/*
 * Copyright 1998-2002 The OpenLDAP Foundation, All Rights Reserved.
 * COPYING RESTRICTIONS APPLY, see COPYRIGHT file
 */
/*
 * OpenLDAP DN API Test
 *      Written by: Pierangelo Masarati <ando@OpenLDAP.org>
 *
 * This program is designed to test the ldap_str2dn/ldap_dn2str
 * functions
 */
#include "portable.h"

#include <ac/stdlib.h>
#include <ac/string.h>
#include <ac/unistd.h>

#include <stdio.h>

#include <ldap.h>

#include "ldap-int.h"

#include "ldif.h"
#include "lutil.h"
#include "lutil_ldap.h"
#include "ldap_defaults.h"

int
main( int argc, char *argv[] )
{
	int 		rc, i, debug = 0, f2 = 0;
	unsigned 	flags[ 2 ] = { 0U, LDAP_DN_FORMAT_LDAPV3 };
	char		*strin, *str, *str2, buf[ 1024 ];
	LDAPDN		*dn, *dn2 = NULL;

	while ( 1 ) {
		int opt = getopt( argc, argv, "d:" );

		if ( opt == EOF ) {
			break;
		}

		switch ( opt ) {
		case 'd':
			debug = atoi( optarg );
			break;
		}
	}

	optind--;
	argc -= optind;
	argv += optind;

	if ( argc < 2 ) {
		fprintf( stderr, "usage: dntest <dn> [flags-in[,...]] [flags-out[,...]]\n\n" );
		fprintf( stderr, "\tflags-in:   V3,V2,DCE,<flags>\n" );
		fprintf( stderr, "\tflags-out:  V3,V2,UFN,DCE,AD,<flags>\n\n" );
		fprintf( stderr, "\t<flags>: PRETTY,PEDANTIC,NOSPACES,NOONESPACE\n\n" );
		return( 0 );
	}

	if ( ber_set_option( NULL, LBER_OPT_DEBUG_LEVEL, &debug ) != LBER_OPT_SUCCESS ) {
		fprintf( stderr, "Could not set LBER_OPT_DEBUG_LEVEL %d\n", debug );
	}
	if ( ldap_set_option( NULL, LDAP_OPT_DEBUG_LEVEL, &debug ) != LDAP_OPT_SUCCESS ) {
		fprintf( stderr, "Could not set LDAP_OPT_DEBUG_LEVEL %d\n", debug );
	}

	if ( strcmp( argv[ 1 ], "-" ) == 0 ) {
		size_t len;
		
		fgets( buf, sizeof( buf ), stdin );
		len = strlen( buf ) - 1;
		if ( len >= 0 && buf[ len ] == '\n' ) {
			buf[ len ] = '\0';
		}
		strin = buf;
	} else {
		strin = argv[ 1 ];
	}

	if ( argc >= 3 ) {
		for ( i = 0; i < argc - 2; i++ ) {
			char *s, *e;
			for ( s = argv[ 2 + i ]; s; s = e ) {
				e = strchr( s, ',' );
				if ( e != NULL ) {
					e[ 0 ] = '\0';
					e++;
				}
	
				if ( !strcasecmp( s, "V3" ) ) {
					flags[ i ] |= LDAP_DN_FORMAT_LDAPV3;
				} else if ( !strcasecmp( s, "V2" ) ) {
					flags[ i ] |= LDAP_DN_FORMAT_LDAPV2;
				} else if ( !strcasecmp( s, "DCE" ) ) {
					flags[ i ] |= LDAP_DN_FORMAT_DCE;
				} else if ( !strcasecmp( s, "UFN" ) ) {
					flags[ i ] |= LDAP_DN_FORMAT_UFN;
				} else if ( !strcasecmp( s, "AD" ) ) {
					flags[ i ] |= LDAP_DN_FORMAT_AD_CANONICAL;
				} else if ( !strcasecmp( s, "PRETTY" ) ) {
					flags[ i ] |= LDAP_DN_PRETTY;
				} else if ( !strcasecmp( s, "PEDANTIC" ) ) {
					flags[ i ] |= LDAP_DN_PEDANTIC;
				} else if ( !strcasecmp( s, "NOSPACES" ) ) {
					flags[ i ] |= LDAP_DN_P_NOLEADTRAILSPACES;
				} else if ( !strcasecmp( s, "NOONESPACE" ) ) {
					flags[ i ] |= LDAP_DN_P_NOSPACEAFTERRDN;
				}
			}
		}
	}

	f2 = 1;

	rc = ldap_str2dn( strin, &dn, flags[ 0 ] );

	if ( rc == LDAP_SUCCESS ) {
		int i;
		if( dn ) {
			for ( i = 0; dn[i]; i++ ) {
				LDAPRDN		*rdn = dn[ 0 ][ i ];
				char		*rstr;

				if ( ldap_rdn2str( rdn, &rstr, flags[ f2 ] ) ) {
					fprintf( stdout, "\tldap_rdn2str() failed\n" );
					continue;
				}

				fprintf( stdout, "\tldap_rdn2str() = \"%s\"\n", rstr );
				ldap_memfree( rstr );
			}
		} else {
			fprintf( stdout, "\tempty DN\n" );
		}
	}

	if ( rc == LDAP_SUCCESS &&
		ldap_dn2str( dn, &str, flags[ f2 ] ) == LDAP_SUCCESS )
	{
		char	**values, *tmp, *tmp2;
		int	n;
		
		fprintf( stdout, "\nldap_dn2str(ldap_str2dn(\"%s\"))\n"
				"\t= \"%s\"\n", strin, str );
			
		switch ( flags[ f2 ] & LDAP_DN_FORMAT_MASK ) {
		case LDAP_DN_FORMAT_UFN:
		case LDAP_DN_FORMAT_AD_CANONICAL:
			return( 0 );

		case LDAP_DN_FORMAT_LDAPV3:
		case LDAP_DN_FORMAT_LDAPV2:
			tmp = ldap_dn2ufn( strin );
			fprintf( stdout, "\nldap_dn2ufn(\"%s\")\n"
					"\t= \"%s\"\n", strin, tmp );
			ldap_memfree( tmp );
			tmp = ldap_dn2dcedn( strin );
			fprintf( stdout, "\nldap_dn2dcedn(\"%s\")\n"
					"\t= \"%s\"\n", strin, tmp );
			tmp2 = ldap_dcedn2dn( tmp );
			fprintf( stdout, "\nldap_dcedn2dn(\"%s\")\n"
					"\t= \"%s\"\n", tmp, tmp2 );
			ldap_memfree( tmp );
			ldap_memfree( tmp2 );
			tmp = ldap_dn2ad_canonical( strin );
			fprintf( stdout, "\nldap_dn2ad_canonical(\"%s\")\n"
					"\t= \"%s\"\n", strin, tmp );
			ldap_memfree( tmp );

			fprintf( stdout, "\nldap_explode_dn(\"%s\"):\n", str );
			values = ldap_explode_dn( str, 0 );
			for ( n = 0; values && values[ n ]; n++ ) {
				char	**vv;
				int	nn;
				
				fprintf( stdout, "\t\"%s\"\n", values[ n ] );

				fprintf( stdout, "\tldap_explode_rdn(\"%s\")\n",
						values[ n ] );
				vv = ldap_explode_rdn( values[ n ], 0 );
				for ( nn = 0; vv && vv[ nn ]; nn++ ) {
					fprintf( stdout, "\t\t'%s'\n", 
							vv[ nn ] );
				}
				LDAP_VFREE( vv );

				fprintf( stdout, "\tldap_explode_rdn(\"%s\")"
					       " (no types)\n", values[ n ] );
				vv = ldap_explode_rdn( values[ n ], 1 );
				for ( nn = 0; vv && vv[ nn ]; nn++ ) {
					fprintf( stdout, "\t\t\t\"%s\"\n", 
							vv[ nn ] );
				}
				LDAP_VFREE( vv );
				
			}
			LDAP_VFREE( values );

			fprintf( stdout, "\nldap_explode_dn(\"%s\")"
					" (no types):\n", str );
			values = ldap_explode_dn( str, 1 );
			for ( n = 0; values && values[ n ]; n++ ) {
				fprintf( stdout, "\t\"%s\"\n", values[ n ] );
			}
			LDAP_VFREE( values );

			break;
		}
	
		rc = ldap_str2dn( str, &dn2, flags[ f2 ] );
		if ( rc == LDAP_SUCCESS && 
				ldap_dn2str( dn2, &str2, flags[ f2 ] )
				== LDAP_SUCCESS ) {
			int 	iRDN;
			
			fprintf( stdout, "\n\"%s\"\n\t == \"%s\" ? %s\n", 
				str, str2, 
				strcmp( str, str2 ) == 0 ? "yes" : "no" );

			if(( dn != NULL && dn2 == NULL )
				|| ( dn != NULL && dn2 == NULL ))
			{
				fprintf( stdout, "mismatch\n" );
			} else if (( dn != NULL ) && (dn2 != NULL))
				for ( iRDN = 0; dn[ 0 ][ iRDN ] && dn2[ 0 ][ iRDN ]; iRDN++ )
			{
				LDAPRDN 	*r = dn[ 0 ][ iRDN ];
				LDAPRDN 	*r2 = dn2[ 0 ][ iRDN ];
				int 		iAVA;
				
				for ( iAVA = 0; r[ 0 ][ iAVA ] && r2[ 0 ][ iAVA ]; iAVA++ ) {
					LDAPAVA		*a = r[ 0 ][ iAVA ];
					LDAPAVA		*a2 = r2[ 0 ][ iAVA ];

					if ( a->la_attr.bv_len != a2->la_attr.bv_len
						|| memcmp( a->la_attr.bv_val, a2->la_attr.bv_val,
							a->la_attr.bv_len )
						|| a->la_flags != a2->la_flags
						|| a->la_value.bv_len != a2->la_value.bv_len
						|| memcmp( a->la_value.bv_val, a2->la_value.bv_val,
							a->la_value.bv_len ) )
					{
						fprintf( stdout, "mismatch\n" );
					}
				}
			}
			
			ldap_dnfree( dn2 );
			ldap_memfree( str2 );
		}
		ldap_memfree( str );
	}
	ldap_dnfree( dn );

	/* note: dn is not freed */

	return( 0 );
}

