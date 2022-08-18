#include <linux/string.h>

#include "mbedtls/ecp.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/asn1.h"
//#include "mbedtls/x509_crt.h"

#define ECDSA_VALIDATE_RET( cond )    \
    MBEDTLS_INTERNAL_VALIDATE_RET( cond, MBEDTLS_ERR_ECP_BAD_INPUT_DATA )
#define ECDSA_VALIDATE( cond )        \
    MBEDTLS_INTERNAL_VALIDATE( cond )


#define ECDSA_RS_ECP    NULL

#define ECDSA_BUDGET( ops )   /* no-op; for compatibility */

#define ECDSA_RS_ENTER( SUB )   (void) rs_ctx
#define ECDSA_RS_LEAVE( SUB )   (void) rs_ctx

static int derive_mpi( const mbedtls_ecp_group *grp, mbedtls_mpi *x,
                       const unsigned char *buf, size_t blen )
{
    int ret;
    size_t n_size = ( grp->nbits + 7 ) / 8;
    size_t use_size = blen > n_size ? n_size : blen;

    MBEDTLS_MPI_CHK( mbedtls_mpi_read_binary( x, buf, use_size ) );
    if( use_size * 8 > grp->nbits )
        MBEDTLS_MPI_CHK( mbedtls_mpi_shift_r( x, use_size * 8 - grp->nbits ) );

    /* While at it, reduce modulo N */
    if( mbedtls_mpi_cmp_mpi( x, &grp->N ) >= 0 )
        MBEDTLS_MPI_CHK( mbedtls_mpi_sub_mpi( x, x, &grp->N ) );

cleanup:
    return( ret );
}

static int ecdsa_verify_restartable( mbedtls_ecp_group *grp,
                                     const unsigned char *buf, size_t blen,
                                     const mbedtls_ecp_point *Q,
                                     const mbedtls_mpi *r, const mbedtls_mpi *s,
                                     mbedtls_ecdsa_restart_ctx *rs_ctx )
{
    int ret;
    mbedtls_mpi e, s_inv, u1, u2;
    mbedtls_ecp_point R;
    mbedtls_mpi *pu1 = &u1, *pu2 = &u2;

    mbedtls_ecp_point_init( &R );
    mbedtls_mpi_init( &e ); mbedtls_mpi_init( &s_inv );
    mbedtls_mpi_init( &u1 ); mbedtls_mpi_init( &u2 );

    /* Fail cleanly on curves such as Curve25519 that can't be used for ECDSA */
    if( grp->N.p == NULL )
        return( MBEDTLS_ERR_ECP_BAD_INPUT_DATA );

    ECDSA_RS_ENTER( ver );

#if defined(MBEDTLS_ECP_RESTARTABLE)
    if( rs_ctx != NULL && rs_ctx->ver != NULL )
    {
        /* redirect to our context */
        pu1 = &rs_ctx->ver->u1;
        pu2 = &rs_ctx->ver->u2;

        /* jump to current step */
        if( rs_ctx->ver->state == ecdsa_ver_muladd )
            goto muladd;
    }
#endif /* MBEDTLS_ECP_RESTARTABLE */

    /*
     * Step 1: make sure r and s are in range 1..n-1
     */
    if( mbedtls_mpi_cmp_int( r, 1 ) < 0 || mbedtls_mpi_cmp_mpi( r, &grp->N ) >= 0 ||
        mbedtls_mpi_cmp_int( s, 1 ) < 0 || mbedtls_mpi_cmp_mpi( s, &grp->N ) >= 0 )
    {
        ret = MBEDTLS_ERR_ECP_VERIFY_FAILED;
        goto cleanup;
    }

    /*
     * Step 3: derive MPI from hashed message
     */
    MBEDTLS_MPI_CHK( derive_mpi( grp, &e, buf, blen ) );

    /*
     * Step 4: u1 = e / s mod n, u2 = r / s mod n
     */
    ECDSA_BUDGET( MBEDTLS_ECP_OPS_CHK + MBEDTLS_ECP_OPS_INV + 2 );

    MBEDTLS_MPI_CHK( mbedtls_mpi_inv_mod( &s_inv, s, &grp->N ) );

    MBEDTLS_MPI_CHK( mbedtls_mpi_mul_mpi( pu1, &e, &s_inv ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_mod_mpi( pu1, pu1, &grp->N ) );

    MBEDTLS_MPI_CHK( mbedtls_mpi_mul_mpi( pu2, r, &s_inv ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_mod_mpi( pu2, pu2, &grp->N ) );

#if defined(MBEDTLS_ECP_RESTARTABLE)
    if( rs_ctx != NULL && rs_ctx->ver != NULL )
        rs_ctx->ver->state = ecdsa_ver_muladd;

muladd:
#endif
    /*
     * Step 5: R = u1 G + u2 Q
     */
    MBEDTLS_MPI_CHK( mbedtls_ecp_muladd_restartable( grp,
                     &R, pu1, &grp->G, pu2, Q, ECDSA_RS_ECP ) );

    if( mbedtls_ecp_is_zero( &R ) )
    {
        ret = MBEDTLS_ERR_ECP_VERIFY_FAILED;
        goto cleanup;
    }

    /*
     * Step 6: convert xR to an integer (no-op)
     * Step 7: reduce xR mod n (gives v)
     */
    MBEDTLS_MPI_CHK( mbedtls_mpi_mod_mpi( &R.X, &R.X, &grp->N ) );

    /*
     * Step 8: check if v (that is, R.X) is equal to r
     */
    if( mbedtls_mpi_cmp_mpi( &R.X, r ) != 0 )
    {
        ret = MBEDTLS_ERR_ECP_VERIFY_FAILED;
        goto cleanup;
    }

cleanup:
    mbedtls_ecp_point_free( &R );
    mbedtls_mpi_free( &e ); mbedtls_mpi_free( &s_inv );
    mbedtls_mpi_free( &u1 ); mbedtls_mpi_free( &u2 );

    ECDSA_RS_LEAVE( ver );

    return( ret );
}


int mbedtls_ecdsa_verify( mbedtls_ecp_group *grp,
                          const unsigned char *buf, size_t blen,
                          const mbedtls_ecp_point *Q,
                          const mbedtls_mpi *r,
                          const mbedtls_mpi *s)
{
    ECDSA_VALIDATE_RET( grp != NULL );
    ECDSA_VALIDATE_RET( Q   != NULL );
    ECDSA_VALIDATE_RET( r   != NULL );
    ECDSA_VALIDATE_RET( s   != NULL );
    ECDSA_VALIDATE_RET( buf != NULL || blen == 0 );

    return( ecdsa_verify_restartable( grp, buf, blen, Q, r, s, NULL ) );
}

int mbedtls_ecdsa_read_signature_restartable( mbedtls_ecdsa_context *ctx,
                          const unsigned char *hash, size_t hlen,
                          const unsigned char *sig, size_t slen,
                          mbedtls_ecdsa_restart_ctx *rs_ctx )
{
    int ret;
    unsigned char *p = (unsigned char *) sig;
    const unsigned char *end = sig + slen;
    size_t len;
    mbedtls_mpi r, s;
    ECDSA_VALIDATE_RET( ctx  != NULL );
    ECDSA_VALIDATE_RET( hash != NULL );
    ECDSA_VALIDATE_RET( sig  != NULL );

    mbedtls_mpi_init( &r );
    mbedtls_mpi_init( &s );

    if( ( ret = mbedtls_asn1_get_tag( &p, end, &len,
                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE ) ) != 0 )
    {
        ret += MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
        goto cleanup;
    }

    if( p + len != end )
    {
        ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA +
              MBEDTLS_ERR_ASN1_LENGTH_MISMATCH;
        goto cleanup;
    }

    if( ( ret = mbedtls_asn1_get_mpi( &p, end, &r ) ) != 0 ||
        ( ret = mbedtls_asn1_get_mpi( &p, end, &s ) ) != 0 )
    {
        ret += MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
        goto cleanup;
    }
#if defined(MBEDTLS_ECDSA_VERIFY_ALT)
    if( ( ret = mbedtls_ecdsa_verify( &ctx->grp, hash, hlen,
                                      &ctx->Q, &r, &s ) ) != 0 )
        goto cleanup;
#else
    if( ( ret = ecdsa_verify_restartable( &ctx->grp, hash, hlen,
                              &ctx->Q, &r, &s, rs_ctx ) ) != 0 )
        goto cleanup;
#endif /* MBEDTLS_ECDSA_VERIFY_ALT */

    /* At this point we know that the buffer starts with a valid signature.
     * Return 0 if the buffer just contains the signature, and a specific
     * error code if the valid signature is followed by more data. */
    if( p != end )
        ret = MBEDTLS_ERR_ECP_SIG_LEN_MISMATCH;

cleanup:
    mbedtls_mpi_free( &r );
    mbedtls_mpi_free( &s );

    return( ret );
}

int mbedtls_ecdsa_read_signature( mbedtls_ecdsa_context *ctx,
                          const unsigned char *hash, size_t hlen,
                          const unsigned char *sig, size_t slen )
{
#if 0
    size_t i;
    const unsigned char *hash_tmp, *sig_tmp;
#endif

    ECDSA_VALIDATE_RET( ctx  != NULL );
    ECDSA_VALIDATE_RET( hash != NULL );
    ECDSA_VALIDATE_RET( sig  != NULL );

#if 0
    hash_tmp = hash;
    printf("the len is %ld, the hash is: \n", hlen);
    for(i = 0; i < hlen; i++) {
        if (((i + 1) % 10) != 0)
            printf("0x%02x, ", *hash_tmp);
        else {
            printf("0x%02x", *hash_tmp);
            printf("\n");
        }
        hash_tmp++;
    }

    printf("\n");

    sig_tmp = sig;
    printf("the len is %ld, the sig is: \n", slen);
    for(i = 0; i < slen; i++) {
        if (((i + 1) % 10) != 0)
            printf("0x%02x, ", *sig_tmp);
        else {
            printf("0x%02x", *sig_tmp);
            printf("\n");
        }
        sig_tmp++;
    }

    printf("\n");

    printf("sizeof mbedtls_mpi_uint is %d\n\n", sizeof(mbedtls_mpi_uint));

    //ctx->grp
    printf("ctx->grp.id, %d\n", ctx->grp.id); 

    printf("ctx->grp.P.s, %d\n", ctx->grp.P.s);
    printf("ctx->grp.P.n, %ld\n", ctx->grp.P.n);
    if (ctx->grp.P.p) {
        printf("ctx->grp.P.p:\n");
        for (i=0; i<ctx->grp.P.n; i++) {
            printf("%uU, ", *(ctx->grp.P.p + i));
        }
        printf("\n");
    }

    printf("ctx->grp.A.s, %d\n", ctx->grp.A.s);
    printf("ctx->grp.A.n, %ld\n", ctx->grp.A.n);
    if (ctx->grp.A.p)
        printf("ctx->grp.A.p, %lu\n", *(ctx->grp.A.p));

    printf("ctx->grp.B.s, %d\n", ctx->grp.B.s);
    printf("ctx->grp.B.n, %ld\n", ctx->grp.B.n);
    if (ctx->grp.B.p)
        printf("ctx->grp.B.p, %lu\n", *(ctx->grp.B.p));

    printf("ctx->grp.G.X.s, %d\n", ctx->grp.G.X.s);
    printf("ctx->grp.G.X.n, %ld\n", ctx->grp.G.X.n);
    if (ctx->grp.G.X.p) {
        printf("ctx->grp.G.X.p:\n");
        for (i=0; i<ctx->grp.G.X.n; i++) {
            printf("%uU, ", *(ctx->grp.G.X.p + i));
        }
        printf("\n");
    }

    printf("ctx->grp.G.Y.s, %d\n", ctx->grp.G.Y.s);
    printf("ctx->grp.G.Y.n, %ld\n", ctx->grp.G.Y.n);
    if (ctx->grp.G.Y.p) {
        printf("ctx->grp.G.Y.p:\n");
        for (i=0; i<ctx->grp.G.Y.n; i++) {
            printf("%uU, ", *(ctx->grp.G.Y.p + i));
        }
        printf("\n");
    }

    printf("ctx->grp.G.Z.s, %d\n", ctx->grp.G.Z.s);
    printf("ctx->grp.G.Z.n, %ld\n", ctx->grp.G.Z.n);
    if (ctx->grp.G.Z.p)
        printf("ctx->grp.G.Z.p, %lu\n", *(ctx->grp.G.Z.p));

    printf("ctx->grp.N.s, %d\n", ctx->grp.N.s);
    printf("ctx->grp.N.n, %ld\n", ctx->grp.N.n);
    if (ctx->grp.N.p) {
        printf("ctx->grp.N.p:\n");
        for (i=0; i<ctx->grp.N.n; i++) {
            printf("%uU, ", *(ctx->grp.N.p + i));
        }
        printf("\n");
    }

    printf("ctx->grp.pbits, %u\n", ctx->grp.pbits);
    printf("ctx->grp.nbits, %u\n", ctx->grp.nbits);
    printf("ctx->grp.h, %d\n", ctx->grp.h);

    if(ctx->grp.T) {
        printf("ctx->grp.T->X.s, %d\n", ctx->grp.T->X.s);
        printf("ctx->grp.T->X.n, %ld\n", ctx->grp.T->X.n);
        if (ctx->grp.T->X.p)
            printf("ctx->grp.T->X.p, %lu\n", *(ctx->grp.T->X.p));

        printf("ctx->grp.T->Y.s, %d\n", ctx->grp.T->Y.s);
        printf("ctx->grp.T->Y.n, %ld\n", ctx->grp.T->Y.n);
        if (ctx->grp.T->Y.p)
            printf("ctx->grp.T->Y.p, %lu\n", *(ctx->grp.T->Y.p));

        printf("ctx->grp.T->Z.s, %d\n", ctx->grp.T->Z.s);
        printf("ctx->grp.T->Z.n, %ld\n", ctx->grp.T->Z.n);
        if (ctx->grp.T->Z.p)
            printf("ctx->grp.T->Z.p, %lu\n", *(ctx->grp.T->Z.p));
    }

    printf("ctx->grp.T_size, %ld\n", ctx->grp.T_size);


    //d
    printf("ctx->d.s, %d\n", ctx->d.s);
    printf("ctx->d.n, %ld\n", ctx->d.n);
    if (ctx->d.p)
        printf("ctx->d.p, %lu\n", *(ctx->d.p));

    printf("ctx->Q.X.s, %d\n", ctx->Q.X.s);
    printf("ctx->Q.X.n, %ld\n", ctx->Q.X.n);
    if (ctx->Q.X.p) {
        printf("ctx->Q.X.p:\n");
        for (i=0; i<ctx->Q.X.n; i++) {
            printf("%uU, ", *(ctx->Q.X.p + i));
        }
        printf("\n");
    }

    printf("ctx->Q.Y.s, %d\n", ctx->Q.Y.s);
    printf("ctx->Q.Y.n, %ld\n", ctx->Q.Y.n);
    if (ctx->Q.Y.p) {
        printf("ctx->Q.Y.p:\n");
        for (i=0; i<ctx->Q.Y.n; i++) {
            printf("%uU, ", *(ctx->Q.Y.p + i));
        }
        printf("\n");
    }

    printf("ctx->Q.Z.s, %d\n", ctx->Q.Z.s);
    printf("ctx->Q.Z.n, %ld\n", ctx->Q.Z.n);
    if (ctx->Q.Z.p)
        printf("ctx->Q.Z.p, %lu\n", *(ctx->Q.Z.p));
#endif


    return( mbedtls_ecdsa_read_signature_restartable(
                ctx, hash, hlen, sig, slen, NULL ) );
}

void mbedtls_ecdsa_init( mbedtls_ecdsa_context *ctx )
{
    ECDSA_VALIDATE( ctx != NULL );

    mbedtls_ecp_keypair_init( ctx );
}

void mbedtls_ecdsa_free( mbedtls_ecdsa_context *ctx )
{
    if( ctx == NULL )
        return;

    mbedtls_ecp_keypair_free( ctx );
}

#if 0
static int sign_size = 72;

static unsigned char sign[72] = {0x30, 0x46, 0x2, 0x21, 0x0, 0xc1, 0x6b, 0xd5, 0x6c, 0x1a, 0x20, 0x5b, 0x5a, 0xe5, 0x5f, 
    0xff, 0xc8, 0x73, 0xeb, 0x13, 0x14, 0x94, 0xc7, 0x42, 0xeb, 0x5e, 0x74, 0x99, 0xf1, 0xd9, 0x3f, 0x3e, 0xe7, 0x2a, 0xaf, 
    0x7d, 0xc0, 0x2, 0x21, 0x0, 0x90, 0x4e, 0x1f, 0xe2, 0x21, 0x4e, 0xbd, 0xed, 0x1a, 0xcb, 0xd3, 0x76, 0xa0, 0x5b, 0x72, 
    0xf, 0xb3, 0x8b, 0x2c, 0x73, 0xb6, 0x9c, 0xbd, 0x1e, 0x2c, 0x3d, 0x51, 0x3d, 0xfa, 0xd4, 0x99, 0xe2};
static unsigned char hash[32] = {0xea, 0xe9, 0x7b, 0xd7, 0xec, 0xa2, 0x75,  0x8, 0x65, 0x56,  0xa, 0x9f, 0x27, 0x33, 0xde, 
    0x1c, 0x26, 0xf4, 0x9a, 0xaf, 0x87, 0xe8, 0xb7, 0x10, 0x35, 0x30, 0x53, 0x76, 0x83, 0x51, 0x1e, 0x60};
#endif

#define TUYA_PROD       1
int ecdsa_verify_old(unsigned char *hash, int hash_size, unsigned char *sign, int sign_size)
{
    mbedtls_ecdsa_context ctx_verify;
    int ret;

    mbedtls_ecdsa_init(&ctx_verify);

    //ctx_verify.grp
    ctx_verify.grp.id = 12;

    ctx_verify.grp.P.s = 1;
    ctx_verify.grp.P.n = 8;
    mbedtls_mpi_uint tmp1[8] = {4294966319U, 4294967294U, 4294967295U, 4294967295U, 4294967295U, 4294967295U, 4294967295U, 4294967295U};
    ctx_verify.grp.P.p = tmp1;

    ctx_verify.grp.A.s = 1;
    ctx_verify.grp.A.n = 1;
    mbedtls_mpi_uint tmp2 = 0;
    ctx_verify.grp.A.p = &tmp2;

    ctx_verify.grp.B.s = 1;
    ctx_verify.grp.B.n = 1;
    mbedtls_mpi_uint tmp3 = 7;
    ctx_verify.grp.B.p = &tmp3;

    ctx_verify.grp.G.X.s = 1;
    ctx_verify.grp.G.X.n = 8;
    mbedtls_mpi_uint tmp4[8] = {385357720U, 1509065051U, 768485593U, 43777243U, 3464956679U, 1436574357U, 4191992748U, 2042521214U};
    ctx_verify.grp.G.X.p = tmp4;

    ctx_verify.grp.G.Y.s = 1;
    ctx_verify.grp.G.Y.n = 8;
    mbedtls_mpi_uint tmp5[8] = {4212184248U, 2621952143U, 2793755673U, 4246189128U, 235997352U, 1571093500U, 648266853U, 1211816567U};
    ctx_verify.grp.G.Y.p = tmp5;

    ctx_verify.grp.G.Z.s = 1;
    ctx_verify.grp.G.Z.n = 1;
    mbedtls_mpi_uint tmp6 = 1;
    ctx_verify.grp.G.Z.p = &tmp6;

    ctx_verify.grp.N.s = 1;
    ctx_verify.grp.N.n = 8;
    mbedtls_mpi_uint tmp7[8] = {3493216577U, 3218235020U, 2940772411U, 3132021990U, 4294967294U, 4294967295U, 4294967295U, 4294967295U};
    ctx_verify.grp.N.p = tmp7;

    ctx_verify.grp.pbits = 256;
    ctx_verify.grp.nbits = 256;

    ctx_verify.grp.h = 1;

    ctx_verify.grp.T_size = 0;


    //ctx_verify.d
    ctx_verify.d.s = 1;
    ctx_verify.d.n = 0;

#ifdef TUYA_DAILY
    //ctx_verify.Q
    ctx_verify.Q.X.s = 1;
    ctx_verify.Q.X.n = 8;
    mbedtls_mpi_uint tmp8[8] = {1711319298U, 3690224112U, 1730334213U, 2723995408U, 1788899776U, 2050291599U, 1159660541U, 2600726607U};
    ctx_verify.Q.X.p = tmp8;

    ctx_verify.Q.Y.s = 1;
    ctx_verify.Q.Y.n = 8;
    mbedtls_mpi_uint tmp9[8] = {360676868U, 2078962270U, 3337027526U, 371674626U, 1353709277U, 3026587826U, 4110114658U, 2074646257U};
    ctx_verify.Q.Y.p = tmp9;

    ctx_verify.Q.Z.s = 1;
    ctx_verify.Q.Z.n = 1;
    mbedtls_mpi_uint tmp10 = 1;
    ctx_verify.Q.Z.p = &tmp10;
#elif TUYA_PROD
    //ctx_verify.Q
    ctx_verify.Q.X.s = 1;
    ctx_verify.Q.X.n = 8;
    mbedtls_mpi_uint tmp8[8] = {3995872882U, 4253799712U, 1485254100U, 1798291336U, 180968268U, 1190566364U, 3991223908U, 3659028261U};
    ctx_verify.Q.X.p = tmp8;

    ctx_verify.Q.Y.s = 1;
    ctx_verify.Q.Y.n = 8;
    mbedtls_mpi_uint tmp9[8] = {3654297186U, 2603863648U, 975225792U, 1785965055U, 594581294U, 112132902U, 111521371U, 2381460908U};
    ctx_verify.Q.Y.p = tmp9;

    ctx_verify.Q.Z.s = 1;
    ctx_verify.Q.Z.n = 1;
    mbedtls_mpi_uint tmp10 = 1;
    ctx_verify.Q.Z.p = &tmp10;
#else
    printf("error, no MACRO in process\n");
    return -1;
#endif


    if( ( ret = mbedtls_ecdsa_read_signature( &ctx_verify,
                    hash, hash_size,
                    sign, sign_size ) ) != 0 ) {
        //printf(" failed\n  ! returned %d\n", ret );
        return -1;
    }

    printf("tuya verify ok\n");

    return 0;
}

