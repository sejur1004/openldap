/* Copyright 2004 IBM Corporation
 * All rights reserved.
 * Redisribution and use in source and binary forms, with or without
 * modification, are permitted only as authorizd by the OpenLADP
 * Public License.
 */
/* ACKNOWLEDGEMENTS
 * This work originally developed by Sang Seok Lim
 * 2004/06/18	03:20:00	slim@OpenLDAP.org
 */

#include "portable.h"
#include <ac/string.h>
#include <ac/socket.h>
#include <ldap_pvt.h>
#include "lutil.h"
#include <ldap.h>
#include "slap.h"

#include "component.h"
#include "asn.h"
#include <asn-gser.h>
#include <stdlib.h>

#include <string.h>

#ifndef SLAPD_COMP_MATCH
#define SLAPD_COMP_MATCH SLAPD_MOD_DYNAMIC
#endif

#ifdef SLAPD_COMP_MATCH
/*
 * Matching function : BIT STRING
 */
int
MatchingComponentBits ( char* oid, ComponentSyntaxInfo *csi_attr,
			ComponentSyntaxInfo *csi_assert )
{
	int rc;
        MatchingRule* mr;
        ComponentBits *a, *b;
                                                                          
        if ( oid ) {
                mr = retrieve_matching_rule(oid, (AsnTypeId)csi_attr->csi_comp_desc->cd_type_id );
                if ( mr )
                        return component_value_match( mr, csi_attr , csi_assert );
        }
        a = ((ComponentBits*)csi_attr);
        b = ((ComponentBits*)csi_assert);
	rc = ( a->value.bitLen == b->value.bitLen && 
		strncmp( a->value.bits,b->value.bits,a->value.bitLen ) == 0 );
        return rc ? LDAP_COMPARE_TRUE:LDAP_COMPARE_FALSE;
}

/*
 * Free function: BIT STRING
 */
void
FreeComponentBits ( ComponentBits* v ) {
	FreeAsnBits( &v->value );
}

/*
 * GSER Encoder : BIT STRING
 */
int
GEncComponentBits ( GenBuf *b, ComponentBits *in )
{
    if ( !in )
	return (-1);
    return GEncAsnBitsContent ( b, &in->value );
}


/*
 * GSER Decoder : BIT STRING
 */
int
GDecComponentBits ( void* mem_op, GenBuf *b, void *v, AsnLen *bytesDecoded, int mode )
{
        char* peek_head;
        int i, strLen;
        void* component_values;
        ComponentBits* k, **k2;
	GAsnBits result;

        k = (ComponentBits*) v;
                                                                          
        if ( mode & DEC_ALLOC_MODE_0 ) {
                k2 = (ComponentBits**) v;
                *k2 = (ComponentBits*) CompAlloc( mem_op, sizeof( ComponentBits ) );
		if ( !*k2 ) return LDAP_DECODING_ERROR;
                k = *k2;
        }
        
	if ( GDecAsnBitsContent ( mem_op, b, &result, bytesDecoded ) < 0 ) {
		if ( k ) CompFree( mem_op,  k );
		return LDAP_DECODING_ERROR;
	}
	k->value = result.value;

	k->comp_desc = CompAlloc( mem_op, sizeof( ComponentDesc ) );
	if ( !k->comp_desc ) {
		if ( k ) CompFree( mem_op,  k );
		return LDAP_DECODING_ERROR;
	}
	k->comp_desc->cd_gser_decoder = (gser_decoder_func*)GDecComponentBits;
	k->comp_desc->cd_ber_decoder = (ber_decoder_func*)BDecComponentBits;
	k->comp_desc->cd_free = (comp_free_func*)FreeComponentBits;
	k->comp_desc->cd_pretty = (slap_syntax_transform_func*)NULL;
	k->comp_desc->cd_validate = (slap_syntax_validate_func*)NULL;
	k->comp_desc->cd_extract_i = NULL;
	k->comp_desc->cd_type = ASN_BASIC;
	k->comp_desc->cd_type_id = BASICTYPE_BITSTRING;
	k->comp_desc->cd_all_match = (allcomponent_matching_func*)MatchingComponentBits;
 
	/* Real Decoding code need to be followed */
	return LDAP_SUCCESS;
}

/*
 * Component BER Decoder : BIT STRING
 */
int
BDecComponentBitsTag ( void* mem_op, GenBuf *b, void *v, AsnLen *bytesDecoded, int mode ) {
	return BDecComponentBits ( mem_op, b, 0, 0, v, bytesDecoded, mode|CALL_TAG_DECODER );
}

int
BDecComponentBits ( void* mem_op, GenBuf *b, AsnTag tagId, AsnLen len, void *v,
			AsnLen *bytesDecoded, int mode )
{
        char* peek_head;
        int i, strLen, rc;
        void* component_values;
        ComponentBits* k, **k2;
	AsnBits result;
                                                                          
        k = (ComponentBits*) v;
                                                                          
        if ( mode & DEC_ALLOC_MODE_0 ) {
                k2 = (ComponentBits**) v;
                *k2 = (ComponentBits*) CompAlloc( mem_op, sizeof( ComponentBits ) );
		if ( !*k2 ) return LDAP_DECODING_ERROR;
                k = *k2;
        }
        
	if ( mode & CALL_TAG_DECODER ){
		mode = mode & CALL_CONTENT_DECODER;
		rc = BDecAsnBits ( mem_op, b, &result, bytesDecoded );
	} else {
		rc = BDecAsnBitsContent ( mem_op, b, tagId, len, &result, bytesDecoded );
	}

	if ( rc < 0 ) {
		if ( k ) CompFree( mem_op,  k );
		return LDAP_DECODING_ERROR;
	}

	k->value = result;

        k->comp_desc = CompAlloc( mem_op, sizeof( ComponentDesc ) );
	if ( !k->comp_desc )  {
		if ( k ) CompFree( mem_op,  k );
		return LDAP_DECODING_ERROR;
	}
        k->comp_desc->cd_gser_decoder = (gser_decoder_func*)GDecComponentBits;
        k->comp_desc->cd_ber_decoder = (ber_decoder_func*)BDecComponentBits;
	k->comp_desc->cd_free = (comp_free_func*)FreeComponentBits;
	k->comp_desc->cd_pretty = (slap_syntax_transform_func*)NULL;
	k->comp_desc->cd_validate = (slap_syntax_validate_func*)NULL;
        k->comp_desc->cd_extract_i = NULL;
        k->comp_desc->cd_type = ASN_BASIC;
        k->comp_desc->cd_type_id = BASICTYPE_BITSTRING;
        k->comp_desc->cd_all_match = (allcomponent_matching_func*)MatchingComponentBits;
 
	return LDAP_SUCCESS;
}

/*
 * Component GSER BMPString Encoder
 */
 int
 GEncComponentBMPString ( GenBuf *b, ComponentBMPString *in )
 {
    if ( !in || in->value.octetLen <= 0 ) return (-1);
    return GEncBMPStringContent ( b, &in->value );
 }

/*
 * Component GSER BMPString Decoder
 */
int
GDecComponentBMPString ( void* mem_op, GenBuf *b, void *v, AsnLen *bytesDecoded, int mode)
{
        char* peek_head;
        int i, strLen;
        void* component_values;
        ComponentBMPString* k, **k2;
	GBMPString result;
                                                                          
        k = (ComponentBMPString*) v;
                                                                          
        if ( mode & DEC_ALLOC_MODE_0 ) {
                k2 = (ComponentBMPString**) v;
                *k2 = (ComponentBMPString*) CompAlloc( mem_op, sizeof( ComponentBMPString ) );
		if ( !*k2 ) return LDAP_DECODING_ERROR;
                k = *k2;
        }

        *bytesDecoded = 0;

	if ( GDecBMPStringContent ( mem_op, b, &result, bytesDecoded ) < 0 ) {
		if ( k ) CompFree( mem_op, k );
		return LDAP_DECODING_ERROR;
	}

	k->value = result.value;

	k->comp_desc = CompAlloc( mem_op, sizeof( ComponentDesc ) );
	if ( !k->comp_desc )  {
		if ( k ) CompFree( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
	k->comp_desc->cd_gser_decoder = (gser_decoder_func*)GDecComponentBMPString;
	k->comp_desc->cd_ber_decoder = (ber_decoder_func*)BDecComponentBMPString;
	k->comp_desc->cd_free = (comp_free_func*)FreeComponentBMPString;
	k->comp_desc->cd_pretty = (slap_syntax_transform_func*)NULL;
	k->comp_desc->cd_validate = (slap_syntax_validate_func*)NULL;
	k->comp_desc->cd_extract_i = NULL;
	k->comp_desc->cd_type = ASN_BASIC;
	k->comp_desc->cd_type_id = BASICTYPE_BMP_STR;
	k->comp_desc->cd_all_match = (allcomponent_matching_func*)MatchingComponentBMPString;
 
	return LDAP_SUCCESS;

}

/*
 * Component BER BMPString Decoder
 */
int
BDecComponentBMPStringTag ( void* mem_op, GenBuf *b, void *v, AsnLen *bytesDecoded, int mode ) {
	return BDecComponentBMPString ( mem_op, b, 0, 0, v, bytesDecoded, mode|CALL_TAG_DECODER );
}

int
BDecComponentBMPString ( void* mem_op, GenBuf *b, AsnTag tagId, AsnLen len, void *v,
			AsnLen *bytesDecoded, int mode )
{
        char* peek_head;
        int i, strLen, rc;
        void* component_values;
        ComponentBMPString* k, **k2;
	BMPString result;
                                                                          
        k = (ComponentBMPString*) v;
                                                                          
        if ( mode & DEC_ALLOC_MODE_0 ) {
                k2 = (ComponentBMPString**) v;
                *k2 = (ComponentBMPString*) CompAlloc( mem_op, sizeof( ComponentBMPString ) );
		if ( !*k2 ) return LDAP_DECODING_ERROR;
                k = *k2;
        }

	if ( mode & CALL_TAG_DECODER ){
		mode = mode & CALL_CONTENT_DECODER;
		rc = BDecBMPString ( mem_op, b, &result, bytesDecoded );
	} else {
		rc = BDecBMPStringContent ( mem_op, b, tagId, len, &result, bytesDecoded );
	}

	if ( rc < 0 ) {
		if ( k ) CompFree( mem_op, k );
		return LDAP_DECODING_ERROR;
	}

	k->value = result;

        k->comp_desc = CompAlloc( mem_op, sizeof( ComponentDesc ) );
	if ( !k->comp_desc )  {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
        k->comp_desc->cd_gser_decoder = (gser_decoder_func*)GDecComponentBMPString;
        k->comp_desc->cd_ber_decoder = (ber_decoder_func*)BDecComponentBMPString;
	k->comp_desc->cd_free = (comp_free_func*)FreeComponentBMPString;
	k->comp_desc->cd_pretty = (slap_syntax_transform_func*)NULL;
	k->comp_desc->cd_validate = (slap_syntax_validate_func*)NULL;
        k->comp_desc->cd_extract_i = NULL;
        k->comp_desc->cd_type = ASN_BASIC;
        k->comp_desc->cd_type_id = BASICTYPE_BMP_STR;
        k->comp_desc->cd_all_match = (allcomponent_matching_func*)MatchingComponentBMPString;
 
	return LDAP_SUCCESS;

}

/*
 * Component GSER Encoder : UTF8 String
 */
int
GEncComponentUTF8String ( GenBuf *b, ComponentUTF8String *in )
{
    if ( !in || in->value.octetLen <= 0 )
        return (-1);
    return GEncUTF8StringContent ( b, &in->value );
}

/*
 * Component GSER Decoder :  UTF8 String
 */
int
GDecComponentUTF8String ( void* mem_op, GenBuf *b, void *v,
				AsnLen *bytesDecoded, int mode) {
        char* peek_head;
        int i, strLen;
        void* component_values;
        ComponentUTF8String* k, **k2;
	GUTF8String result;
                                                                          
        k = (ComponentUTF8String*) v;
                                                                          
        if ( mode & DEC_ALLOC_MODE_0 ) {
                k2 = (ComponentUTF8String**) v;
                *k2 = (ComponentUTF8String*)CompAlloc( mem_op, sizeof( ComponentUTF8String ) );
		if ( !*k2 ) return LDAP_DECODING_ERROR;
                k = *k2;
        }

        *bytesDecoded = 0;

	if ( GDecUTF8StringContent ( mem_op, b, &result, bytesDecoded ) < 0 ) {
		if ( k ) CompFree( mem_op,  k );
		return LDAP_DECODING_ERROR;
	}
	
	k->value = result.value;

	k->comp_desc = CompAlloc( mem_op, sizeof( ComponentDesc ) );
	if ( !k->comp_desc )  {
		if ( k ) CompFree( mem_op,  k );
		return LDAP_DECODING_ERROR;
	}
	k->comp_desc->cd_gser_decoder = (gser_decoder_func*)GDecComponentUTF8String;
	k->comp_desc->cd_ber_decoder = (ber_decoder_func*)BDecComponentUTF8String;
	k->comp_desc->cd_free = (comp_free_func*)FreeComponentUTF8String;
	k->comp_desc->cd_pretty = (slap_syntax_transform_func*)NULL;
	k->comp_desc->cd_validate = (slap_syntax_validate_func*)NULL;
	k->comp_desc->cd_extract_i = NULL;
	k->comp_desc->cd_type = ASN_BASIC;
	k->comp_desc->cd_type_id = BASICTYPE_UTF8_STR;
	k->comp_desc->cd_all_match = (allcomponent_matching_func*)MatchingComponentUTF8String;
 
	return LDAP_SUCCESS;
}

/*
 * Component BER Decoder : UTF8String
 */
int
BDecComponentUTF8StringTag ( void* mem_op, GenBuf *b, void *v, AsnLen *bytesDecoded, int mode ) {
	return BDecComponentUTF8String ( mem_op, b, 0, 0, v, bytesDecoded, mode|CALL_TAG_DECODER );
}

int
BDecComponentUTF8String ( void* mem_op, GenBuf *b, AsnTag tagId, AsnLen len,
				void *v, AsnLen *bytesDecoded, int mode )
{
        char* peek_head;
        int i, strLen, rc;
        void* component_values;
        ComponentUTF8String* k, **k2;
	UTF8String result;
                                                                          
        k = (ComponentUTF8String*) v;
                                                                          
        if ( mode & DEC_ALLOC_MODE_0 ) {
                k2 = (ComponentUTF8String**) v;
                *k2 = (ComponentUTF8String*) CompAlloc( mem_op, sizeof( ComponentUTF8String ) );
		if ( !*k2 ) return LDAP_DECODING_ERROR;
                k = *k2;
        }
	
	if ( mode & CALL_TAG_DECODER ){
		mode = mode & CALL_CONTENT_DECODER;
		rc = BDecUTF8String ( mem_op, b, &result, bytesDecoded );
	} else {
		rc = BDecUTF8StringContent ( mem_op, b, tagId, len, &result, bytesDecoded );
	}
	if ( rc < 0 ) {
		if ( k ) CompFree( mem_op,  k );
		return LDAP_DECODING_ERROR;
	}

	k->value = result;

        k->comp_desc = CompAlloc( mem_op, sizeof( ComponentDesc ) );
	if ( !k->comp_desc )  {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
        k->comp_desc->cd_gser_decoder = (gser_decoder_func*)GDecComponentUTF8String;
        k->comp_desc->cd_ber_decoder = (ber_decoder_func*)BDecComponentUTF8String;
	k->comp_desc->cd_free = (comp_free_func*)FreeComponentUTF8String;
	k->comp_desc->cd_pretty = (slap_syntax_transform_func*)NULL;
	k->comp_desc->cd_validate = (slap_syntax_validate_func*)NULL;
        k->comp_desc->cd_extract_i = NULL;
        k->comp_desc->cd_type = ASN_BASIC;
        k->comp_desc->cd_type_id = BASICTYPE_UTF8_STR;
        k->comp_desc->cd_all_match = (allcomponent_matching_func*)MatchingComponentUTF8String;
}

/*
 * Component GSER Encoder :  Teletex String
 */
int
GEncComponentTeletexString ( GenBuf *b, ComponentTeletexString *in )
{
    if ( !in || in->value.octetLen <= 0 )
        return (-1);
    return GEncTeletexStringContent ( b, &in->value );
}

/*
 * Component GSER Decoder :  Teletex String
 */
int
GDecComponentTeletexString  ( void* mem_op, GenBuf *b, void *v,
					AsnLen *bytesDecoded, int mode) {
        char* peek_head;
        int i, strLen;
        void* component_values;
        ComponentTeletexString* k, **k2;
	GTeletexString result;
                                                                          
        k = (ComponentTeletexString*) v;
                                                                          
        if ( mode & DEC_ALLOC_MODE_0 ) {
                k2 = (ComponentTeletexString**) v;
                *k2 = (ComponentTeletexString*)CompAlloc( mem_op, sizeof( ComponentTeletexString ) );
		if ( !*k2 ) return LDAP_DECODING_ERROR;
                k = *k2;
        }

        *bytesDecoded = 0;

	if ( GDecTeletexStringContent ( mem_op, b, &result, bytesDecoded ) < 0 ) {
		if ( k ) CompFree( mem_op,  k );
		return LDAP_DECODING_ERROR;
	}

	k->value = result.value;

	k->comp_desc = CompAlloc( mem_op, sizeof( ComponentDesc ) );
	if ( !k->comp_desc )  {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
	k->comp_desc->cd_gser_decoder = (gser_decoder_func*)GDecComponentTeletexString;
	k->comp_desc->cd_ber_decoder = (ber_decoder_func*)BDecComponentTeletexString;
	k->comp_desc->cd_free = (comp_free_func*)FreeComponentTeletexString;
	k->comp_desc->cd_pretty = (slap_syntax_transform_func*)NULL;
	k->comp_desc->cd_validate = (slap_syntax_validate_func*)NULL;
	k->comp_desc->cd_extract_i = NULL;
	k->comp_desc->cd_type = ASN_BASIC;
	k->comp_desc->cd_type_id = BASICTYPE_VIDEOTEX_STR;
	k->comp_desc->cd_all_match = (allcomponent_matching_func*)MatchingComponentTeletexString;
 
	return LDAP_SUCCESS;
}


/*
 * Matching function : BOOLEAN
 */
int
MatchingComponentBool(char* oid, ComponentSyntaxInfo* csi_attr,
                        ComponentSyntaxInfo* csi_assert )
{
        MatchingRule* mr;
        ComponentBool *a, *b;
                                                                          
        if( oid ) {
                mr = retrieve_matching_rule(oid, csi_attr->csi_comp_desc->cd_type_id );
                if ( mr )
                        return component_value_match( mr, csi_attr , csi_assert );
        }

        a = ((ComponentBool*)csi_attr);
        b = ((ComponentBool*)csi_assert);

        return (a->value == b->value) ? LDAP_COMPARE_TRUE:LDAP_COMPARE_FALSE;
}

/*
 * GSER Encoder : BOOLEAN
 */
int
GEncComponentBool ( GenBuf *b, ComponentBool *in )
{
    if ( !in )
        return (-1);
    return GEncAsnBoolContent ( b, &in->value );
}

/*
 * GSER Decoder : BOOLEAN
 */
int
GDecComponentBool ( void* mem_op, GenBuf *b, void *v, AsnLen *bytesDecoded, int mode )
{
        char* peek_head;
        int i, strLen;
        ComponentBool* k, **k2;
	GAsnBool result;
                                                                          
        k = (ComponentBool*) v;
                                                                          
        if ( mode & DEC_ALLOC_MODE_0 ) {
                k2 = (ComponentBool**) v;
                *k2 = (ComponentBool*) CompAlloc( mem_op, sizeof( ComponentBool ) );
		if ( !*k2 ) return LDAP_DECODING_ERROR;
                k = *k2;
        }

	if ( GDecAsnBoolContent( mem_op, b, &result, bytesDecoded ) < 0 ) {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}

	k->value = result.value;

	k->comp_desc = CompAlloc( mem_op, sizeof( ComponentDesc ) );
	if ( !k->comp_desc )  {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
	k->comp_desc->cd_gser_decoder = (gser_decoder_func*)GDecComponentBool;
	k->comp_desc->cd_ber_decoder = (ber_decoder_func*)BDecComponentBool;
	k->comp_desc->cd_free = (comp_free_func*)NULL;
	k->comp_desc->cd_pretty = (slap_syntax_transform_func*)NULL;
	k->comp_desc->cd_validate = (slap_syntax_validate_func*)NULL;
	k->comp_desc->cd_extract_i = NULL;
	k->comp_desc->cd_type = ASN_BASIC;
	k->comp_desc->cd_type_id = BASICTYPE_BOOLEAN;
	k->comp_desc->cd_all_match = (allcomponent_matching_func*)MatchingComponentBool;
 
        return LDAP_SUCCESS;
}

/*
 * Component BER Decoder : BOOLEAN
 */
int
BDecComponentBoolTag ( void* mem_op, GenBuf *b, void *v, AsnLen *bytesDecoded, int mode ) {
	return BDecComponentBool ( mem_op, b, 0, 0, v, bytesDecoded, mode|CALL_TAG_DECODER );
}

int
BDecComponentBool ( void* mem_op, GenBuf *b, AsnTag tagId, AsnLen len, void *v,
			AsnLen *bytesDecoded, int mode )
{
        char* peek_head;
        int i, strLen, rc;
        ComponentBool* k, **k2;
	AsnBool result;
                                                                          
        k = (ComponentBool*) v;
                                                                          
        if ( mode & DEC_ALLOC_MODE_0 ) {
                k2 = (ComponentBool**) v;
                *k2 = (ComponentBool*) CompAlloc( mem_op, sizeof( ComponentBool ) );
		if ( !*k2 ) return LDAP_DECODING_ERROR;
                k = *k2;
        }

	if ( mode & CALL_TAG_DECODER ){
		mode = mode & CALL_CONTENT_DECODER;
		rc = BDecAsnBool ( mem_op, b, &result, bytesDecoded );
	} else {
		rc = BDecAsnBoolContent( mem_op, b, tagId, len, &result, bytesDecoded );
	}
	if ( rc < 0 ) {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}

	k->value = result;

        k->comp_desc = CompAlloc( mem_op, sizeof( ComponentDesc ) );
	if ( !k->comp_desc )  {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
        k->comp_desc->cd_gser_decoder = (gser_decoder_func*)GDecComponentBool;
        k->comp_desc->cd_ber_decoder = (ber_decoder_func*)BDecComponentBool;
	k->comp_desc->cd_free = (comp_free_func*)NULL;
	k->comp_desc->cd_pretty = (slap_syntax_transform_func*)NULL;
	k->comp_desc->cd_validate = (slap_syntax_validate_func*)NULL;
        k->comp_desc->cd_extract_i = NULL;
        k->comp_desc->cd_type = ASN_BASIC;
        k->comp_desc->cd_type_id = BASICTYPE_BOOLEAN;
        k->comp_desc->cd_all_match = (allcomponent_matching_func*)MatchingComponentBool;
 
        return LDAP_SUCCESS;
}

/*
 * Matching function : ENUMERATE
 */
int
MatchingComponentEnum ( char* oid, ComponentSyntaxInfo *csi_attr,
			ComponentSyntaxInfo *csi_assert )
{
        int rc;
        MatchingRule* mr;
        ComponentEnum *a, *b;
                                                                          
        if( oid ) {
                mr = retrieve_matching_rule(oid, csi_attr->csi_comp_desc->cd_type_id );
                if ( mr )
                        return component_value_match( mr, csi_attr , csi_assert );
        }
        a = ((ComponentEnum*)csi_attr);
        b = ((ComponentEnum*)csi_assert);
        rc = (a->value == b->value);
                                                                          
        return rc ? LDAP_COMPARE_TRUE:LDAP_COMPARE_FALSE;
}

/*
 * GSER Encoder : ENUMERATE
 */
int
GEncComponentEnum ( GenBuf *b, ComponentEnum *in )
{
    if ( !in )
	return (-1);
    return GEncAsnEnumContent ( b, &in->value );
}

/*
 * GSER Decoder : ENUMERATE
 */
int
GDecComponentEnum ( void* mem_op, GenBuf *b, void *v, AsnLen *bytesDecoded, int mode )
{
        char* peek_head;
        int i, strLen;
        void* component_values;
        ComponentEnum* k, **k2;
	GAsnEnum result;
                                                                          
        k = (ComponentEnum*) v;
                                                                          
        if ( mode & DEC_ALLOC_MODE_0 ) {
                k2 = (ComponentEnum**) v;
                *k2 = (ComponentEnum*) CompAlloc( mem_op, sizeof( ComponentEnum ) );
		if ( !*k2 ) return LDAP_DECODING_ERROR;
                k = *k2;
        }

	if ( GDecAsnEnumContent ( mem_op, b, &result, bytesDecoded ) < 0 ) {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}

	k->value_identifier.bv_val = result.value_identifier;
	k->value_identifier.bv_len = result.len;

	k->comp_desc = CompAlloc( mem_op, sizeof( ComponentDesc ) );
	if ( !k->comp_desc )  {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
	k->comp_desc->cd_gser_decoder = (gser_decoder_func*)GDecComponentEnum;
	k->comp_desc->cd_ber_decoder = (ber_decoder_func*)BDecComponentEnum;
	k->comp_desc->cd_free = (comp_free_func*)NULL;
	k->comp_desc->cd_pretty = (slap_syntax_transform_func*)NULL;
	k->comp_desc->cd_validate = (slap_syntax_validate_func*)NULL;
	k->comp_desc->cd_extract_i = NULL;
	k->comp_desc->cd_type = ASN_BASIC;
	k->comp_desc->cd_type_id = BASICTYPE_ENUMERATED;
	k->comp_desc->cd_all_match = (allcomponent_matching_func*)MatchingComponentEnum;

	return LDAP_SUCCESS;
}

/*
 * Component BER Decoder : ENUMERATE
 */
int
BDecComponentEnumTag ( void* mem_op, GenBuf *b, void *v, AsnLen *bytesDecoded, int mode ) {
	return BDecComponentEnum ( mem_op, b, 0, 0, v, bytesDecoded, mode|CALL_TAG_DECODER );
}

int
BDecComponentEnum ( void* mem_op, GenBuf *b, AsnTag tagId, AsnLen len, void *v,
			AsnLen *bytesDecoded, int mode )
{
        char* peek_head;
        int i, strLen, rc;
        void* component_values;
        ComponentEnum* k, **k2;
	AsnEnum result;
                                                                          
        k = (ComponentEnum*) v;
                                                                          
        if ( mode & DEC_ALLOC_MODE_0 ) {
                k2 = (ComponentEnum**) v;
                *k2 = (ComponentEnum*) CompAlloc( mem_op, sizeof( ComponentEnum ) );
		if ( k ) return LDAP_DECODING_ERROR;
                k = *k2;
        }

	if ( mode & CALL_TAG_DECODER ){
		mode = mode & CALL_CONTENT_DECODER;
		rc = BDecAsnEnum ( mem_op, b, &result, bytesDecoded );
	} else {
		rc = BDecAsnEnumContent ( mem_op, b, tagId, len, &result, bytesDecoded );
	}
	if ( rc < 0 ) {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}

	k->value = result;

        k->comp_desc = CompAlloc( mem_op, sizeof( ComponentDesc ) );
	if ( !k->comp_desc )  {
		if ( k  ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
        k->comp_desc->cd_gser_decoder = (gser_decoder_func*)GDecComponentEnum;
        k->comp_desc->cd_ber_decoder = (ber_decoder_func*)BDecComponentEnum;
	k->comp_desc->cd_free = (comp_free_func*)NULL;
	k->comp_desc->cd_pretty = (slap_syntax_transform_func*)NULL;
	k->comp_desc->cd_validate = (slap_syntax_validate_func*)NULL;
        k->comp_desc->cd_extract_i = NULL;
        k->comp_desc->cd_type = ASN_BASIC;
        k->comp_desc->cd_type_id = BASICTYPE_ENUMERATED;
        k->comp_desc->cd_all_match = (allcomponent_matching_func*)MatchingComponentEnum;

	return LDAP_SUCCESS;
}

/*
 * Component GSER Encoder : IA5String
 */
int
GEncComponentIA5Stirng ( GenBuf *b, ComponentIA5String* in )
{
    if ( !in || in->value.octetLen <= 0 ) return (-1);
    return GEncIA5StringContent( b, &in->value );
}

/*
 * Component BER Decoder : IA5String
 */
int
BDecComponentIA5StringTag ( void* mem_op, GenBuf *b, void *v, AsnLen *bytesDecoded, int mode ) {
	return BDecComponentIA5String ( mem_op, b, 0, 0, v, bytesDecoded, mode|CALL_TAG_DECODER );
}

int
BDecComponentIA5String ( void* mem_op, GenBuf *b, AsnTag tagId, AsnLen len, void *v,
			AsnLen *bytesDecoded, int mode )
{
        char* peek_head;
        int i, strLen, rc;
        void* component_values;
        ComponentIA5String* k, **k2;
	IA5String result;
                                                                          
        k = (ComponentIA5String*) v;
                                                                          
        if ( mode & DEC_ALLOC_MODE_0 ) {
                k2 = (ComponentIA5String**) v;
                *k2 = (ComponentIA5String*) CompAlloc( mem_op, sizeof( ComponentIA5String ) );
		if ( !*k2 ) return LDAP_DECODING_ERROR;
                k = *k2;
        }

	if ( mode & CALL_TAG_DECODER ){
		mode = mode & CALL_CONTENT_DECODER;
		rc = BDecIA5String ( mem_op, b, &result, bytesDecoded );
	} else {
		rc = BDecIA5StringContent ( mem_op, b, tagId, len, &result, bytesDecoded );
	}
	if ( rc < 0 ) {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}

	k->value = result;

        k->comp_desc = CompAlloc( mem_op, sizeof( ComponentDesc ) );
	if ( !k->comp_desc )  {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
        k->comp_desc->cd_gser_decoder = (gser_decoder_func*)GDecComponentIA5String;
        k->comp_desc->cd_ber_decoder = (ber_decoder_func*)BDecComponentIA5String;
	k->comp_desc->cd_free = (comp_free_func*)FreeComponentIA5String;
	k->comp_desc->cd_pretty = (slap_syntax_transform_func*)NULL;
	k->comp_desc->cd_validate = (slap_syntax_validate_func*)NULL;
        k->comp_desc->cd_extract_i = NULL;
        k->comp_desc->cd_type = ASN_BASIC;
        k->comp_desc->cd_type_id = BASICTYPE_IA5_STR;
        k->comp_desc->cd_all_match = (allcomponent_matching_func*)MatchingComponentIA5String;

	return LDAP_SUCCESS;
}

/*
 * Matching function : INTEGER
 */
int
MatchingComponentInt(char* oid, ComponentSyntaxInfo* csi_attr,
                        ComponentSyntaxInfo* csi_assert )
{
        MatchingRule* mr;
        ComponentInt *a, *b;
                                                                          
        if( oid ) {
                /* check if this ASN type's matching rule is overrided */
                mr = retrieve_matching_rule(oid, csi_attr->csi_comp_desc->cd_type_id );
                /* if existing function is overrided, call the overriding
function*/
                if ( mr )
                        return component_value_match( mr, csi_attr , csi_assert );
        }
        a = ((ComponentInt*)csi_attr);
        b = ((ComponentInt*)csi_assert);
                                                                          
        return ( a->value == b->value ) ? LDAP_COMPARE_TRUE:LDAP_COMPARE_FALSE;
}

/*
 * GSER Encoder : INTEGER
 */
int
GEncComponentInt ( GenBuf *b, ComponentInt* in )
{
    if ( !in ) return (-1);
    return GEncAsnIntContent ( b, &in->value );
}

/*
 * GSER Decoder : INTEGER 
 */
int
GDecComponentInt( void* mem_op, GenBuf * b, void *v, AsnLen *bytesDecoded, int mode)
{
        char* peek_head;
        int i, strLen;
        void* component_values;
        ComponentInt* k, **k2;
	GAsnInt result;
                                                                          
        k = (ComponentInt*) v;
                                                                          
        if ( mode & DEC_ALLOC_MODE_0 ) {
                k2 = (ComponentInt**) v;
                *k2 = (ComponentInt*) CompAlloc( mem_op, sizeof( ComponentInt ) );
		if ( !*k2 ) return LDAP_DECODING_ERROR;
                k = *k2;
        }

	if ( GDecAsnIntContent ( mem_op, b, &result, bytesDecoded ) < 0 ) {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
	k->value = result.value;

	k->comp_desc = CompAlloc( mem_op, sizeof( ComponentDesc ) );
	if ( !k->comp_desc )  {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
	k->comp_desc->cd_gser_decoder = (gser_decoder_func*)GDecComponentInt;
	k->comp_desc->cd_ber_decoder = (ber_decoder_func*)BDecComponentInt;
	k->comp_desc->cd_free = (comp_free_func*)NULL;
	k->comp_desc->cd_pretty = (slap_syntax_transform_func*)NULL;
	k->comp_desc->cd_validate = (slap_syntax_validate_func*)NULL;
	k->comp_desc->cd_extract_i = NULL;
	k->comp_desc->cd_type = ASN_BASIC;
	k->comp_desc->cd_type_id = BASICTYPE_INTEGER;
	k->comp_desc->cd_all_match = (allcomponent_matching_func*)MatchingComponentInt;

        return LDAP_SUCCESS;
}

/*
 * Component BER Decoder : INTEGER 
 */
int
BDecComponentIntTag ( void* mem_op, GenBuf *b, void *v, AsnLen *bytesDecoded, int mode ) {
	return BDecComponentInt ( mem_op, b, 0, 0, v, bytesDecoded, mode|CALL_TAG_DECODER );
}

int
BDecComponentInt ( void* mem_op, GenBuf *b, AsnTag tagId, AsnLen len, void *v,
			AsnLen *bytesDecoded, int mode )
{
        char* peek_head;
        int i, strLen, rc;
        void* component_values;
        ComponentInt* k, **k2;
	AsnInt result;
                                                                          
        k = (ComponentInt*) v;
                                                                          
        if ( mode & DEC_ALLOC_MODE_0 ) {
                k2 = (ComponentInt**) v;
                *k2 = (ComponentInt*) CompAlloc( mem_op, sizeof( ComponentInt ) );
		if ( !*k2 ) return LDAP_DECODING_ERROR;
                k = *k2;
        }

	if ( mode & CALL_TAG_DECODER ){
		mode = mode & CALL_CONTENT_DECODER;
		rc = BDecAsnInt ( mem_op, b, &result, bytesDecoded );
	} else {
		rc = BDecAsnIntContent ( mem_op, b, tagId, len, &result, bytesDecoded );
	}
	k->value = result;

        k->comp_desc = CompAlloc( mem_op, sizeof( ComponentDesc ) );
	if ( !k->comp_desc )  {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
        k->comp_desc->cd_gser_decoder = (gser_decoder_func*)GDecComponentInt;
        k->comp_desc->cd_ber_decoder = (ber_decoder_func*)BDecComponentInt;
	k->comp_desc->cd_free = (comp_free_func*)NULL;
	k->comp_desc->cd_pretty = (slap_syntax_transform_func*)NULL;
	k->comp_desc->cd_validate = (slap_syntax_validate_func*)NULL;
        k->comp_desc->cd_extract_i = NULL;
        k->comp_desc->cd_type = ASN_BASIC;
        k->comp_desc->cd_type_id = BASICTYPE_INTEGER;
        k->comp_desc->cd_all_match = (allcomponent_matching_func*)MatchingComponentInt;
        
        return LDAP_SUCCESS;
}

/*
 * Matching function : NULL
 */
int
MatchingComponentNull ( char *oid, ComponentSyntaxInfo *csi_attr,
			ComponentSyntaxInfo *csi_assert )
{
        MatchingRule* mr;
        ComponentNull *a, *b;
                                                                          
        if( oid ) {
                mr = retrieve_matching_rule(oid, csi_attr->csi_comp_desc->cd_type_id );
                if ( mr )
                        return component_value_match( mr, csi_attr , csi_assert );
        }
        a = ((ComponentNull*)csi_attr);
        b = ((ComponentNull*)csi_assert);
                                                                          
        return (a->value == b->value) ? LDAP_COMPARE_TRUE:LDAP_COMPARE_FALSE;
}

/*
 * GSER Encoder : NULL
 */
int
GEncComponentNull ( GenBuf *b, ComponentNull *in )
{
    if ( !in ) return (-1);
    return GEncAsnNullContent ( b, &in->value );
}

/*
 * GSER Decoder : NULL
 */
int
GDecComponentNull ( void* mem_op, GenBuf *b, void *v, AsnLen *bytesDecoded, int mode )
{
        char* peek_head;
        int i, strLen;
        void* component_values;
        ComponentNull* k, **k2;
	GAsnNull result;
                                                                          
        k = (ComponentNull*) v;
                                                                          
        if ( mode & DEC_ALLOC_MODE_0 ) {
                k2 = (ComponentNull**) v;
                *k2 = (ComponentNull*) CompAlloc( mem_op, sizeof( ComponentNull ) );
		if ( !*k2 ) return LDAP_DECODING_ERROR;
                k = *k2;
        }

	if ( GDecAsnNullContent ( mem_op, b, &result, bytesDecoded ) < 0 ) {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
	k->value = result.value;

	k->comp_desc = CompAlloc( mem_op, sizeof( ComponentDesc ) );
	if ( !k->comp_desc )  {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
	k->comp_desc->cd_gser_decoder = (gser_decoder_func*)GDecComponentNull;
	k->comp_desc->cd_ber_decoder = (ber_decoder_func*)BDecComponentNull;
	k->comp_desc->cd_free = (comp_free_func*)FreeComponentNull;
	k->comp_desc->cd_pretty = (slap_syntax_transform_func*)NULL;
	k->comp_desc->cd_validate = (slap_syntax_validate_func*)NULL;
	k->comp_desc->cd_extract_i = NULL;
	k->comp_desc->cd_type = ASN_BASIC;
	k->comp_desc->cd_type_id = BASICTYPE_NULL;
	k->comp_desc->cd_all_match = (allcomponent_matching_func*)MatchingComponentNull;

	return LDAP_SUCCESS;
}

/*
 * Component BER Decoder : NULL
 */
int
BDecComponentNullTag ( void* mem_op, GenBuf *b, void *v, AsnLen *bytesDecoded, int mode )
{
	return BDecComponentNull ( mem_op, b, 0, 0, v,bytesDecoded, mode|CALL_TAG_DECODER );
}

int
BDecComponentNull ( void* mem_op, GenBuf *b, AsnTag tagId, AsnLen len, void *v,
			AsnLen *bytesDecoded, int mode )
{
        char* peek_head;
        int i, strLen, rc;
        void* component_values;
        ComponentNull* k, **k2;
	AsnNull result;

        k = (ComponentNull*) v;
                                                                         
        if ( mode & DEC_ALLOC_MODE_0 ) {
                k2 = (ComponentNull**) v;
                *k2 = (ComponentNull*) CompAlloc( mem_op, sizeof( ComponentNull ) );
		if ( !*k2 ) return LDAP_DECODING_ERROR;
                k = *k2;
        }

	if ( mode & CALL_TAG_DECODER ){
		mode = mode & CALL_CONTENT_DECODER;
		rc = BDecAsnNull ( mem_op, b, &result, bytesDecoded );
	}
	else {
		rc = BDecAsnNullContent ( mem_op, b, tagId, len, &result, bytesDecoded);
	}
	if ( rc < 0 ) {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
	k->value = result;

	k->comp_desc = CompAlloc( mem_op, sizeof( ComponentDesc ) );
	if ( !k->comp_desc )  {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
	k->comp_desc->cd_gser_decoder = (gser_decoder_func*)GDecComponentNull;
	k->comp_desc->cd_ber_decoder = (ber_decoder_func*)BDecComponentNull;
	k->comp_desc->cd_free = (comp_free_func*)FreeComponentNull;
	k->comp_desc->cd_pretty = (slap_syntax_transform_func*)NULL;
	k->comp_desc->cd_validate = (slap_syntax_validate_func*)NULL;
	k->comp_desc->cd_extract_i = NULL;
	k->comp_desc->cd_type = ASN_BASIC;
	k->comp_desc->cd_type_id = BASICTYPE_NULL;
	k->comp_desc->cd_all_match = (allcomponent_matching_func*)MatchingComponentNull;
	return LDAP_SUCCESS;
}

/*
 * Component BER Decoder : NumericString
 */
int
BDecComponentNumericStringTag ( void* mem_op, GenBuf *b, void *v, AsnLen *bytesDecoded, int mode ) {
	return BDecComponentNumericString ( mem_op, b, 0, 0, v, bytesDecoded, mode|CALL_TAG_DECODER );
}

int
BDecComponentNumericString ( void* mem_op, GenBuf *b, AsnTag tagId, AsnLen len, void *v, AsnLen *bytesDecoded, int mode )
{
        char* peek_head;
        int i, strLen, rc;
        void* component_values;
        ComponentNumericString* k, **k2;
	NumericString result;

        k = (ComponentNumericString*) v;
                                                                          
        if ( mode & DEC_ALLOC_MODE_0 ) {
                k2 = (ComponentNumericString**) v;
                *k2 = (ComponentNumericString*) CompAlloc( mem_op, sizeof( ComponentNumericString ) );
		if ( !*k2 ) return LDAP_DECODING_ERROR;
                k = *k2;
        }

	if ( mode & CALL_TAG_DECODER ){
		mode = mode & CALL_CONTENT_DECODER;
		rc = BDecNumericString ( mem_op, b, &result, bytesDecoded );
	} else {
		rc = BDecNumericStringContent ( mem_op, b, tagId, len, &result, bytesDecoded);
	}
	if ( rc < 0 ) {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
	k->value = result;

        k->comp_desc = CompAlloc( mem_op, sizeof( ComponentDesc ) );
	if ( !k->comp_desc )  {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
        k->comp_desc->cd_gser_decoder = (gser_decoder_func*)GDecComponentNumericString;
        k->comp_desc->cd_ber_decoder = (ber_decoder_func*)BDecComponentNumericString;
	k->comp_desc->cd_free = (comp_free_func*)FreeComponentNumericString;
	k->comp_desc->cd_pretty = (slap_syntax_transform_func*)NULL;
	k->comp_desc->cd_validate = (slap_syntax_validate_func*)NULL;
        k->comp_desc->cd_extract_i = NULL;
        k->comp_desc->cd_type = ASN_BASIC;
        k->comp_desc->cd_type_id = BASICTYPE_NUMERIC_STR;
        k->comp_desc->cd_all_match = (allcomponent_matching_func*)MatchingComponentNumericString;

	return LDAP_SUCCESS;
}


/*
 * Free function : OCTET STRING
 */
void
FreeComponentOcts ( ComponentOcts* v) {
	FreeAsnOcts( &v->value );
}

/*
 * Matching function : OCTET STRING
 */
int
MatchingComponentOcts ( char* oid, ComponentSyntaxInfo* csi_attr,
			ComponentSyntaxInfo* csi_assert )
{
        int rc;
        MatchingRule* mr;
        ComponentOcts *a, *b;
                                                                          
        if( oid ) {
                mr = retrieve_matching_rule(oid, csi_attr->csi_comp_desc->cd_type_id );
                if ( mr )
                        return component_value_match( mr, csi_attr , csi_assert );
        }
        a = (ComponentOcts*) csi_attr;
        b = (ComponentOcts*) csi_assert;
	/* Assume that both of OCTET string has end of string character */
	if ( (a->value.octetLen == b->value.octetLen) &&
		strncmp ( a->value.octs, b->value.octs, a->value.octetLen ) == 0 )
        	return LDAP_COMPARE_TRUE;
	else
		return LDAP_COMPARE_FALSE;
}

/*
 * GSER Encoder : OCTET STRING
 */
int
GEncComponentOcts ( GenBuf* b, ComponentOcts *in )
{
    if ( !in || in->value.octetLen <= 0 )
        return (-1);
    return GEncAsnOctsContent ( b, &in->value );
}

/*
 * GSER Decoder : OCTET STRING
 */
int
GDecComponentOcts ( void* mem_op, GenBuf *b, void *v, AsnLen *bytesDecoded, int mode )
{
        char *peek_head, *data;
        int i, j, strLen;
        void* component_values;
        ComponentOcts* k, **k2;
	GAsnOcts result;
                                                                          
        k = (ComponentOcts*) v;
                                                                          
        if ( mode & DEC_ALLOC_MODE_0 ) {
                k2 = (ComponentOcts**) v;
                *k2 = (ComponentOcts*) CompAlloc( mem_op, sizeof( ComponentOcts ) );
		if ( !*k2 ) return LDAP_DECODING_ERROR;
                k = *k2;
        }

	if ( GDecAsnOctsContent ( mem_op, b, &result, bytesDecoded ) < 0 ) {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
	k->value = result.value;

	k->comp_desc = CompAlloc( mem_op, sizeof( ComponentDesc ) );
	if ( !k->comp_desc )  {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
	k->comp_desc->cd_gser_decoder = (gser_decoder_func*)GDecComponentOcts;
	k->comp_desc->cd_ber_decoder = (ber_decoder_func*)BDecComponentOcts;
	k->comp_desc->cd_free = (comp_free_func*)FreeComponentOcts;
	k->comp_desc->cd_pretty = (slap_syntax_transform_func*)NULL;
	k->comp_desc->cd_validate = (slap_syntax_validate_func*)NULL;
	k->comp_desc->cd_extract_i = NULL;
	k->comp_desc->cd_type = ASN_BASIC;
	k->comp_desc->cd_type_id = BASICTYPE_OCTETSTRING;
	k->comp_desc->cd_all_match = (allcomponent_matching_func*)MatchingComponentOcts;

	return LDAP_SUCCESS;
}

/*
 * Component BER Decoder : OCTET STRING
 */
int
BDecComponentOctsTag ( void* mem_op, GenBuf *b, void *v, AsnLen *bytesDecoded, int mode ) {
	return BDecComponentOcts ( mem_op, b, 0, 0, v, bytesDecoded, mode|CALL_TAG_DECODER );
}

int
BDecComponentOcts ( void* mem_op, GenBuf *b, AsnTag tagId, AsnLen len, void *v,
			AsnLen *bytesDecoded, int mode )
{
        char *peek_head, *data;
        int i, strLen, rc;
        void* component_values;
        ComponentOcts* k, **k2;
	AsnOcts result;
                                                                          
        k = (ComponentOcts*) v;
                                                                          
        if ( mode & DEC_ALLOC_MODE_0 ) {
                k2 = (ComponentOcts**) v;
                *k2 = (ComponentOcts*) CompAlloc( mem_op, sizeof( ComponentOcts ) );
		if ( !*k2 ) return LDAP_DECODING_ERROR;
                k = *k2;
        }

	if ( mode & CALL_TAG_DECODER ){
		mode = mode & CALL_CONTENT_DECODER;
		rc = BDecAsnOcts ( mem_op, b, &result, bytesDecoded );
	} else {
		rc = BDecAsnOctsContent ( mem_op, b, tagId, len, &result, bytesDecoded );
	}
	if ( rc < 0 ) {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
	k->value = result;

        k->comp_desc = CompAlloc( mem_op, sizeof( ComponentDesc ) );
	if ( !k->comp_desc )  {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
        k->comp_desc->cd_gser_decoder = (gser_decoder_func*)GDecComponentOcts;
        k->comp_desc->cd_ber_decoder = (ber_decoder_func*)BDecComponentOcts;
	k->comp_desc->cd_free = (comp_free_func*)FreeComponentOcts;
	k->comp_desc->cd_pretty = (slap_syntax_transform_func*)NULL;
	k->comp_desc->cd_validate = (slap_syntax_validate_func*)NULL;
        k->comp_desc->cd_extract_i = NULL;
        k->comp_desc->cd_type = ASN_BASIC;
        k->comp_desc->cd_type_id = BASICTYPE_OCTETSTRING;
        k->comp_desc->cd_all_match = (allcomponent_matching_func*)MatchingComponentOcts;
	return LDAP_SUCCESS;
}

/*
 * Matching function : OBJECT IDENTIFIER
 */
int
MatchingComponentOid ( char *oid, ComponentSyntaxInfo *csi_attr ,
			ComponentSyntaxInfo *csi_assert )
{
        int rc;
        MatchingRule* mr;
        ComponentOid *a, *b;
                                                                          
        if( oid ) {
                mr = retrieve_matching_rule(oid, csi_attr->csi_comp_desc->cd_type_id );
                if ( mr )
                        return component_value_match( mr, csi_attr , csi_assert );
        }

        a = (ComponentOid*)csi_attr;
        b = (ComponentOid*)csi_assert;
	if ( a->value.octetLen != b->value.octetLen )
		return LDAP_COMPARE_FALSE;
        rc = ( strncmp( a->value.octs, b->value.octs, a->value.octetLen ) == 0 );
                                                                          
        return rc ? LDAP_COMPARE_TRUE:LDAP_COMPARE_FALSE;
}

/*
 * GSER Encoder : OID
 */
GEncComponentOid ( GenBuf *b, ComponentOid *in )
{
    if ( !in || in->value.octetLen <= 0 ) return (-1);
    return GEncAsnOidContent( b, &in->value );
}

/*
 * GSER Decoder : OID
 */
int
GDecAsnDescOidContent ( void* mem_op, GenBuf *b, GAsnOid *result, AsnLen *bytesDecoded ){
	AttributeType *ad_type;
	struct berval name;
	char* peek_head;
	int strLen;

	strLen = LocateNextGSERToken ( mem_op, b, &peek_head, GSER_NO_COPY );
	name.bv_val = peek_head;
	name.bv_len = strLen;

	ad_type = at_bvfind( &name );

	if ( !ad_type )
		return LDAP_DECODING_ERROR;

	peek_head = ad_type->sat_atype.at_oid;
	strLen = strlen ( peek_head );

	result->value.octs = EncodeComponentOid ( mem_op, peek_head , &strLen );
	result->value.octetLen = strLen;
	return LDAP_SUCCESS;
}

int
IsNumericOid ( char* peek_head , int strLen ) {
	int i;
	int num_dot;
	for ( i = 0, num_dot = 0 ; i < strLen ; i++ ) {
		if ( peek_head[i] == '.' ) num_dot++;
		else if ( peek_head[i] > '9' || peek_head[i] < '0' )
			return (-1);
	}
	if ( num_dot )
		return (1);
	else
		return (-1);
}

int
GDecComponentOid ( void* mem_op, GenBuf *b, void *v, AsnLen *bytesDecoded, int mode )
{
        char* peek_head;
        int i, strLen, rc;
        void* component_values;
        ComponentOid* k, **k2;
	GAsnOid result;
                                                                          
        k = (ComponentOid*) v;
                                                                          
        if ( mode & DEC_ALLOC_MODE_0 ) {
                k2 = (ComponentOid**) v;
                *k2 = (ComponentOid*) CompAlloc( mem_op, sizeof( ComponentOid ) );
		if ( !*k2 ) return LDAP_DECODING_ERROR;
                k = *k2;
        }

	strLen = LocateNextGSERToken ( mem_op, b, &peek_head, GSER_PEEK );
	if ( IsNumericOid ( peek_head , strLen ) >= 1 ) {
		/* numeric-oid */
		if ( GDecAsnOidContent ( mem_op, b, &result, bytesDecoded ) < 0 ) {
			if ( k ) CompFree ( mem_op, k );
			return LDAP_DECODING_ERROR;
		}
	}
	else {
		/*descr*/
		if ( GDecAsnDescOidContent ( mem_op, b, &result, bytesDecoded ) < 0 ){
			if ( k ) CompFree ( mem_op, k );
			return LDAP_DECODING_ERROR;
		}
	}
	k->value = result.value;

	k->comp_desc = CompAlloc( mem_op, sizeof( ComponentDesc ) );
	if ( !k->comp_desc )  {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
	k->comp_desc->cd_gser_decoder = (gser_decoder_func*)GDecComponentOid;
	k->comp_desc->cd_ber_decoder = (ber_decoder_func*)BDecComponentOid;
	k->comp_desc->cd_free = (comp_free_func*)FreeComponentOid;
	k->comp_desc->cd_pretty = (slap_syntax_transform_func*)NULL;
	k->comp_desc->cd_validate = (slap_syntax_validate_func*)NULL;
	k->comp_desc->cd_extract_i = NULL;
	k->comp_desc->cd_type = ASN_BASIC;
	k->comp_desc->cd_type_id = BASICTYPE_OID;
	k->comp_desc->cd_all_match = (allcomponent_matching_func*)MatchingComponentOid;

	return LDAP_SUCCESS;
}

/*
 * Component BER Decoder : OID
 */
int
BDecComponentOidTag ( void* mem_op, GenBuf *b, void *v, AsnLen *bytesDecoded, int mode ) {
	return BDecComponentOid ( mem_op, b, 0, 0, v, bytesDecoded, mode|CALL_TAG_DECODER );
}

int
BDecComponentOid ( void* mem_op, GenBuf *b, AsnTag tagId, AsnLen len, void *v,
			AsnLen *bytesDecoded, int mode )
{
        char* peek_head;
        int i, strLen, rc;
        void* component_values;
        ComponentOid* k, **k2;
	AsnOid result;
                                                                          
        k = (ComponentOid*) v;
                                                                          
        if ( mode & DEC_ALLOC_MODE_0 ) {
                k2 = (ComponentOid**) v;
                *k2 = (ComponentOid*) CompAlloc( mem_op, sizeof( ComponentOid ) );
		if ( !*k2 ) return LDAP_DECODING_ERROR;
                k = *k2;
        }
	
	if ( mode & CALL_TAG_DECODER ){
		mode = mode & CALL_CONTENT_DECODER;
		rc = BDecAsnOid ( mem_op, b, &result, bytesDecoded );
	} else {
		rc = BDecAsnOidContent ( mem_op, b, tagId, len, &result, bytesDecoded );
	}
	if ( rc < 0 ) {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
	k->value = result;

        k->comp_desc = CompAlloc( mem_op, sizeof( ComponentDesc ) );
	if ( !k->comp_desc )  {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
        k->comp_desc->cd_gser_decoder = (gser_decoder_func*)GDecComponentOid;
        k->comp_desc->cd_ber_decoder = (ber_decoder_func*)BDecComponentOid;
	k->comp_desc->cd_free = (comp_free_func*)FreeComponentOid;
	k->comp_desc->cd_pretty = (slap_syntax_transform_func*)NULL;
	k->comp_desc->cd_validate = (slap_syntax_validate_func*)NULL;
        k->comp_desc->cd_extract_i = NULL;
        k->comp_desc->cd_type = ASN_BASIC;
        k->comp_desc->cd_type_id = BASICTYPE_OID;
        k->comp_desc->cd_all_match = (allcomponent_matching_func*)MatchingComponentOid;
	return LDAP_SUCCESS;
}

/*
 * Component BER Decoder : PrintiableString
 */

int
BDecComponentPrintableStringTag ( void* mem_op, GenBuf *b, void *v, AsnLen *bytesDecoded, int mode )
{
	return BDecComponentPrintableString ( mem_op, b, 0, 0, v, bytesDecoded, mode|CALL_TAG_DECODER );
}

int
BDecComponentPrintableString( void* mem_op, GenBuf *b, AsnTag tagId, AsnLen len, void *v, AsnLen *bytesDecoded, int mode )
{
        char* peek_head;
        int i, strLen, rc;
        void* component_values;
        ComponentPrintableString* k, **k2;
	AsnOid result;
                                                                          
        k = (ComponentPrintableString*) v;
                                                                          
        if ( mode & DEC_ALLOC_MODE_0 ) {
                k2 = (ComponentPrintableString**) v;
                *k2 = (ComponentPrintableString*) CompAlloc( mem_op, sizeof( ComponentPrintableString ) );
		if ( !*k2 ) return LDAP_DECODING_ERROR;
                k = *k2;
        }

	if ( mode & CALL_TAG_DECODER ) {
		mode = mode & CALL_CONTENT_DECODER;
		rc = BDecPrintableString ( mem_op, b, &result, bytesDecoded );
	} else {
		rc = BDecPrintableStringContent ( mem_op, b, tagId, len, &result, bytesDecoded );
	}
	if ( rc < 0 ) {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
	k->value = result;

	k->comp_desc = CompAlloc( mem_op, sizeof( ComponentDesc ) );
	if ( !k->comp_desc )  {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
	k->comp_desc->cd_gser_decoder = (gser_decoder_func*)GDecComponentPrintableString;
	k->comp_desc->cd_ber_decoder = (ber_decoder_func*)BDecComponentPrintableString;
	k->comp_desc->cd_free = (comp_free_func*)FreeComponentPrintableString;
	k->comp_desc->cd_pretty = (slap_syntax_transform_func*)NULL;
	k->comp_desc->cd_validate = (slap_syntax_validate_func*)NULL;
	k->comp_desc->cd_extract_i = NULL;
	k->comp_desc->cd_type = ASN_BASIC;
	k->comp_desc->cd_type_id = BASICTYPE_PRINTABLE_STR;
	k->comp_desc->cd_all_match = (allcomponent_matching_func*)MatchingComponentPrintableString;
	return LDAP_SUCCESS;
}

/*
 * Matching function : Real
 */
int
MatchingComponentReal (char* oid, ComponentSyntaxInfo *csi_attr,
			ComponentSyntaxInfo *csi_assert )
{
        int rc;
        MatchingRule* mr;
        ComponentReal *a, *b;
                                                                          
        if( oid ) {
                mr = retrieve_matching_rule(oid, csi_attr->csi_comp_desc->cd_type_id );
                if ( mr )
                        return component_value_match( mr, csi_attr , csi_assert );
        }
        a = (ComponentReal*)csi_attr;
        b = (ComponentReal*)csi_assert;
        rc = (a->value == b->value);
                                                                          
        return rc ? LDAP_COMPARE_TRUE:LDAP_COMPARE_FALSE;
}

/*
 * GSER Encoder : Real
 */
int
GEncComponentReal ( GenBuf *b, ComponentReal *in )
{
    if ( !in )
	return (-1);
    return GEncAsnRealContent ( b, &in->value );
}

/*
 * GSER Decoder : Real
 */
int
GDecComponentReal ( void* mem_op, GenBuf *b, void *v, AsnLen *bytesDecoded, int mode )
{
        char* peek_head;
        int i, strLen;
        void* component_values;
        ComponentReal* k, **k2;
	GAsnReal result;
                                                                          
        k = (ComponentReal*) v;
                                                                          
        if ( mode & DEC_ALLOC_MODE_0 ) {
                k2 = (ComponentReal**) v;
                *k2 = (ComponentReal*) CompAlloc( mem_op, sizeof( ComponentReal ) );
		if ( !*k2 ) return LDAP_DECODING_ERROR;
                k = *k2;
        }

	if ( GDecAsnRealContent ( mem_op, b, &result, bytesDecoded ) < 0 ) {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
	k->value = result.value;

	k->comp_desc = CompAlloc( mem_op, sizeof( ComponentDesc ) );
	if ( !k->comp_desc )  {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
	k->comp_desc->cd_gser_decoder = (gser_decoder_func*)GDecComponentReal;
	k->comp_desc->cd_ber_decoder = (ber_decoder_func*)BDecComponentReal;
	k->comp_desc->cd_free = (comp_free_func*)NULL;
	k->comp_desc->cd_pretty = (slap_syntax_transform_func*)NULL;
	k->comp_desc->cd_validate = (slap_syntax_validate_func*)NULL;
	k->comp_desc->cd_extract_i = NULL;
	k->comp_desc->cd_type = ASN_BASIC;
	k->comp_desc->cd_type_id = BASICTYPE_REAL;
	k->comp_desc->cd_all_match = (allcomponent_matching_func*)MatchingComponentReal;

        return LDAP_SUCCESS;
}

/*
 * Component BER Decoder : Real
 */
int
BDecComponentRealTag ( void* mem_op, GenBuf *b, void *v, AsnLen *bytesDecoded, int mode ) {
	return BDecComponentReal ( mem_op, b, 0, 0, v, bytesDecoded, mode|CALL_TAG_DECODER );
}

int
BDecComponentReal ( void* mem_op, GenBuf *b, AsnTag tagId, AsnLen len, void *v, AsnLen *bytesDecoded, int mode )
{
        char* peek_head;
        int i, strLen, rc;
        void* component_values;
        ComponentReal* k, **k2;
	AsnReal result;
                                                                          
        k = (ComponentReal*) v;
                                                                          
        if ( mode & DEC_ALLOC_MODE_0 ) {
                k2 = (ComponentReal**) v;
                *k2 = (ComponentReal*) CompAlloc( mem_op, sizeof( ComponentReal ) );
		if ( !*k2 ) return LDAP_DECODING_ERROR;
                k = *k2;
        }

	if ( mode & CALL_TAG_DECODER ){
		mode = mode & CALL_CONTENT_DECODER;
		rc = BDecAsnReal ( mem_op, b, &result, bytesDecoded );
	} else {
		rc = BDecAsnRealContent ( mem_op, b, tagId, len, &result, bytesDecoded );
	}
	if ( rc < 0 ) {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
	k->value = result;

        k->comp_desc = CompAlloc( mem_op, sizeof( ComponentDesc ) );
	if ( !k->comp_desc )  {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
        k->comp_desc->cd_gser_decoder = (gser_decoder_func*)GDecComponentReal;
        k->comp_desc->cd_ber_decoder = (ber_decoder_func*)BDecComponentReal;
	k->comp_desc->cd_free = (comp_free_func*)NULL;
	k->comp_desc->cd_pretty = (slap_syntax_transform_func*)NULL;
	k->comp_desc->cd_validate = (slap_syntax_validate_func*)NULL;
        k->comp_desc->cd_extract_i = NULL;
        k->comp_desc->cd_type = ASN_BASIC;
        k->comp_desc->cd_type_id = BASICTYPE_REAL;
        k->comp_desc->cd_all_match = (allcomponent_matching_func*)MatchingComponentReal;

        return LDAP_SUCCESS;
}

/*
 * Matching function : Relative OID
 */
int
MatchingComponentRelativeOid ( char* oid, ComponentSyntaxInfo *csi_attr,
					ComponentSyntaxInfo *csi_assert )
{
        int rc;
        MatchingRule* mr;
        ComponentRelativeOid *a, *b;
                                                                          
        if( oid ) {
                mr = retrieve_matching_rule(oid, csi_attr->csi_comp_desc->cd_type_id );
                if ( mr )
                        return component_value_match( mr, csi_attr , csi_assert );
        }

        a = (ComponentRelativeOid*)csi_attr;
        b = (ComponentRelativeOid*)csi_assert;

	if ( a->value.octetLen != b->value.octetLen )
		return LDAP_COMPARE_FALSE;
        rc = ( strncmp( a->value.octs, b->value.octs, a->value.octetLen ) == 0 );
                                                                          
        return rc ? LDAP_COMPARE_TRUE:LDAP_COMPARE_FALSE;
}

/*
 * GSER Encoder : RELATIVE_OID.
 */
int
GEncComponentRelativeOid ( GenBuf *b, ComponentRelativeOid *in )
{
    if ( !in || in->value.octetLen <= 0 )
	return (-1);
    return GEncAsnRelativeOidContent ( b , &in->value );
}

/*
 * GSER Decoder : RELATIVE_OID.
 */
int
GDecComponentRelativeOid ( void* mem_op, GenBuf *b,void *v, AsnLen *bytesDecoded, int mode )
{
        char* peek_head;
        int i, strLen;
        void* component_values;
        ComponentRelativeOid* k, **k2;
	GAsnRelativeOid result;
                                                                          
        k = (ComponentRelativeOid*) v;
                                                                          
        if ( mode & DEC_ALLOC_MODE_0 ) {
                k2 = (ComponentRelativeOid**) v;
                *k2 = (ComponentRelativeOid*) CompAlloc( mem_op, sizeof( ComponentRelativeOid ) );
		if ( !*k2 ) return LDAP_DECODING_ERROR;
                k = *k2;
        }
	
	if ( GDecAsnRelativeOidContent ( mem_op, b, &result, bytesDecoded ) < 0 ) {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
	k->value = result.value;

	k->comp_desc = CompAlloc( mem_op, sizeof( ComponentDesc ) );
	if ( !k->comp_desc )  {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
	k->comp_desc->cd_gser_decoder = (gser_decoder_func*)GDecComponentRelativeOid;
	k->comp_desc->cd_ber_decoder = (ber_decoder_func*)BDecComponentRelativeOid;
	k->comp_desc->cd_free = (comp_free_func*)FreeComponentRelativeOid;
	k->comp_desc->cd_pretty = (slap_syntax_transform_func*)NULL;
	k->comp_desc->cd_validate = (slap_syntax_validate_func*)NULL;
	k->comp_desc->cd_extract_i = NULL;
	k->comp_desc->cd_type = ASN_BASIC;
	k->comp_desc->cd_type_id = BASICTYPE_OID;
	k->comp_desc->cd_all_match = (allcomponent_matching_func*)MatchingComponentRelativeOid;
	
	return LDAP_SUCCESS;
}

/*
 * Component BER Decoder : RELATIVE_OID.
 */
int
BDecComponentRelativeOidTag ( void* mem_op, GenBuf *b, void *v, AsnLen *bytesDecoded, int mode ) {
	return BDecComponentRelativeOid ( mem_op, b, 0, 0, v, bytesDecoded, mode|CALL_TAG_DECODER );
}

int
BDecComponentRelativeOid ( void* mem_op, GenBuf *b, AsnTag tagId, AsnLen len, void *v, AsnLen *bytesDecoded, int mode )
{
        char* peek_head;
        int i, strLen, rc;
        void* component_values;
        ComponentRelativeOid* k, **k2;
	AsnRelativeOid result;
                                                                          
        k = (ComponentRelativeOid*) v;
                                                                          
        if ( mode & DEC_ALLOC_MODE_0 ) {
                k2 = (ComponentRelativeOid**) v;
                *k2 = (ComponentRelativeOid*) CompAlloc( mem_op, sizeof( ComponentRelativeOid ) );
		if ( !*k2 ) return LDAP_DECODING_ERROR;
                k = *k2;
        }
	
	if ( mode & CALL_TAG_DECODER ){
		mode = mode & CALL_CONTENT_DECODER;
		rc = BDecAsnRelativeOid ( mem_op, b, &result, bytesDecoded );
	} else {
		rc = BDecAsnRelativeOidContent ( mem_op, b, tagId, len, &result, bytesDecoded );
	}
	if ( rc < 0 ) {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
	k->value = result;

        k->comp_desc = CompAlloc( mem_op, sizeof( ComponentDesc ) );
	if ( !k->comp_desc )  {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
        k->comp_desc->cd_gser_decoder = (gser_decoder_func*)GDecComponentRelativeOid;
        k->comp_desc->cd_ber_decoder = (ber_decoder_func*)BDecComponentRelativeOid;
	k->comp_desc->cd_free = (comp_free_func*)FreeComponentRelativeOid;
	k->comp_desc->cd_pretty = (slap_syntax_transform_func*)NULL;
	k->comp_desc->cd_validate = (slap_syntax_validate_func*)NULL;
        k->comp_desc->cd_extract_i = NULL;
        k->comp_desc->cd_type = ASN_BASIC;
        k->comp_desc->cd_type_id = BASICTYPE_RELATIVE_OID;
        k->comp_desc->cd_all_match = (allcomponent_matching_func*)MatchingComponentRelativeOid;
	return LDAP_SUCCESS;
}

/*
 * GSER Encoder : UniversalString
 */
int
GEncComponentUniversalString ( GenBuf *b, ComponentUniversalString *in )
{
    if ( !in || in->value.octetLen <= 0 )
	return (-1);
    return GEncUniversalStringContent( b, &in->value );
}

/*
 * GSER Decoder : UniversalString
 */
static int
UTF8toUniversalString( char* octs, int len){
	/* Need to be Implemented */
	return LDAP_SUCCESS;
}

int
GDecComponentUniversalString ( void* mem_op, GenBuf *b, void *v, AsnLen *bytesDecoded, int mode )
{
	if ( GDecComponentUTF8String ( mem_op, b, v, bytesDecoded, mode) < 0 )
	UTF8toUniversalString( ((ComponentUniversalString*)v)->value.octs, ((ComponentUniversalString*)v)->value.octetLen );
		return LDAP_DECODING_ERROR;
}

/*
 * Component BER Decoder : UniverseString
 */
int
BDecComponentUniversalStringTag ( void* mem_op, GenBuf *b, void *v, AsnLen *bytesDecoded, int mode ) {
	return BDecComponentUniversalString ( mem_op, b, 0, 0, v, bytesDecoded, mode|CALL_TAG_DECODER );
}

int
BDecComponentUniversalString ( void* mem_op, GenBuf *b, AsnTag tagId, AsnLen len, void *v, AsnLen *bytesDecoded, int mode )
{
        char* peek_head;
        int i, strLen, rc;
        void* component_values;
        ComponentUniversalString* k, **k2;
	UniversalString result;

        k = (ComponentUniversalString*) v;

        if ( mode & DEC_ALLOC_MODE_0 ) {
                k2 = (ComponentUniversalString**) v;
                *k2 = (ComponentUniversalString*) CompAlloc( mem_op, sizeof( ComponentUniversalString ) );
		if ( !*k2 ) return LDAP_DECODING_ERROR;
                k = *k2;
        }
	
	if ( mode & CALL_TAG_DECODER ){
		mode = mode & CALL_CONTENT_DECODER;
		rc = BDecUniversalString ( mem_op, b, &result, bytesDecoded );
	} else {
		rc = BDecUniversalStringContent ( mem_op, b, tagId, len, &result, bytesDecoded );
	}
	if ( rc < 0 ) {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
	k->value = result;

        k->comp_desc = CompAlloc( mem_op, sizeof( ComponentDesc ) );
	if ( !k->comp_desc )  {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
        k->comp_desc->cd_gser_decoder = (gser_decoder_func*)GDecComponentUniversalString;
        k->comp_desc->cd_ber_decoder = (ber_decoder_func*)BDecComponentUniversalString;
	k->comp_desc->cd_free = (comp_free_func*)FreeComponentUniversalString;
	k->comp_desc->cd_pretty = (slap_syntax_transform_func*)NULL;
	k->comp_desc->cd_validate = (slap_syntax_validate_func*)NULL;
        k->comp_desc->cd_extract_i = NULL;
        k->comp_desc->cd_type = ASN_BASIC;
        k->comp_desc->cd_type_id = BASICTYPE_UNIVERSAL_STR;
        k->comp_desc->cd_all_match = (allcomponent_matching_func*)MatchingComponentUniversalString;
	return LDAP_SUCCESS;
}

/*
 * Component BER Decoder : VisibleString
 */
int
BDecComponentVisibleStringTag ( void* mem_op, GenBuf *b, void *v, AsnLen *bytesDecoded, int mode ) {
	return BDecComponentVisibleString ( mem_op, b, 0, 0, v, bytesDecoded, mode|CALL_TAG_DECODER );
}

int
BDecComponentVisibleString ( void* mem_op, GenBuf *b, AsnTag tagId, AsnLen len, void *v, AsnLen *bytesDecoded, int mode )
{
        char* peek_head;
        int i, strLen, rc;
        void* component_values;
        ComponentVisibleString* k, **k2;
	VisibleString result;
                                                                          
        k = (ComponentVisibleString*) v;
                                                                          
        if ( mode & DEC_ALLOC_MODE_0 ) {
                k2 = (ComponentVisibleString**) v;
                *k2 = (ComponentVisibleString*) CompAlloc( mem_op, sizeof( ComponentVisibleString ) );
		if ( !*k2 ) return LDAP_DECODING_ERROR;
                k = *k2;
        }
	
	if ( mode & CALL_TAG_DECODER ){
		mode = mode & CALL_CONTENT_DECODER;
		rc = BDecVisibleString ( mem_op, b, &result, bytesDecoded );
	} else {
		rc = BDecVisibleStringContent ( mem_op, b, tagId, len, &result, bytesDecoded );
	}
	k->value = result;

        k->comp_desc = CompAlloc( mem_op, sizeof( ComponentDesc ) );
	if ( !k->comp_desc )  {
		if ( k ) CompFree ( mem_op, k );
		return LDAP_DECODING_ERROR;
	}
        k->comp_desc->cd_gser_decoder = (gser_decoder_func*)GDecComponentVisibleString;
        k->comp_desc->cd_ber_decoder = (ber_decoder_func*)BDecComponentVisibleString;
	k->comp_desc->cd_free = (comp_free_func*)FreeComponentVisibleString;
	k->comp_desc->cd_pretty = (slap_syntax_transform_func*)NULL;
	k->comp_desc->cd_validate = (slap_syntax_validate_func*)NULL;
        k->comp_desc->cd_extract_i = NULL;
        k->comp_desc->cd_type = ASN_BASIC;
        k->comp_desc->cd_type_id = BASICTYPE_VISIBLE_STR;
        k->comp_desc->cd_all_match = (allcomponent_matching_func*)MatchingComponentVisibleString;
	return LDAP_SUCCESS;
}

/*
 * Routines for handling an ANY DEFINED Type
 */
void
SetAnyTypeByComponentOid ( ComponentAny *v, ComponentOid *id ) {
	Hash hash;
	void *anyInfo;

	/* use encoded oid as hash string */
	hash = MakeHash (id->value.octs, id->value.octetLen);
	if (CheckForAndReturnValue (anyOidHashTblG, hash, &anyInfo))
		v->cai = (ComponentAnyInfo*) anyInfo;
	else
		v->cai = NULL;

	if ( !v->cai ) {
	/*
	 * If not found, the data considered as octet chunk
	 * Yet-to-be-Implemented
	 */
	}
}

void
SetAnyTypeByComponentInt( ComponentAny *v, ComponentInt id) {
	Hash hash;
	void *anyInfo;

	hash = MakeHash ((char*)&id, sizeof (id));
	if (CheckForAndReturnValue (anyIntHashTblG, hash, &anyInfo))
		v->cai = (ComponentAnyInfo*) anyInfo;
	else
		v->cai = NULL;
}

int
GEncComponentAny ( GenBuf *b, ComponentAny *in )
{
    if ( in->cai != NULL  && in->cai->Encode != NULL )
        return in->cai->Encode(b, &in->value );
    else return (-1);
}

int
BDecComponentAny ( void* mem_op, GenBuf *b, ComponentAny *result, AsnLen *bytesDecoded, int mode) {
        ComponentAny *k, **k2;
                                                                          
        k = (ComponentAny*) result;
                                                                          
        if ( mode & DEC_ALLOC_MODE_0 ) {
                k2 = (ComponentAny**) result;
                *k2 = (ComponentAny*) CompAlloc( mem_op, sizeof( ComponentAny ) );
		if ( !*k2 ) return LDAP_DECODING_ERROR;
                k = *k2;
        }
	
	if ((result->cai != NULL) && (result->cai->BER_Decode != NULL)) {
		result->value = (void*) CompAlloc ( mem_op, result->cai->size );
		if ( !result->value ) return 0;
		result->cai->BER_Decode ( mem_op, b, result->value, (int*)bytesDecoded, DEC_ALLOC_MODE_1);

		k->comp_desc = CompAlloc( mem_op, sizeof( ComponentDesc ) );
		if ( !k->comp_desc )  {
			if ( k ) CompFree ( mem_op, k );
			return LDAP_DECODING_ERROR;
		}
		k->comp_desc->cd_gser_decoder = (gser_decoder_func*)GDecComponentAny;
		k->comp_desc->cd_ber_decoder = (ber_decoder_func*)BDecComponentAny;
		k->comp_desc->cd_free = (comp_free_func*)FreeComponentAny;
		k->comp_desc->cd_pretty = (slap_syntax_transform_func*)NULL;
		k->comp_desc->cd_validate = (slap_syntax_validate_func*)NULL;
		k->comp_desc->cd_extract_i = NULL;
		k->comp_desc->cd_type = ASN_BASIC;
		k->comp_desc->cd_type_id = BASICTYPE_ANY;
		k->comp_desc->cd_all_match = (allcomponent_matching_func*)MatchingComponentAny;
		return LDAP_SUCCESS;
	}
	else {
		Asn1Error ("ERROR - Component ANY Decode routine is NULL\n");
		return 0;
	}
}

int
GDecComponentAny ( void* mem_op, GenBuf *b, ComponentAny *result, AsnLen *bytesDecoded, int mode) {
        ComponentAny *k, **k2;
                                                                          
        k = (ComponentAny*) result;
                                                                          
        if ( mode & DEC_ALLOC_MODE_0 ) {
                k2 = (ComponentAny**) result;
                *k2 = (ComponentAny*) CompAlloc( mem_op, sizeof( ComponentAny ) );
		if ( !*k2 ) return LDAP_DECODING_ERROR;
                k = *k2;
        }
	if ((result->cai != NULL) && (result->cai->GSER_Decode != NULL)) {
		result->value = (void*) CompAlloc ( mem_op, result->cai->size );
		if ( !result->value ) return 0;
		result->cai->GSER_Decode ( mem_op, b, result->value, (int*)bytesDecoded, DEC_ALLOC_MODE_1);
		k->comp_desc = CompAlloc( mem_op, sizeof( ComponentDesc ) );
		if ( !k->comp_desc )  {
			if ( k ) CompFree ( mem_op, k );
			return LDAP_DECODING_ERROR;
		}
		k->comp_desc->cd_gser_decoder = (gser_decoder_func*)GDecComponentAny;
		k->comp_desc->cd_ber_decoder = (ber_decoder_func*)BDecComponentAny;
		k->comp_desc->cd_free = (comp_free_func*)FreeComponentAny;
		k->comp_desc->cd_type = ASN_BASIC;
		k->comp_desc->cd_extract_i = NULL;
		k->comp_desc->cd_type_id = BASICTYPE_ANY;
		k->comp_desc->cd_all_match = (allcomponent_matching_func*)MatchingComponentAny;
		return LDAP_SUCCESS;
	}
	else {
		Asn1Error ("ERROR - ANY Decode routine is NULL\n");
		return 0;
	}
}

int
MatchingComponentAny (char* oid, ComponentAny *result, ComponentAny *result2) {
	void *comp1, *comp2;

	if ( result->comp_desc->cd_type_id == BASICTYPE_ANY )
		comp1 = result->value;
	else
		comp1 = result;

	if ( result2->comp_desc->cd_type_id == BASICTYPE_ANY )
		comp2 = result2->value;
	else
		comp2 = result2;
		
	if ((result->cai != NULL) && (result->cai->Match != NULL)) {
		if ( result->comp_desc->cd_type_id == BASICTYPE_ANY )
			return result->cai->Match(oid, comp1, comp2 );
		else if ( result2->comp_desc->cd_type_id == BASICTYPE_ANY )
			return result2->cai->Match(oid, comp1, comp2);
		else 
			return LDAP_INVALID_SYNTAX;
	}
	else {
		Asn1Error ("ERROR - ANY Matching routine is NULL\n");
		return LDAP_INVALID_SYNTAX;
	}
}

void*
ExtractingComponentAny ( void* mem_op, ComponentReference* cr,  ComponentAny *result ) {
	if ((result->cai != NULL) && (result->cai->Extract != NULL)) {
		return (void*) result->cai->Extract( mem_op, cr , result->value );
	}
	else {
		Asn1Error ("ERROR - ANY Extracting routine is NULL\n");
		return (void*)NULL;
	}
}

void
FreeComponentAny (ComponentAny* any) {
	if ( any->cai != NULL && any->cai->Free != NULL ) {
		any->cai->Free( any->value );
		free ( ((ComponentSyntaxInfo*)any->value)->csi_comp_desc );
		free ( any->value );
	}
	else
		Asn1Error ("ERROR - ANY Free routine is NULL\n");
}

void
InstallAnyByComponentInt (int anyId, ComponentInt intId, unsigned int size,
			EncodeFcn encode, gser_decoder_func* G_decode,
			ber_tag_decoder_func* B_decode, ExtractFcn extract,
			MatchFcn match, FreeFcn free,
			PrintFcn print)
{
	ComponentAnyInfo *a;
	Hash h;

	a = (ComponentAnyInfo*) malloc(sizeof (ComponentAnyInfo));
	a->anyId = anyId;
	a->oid.octs = NULL;
	a->oid.octetLen = 0;
	a->intId = intId;
	a->size = size;
	a->Encode = encode;
	a->GSER_Decode = G_decode;
	a->BER_Decode = B_decode;
	a->Match = match;
	a->Extract = extract;
	a->Free = free;
	a->Print = print;

	if (anyIntHashTblG == NULL)
		anyIntHashTblG = InitHash();

	h = MakeHash ((char*)&intId, sizeof (intId));

	if(anyIntHashTblG != NULL)
		Insert(anyIntHashTblG, a, h);
}

void
InstallAnyByComponentOid (int anyId, AsnOid *oid, unsigned int size,
			EncodeFcn encode, gser_decoder_func* G_decode,
			ber_tag_decoder_func* B_decode, ExtractFcn extract,
			 MatchFcn match, FreeFcn free, PrintFcn print)
{
	ComponentAnyInfo *a;
	Hash h;

	a = (ComponentAnyInfo*) malloc (sizeof (ComponentAnyInfo));
	a->anyId = anyId;
	a->oid.octs = NULL;
	a->oid.octetLen = 0;
	a->size = size;
	a->Encode = encode;
	a->GSER_Decode = G_decode;
	a->BER_Decode = B_decode;
	a->Match = match;
	a->Extract = extract;
	a->Free = free;
	a->Print = print;

	h = MakeHash (oid->octs, oid->octetLen);

	if (anyOidHashTblG == NULL)
		anyOidHashTblG = InitHash();

	if(anyOidHashTblG != NULL)
		Insert(anyOidHashTblG, a, h);
}

int
BDecComponentTop  (
ber_decoder_func *decoder _AND_
void* mem_op _AND_
GenBuf *b _AND_
AsnTag tag _AND_
AsnLen elmtLen _AND_
void **v _AND_
AsnLen *bytesDecoded _AND_
int mode) {
	tag = BDecTag ( b, bytesDecoded );
	elmtLen = BDecLen ( b, bytesDecoded );
	if ( tag != MAKE_TAG_ID (UNIV, CONS, SEQ_TAG_CODE) ) {
		printf("Invliad Tag\n");
		exit (1);
	}
		
	return (*decoder)( mem_op, b, tag, elmtLen, (ComponentSyntaxInfo*)v,(int*)bytesDecoded, mode );
}

#endif
