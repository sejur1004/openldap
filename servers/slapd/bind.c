/* bind.c - decode an ldap bind operation and pass it to a backend db */
/* $OpenLDAP$ */
/* This work is part of OpenLDAP Software <http://www.openldap.org/>.
 *
 * Copyright 1998-2005 The OpenLDAP Foundation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP
 * Public License.
 *
 * A copy of this license is available in the file LICENSE in the
 * top-level directory of the distribution or, alternatively, at
 * <http://www.OpenLDAP.org/license.html>.
 */
/* Portions Copyright (c) 1995 Regents of the University of Michigan.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of Michigan at Ann Arbor. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 */

#include "portable.h"

#include <stdio.h>

#include <ac/string.h>
#include <ac/socket.h>

#include "slap.h"
#ifdef LDAP_SLAPI
#include "slapi/slapi.h"
#endif


int
do_bind(
    Operation	*op,
    SlapReply	*rs )
{
	BerElement *ber = op->o_ber;
	ber_int_t version;
	ber_tag_t method;
	struct berval mech = BER_BVNULL;
	struct berval dn = BER_BVNULL;
	ber_tag_t tag;
	Backend *be = NULL;

	Debug( LDAP_DEBUG_TRACE, "do_bind\n", 0, 0, 0 );

	/*
	 * Force to connection to "anonymous" until bind succeeds.
	 */
	ldap_pvt_thread_mutex_lock( &op->o_conn->c_mutex );
	if ( op->o_conn->c_sasl_bind_in_progress ) {
		be = op->o_conn->c_authz_backend;
	}
	if ( !BER_BVISEMPTY( &op->o_conn->c_dn ) ) {
		/* log authorization identity demotion */
		Statslog( LDAP_DEBUG_STATS,
			"%s BIND anonymous mech=implicit ssf=0\n",
			op->o_log_prefix, 0, 0, 0, 0 );
	}
	connection2anonymous( op->o_conn );
	if ( op->o_conn->c_sasl_bind_in_progress ) {
		op->o_conn->c_authz_backend = be;
	}
	ldap_pvt_thread_mutex_unlock( &op->o_conn->c_mutex );
	if ( !BER_BVISNULL( &op->o_dn ) ) {
		/* NOTE: temporarily wasting few bytes
		 * (until bind is completed), but saving
		 * a couple of ch_free() and ch_strdup("") */ 
		op->o_dn.bv_val[0] = '\0';
		op->o_dn.bv_len = 0;
	}
	if ( !BER_BVISNULL( &op->o_ndn ) ) {
		op->o_ndn.bv_val[0] = '\0';
		op->o_ndn.bv_len = 0;
	}

	/*
	 * Parse the bind request.  It looks like this:
	 *
	 *	BindRequest ::= SEQUENCE {
	 *		version		INTEGER,		 -- version
	 *		name		DistinguishedName,	 -- dn
	 *		authentication	CHOICE {
	 *			simple		[0] OCTET STRING -- passwd
	 *			krbv42ldap	[1] OCTET STRING
	 *			krbv42dsa	[2] OCTET STRING
	 *			SASL		[3] SaslCredentials
	 *		}
	 *	}
	 *
	 *	SaslCredentials ::= SEQUENCE {
	 *		mechanism	    LDAPString,
	 *		credentials	    OCTET STRING OPTIONAL
	 *	}
	 */

	tag = ber_scanf( ber, "{imt" /*}*/, &version, &dn, &method );

	if ( tag == LBER_ERROR ) {
		Debug( LDAP_DEBUG_ANY, "bind: ber_scanf failed\n", 0, 0, 0 );
		send_ldap_discon( op, rs, LDAP_PROTOCOL_ERROR, "decoding error" );
		rs->sr_err = SLAPD_DISCONNECT;
		goto cleanup;
	}

	op->o_protocol = version;
	op->orb_method = method;

	if( op->orb_method != LDAP_AUTH_SASL ) {
		tag = ber_scanf( ber, /*{*/ "m}", &op->orb_cred );

	} else {
		tag = ber_scanf( ber, "{m" /*}*/, &mech );

		if ( tag != LBER_ERROR ) {
			ber_len_t len;
			tag = ber_peek_tag( ber, &len );

			if ( tag == LDAP_TAG_LDAPCRED ) { 
				tag = ber_scanf( ber, "m", &op->orb_cred );
			} else {
				tag = LDAP_TAG_LDAPCRED;
				BER_BVZERO( &op->orb_cred );
			}

			if ( tag != LBER_ERROR ) {
				tag = ber_scanf( ber, /*{{*/ "}}" );
			}
		}
	}

	if ( tag == LBER_ERROR ) {
		send_ldap_discon( op, rs, LDAP_PROTOCOL_ERROR, "decoding error" );
		rs->sr_err = SLAPD_DISCONNECT;
		goto cleanup;
	}

	if( get_ctrls( op, rs, 1 ) != LDAP_SUCCESS ) {
		Debug( LDAP_DEBUG_ANY, "do_bind: get_ctrls failed\n", 0, 0, 0 );
		goto cleanup;
	} 

	/* We use the tmpmemctx here because it speeds up normalization.
	 * However, we must dup with regular malloc when storing any
	 * resulting DNs in the op or conn structures.
	 */
	rs->sr_err = dnPrettyNormal( NULL, &dn, &op->o_req_dn, &op->o_req_ndn,
		op->o_tmpmemctx );
	if ( rs->sr_err != LDAP_SUCCESS ) {
		Debug( LDAP_DEBUG_ANY, "bind: invalid dn (%s)\n",
			dn.bv_val, 0, 0 );
		send_ldap_error( op, rs, LDAP_INVALID_DN_SYNTAX, "invalid DN" );
		goto cleanup;
	}

	if( op->orb_method == LDAP_AUTH_SASL ) {
		Debug( LDAP_DEBUG_TRACE, "do_sasl_bind: dn (%s) mech %s\n",
			op->o_req_dn.bv_val, mech.bv_val, NULL );

	} else {
		Debug( LDAP_DEBUG_TRACE,
			"do_bind: version=%ld dn=\"%s\" method=%ld\n",
			(unsigned long) version, op->o_req_dn.bv_val,
			(unsigned long) op->orb_method );
	}

	Statslog( LDAP_DEBUG_STATS, "%s BIND dn=\"%s\" method=%ld\n",
	    op->o_log_prefix, op->o_req_dn.bv_val,
		(unsigned long) op->orb_method, 0, 0 );

	if ( version < LDAP_VERSION_MIN || version > LDAP_VERSION_MAX ) {
		Debug( LDAP_DEBUG_ANY, "do_bind: unknown version=%ld\n",
			(unsigned long) version, 0, 0 );
		send_ldap_error( op, rs, LDAP_PROTOCOL_ERROR,
			"requested protocol version not supported" );
		goto cleanup;

	} else if (!( global_allows & SLAP_ALLOW_BIND_V2 ) &&
		version < LDAP_VERSION3 )
	{
		send_ldap_error( op, rs, LDAP_PROTOCOL_ERROR,
			"historical protocol version requested, use LDAPv3 instead" );
		goto cleanup;
	}

	/*
	 * we set connection version regardless of whether bind succeeds or not.
	 */
	ldap_pvt_thread_mutex_lock( &op->o_conn->c_mutex );
	op->o_conn->c_protocol = version;
	ldap_pvt_thread_mutex_unlock( &op->o_conn->c_mutex );

	op->orb_tmp_mech = mech;

	op->o_bd = frontendDB;
	rs->sr_err = frontendDB->be_bind( op, rs );

cleanup:
	if ( rs->sr_err == LDAP_SUCCESS ) {
		if ( op->orb_method != LDAP_AUTH_SASL ) {
			ber_dupbv( &op->o_conn->c_authmech, &mech );
		}
		op->o_conn->c_authtype = op->orb_method;
	}

	op->o_conn->c_sasl_bindop = NULL;

	if( !BER_BVISNULL( &op->o_req_dn ) ) {
		slap_sl_free( op->o_req_dn.bv_val, op->o_tmpmemctx );
		BER_BVZERO( &op->o_req_dn );
	}
	if( !BER_BVISNULL( &op->o_req_ndn ) ) {
		slap_sl_free( op->o_req_ndn.bv_val, op->o_tmpmemctx );
		BER_BVZERO( &op->o_req_ndn );
	}

	return rs->sr_err;
}

int
fe_op_bind( Operation *op, SlapReply *rs )
{
	struct berval	mech = op->orb_tmp_mech;

	/* check for inappropriate controls */
	if( get_manageDSAit( op ) == SLAP_CONTROL_CRITICAL ) {
		send_ldap_error( op, rs,
			LDAP_UNAVAILABLE_CRITICAL_EXTENSION,
			"manageDSAit control inappropriate" );
		goto cleanup;
	}

	/* Set the bindop for the benefit of in-directory SASL lookups */
	op->o_conn->c_sasl_bindop = op;

	if ( op->orb_method == LDAP_AUTH_SASL ) {
		if ( op->o_protocol < LDAP_VERSION3 ) {
			Debug( LDAP_DEBUG_ANY, "do_bind: sasl with LDAPv%ld\n",
				(unsigned long)op->o_protocol, 0, 0 );
			send_ldap_discon( op, rs,
				LDAP_PROTOCOL_ERROR, "SASL bind requires LDAPv3" );
			rs->sr_err = SLAPD_DISCONNECT;
			goto cleanup;
		}

		if( BER_BVISNULL( &mech ) || BER_BVISEMPTY( &mech ) ) {
			Debug( LDAP_DEBUG_ANY,
				"do_bind: no sasl mechanism provided\n",
				0, 0, 0 );
			send_ldap_error( op, rs, LDAP_AUTH_METHOD_NOT_SUPPORTED,
				"no SASL mechanism provided" );
			goto cleanup;
		}

		/* check restrictions */
		if( backend_check_restrictions( op, rs, &mech ) != LDAP_SUCCESS ) {
			send_ldap_result( op, rs );
			goto cleanup;
		}

		ldap_pvt_thread_mutex_lock( &op->o_conn->c_mutex );
		if ( op->o_conn->c_sasl_bind_in_progress ) {
			if( !bvmatch( &op->o_conn->c_sasl_bind_mech, &mech ) ) {
				/* mechanism changed between bind steps */
				slap_sasl_reset(op->o_conn);
			}
		} else {
			ber_dupbv(&op->o_conn->c_sasl_bind_mech, &mech);
		}
		ldap_pvt_thread_mutex_unlock( &op->o_conn->c_mutex );

		rs->sr_err = slap_sasl_bind( op, rs );

		ldap_pvt_thread_mutex_lock( &op->o_conn->c_mutex );
		if( rs->sr_err == LDAP_SUCCESS ) {
			ber_dupbv(&op->o_conn->c_dn, &op->orb_edn);
			if( !BER_BVISEMPTY( &op->orb_edn ) ) {
				/* edn is always normalized already */
				ber_dupbv( &op->o_conn->c_ndn, &op->o_conn->c_dn );
			}
			op->o_tmpfree( op->orb_edn.bv_val, op->o_tmpmemctx );
			BER_BVZERO( &op->orb_edn );
			op->o_conn->c_authmech = op->o_conn->c_sasl_bind_mech;
			BER_BVZERO( &op->o_conn->c_sasl_bind_mech );
			op->o_conn->c_sasl_bind_in_progress = 0;

			op->o_conn->c_sasl_ssf = op->orb_ssf;
			if( op->orb_ssf > op->o_conn->c_ssf ) {
				op->o_conn->c_ssf = op->orb_ssf;
			}

			if( !BER_BVISEMPTY( &op->o_conn->c_dn ) ) {
				ber_len_t max = sockbuf_max_incoming_auth;
				ber_sockbuf_ctrl( op->o_conn->c_sb,
					LBER_SB_OPT_SET_MAX_INCOMING, &max );
			}

			/* log authorization identity */
			Statslog( LDAP_DEBUG_STATS,
				"%s BIND dn=\"%s\" mech=%s ssf=%d\n",
				op->o_log_prefix,
				BER_BVISNULL( &op->o_conn->c_dn ) ? "<empty>" : op->o_conn->c_dn.bv_val,
				op->o_conn->c_authmech.bv_val, op->orb_ssf, 0 );

			Debug( LDAP_DEBUG_TRACE,
				"do_bind: SASL/%s bind: dn=\"%s\" ssf=%d\n",
				op->o_conn->c_authmech.bv_val,
				BER_BVISNULL( &op->o_conn->c_dn ) ? "<empty>" : op->o_conn->c_dn.bv_val,
				op->orb_ssf );

		} else if ( rs->sr_err == LDAP_SASL_BIND_IN_PROGRESS ) {
			op->o_conn->c_sasl_bind_in_progress = 1;

		} else {
			if ( !BER_BVISNULL( &op->o_conn->c_sasl_bind_mech ) ) {
				free( op->o_conn->c_sasl_bind_mech.bv_val );
				BER_BVZERO( &op->o_conn->c_sasl_bind_mech );
			}
			op->o_conn->c_sasl_bind_in_progress = 0;
		}

#ifdef LDAP_SLAPI
#define	pb	op->o_pb
		/*
		 * Normally post-operation plugins are called only after the
		 * backend operation. Because the front-end performs SASL
		 * binds on behalf of the backend, we'll make a special
		 * exception to call the post-operation plugins after a
		 * SASL bind.
		 */
		if ( pb ) {
			slapi_int_pblock_set_operation( pb, op );
			slapi_pblock_set( pb, SLAPI_BIND_TARGET, (void *)op->o_req_dn.bv_val );
			slapi_pblock_set( pb, SLAPI_BIND_METHOD, (void *)op->orb_method );
			slapi_pblock_set( pb,
				SLAPI_BIND_CREDENTIALS, (void *)&op->orb_cred );
			slapi_pblock_set( pb, SLAPI_MANAGEDSAIT, (void *)(0) );
			(void) slapi_int_call_plugins( op->o_bd,
				SLAPI_PLUGIN_POST_BIND_FN, pb );
		}
#endif /* LDAP_SLAPI */

		ldap_pvt_thread_mutex_unlock( &op->o_conn->c_mutex );

		goto cleanup;

	} else {
		/* Not SASL, cancel any in-progress bind */
		ldap_pvt_thread_mutex_lock( &op->o_conn->c_mutex );

		if ( !BER_BVISNULL( &op->o_conn->c_sasl_bind_mech ) ) {
			free( op->o_conn->c_sasl_bind_mech.bv_val );
			BER_BVZERO( &op->o_conn->c_sasl_bind_mech );
		}
		op->o_conn->c_sasl_bind_in_progress = 0;

		slap_sasl_reset( op->o_conn );
		ldap_pvt_thread_mutex_unlock( &op->o_conn->c_mutex );
	}

	if ( op->orb_method == LDAP_AUTH_SIMPLE ) {
		BER_BVSTR( &mech, "SIMPLE" );
		/* accept "anonymous" binds */
		if ( BER_BVISEMPTY( &op->orb_cred ) || BER_BVISEMPTY( &op->o_req_ndn ) ) {
			rs->sr_err = LDAP_SUCCESS;

			if( !BER_BVISEMPTY( &op->orb_cred ) &&
				!( global_allows & SLAP_ALLOW_BIND_ANON_CRED ))
			{
				/* cred is not empty, disallow */
				rs->sr_err = LDAP_INVALID_CREDENTIALS;

			} else if ( !BER_BVISEMPTY( &op->o_req_ndn ) &&
				!( global_allows & SLAP_ALLOW_BIND_ANON_DN ))
			{
				/* DN is not empty, disallow */
				rs->sr_err = LDAP_UNWILLING_TO_PERFORM;
				rs->sr_text =
					"unauthenticated bind (DN with no password) disallowed";

			} else if ( global_disallows & SLAP_DISALLOW_BIND_ANON ) {
				/* disallow */
				rs->sr_err = LDAP_INAPPROPRIATE_AUTH;
				rs->sr_text = "anonymous bind disallowed";

			} else {
				backend_check_restrictions( op, rs, &mech );
			}

			/*
			 * we already forced connection to "anonymous",
			 * just need to send success
			 */
			send_ldap_result( op, rs );
			Debug( LDAP_DEBUG_TRACE, "do_bind: v%d anonymous bind\n",
				op->o_protocol, 0, 0 );
			goto cleanup;

		} else if ( global_disallows & SLAP_DISALLOW_BIND_SIMPLE ) {
			/* disallow simple authentication */
			rs->sr_err = LDAP_UNWILLING_TO_PERFORM;
			rs->sr_text = "unwilling to perform simple authentication";

			send_ldap_result( op, rs );
			Debug( LDAP_DEBUG_TRACE,
				"do_bind: v%d simple bind(%s) disallowed\n",
				op->o_protocol, op->o_req_ndn.bv_val, 0 );
			goto cleanup;
		}

#ifdef LDAP_API_FEATURE_X_OPENLDAP_V2_KBIND
	} else if ( op->orb_method == LDAP_AUTH_KRBV41 ) {
		if ( global_disallows & SLAP_DISALLOW_BIND_KRBV4 ) {
			/* disallow krbv4 authentication */
			rs->sr_err = LDAP_UNWILLING_TO_PERFORM;
			rs->sr_text = "unwilling to perform Kerberos V4 bind";

			send_ldap_result( op, rs );

			Debug( LDAP_DEBUG_TRACE,
				"do_bind: v%d Kerberos V4 (step 1) bind refused\n",
				op->o_protocol, 0, 0 );
			goto cleanup;
		}
		BER_BVSTR( &mech, "KRBV4" );

	} else if ( op->orb_method == LDAP_AUTH_KRBV42 ) {
		rs->sr_err = LDAP_AUTH_METHOD_NOT_SUPPORTED;
		rs->sr_text = "Kerberos V4 (step 2) bind not supported";
		send_ldap_result( op, rs );

		Debug( LDAP_DEBUG_TRACE,
			"do_bind: v%d Kerberos V4 (step 2) bind refused\n",
			op->o_protocol, 0, 0 );
		goto cleanup;
#endif

	} else {
		rs->sr_err = LDAP_AUTH_METHOD_NOT_SUPPORTED;
		rs->sr_text = "unknown authentication method";

		send_ldap_result( op, rs );
		Debug( LDAP_DEBUG_TRACE,
			"do_bind: v%d unknown authentication method (%ld)\n",
			op->o_protocol, op->orb_method, 0 );
		goto cleanup;
	}

	/*
	 * We could be serving multiple database backends.  Select the
	 * appropriate one, or send a referral to our "referral server"
	 * if we don't hold it.
	 */

	if ( (op->o_bd = select_backend( &op->o_req_ndn, 0, 0 )) == NULL ) {
		/* don't return referral for bind requests */
		/* noSuchObject is not allowed to be returned by bind */
		rs->sr_err = LDAP_INVALID_CREDENTIALS;
		send_ldap_result( op, rs );
		goto cleanup;
	}

	/* check restrictions */
	if( backend_check_restrictions( op, rs, NULL ) != LDAP_SUCCESS ) {
		send_ldap_result( op, rs );
		goto cleanup;
	}

#ifdef LDAP_SLAPI
	if ( pb ) {
		int rc;
		slapi_int_pblock_set_operation( pb, op );
		slapi_pblock_set( pb, SLAPI_BIND_TARGET, (void *)op->o_req_dn.bv_val );
		slapi_pblock_set( pb, SLAPI_BIND_METHOD, (void *)op->orb_method );
		slapi_pblock_set( pb, SLAPI_BIND_CREDENTIALS, (void *)&op->orb_cred );
		slapi_pblock_set( pb, SLAPI_MANAGEDSAIT, (void *)(0) );
		slapi_pblock_set( pb, SLAPI_CONN_DN, (void *)(0) );

		rc = slapi_int_call_plugins( op->o_bd, SLAPI_PLUGIN_PRE_BIND_FN, pb );

		Debug(LDAP_DEBUG_TRACE,
			"do_bind: Bind preoperation plugin returned %d.\n",
			rs->sr_err, 0, 0);

		switch ( rc ) {
		case SLAPI_BIND_SUCCESS:
			/* Continue with backend processing */
			break;
		case SLAPI_BIND_FAIL:
			/* Failure, server sends result */
			rs->sr_err = LDAP_INVALID_CREDENTIALS;
			send_ldap_result( op, rs );
			goto cleanup;
			break;
		case SLAPI_BIND_ANONYMOUS:
			/* SLAPI_BIND_ANONYMOUS is undocumented XXX */
		default:
			/* Authoritative, plugin sent result, or no plugins called. */
			if ( slapi_pblock_get( op->o_pb, SLAPI_RESULT_CODE,
				(void *)&rs->sr_err) != 0 )
			{
				rs->sr_err = LDAP_OTHER;
			}

			BER_BVZERO( &op->orb_edn );

			if ( rs->sr_err == LDAP_SUCCESS ) {
				slapi_pblock_get( pb, SLAPI_CONN_DN,
					(void *)&op->orb_edn.bv_val );
				if ( BER_BVISNULL( &op->orb_edn ) ) {
					if ( rc == 1 ) {
						/* No plugins were called; continue. */
						break;
					}
				} else {
					op->orb_edn.bv_len = strlen( op->orb_edn.bv_val );
				}
				rs->sr_err = dnPrettyNormal( NULL, &op->orb_edn,
					&op->o_req_dn, &op->o_req_ndn, op->o_tmpmemctx );
				ldap_pvt_thread_mutex_lock( &op->o_conn->c_mutex );
				ber_dupbv(&op->o_conn->c_dn, &op->o_req_dn);
				ber_dupbv(&op->o_conn->c_ndn, &op->o_req_ndn);
				op->o_tmpfree( op->o_req_dn.bv_val, op->o_tmpmemctx );
				BER_BVZERO( &op->o_req_dn );
				op->o_tmpfree( op->o_req_ndn.bv_val, op->o_tmpmemctx );
				BER_BVZERO( &op->o_req_ndn );
				if ( !BER_BVISEMPTY( &op->o_conn->c_dn ) ) {
					ber_len_t max = sockbuf_max_incoming_auth;
					ber_sockbuf_ctrl( op->o_conn->c_sb,
						LBER_SB_OPT_SET_MAX_INCOMING, &max );
				}
				/* log authorization identity */
				Statslog( LDAP_DEBUG_STATS,
					"%s BIND dn=\"%s\" mech=%s (SLAPI) ssf=0\n",
					op->o_log_prefix,
					BER_BVISNULL( &op->o_conn->c_dn )
						? "<empty>" : op->o_conn->c_dn.bv_val,
					mech.bv_val, 0, 0 );
				ldap_pvt_thread_mutex_unlock( &op->o_conn->c_mutex );
			}
			goto cleanup;
			break;
		}
	}
#endif /* LDAP_SLAPI */

	if( op->o_bd->be_bind ) {
		rs->sr_err = (op->o_bd->be_bind)( op, rs );

		if ( rs->sr_err == 0 ) {
			ldap_pvt_thread_mutex_lock( &op->o_conn->c_mutex );

			if( op->o_conn->c_authz_backend == NULL ) {
				op->o_conn->c_authz_backend = op->o_bd;
			}

			/* be_bind returns regular/global edn */
			if( !BER_BVISEMPTY( &op->orb_edn ) ) {
				op->o_conn->c_dn = op->orb_edn;
			} else {
				ber_dupbv(&op->o_conn->c_dn, &op->o_req_dn);
			}

			ber_dupbv( &op->o_conn->c_ndn, &op->o_req_ndn );

			if( !BER_BVISEMPTY( &op->o_conn->c_dn ) ) {
				ber_len_t max = sockbuf_max_incoming_auth;
				ber_sockbuf_ctrl( op->o_conn->c_sb,
					LBER_SB_OPT_SET_MAX_INCOMING, &max );
			}

			/* log authorization identity */
			Statslog( LDAP_DEBUG_STATS,
				"%s BIND dn=\"%s\" mech=%s ssf=0\n",
				op->o_log_prefix,
				op->o_conn->c_dn.bv_val, mech.bv_val, 0, 0 );

			Debug( LDAP_DEBUG_TRACE,
				"do_bind: v%d bind: \"%s\" to \"%s\"\n",
				op->o_protocol, op->o_req_dn.bv_val, op->o_conn->c_dn.bv_val );

			ldap_pvt_thread_mutex_unlock( &op->o_conn->c_mutex );

			/* send this here to avoid a race condition */
			send_ldap_result( op, rs );

		} else if ( !BER_BVISNULL( &op->orb_edn ) ) {
			free( op->orb_edn.bv_val );
		}

	} else {
		send_ldap_error( op, rs, LDAP_UNWILLING_TO_PERFORM,
			"operation not supported within naming context" );
	}

#ifdef LDAP_SLAPI
	if ( pb != NULL &&
		slapi_int_call_plugins( op->o_bd, SLAPI_PLUGIN_POST_BIND_FN, pb ) < 0 )
	{
		Debug(LDAP_DEBUG_TRACE,
			"do_bind: Bind postoperation plugins failed.\n",
			0, 0, 0);
	}
#endif /* LDAP_SLAPI */

cleanup:;
	return rs->sr_err;
}

