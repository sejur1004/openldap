/* search.c - search operation */
/* $OpenLDAP$ */
/*
 * Copyright 1998-2002 The OpenLDAP Foundation, All Rights Reserved.
 * COPYING RESTRICTIONS APPLY, see COPYRIGHT file
 */

#include "portable.h"

#include <stdio.h>
#include <ac/string.h>

#include "back-bdb.h"
#include "idl.h"
#include "external.h"

static int base_candidate(
	BackendDB	*be,
	Entry	*e,
	ID		*ids );
static int search_candidates(
	BackendDB *be,
	Operation *op,
	Entry *e,
	Filter *filter,
	int scope,
	int deref,
	ID	*ids );

int
bdb_search(
	BackendDB	*be,
	Connection	*conn,
	Operation	*op,
	struct berval	*base,
	struct berval	*nbase,
	int		scope,
	int		deref,
	int		slimit,
	int		tlimit,
	Filter	*filter,
	struct berval	*filterstr,
	AttributeName	*attrs,
	int		attrsonly )
{
	struct bdb_info *bdb = (struct bdb_info *) be->be_private;
	int		rc;
	const char *text = NULL;
	time_t		stoptime;
	ID		id, cursor;
	ID		candidates[BDB_IDL_UM_SIZE];
	Entry		*e = NULL;
	BerVarray v2refs = NULL;
	Entry	*matched = NULL;
	struct berval	realbase = { 0, NULL };
	int		nentries = 0;
	int		manageDSAit;

#ifdef LDAP_CLIENT_UPDATE
	Filter lcupf, csnfnot, csnfeq, csnfand, csnfge;
	AttributeAssertion aa_ge, aa_eq;
	int		entry_count = 0;
	struct berval entrycsn_bv = { 0, NULL };
	struct berval latest_entrycsn_bv = { 0, NULL };
#endif /* LDAP_CLIENT_UPDATE */

	struct slap_limits_set *limit = NULL;
	int isroot = 0;

	u_int32_t	locker;
	DB_LOCK		lock;

#ifdef NEW_LOGGING
	LDAP_LOG ( OPERATION, ENTRY, "bdb_back_search\n", 0, 0, 0 );
#else
	Debug( LDAP_DEBUG_TRACE, "=> bdb_back_search\n",
		0, 0, 0);
#endif

#ifdef LDAP_CLIENT_UPDATE
	if ( op->o_clientupdate_type & SLAP_LCUP_PERSIST ) {
		bdb_add_psearch_spec( be, conn, op, base, base, scope,
			deref, slimit, tlimit, filter, filterstr, attrs, attrsonly );
		return LDAP_SUCCESS;
	}
#endif


	manageDSAit = get_manageDSAit( op );

	rc = LOCK_ID (bdb->bi_dbenv, &locker );
	switch(rc) {
	case 0:
		break;
	default:
		send_ldap_result( conn, op, rc=LDAP_OTHER,
			NULL, "internal error", NULL, NULL );
		return rc;
	}

	if ( nbase->bv_len == 0 ) {
		/* DIT root special case */
		e = (Entry *) &slap_entry_root;
		rc = 0;
	} else						
#ifdef BDB_ALIASES
	/* get entry with reader lock */
	if ( deref & LDAP_DEREF_FINDING ) {
		e = deref_dn_r( be, nbase-, &err, &matched, &text );

	} else
#endif
	{
dn2entry_retry:
		rc = bdb_dn2entry_r( be, NULL, nbase, &e, &matched, 0, locker, &lock );
	}

	switch(rc) {
	case DB_NOTFOUND:
	case 0:
		break;
	case LDAP_BUSY:
		if (e != NULL) {
			bdb_cache_return_entry_r(bdb->bi_dbenv, &bdb->bi_cache, e, &lock);
		}
		if (matched != NULL) {
			bdb_cache_return_entry_r(bdb->bi_dbenv, &bdb->bi_cache, matched, &lock);
		}
		send_ldap_result( conn, op, LDAP_BUSY,
			NULL, "ldap server busy", NULL, NULL );
		LOCK_ID_FREE (bdb->bi_dbenv, locker );
		return LDAP_BUSY;
	case DB_LOCK_DEADLOCK:
	case DB_LOCK_NOTGRANTED:
		goto dn2entry_retry;
	default:
		if (e != NULL) {
			bdb_cache_return_entry_r(bdb->bi_dbenv, &bdb->bi_cache, e, &lock);
		}
		if (matched != NULL) {
			bdb_cache_return_entry_r(bdb->bi_dbenv, &bdb->bi_cache, matched, &lock);
		}
		send_ldap_result( conn, op, rc=LDAP_OTHER,
			NULL, "internal error", NULL, NULL );
		LOCK_ID_FREE (bdb->bi_dbenv, locker );
		return rc;
	}

	if ( e == NULL ) {
		struct berval matched_dn = { 0, NULL };
		BerVarray refs = NULL;

		if ( matched != NULL ) {
			BerVarray erefs;
			ber_dupbv( &matched_dn, &matched->e_name );

			erefs = is_entry_referral( matched )
				? get_entry_referrals( be, conn, op, matched )
				: NULL;

			bdb_cache_return_entry_r (bdb->bi_dbenv, &bdb->bi_cache, matched, &lock);
			matched = NULL;

			if( erefs ) {
				refs = referral_rewrite( erefs, &matched_dn,
					base, scope );
				ber_bvarray_free( erefs );
			}

		} else {
			refs = referral_rewrite( default_referral,
				NULL, base, scope );
		}

		send_ldap_result( conn, op,	rc=LDAP_REFERRAL ,
			matched_dn.bv_val, text, refs, NULL );

		LOCK_ID_FREE (bdb->bi_dbenv, locker );
		if ( refs ) ber_bvarray_free( refs );
		if ( matched_dn.bv_val ) ber_memfree( matched_dn.bv_val );
		return rc;
	}

	if (!manageDSAit && e != &slap_entry_root && is_entry_referral( e ) ) {
		/* entry is a referral, don't allow add */
		struct berval matched_dn;
		BerVarray erefs, refs;
		
		ber_dupbv( &matched_dn, &e->e_name );
		erefs = get_entry_referrals( be, conn, op, e );
		refs = NULL;

		bdb_cache_return_entry_r( bdb->bi_dbenv, &bdb->bi_cache, e, &lock );
		e = NULL;

		if( erefs ) {
			refs = referral_rewrite( erefs, &matched_dn,
				base, scope );
			ber_bvarray_free( erefs );
		}

#ifdef NEW_LOGGING
		LDAP_LOG ( OPERATION, RESULTS, 
			"bdb_search: entry is referral\n", 0, 0, 0 );
#else
		Debug( LDAP_DEBUG_TRACE, "bdb_search: entry is referral\n",
			0, 0, 0 );
#endif

		send_ldap_result( conn, op, LDAP_REFERRAL,
			matched_dn.bv_val,
			refs ? NULL : "bad referral object",
			refs, NULL );

		LOCK_ID_FREE (bdb->bi_dbenv, locker );
		ber_bvarray_free( refs );
		ber_memfree( matched_dn.bv_val );
		return 1;
	}

	/* if not root, get appropriate limits */
	if ( be_isroot( be, &op->o_ndn ) ) {
		isroot = 1;
	} else {
		( void ) get_limits( be, &op->o_ndn, &limit );
	}

	/* The time/size limits come first because they require very little
	 * effort, so there's no chance the candidates are selected and then 
	 * the request is not honored only because of time/size constraints */

	/* if no time limit requested, use soft limit (unless root!) */
	if ( isroot ) {
		if ( tlimit == 0 ) {
			tlimit = -1;	/* allow root to set no limit */
		}

		if ( slimit == 0 ) {
			slimit = -1;
		}

	} else {
		/* if no limit is required, use soft limit */
		if ( tlimit <= 0 ) {
			tlimit = limit->lms_t_soft;

		/* if requested limit higher than hard limit, abort */
		} else if ( tlimit > limit->lms_t_hard ) {
			/* no hard limit means use soft instead */
			if ( limit->lms_t_hard == 0
					&& limit->lms_t_soft > -1
					&& tlimit > limit->lms_t_soft ) {
				tlimit = limit->lms_t_soft;

			/* positive hard limit means abort */
			} else if ( limit->lms_t_hard > 0 ) {
				send_search_result( conn, op, 
						LDAP_ADMINLIMIT_EXCEEDED,
						NULL, NULL, NULL, NULL, 0 );
				rc = 0;
				goto done;
			}
		
			/* negative hard limit means no limit */
		}
		
		/* if no limit is required, use soft limit */
		if ( slimit <= 0 ) {
			slimit = limit->lms_s_soft;

		/* if requested limit higher than hard limit, abort */
		} else if ( slimit > limit->lms_s_hard ) {
			/* no hard limit means use soft instead */
			if ( limit->lms_s_hard == 0
					&& limit->lms_s_soft > -1
					&& slimit > limit->lms_s_soft ) {
				slimit = limit->lms_s_soft;

			/* positive hard limit means abort */
			} else if ( limit->lms_s_hard > 0 ) {
				send_search_result( conn, op, 
						LDAP_ADMINLIMIT_EXCEEDED,
						NULL, NULL, NULL, NULL, 0 );
				rc = 0;	
				goto done;
			}
			
			/* negative hard limit means no limit */
		}
	}

	/* compute it anyway; root does not use it */
	stoptime = op->o_time + tlimit;

	/* select candidates */
	if ( scope == LDAP_SCOPE_BASE ) {
		rc = base_candidate( be, e, candidates );

	} else {
		BDB_IDL_ALL( bdb, candidates );
		rc = search_candidates( be, op, e, filter,
			scope, deref, candidates );
	}

	/* need normalized dn below */
	ber_dupbv( &realbase, &e->e_nname );

	/* start cursor at beginning of candidates.
	 */
	cursor = 0;

	if ( e != &slap_entry_root ) {
		bdb_cache_return_entry_r(bdb->bi_dbenv, &bdb->bi_cache, e, &lock);
	}
	e = NULL;

	if ( candidates[0] == 0 ) {
#ifdef NEW_LOGGING
		LDAP_LOG ( OPERATION, RESULTS,
			"bdb_search: no candidates\n", 0, 0, 0 );
#else
		Debug( LDAP_DEBUG_TRACE, "bdb_search: no candidates\n",
			0, 0, 0 );
#endif

		send_search_result( conn, op,
			LDAP_SUCCESS,
			NULL, NULL, NULL, NULL, 0 );

		rc = 1;
		goto done;
	}

	/* if not root and candidates exceed to-be-checked entries, abort */
	if ( !isroot && limit->lms_s_unchecked != -1 ) {
		if ( BDB_IDL_N(candidates) > (unsigned) limit->lms_s_unchecked ) {
			send_search_result( conn, op, 
					LDAP_ADMINLIMIT_EXCEEDED,
					NULL, NULL, NULL, NULL, 0 );
			rc = 1;
			goto done;
		}
	}

#ifdef LDAP_CLIENT_UPDATE
	if ( op->o_clientupdate_type & SLAP_LCUP_SYNC ) {
		lcupf.f_choice = LDAP_FILTER_AND;
		lcupf.f_and = &csnfnot;
		lcupf.f_next = NULL;

		csnfnot.f_choice = LDAP_FILTER_NOT;
		csnfnot.f_not = &csnfeq;
		csnfnot.f_next = &csnfand;

		csnfeq.f_choice = LDAP_FILTER_EQUALITY;
		csnfeq.f_ava = &aa_eq;
		csnfeq.f_av_desc = slap_schema.si_ad_entryCSN;
		ber_dupbv( &csnfeq.f_av_value, &op->o_clientupdate_state );

		csnfand.f_choice = LDAP_FILTER_AND;
		csnfand.f_and = &csnfge;
		csnfand.f_next = NULL;

		csnfge.f_choice = LDAP_FILTER_GE;
		csnfge.f_ava = &aa_ge;
		csnfge.f_av_desc = slap_schema.si_ad_entryCSN;
		ber_dupbv( &csnfge.f_av_value, &op->o_clientupdate_state );
		csnfge.f_next = filter;
	}
#endif /* LDAP_CLIENT_UPDATE */

	for ( id = bdb_idl_first( candidates, &cursor );
		id != NOID;
		id = bdb_idl_next( candidates, &cursor ) )
	{
		int		scopeok = 0;

		/* check for abandon */
		if ( op->o_abandon ) {
			rc = 0;
			goto done;
		}

		/* check time limit */
		if ( tlimit != -1 && slap_get_time() > stoptime ) {
			send_search_result( conn, op, rc = LDAP_TIMELIMIT_EXCEEDED,
				NULL, NULL, v2refs, NULL, nentries );
			goto done;
		}

id2entry_retry:
		/* get the entry with reader lock */
		rc = bdb_id2entry_r( be, NULL, id, &e, locker, &lock );

		if (rc == LDAP_BUSY) {
			send_ldap_result( conn, op, rc=LDAP_BUSY,
				NULL, "ldap server busy", NULL, NULL );
			goto done;

		} else if ( rc == DB_LOCK_DEADLOCK || rc == DB_LOCK_NOTGRANTED ) {
			goto id2entry_retry;	
		}

		if ( e == NULL ) {
			if( !BDB_IDL_IS_RANGE(candidates) ) {
				/* only complain for non-range IDLs */
#ifdef NEW_LOGGING
				LDAP_LOG ( OPERATION, RESULTS,
					"bdb_search: candidate %ld not found\n", (long) id, 0, 0);
#else
				Debug( LDAP_DEBUG_TRACE,
					"bdb_search: candidate %ld not found\n",
					(long) id, 0, 0 );
#endif
			}

			goto loop_continue;
		}

#ifdef BDB_SUBENTRIES
		if ( is_entry_subentry( e ) ) {
			if( scope != LDAP_SCOPE_BASE ) {
				if(!get_subentries_visibility( op )) {
					/* only subentries are visible */
					goto loop_continue;
				}

			} else if ( get_subentries( op ) &&
				!get_subentries_visibility( op ))
			{
				/* only subentries are visible */
				goto loop_continue;
			}

		} else if ( get_subentries_visibility( op )) {
			/* only subentries are visible */
			goto loop_continue;
		}
#endif

#ifdef BDB_ALIASES
		if ( deref & LDAP_DEREF_SEARCHING && is_entry_alias( e ) ) {
			Entry *matched;
			int err;
			const char *text;
			
			e = deref_entry_r( be, e, &err, &matched, &text );

			if( e == NULL ) {
				e = matched;
				goto loop_continue;
			}

			if( e->e_id == id ) {
				/* circular loop */
				goto loop_continue;
			}

			/* need to skip alias which deref into scope */
			if( scope & LDAP_SCOPE_ONELEVEL ) {
				struct berval	pdn;
				
				dnParent( &e->e_nname, &pdn ):
				if ( ber_bvcmp( pdn, &realbase ) ) {
					goto loop_continue;
				}

			} else if ( dnIsSuffix( &e->e_nname, &realbase ) ) {
				/* alias is within scope */
#ifdef NEW_LOGGING
				LDAP_LOG ( OPERATION, RESULTS,
					"bdb_search: \"%s\" in subtree\n", e->edn, 0, 0);
#else
				Debug( LDAP_DEBUG_TRACE,
					"bdb_search: \"%s\" in subtree\n",
					e->e_dn, 0, 0 );
#endif
				goto loop_continue;
			}

			scopeok = 1;
		}
#endif

		/*
		 * if it's a referral, add it to the list of referrals. only do
		 * this for non-base searches, and don't check the filter
		 * explicitly here since it's only a candidate anyway.
		 */
		if ( !manageDSAit && scope != LDAP_SCOPE_BASE &&
			is_entry_referral( e ) )
		{
			struct berval	dn;

			/* check scope */
			if ( !scopeok && scope == LDAP_SCOPE_ONELEVEL ) {
				if ( !be_issuffix( be, &e->e_nname ) ) {
					dnParent( &e->e_nname, &dn );
					scopeok = dn_match( &dn, &realbase );
				} else {
					scopeok = (realbase.bv_len == 0);
				}

			} else if ( !scopeok && scope == LDAP_SCOPE_SUBTREE ) {
				scopeok = dnIsSuffix( &e->e_nname, &realbase );

			} else {
				scopeok = 1;
			}

			if( scopeok ) {
				BerVarray erefs = get_entry_referrals(
					be, conn, op, e );
				BerVarray refs = referral_rewrite( erefs,
					&e->e_name, NULL,
					scope == LDAP_SCOPE_SUBTREE
						? LDAP_SCOPE_SUBTREE
						: LDAP_SCOPE_BASE );

				send_search_reference( be, conn, op,
					e, refs, NULL, &v2refs );

				ber_bvarray_free( refs );

			} else {
#ifdef NEW_LOGGING
				LDAP_LOG(OPERATION, DETAIL2, 
					"bdb_search: candidate referral %ld scope not okay\n",
					id, 0, 0 );
#else
				Debug( LDAP_DEBUG_TRACE,
					"bdb_search: candidate referral %ld scope not okay\n",
					id, 0, 0 );
#endif
			}

			goto loop_continue;
		}

		/* if it matches the filter and scope, send it */
#ifdef LDAP_CLIENT_UPDATE
		if ( op->o_clientupdate_type & SLAP_LCUP_SYNC ) {
			rc = test_filter( be, conn, op, e, &lcupf );
		} else
#endif /* LDAP_CLIENT_UPDATE */
		{
			rc = test_filter( be, conn, op, e, filter );
		}

		if ( rc == LDAP_COMPARE_TRUE ) {
			struct berval	dn;

			/* check scope */
			if ( !scopeok && scope == LDAP_SCOPE_ONELEVEL ) {
				if ( be_issuffix( be, &e->e_nname ) ) {
					scopeok = (realbase.bv_len == 0);
				} else {
					dnParent( &e->e_nname, &dn );
					scopeok = dn_match( &dn, &realbase );
				}

			} else if ( !scopeok && scope == LDAP_SCOPE_SUBTREE ) {
				scopeok = dnIsSuffix( &e->e_nname, &realbase );

			} else {
				scopeok = 1;
			}

			if ( scopeok ) {
				/* check size limit */
				if ( --slimit == -1 ) {
					bdb_cache_return_entry_r( bdb->bi_dbenv,
						&bdb->bi_cache, e, &lock );
					e = NULL;
					send_search_result( conn, op,
						rc = LDAP_SIZELIMIT_EXCEEDED, NULL, NULL,
						v2refs, NULL, nentries );
					goto done;
				}

				if (e) {
					int result;
					
#if 0	/* noop is masked SLAP_CTRL_UPDATE */
					if( op->o_noop ) {
						result = 0;
					} else
#endif
					{
#ifdef LDAP_CLIENT_UPDATE
						if ( op->o_clientupdate_type & SLAP_LCUP_SYNC ) {
							Attribute* a;
							int ret;
							int res;
							const char *text = NULL;
							LDAPControl *ctrls[2];
							struct berval *bv;

							BerElement *ber = ber_alloc_t( LBER_USE_DER );

							if ( ber == NULL ) {
#ifdef NEW_LOGGING
								LDAP_LOG ( OPERATION, RESULTS, 
									"bdb_search: ber_alloc_t failed\n",
									0, 0, 0 );
#else
								Debug( LDAP_DEBUG_TRACE,
									"bdb_search: ber_alloc_t failed\n",
									0, 0, 0 );
#endif
								send_ldap_result( conn, op, rc=LDAP_OTHER,
									NULL, "internal error", NULL, NULL );
								goto done;
							}

							entry_count++;

							ctrls[0] = ch_malloc ( sizeof ( LDAPControl ) );
							ctrls[1] = NULL;

							if ( entry_count % op->o_clientupdate_interval == 0 ) {
								/* Send cookie */
								for ( a = e->e_attrs; a != NULL; a = a->a_next ) {
									AttributeDescription *desc = a->a_desc;
									if ( desc == slap_schema.si_ad_entryCSN ) {
										ber_dupbv( &entrycsn_bv, &a->a_vals[0] );
										if ( latest_entrycsn_bv.bv_val == NULL ) {
											ber_dupbv( &latest_entrycsn_bv, &entrycsn_bv );
										} else {
											res = value_match( &ret, desc,
												desc->ad_type->sat_ordering,
												SLAP_MR_ASSERTION_SYNTAX_MATCH,
												&entrycsn_bv, &latest_entrycsn_bv, &text );
											if ( res != LDAP_SUCCESS ) {
												ret = 0;
#ifdef NEW_LOGGING
												LDAP_LOG ( OPERATION, RESULTS, 
													"bdb_search: value_match failed\n",
													0, 0, 0 );
#else
												Debug( LDAP_DEBUG_TRACE,
													"bdb_search: value_match failed\n",
													0, 0, 0 );
#endif
											}

											if ( ret > 0 ) {
												ch_free( latest_entrycsn_bv.bv_val );
												latest_entrycsn_bv.bv_val = NULL;
												ber_dupbv( &latest_entrycsn_bv,
													&entrycsn_bv );
											}
										}
									}
								}

								ber_printf( ber,
									"{bb{sON}N}",
									SLAP_LCUP_STATE_UPDATE_FALSE,
									SLAP_LCUP_ENTRY_DELETED_FALSE,
									LCUP_COOKIE_OID, &entrycsn_bv );

								ch_free( entrycsn_bv.bv_val );
								entrycsn_bv.bv_val = NULL;

							} else {
								/* Do not send cookie */
								ber_printf( ber,
									"{bbN}",
									SLAP_LCUP_STATE_UPDATE_FALSE,
									SLAP_LCUP_ENTRY_DELETED_FALSE );
							}

							ctrls[0]->ldctl_oid = LDAP_CONTROL_ENTRY_UPDATE;
							ctrls[0]->ldctl_iscritical = op->o_clientupdate;
							ret = ber_flatten( ber, &bv );

							if ( ret < 0 ) {
#ifdef NEW_LOGGING
								LDAP_LOG ( OPERATION, RESULTS, 
									"bdb_search: ber_flatten failed\n",
									0, 0, 0 );
#else
								Debug( LDAP_DEBUG_TRACE,
									"bdb_search: ber_flatten failed\n",
									0, 0, 0 );
#endif
								send_ldap_result( conn, op, rc=LDAP_OTHER,
									NULL, "internal error", NULL, NULL );
								goto done;
							}

							ber_dupbv( &ctrls[0]->ldctl_value, bv );
							
							result = send_search_entry( be, conn, op,
								e, attrs, attrsonly, ctrls);

							ch_free( ctrls[0]->ldctl_value.bv_val );
							ch_free( ctrls[0] );
							ber_free( ber, 1 );
							ber_bvfree( bv );
						} else
#endif /* LDAP_CLIENT_UPDATE */
						{
							result = send_search_entry( be, conn, op,
								e, attrs, attrsonly, NULL);
						}
					}

					switch (result) {
					case 0:		/* entry sent ok */
						nentries++;
						break;
					case 1:		/* entry not sent */
						break;
					case -1:	/* connection closed */
						bdb_cache_return_entry_r(bdb->bi_dbenv,
							&bdb->bi_cache, e, &lock);
						e = NULL;
						rc = LDAP_OTHER;
						goto done;
					}
				}
			} else {
#ifdef NEW_LOGGING
				LDAP_LOG ( OPERATION, RESULTS,
					"bdb_search: %ld scope not okay\n", (long) id, 0, 0);
#else
				Debug( LDAP_DEBUG_TRACE,
					"bdb_search: %ld scope not okay\n",
					(long) id, 0, 0 );
#endif
			}
		} else {
#ifdef NEW_LOGGING
			LDAP_LOG ( OPERATION, RESULTS,
				"bdb_search: %ld does match filter\n", (long) id, 0, 0);
#else
			Debug( LDAP_DEBUG_TRACE,
				"bdb_search: %ld does match filter\n",
				(long) id, 0, 0 );
#endif
		}

loop_continue:
		if( e != NULL ) {
			/* free reader lock */
			bdb_cache_return_entry_r( bdb->bi_dbenv,
				&bdb->bi_cache, e , &lock);
			e = NULL;
		}

		ldap_pvt_thread_yield();
	}

#ifdef LDAP_CLIENT_UPDATE
	if ( op->o_clientupdate_type & SLAP_LCUP_SYNC ) {
		int ret;
		LDAPControl *ctrls[2];
		BerElement *ber = ber_alloc_t( LBER_USE_DER );
		struct berval *bv;

		if ( ber == NULL ) {
#ifdef NEW_LOGGING
			LDAP_LOG ( OPERATION, RESULTS, 
				"bdb_search: ber_alloc_t failed\n", 0, 0, 0 );
#else
			Debug( LDAP_DEBUG_TRACE, "bdb_search: ber_alloc_t failed\n",
				0, 0, 0 );
#endif
			send_ldap_result( conn, op, rc=LDAP_OTHER,
				NULL, "internal error", NULL, NULL );
			goto done;
		}

		ctrls[0] = ch_malloc ( sizeof ( LDAPControl ) );
		ctrls[1] = NULL;

		ber_printf( ber, "{sO", LCUP_COOKIE_OID, &latest_entrycsn_bv );
		ber_printf( ber, "N}" );

		ctrls[0]->ldctl_oid = LDAP_CONTROL_CLIENT_UPDATE_DONE;
		ctrls[0]->ldctl_iscritical = op->o_clientupdate;
		ret = ber_flatten( ber, &bv );

		if ( ret < 0 ) {
#ifdef NEW_LOGGING
			LDAP_LOG ( OPERATION, RESULTS, 
				"bdb_search: ber_flatten failed\n", 0, 0, 0 );
#else
			Debug( LDAP_DEBUG_TRACE, "bdb_search: ber_flatten failed\n",
				0, 0, 0 );
#endif
			send_ldap_result( conn, op, rc=LDAP_OTHER,
				NULL, "internal error", NULL, NULL );
			goto done;
		}

		ber_dupbv( &ctrls[0]->ldctl_value, bv );

		send_search_result( conn, op,
			v2refs == NULL ? LDAP_SUCCESS : LDAP_REFERRAL,
			NULL, NULL, v2refs, ctrls, nentries );

		ch_free( latest_entrycsn_bv.bv_val );
		latest_entrycsn_bv.bv_val = NULL;
		ch_free( ctrls[0]->ldctl_value.bv_val );
		ch_free( ctrls[0] );
		ber_free( ber, 1 );
		ber_bvfree( bv );
	} else
#endif /* LDAP_CLIENT_UPDATE */
	{
		send_search_result( conn, op,
			v2refs == NULL ? LDAP_SUCCESS : LDAP_REFERRAL,
			NULL, NULL, v2refs, NULL, nentries );
	}

	rc = 0;

done:
	if( e != NULL ) {
		/* free reader lock */
		bdb_cache_return_entry_r ( bdb->bi_dbenv, &bdb->bi_cache, e, &lock );
	}

#ifdef LDAP_CLIENT_UPDATE
	if ( op->o_clientupdate_type & SLAP_LCUP_SYNC ) {
		if ( csnfeq.f_ava != NULL && csnfeq.f_av_value.bv_val != NULL ) {
			ch_free( csnfeq.f_av_value.bv_val );
		}
	
		if ( csnfge.f_ava != NULL && csnfge.f_av_value.bv_val != NULL ) {
			ch_free( csnfge.f_av_value.bv_val );
		}
	}
#endif /* LDAP_CLIENT_UPDATE */

	LOCK_ID_FREE (bdb->bi_dbenv, locker );

	if( v2refs ) ber_bvarray_free( v2refs );
	if( realbase.bv_val ) ch_free( realbase.bv_val );

	return rc;
}


static int base_candidate(
	BackendDB	*be,
	Entry	*e,
	ID		*ids )
{
#ifdef NEW_LOGGING
	LDAP_LOG ( OPERATION, ENTRY,
		"base_candidate: base: \"%s\" (0x%08lx)\n", e->e_dn, (long) e->e_id, 0);
#else
	Debug(LDAP_DEBUG_ARGS, "base_candidates: base: \"%s\" (0x%08lx)\n",
		e->e_dn, (long) e->e_id, 0);
#endif

	ids[0] = 1;
	ids[1] = e->e_id;
	return 0;
}

/* Look for "objectClass Present" in this filter.
 * Also count depth of filter tree while we're at it.
 */
static int oc_filter(
	Filter *f,
	int cur,
	int *max
)
{
	int rc = 0;

	if( cur > *max ) *max = cur;

	switch(f->f_choice) {
	case LDAP_FILTER_PRESENT:
		if (f->f_desc == slap_schema.si_ad_objectClass) {
			rc = 1;
		}
		break;

	case LDAP_FILTER_AND:
	case LDAP_FILTER_OR:
		cur++;
		for (f=f->f_and; f; f=f->f_next) {
			(void) oc_filter(f, cur, max);
		}
		break;

	default:
		break;
	}
	return rc;
}

static void search_stack_free( void *key, void *data)
{
	ch_free(data);
}

static void *search_stack(
	BackendDB *be,
	Operation *op
)
{
	struct bdb_info *bdb = (struct bdb_info *) be->be_private;
	void *ret = NULL;

	if ( op->o_threadctx ) {
		ldap_pvt_thread_pool_getkey( op->o_threadctx, search_stack,
			&ret, NULL );
	} else {
		ret = bdb->bi_search_stack;
	}

	if ( !ret ) {
		ret = ch_malloc( bdb->bi_search_stack_depth * BDB_IDL_UM_SIZE * sizeof( ID ) );
		if ( op->o_threadctx ) {
			ldap_pvt_thread_pool_setkey( op->o_threadctx, search_stack,
				ret, search_stack_free );
		} else {
			bdb->bi_search_stack = ret;
		}
	}
	return ret;
}

static int search_candidates(
	BackendDB *be,
	Operation *op,
	Entry *e,
	Filter *filter,
	int scope,
	int deref,
	ID	*ids )
{
	struct bdb_info *bdb = (struct bdb_info *) be->be_private;
	int rc, depth = 1;
	Filter		f, scopef, rf, xf;
	ID		*stack;
	AttributeAssertion aa_ref;
#ifdef BDB_SUBENTRIES
	Filter	sf;
	AttributeAssertion aa_subentry;
#endif
#ifdef BDB_ALIASES
	Filter	af;
	AttributeAssertion aa_alias;
#endif

	/*
	 * This routine takes as input a filter (user-filter)
	 * and rewrites it as follows:
	 *	(&(scope=DN)[(objectClass=subentry)]
	 *		(|[(objectClass=referral)(objectClass=alias)](user-filter))
	 */

#ifdef NEW_LOGGING
	LDAP_LOG ( OPERATION, ENTRY,
		"search_candidates: base=\"%s\" (0x%08lx) scope=%d\n", 
		e->e_dn, (long) e->e_id, scope);
#else
	Debug(LDAP_DEBUG_TRACE,
		"search_candidates: base=\"%s\" (0x%08lx) scope=%d\n",
		e->e_dn, (long) e->e_id, scope );
#endif

	xf.f_or = filter;
	xf.f_choice = LDAP_FILTER_OR;
	xf.f_next = NULL;

	/* If the user's filter uses objectClass=*,
	 * these clauses are redundant.
	 */
	if (!oc_filter(filter, 1, &depth) && !get_subentries_visibility(op) ) {
		if( !get_manageDSAit(op) ) { /* match referrals */
			struct berval bv_ref = { sizeof("referral")-1, "referral" };
			rf.f_choice = LDAP_FILTER_EQUALITY;
			rf.f_ava = &aa_ref;
			rf.f_av_desc = slap_schema.si_ad_objectClass;
			rf.f_av_value = bv_ref;
			rf.f_next = xf.f_or;
			xf.f_or = &rf;
		}

#ifdef BDB_ALIASES
		if( deref & LDAP_DEREF_SEARCHING ) { /* match aliases */
			struct berval bv_alias = { sizeof("alias")-1, "alias" };
			af.f_choice = LDAP_FILTER_EQUALITY;
			af.f_ava = &aa_alias;
			af.f_av_desc = slap_schema.si_ad_objectClass;
			af.f_av_value = bv_alias;
			af.f_next = xf.f_or;
			xf.f_or = &af;
		}
#endif
		/* We added one of these clauses, filter depth increased */
		if( xf.f_or != filter ) depth++;
	}

	f.f_next = NULL;
	f.f_choice = LDAP_FILTER_AND;
	f.f_and = &scopef;
	scopef.f_choice = scope == LDAP_SCOPE_SUBTREE
		? SLAPD_FILTER_DN_SUBTREE
		: SLAPD_FILTER_DN_ONE;
	scopef.f_dn = &e->e_nname;
	scopef.f_next = xf.f_or == filter ? filter : &xf ;
	/* Filter depth increased again, adding scope clause */
	depth++;

#ifdef BDB_SUBENTRIES
	if( get_subentries_visibility( op ) ) {
		struct berval bv_subentry = { sizeof("SUBENTRY")-1, "SUBENTRY" };
		sf.f_choice = LDAP_FILTER_EQUALITY;
		sf.f_ava = &aa_subentry;
		sf.f_av_desc = slap_schema.si_ad_objectClass;
		sf.f_av_value = bv_subentry;
		sf.f_next = scopef.f_next;
		scopef.f_next = &sf;
	}
#endif

	/* Allocate IDL stack, plus 1 more for former tmp */
	if ( depth+1 > bdb->bi_search_stack_depth ) {
		stack = ch_malloc( (depth + 1) * BDB_IDL_UM_SIZE * sizeof( ID ) );
	} else {
		stack = search_stack( be, op );
	}

	rc = bdb_filter_candidates( be, &f, ids, stack, stack+BDB_IDL_UM_SIZE );

	if ( depth+1 > bdb->bi_search_stack_depth ) {
		ch_free( stack );
	}

	if( rc ) {
#ifdef NEW_LOGGING
		LDAP_LOG ( OPERATION, DETAIL1,
			"bdb_search_candidates: failed (rc=%d)\n", rc, 0, 0  );
#else
		Debug(LDAP_DEBUG_TRACE,
			"bdb_search_candidates: failed (rc=%d)\n",
			rc, NULL, NULL );
#endif

	} else {
#ifdef NEW_LOGGING
		LDAP_LOG ( OPERATION, DETAIL1,
			"bdb_search_candidates: id=%ld first=%ld last=%ld\n",
			(long) ids[0], (long) BDB_IDL_FIRST(ids), 
			(long) BDB_IDL_LAST(ids));
#else
		Debug(LDAP_DEBUG_TRACE,
			"bdb_search_candidates: id=%ld first=%ld last=%ld\n",
			(long) ids[0],
			(long) BDB_IDL_FIRST(ids),
			(long) BDB_IDL_LAST(ids) );
#endif
	}

	return rc;
}

