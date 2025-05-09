/* $OpenBSD: speed.c,v 1.39 2024/07/13 16:43:56 tb Exp $ */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 *
 * Portions of the attached software ("Contribution") are developed by
 * SUN MICROSYSTEMS, INC., and are contributed to the OpenSSL project.
 *
 * The Contribution is licensed pursuant to the OpenSSL open source
 * license provided above.
 *
 * The ECDH and ECDSA speed test software is originally written by
 * Sumit Gupta of Sun Microsystems Laboratories.
 *
 */

/* most of this code has been pilfered from my libdes speed.c program */

#ifndef OPENSSL_NO_SPEED

#define SECONDS		3
#define RSA_SECONDS	10
#define DSA_SECONDS	10
#define ECDSA_SECONDS   10
#define ECDH_SECONDS    10

#define MAX_UNALIGN	16

#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#include "apps.h"

#include <openssl/bn.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/modes.h>
#include <openssl/objects.h>
#include <openssl/x509.h>

#ifndef OPENSSL_NO_AES
#include <openssl/aes.h>
#endif
#ifndef OPENSSL_NO_BF
#include <openssl/blowfish.h>
#endif
#ifndef OPENSSL_NO_CAST
#include <openssl/cast.h>
#endif
#ifndef OPENSSL_NO_CAMELLIA
#include <openssl/camellia.h>
#endif
#ifndef OPENSSL_NO_DES
#include <openssl/des.h>
#endif
#include <openssl/dsa.h>
#include <openssl/ecdh.h>
#include <openssl/ecdsa.h>
#ifndef OPENSSL_NO_HMAC
#include <openssl/hmac.h>
#endif
#ifndef OPENSSL_NO_IDEA
#include <openssl/idea.h>
#endif
#ifndef OPENSSL_NO_MD4
#include <openssl/md4.h>
#endif
#ifndef OPENSSL_NO_MD5
#include <openssl/md5.h>
#endif
#ifndef OPENSSL_NO_RC2
#include <openssl/rc2.h>
#endif
#ifndef OPENSSL_NO_RC4
#include <openssl/rc4.h>
#endif
#include <openssl/rsa.h>
#ifndef OPENSSL_NO_RIPEMD
#include <openssl/ripemd.h>
#endif
#ifndef OPENSSL_NO_SHA
#include <openssl/sha.h>
#endif
#ifndef OPENSSL_NO_WHIRLPOOL
#include <openssl/whrlpool.h>
#endif

#include "./testdsa.h"
#include "./testrsa.h"

#define BUFSIZE	(1024*8+64)
volatile sig_atomic_t run;

static int mr = 0;
static int usertime = 1;

static double Time_F(int s);
static void print_message(const char *s, long num, int length);
static void
pkey_print_message(const char *str, const char *str2,
    long num, int bits, int sec);
static void print_result(int alg, int run_no, int count, double time_used);
#ifndef _WIN32
static int do_multi(int multi);
#else
void speed_signal(int sigcatch, void (*func)(int sigraised));
unsigned int speed_alarm(unsigned int seconds);
void speed_alarm_free(int run);
#define SIGALRM		14
#define signal(sigcatch, func)	speed_signal((sigcatch), (func))
#define alarm(seconds)		speed_alarm((seconds))
#endif

#define ALGOR_NUM	32
#define SIZE_NUM	5
#define RSA_NUM		4
#define DSA_NUM		3

#define EC_NUM       6
#define MAX_ECDH_SIZE 256

static const char *names[ALGOR_NUM] = {
	"md2", "md4", "md5", "hmac(md5)", "sha1", "rmd160",
	"rc4", "des cbc", "des ede3", "idea cbc", "seed cbc",
	"rc2 cbc", "rc5-32/12 cbc", "blowfish cbc", "cast cbc",
	"aes-128 cbc", "aes-192 cbc", "aes-256 cbc",
	"camellia-128 cbc", "camellia-192 cbc", "camellia-256 cbc",
	"evp", "sha256", "sha512", "whirlpool",
	"aes-128 ige", "aes-192 ige", "aes-256 ige", "ghash",
	"aes-128 gcm", "aes-256 gcm", "chacha20 poly1305",
};
static double results[ALGOR_NUM][SIZE_NUM];
static int lengths[SIZE_NUM] = {16, 64, 256, 1024, 8 * 1024};
static double rsa_results[RSA_NUM][2];
static double dsa_results[DSA_NUM][2];
static double ecdsa_results[EC_NUM][2];
static double ecdh_results[EC_NUM][1];

static void sig_done(int sig);

static void
sig_done(int sig)
{
	run = 0;
}

#define START	TM_RESET
#define STOP	TM_GET


static double
Time_F(int s)
{
	if (usertime)
		return app_timer_user(s);
	else
		return app_timer_real(s);
}


static const int KDF1_SHA1_len = 20;
static void *
KDF1_SHA1(const void *in, size_t inlen, void *out, size_t * outlen)
{
#ifndef OPENSSL_NO_SHA
	if (*outlen < SHA_DIGEST_LENGTH)
		return NULL;
	else
		*outlen = SHA_DIGEST_LENGTH;
	return SHA1(in, inlen, out);
#else
	return NULL;
#endif				/* OPENSSL_NO_SHA */
}

int
speed_main(int argc, char **argv)
{
	unsigned char *real_buf = NULL, *real_buf2 = NULL;
	unsigned char *buf = NULL, *buf2 = NULL;
	size_t unaligned = 0;
	int mret = 1;
	long count = 0, save_count = 0;
	int i, j, k;
	long rsa_count;
	unsigned rsa_num;
	unsigned char md[EVP_MAX_MD_SIZE];
#ifndef OPENSSL_NO_MD4
	unsigned char md4[MD4_DIGEST_LENGTH];
#endif
#ifndef OPENSSL_NO_MD5
	unsigned char md5[MD5_DIGEST_LENGTH];
	unsigned char hmac[MD5_DIGEST_LENGTH];
#endif
#ifndef OPENSSL_NO_SHA
	unsigned char sha[SHA_DIGEST_LENGTH];
#ifndef OPENSSL_NO_SHA256
	unsigned char sha256[SHA256_DIGEST_LENGTH];
#endif
#ifndef OPENSSL_NO_SHA512
	unsigned char sha512[SHA512_DIGEST_LENGTH];
#endif
#endif
#ifndef OPENSSL_NO_WHIRLPOOL
	unsigned char whirlpool[WHIRLPOOL_DIGEST_LENGTH];
#endif
#ifndef OPENSSL_NO_RIPEMD
	unsigned char rmd160[RIPEMD160_DIGEST_LENGTH];
#endif
#ifndef OPENSSL_NO_RC4
	RC4_KEY rc4_ks;
#endif
#ifndef OPENSSL_NO_RC2
	RC2_KEY rc2_ks;
#endif
#ifndef OPENSSL_NO_IDEA
	IDEA_KEY_SCHEDULE idea_ks;
#endif
#ifndef OPENSSL_NO_BF
	BF_KEY bf_ks;
#endif
#ifndef OPENSSL_NO_CAST
	CAST_KEY cast_ks;
#endif
	static const unsigned char key16[16] =
	{0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
	0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12};
#ifndef OPENSSL_NO_AES
	static const unsigned char key24[24] =
	{0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
		0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12,
	0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34};
	static const unsigned char key32[32] =
	{0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
		0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12,
		0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34,
	0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56};
#endif
#ifndef OPENSSL_NO_CAMELLIA
	static const unsigned char ckey24[24] =
	{0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
		0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12,
	0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34};
	static const unsigned char ckey32[32] =
	{0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
		0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12,
		0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34,
	0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56};
#endif
#ifndef OPENSSL_NO_AES
#define MAX_BLOCK_SIZE 128
#else
#define MAX_BLOCK_SIZE 64
#endif
	unsigned char DES_iv[8];
	unsigned char iv[2 * MAX_BLOCK_SIZE / 8];
#ifndef OPENSSL_NO_DES
	static DES_cblock key = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0};
	static DES_cblock key2 = {0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12};
	static DES_cblock key3 = {0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34};
	DES_key_schedule sch;
	DES_key_schedule sch2;
	DES_key_schedule sch3;
#endif
#ifndef OPENSSL_NO_AES
	AES_KEY aes_ks1, aes_ks2, aes_ks3;
#endif
#ifndef OPENSSL_NO_CAMELLIA
	CAMELLIA_KEY camellia_ks1, camellia_ks2, camellia_ks3;
#endif
#define	D_MD2		0
#define	D_MD4		1
#define	D_MD5		2
#define	D_HMAC		3
#define	D_SHA1		4
#define D_RMD160	5
#define	D_RC4		6
#define	D_CBC_DES	7
#define	D_EDE3_DES	8
#define	D_CBC_IDEA	9
#define	D_CBC_SEED	10
#define	D_CBC_RC2	11
#define	D_CBC_RC5	12
#define	D_CBC_BF	13
#define	D_CBC_CAST	14
#define D_CBC_128_AES	15
#define D_CBC_192_AES	16
#define D_CBC_256_AES	17
#define D_CBC_128_CML   18
#define D_CBC_192_CML   19
#define D_CBC_256_CML   20
#define D_EVP		21
#define D_SHA256	22
#define D_SHA512	23
#define D_WHIRLPOOL	24
#define D_IGE_128_AES   25
#define D_IGE_192_AES   26
#define D_IGE_256_AES   27
#define D_GHASH		28
#define D_AES_128_GCM	29
#define D_AES_256_GCM	30
#define D_CHACHA20_POLY1305	31
	double d = 0.0;
	long c[ALGOR_NUM][SIZE_NUM];
#define	R_DSA_512	0
#define	R_DSA_1024	1
#define	R_DSA_2048	2
#define	R_RSA_512	0
#define	R_RSA_1024	1
#define	R_RSA_2048	2
#define	R_RSA_4096	3

#define R_EC_P160    0
#define R_EC_P192    1
#define R_EC_P224    2
#define R_EC_P256    3
#define R_EC_P384    4
#define R_EC_P521    5

	RSA *rsa_key[RSA_NUM];
	long rsa_c[RSA_NUM][2];
	static unsigned int rsa_bits[RSA_NUM] = {512, 1024, 2048, 4096};
	static unsigned char *rsa_data[RSA_NUM] =
	{test512, test1024, test2048, test4096};
	static int rsa_data_length[RSA_NUM] = {
		sizeof(test512), sizeof(test1024),
	sizeof(test2048), sizeof(test4096)};
	DSA *dsa_key[DSA_NUM];
	long dsa_c[DSA_NUM][2];
	static unsigned int dsa_bits[DSA_NUM] = {512, 1024, 2048};
#ifndef OPENSSL_NO_EC
	/*
	 * We only test over the following curves as they are representative,
	 * To add tests over more curves, simply add the curve NID and curve
	 * name to the following arrays and increase the EC_NUM value
	 * accordingly.
	 */
	static unsigned int test_curves[EC_NUM] = {
		NID_secp160r1,
		NID_X9_62_prime192v1,
		NID_secp224r1,
		NID_X9_62_prime256v1,
		NID_secp384r1,
		NID_secp521r1,
	};
	static const char *test_curves_names[EC_NUM] = {
		"secp160r1",
		"nistp192",
		"nistp224",
		"nistp256",
		"nistp384",
		"nistp521",
	};
	static int test_curves_bits[EC_NUM] = {
		160, 192, 224, 256, 384, 521,
	};

#endif

	unsigned char ecdsasig[256];
	unsigned int ecdsasiglen;
	EC_KEY *ecdsa[EC_NUM];
	long ecdsa_c[EC_NUM][2];

	EC_KEY *ecdh_a[EC_NUM], *ecdh_b[EC_NUM];
	unsigned char secret_a[MAX_ECDH_SIZE], secret_b[MAX_ECDH_SIZE];
	int secret_size_a, secret_size_b;
	int ecdh_checks = 0;
	int secret_idx = 0;
	long ecdh_c[EC_NUM][2];

	int rsa_doit[RSA_NUM];
	int dsa_doit[DSA_NUM];
	int ecdsa_doit[EC_NUM];
	int ecdh_doit[EC_NUM];
	int doit[ALGOR_NUM];
	int pr_header = 0;
	const EVP_CIPHER *evp_cipher = NULL;
	const EVP_MD *evp_md = NULL;
	int decrypt = 0;
#ifndef _WIN32
	int multi = 0;
	struct sigaction sa;
#endif
	const char *errstr = NULL;

	if (pledge("stdio proc", NULL) == -1) {
		perror("pledge");
		exit(1);
	}

	usertime = -1;

	memset(results, 0, sizeof(results));
	memset(dsa_key, 0, sizeof(dsa_key));
	for (i = 0; i < EC_NUM; i++)
		ecdsa[i] = NULL;
	for (i = 0; i < EC_NUM; i++) {
		ecdh_a[i] = NULL;
		ecdh_b[i] = NULL;
	}

	memset(rsa_key, 0, sizeof(rsa_key));
	for (i = 0; i < RSA_NUM; i++)
		rsa_key[i] = NULL;

	if ((buf = real_buf = malloc(BUFSIZE + MAX_UNALIGN)) == NULL) {
		BIO_printf(bio_err, "out of memory\n");
		goto end;
	}
	if ((buf2 = real_buf2 = malloc(BUFSIZE + MAX_UNALIGN)) == NULL) {
		BIO_printf(bio_err, "out of memory\n");
		goto end;
	}
	memset(c, 0, sizeof(c));
	memset(DES_iv, 0, sizeof(DES_iv));
	memset(iv, 0, sizeof(iv));

	for (i = 0; i < ALGOR_NUM; i++)
		doit[i] = 0;
	for (i = 0; i < RSA_NUM; i++)
		rsa_doit[i] = 0;
	for (i = 0; i < DSA_NUM; i++)
		dsa_doit[i] = 0;
	for (i = 0; i < EC_NUM; i++)
		ecdsa_doit[i] = 0;
	for (i = 0; i < EC_NUM; i++)
		ecdh_doit[i] = 0;


	j = 0;
	argc--;
	argv++;
	while (argc) {
		if (argc > 0 && strcmp(*argv, "-elapsed") == 0) {
			usertime = 0;
			j--;	/* Otherwise, -elapsed gets confused with an
				 * algorithm. */
		} else if (argc > 0 && strcmp(*argv, "-evp") == 0) {
			argc--;
			argv++;
			if (argc == 0) {
				BIO_printf(bio_err, "no EVP given\n");
				goto end;
			}
			evp_cipher = EVP_get_cipherbyname(*argv);
			if (!evp_cipher) {
				evp_md = EVP_get_digestbyname(*argv);
			}
			if (!evp_cipher && !evp_md) {
				BIO_printf(bio_err, "%s is an unknown cipher or digest\n", *argv);
				goto end;
			}
			doit[D_EVP] = 1;
		} else if (argc > 0 && strcmp(*argv, "-decrypt") == 0) {
			decrypt = 1;
			j--;	/* Otherwise, -decrypt gets confused with an
				 * algorithm. */
#ifndef _WIN32
		} else if (argc > 0 && strcmp(*argv, "-multi") == 0) {
			argc--;
			argv++;
			if (argc == 0) {
				BIO_printf(bio_err, "no multi count given\n");
				goto end;
			}
			multi = strtonum(argv[0], 1, INT_MAX, &errstr);
			if (errstr) {
				BIO_printf(bio_err, "bad multi count: %s", errstr);
				goto end;
			}
			j--;	/* Otherwise, -multi gets confused with an
				 * algorithm. */
#endif
		} else if (argc > 0 && strcmp(*argv, "-unaligned") == 0) {
			argc--;
			argv++;
			if (argc == 0) {
				BIO_printf(bio_err, "no alignment offset given\n");
				goto end;
			}
			unaligned = strtonum(argv[0], 0, MAX_UNALIGN, &errstr);
			if (errstr) {
				BIO_printf(bio_err, "bad alignment offset: %s",
				    errstr);
				goto end;
			}
			buf = real_buf + unaligned;
			buf2 = real_buf2 + unaligned;
			j--;	/* Otherwise, -unaligned gets confused with an
				 * algorithm. */
		} else if (argc > 0 && strcmp(*argv, "-mr") == 0) {
			mr = 1;
			j--;	/* Otherwise, -mr gets confused with an
				 * algorithm. */
		} else
#ifndef OPENSSL_NO_MD4
		if (strcmp(*argv, "md4") == 0)
			doit[D_MD4] = 1;
		else
#endif
#ifndef OPENSSL_NO_MD5
		if (strcmp(*argv, "md5") == 0)
			doit[D_MD5] = 1;
		else
#endif
#ifndef OPENSSL_NO_MD5
		if (strcmp(*argv, "hmac") == 0)
			doit[D_HMAC] = 1;
		else
#endif
#ifndef OPENSSL_NO_SHA
		if (strcmp(*argv, "sha1") == 0)
			doit[D_SHA1] = 1;
		else if (strcmp(*argv, "sha") == 0)
			doit[D_SHA1] = 1,
			    doit[D_SHA256] = 1,
			    doit[D_SHA512] = 1;
		else
#ifndef OPENSSL_NO_SHA256
		if (strcmp(*argv, "sha256") == 0)
			doit[D_SHA256] = 1;
		else
#endif
#ifndef OPENSSL_NO_SHA512
		if (strcmp(*argv, "sha512") == 0)
			doit[D_SHA512] = 1;
		else
#endif
#endif
#ifndef OPENSSL_NO_WHIRLPOOL
		if (strcmp(*argv, "whirlpool") == 0)
			doit[D_WHIRLPOOL] = 1;
		else
#endif
#ifndef OPENSSL_NO_RIPEMD
		if (strcmp(*argv, "ripemd") == 0)
			doit[D_RMD160] = 1;
		else if (strcmp(*argv, "rmd160") == 0)
			doit[D_RMD160] = 1;
		else if (strcmp(*argv, "ripemd160") == 0)
			doit[D_RMD160] = 1;
		else
#endif
#ifndef OPENSSL_NO_RC4
		if (strcmp(*argv, "rc4") == 0)
			doit[D_RC4] = 1;
		else
#endif
#ifndef OPENSSL_NO_DES
		if (strcmp(*argv, "des-cbc") == 0)
			doit[D_CBC_DES] = 1;
		else if (strcmp(*argv, "des-ede3") == 0)
			doit[D_EDE3_DES] = 1;
		else
#endif
#ifndef OPENSSL_NO_AES
		if (strcmp(*argv, "aes-128-cbc") == 0)
			doit[D_CBC_128_AES] = 1;
		else if (strcmp(*argv, "aes-192-cbc") == 0)
			doit[D_CBC_192_AES] = 1;
		else if (strcmp(*argv, "aes-256-cbc") == 0)
			doit[D_CBC_256_AES] = 1;
		else if (strcmp(*argv, "aes-128-ige") == 0)
			doit[D_IGE_128_AES] = 1;
		else if (strcmp(*argv, "aes-192-ige") == 0)
			doit[D_IGE_192_AES] = 1;
		else if (strcmp(*argv, "aes-256-ige") == 0)
			doit[D_IGE_256_AES] = 1;
		else
#endif
#ifndef OPENSSL_NO_CAMELLIA
		if (strcmp(*argv, "camellia-128-cbc") == 0)
			doit[D_CBC_128_CML] = 1;
		else if (strcmp(*argv, "camellia-192-cbc") == 0)
			doit[D_CBC_192_CML] = 1;
		else if (strcmp(*argv, "camellia-256-cbc") == 0)
			doit[D_CBC_256_CML] = 1;
		else
#endif
#ifndef RSA_NULL
		if (strcmp(*argv, "openssl") == 0) {
			RSA_set_default_method(RSA_PKCS1_SSLeay());
			j--;
		} else
#endif
		if (strcmp(*argv, "dsa512") == 0)
			dsa_doit[R_DSA_512] = 2;
		else if (strcmp(*argv, "dsa1024") == 0)
			dsa_doit[R_DSA_1024] = 2;
		else if (strcmp(*argv, "dsa2048") == 0)
			dsa_doit[R_DSA_2048] = 2;
		else if (strcmp(*argv, "rsa512") == 0)
			rsa_doit[R_RSA_512] = 2;
		else if (strcmp(*argv, "rsa1024") == 0)
			rsa_doit[R_RSA_1024] = 2;
		else if (strcmp(*argv, "rsa2048") == 0)
			rsa_doit[R_RSA_2048] = 2;
		else if (strcmp(*argv, "rsa4096") == 0)
			rsa_doit[R_RSA_4096] = 2;
		else
#ifndef OPENSSL_NO_RC2
		if (strcmp(*argv, "rc2-cbc") == 0)
			doit[D_CBC_RC2] = 1;
		else if (strcmp(*argv, "rc2") == 0)
			doit[D_CBC_RC2] = 1;
		else
#endif
#ifndef OPENSSL_NO_IDEA
		if (strcmp(*argv, "idea-cbc") == 0)
			doit[D_CBC_IDEA] = 1;
		else if (strcmp(*argv, "idea") == 0)
			doit[D_CBC_IDEA] = 1;
		else
#endif
#ifndef OPENSSL_NO_BF
		if (strcmp(*argv, "bf-cbc") == 0)
			doit[D_CBC_BF] = 1;
		else if (strcmp(*argv, "blowfish") == 0)
			doit[D_CBC_BF] = 1;
		else if (strcmp(*argv, "bf") == 0)
			doit[D_CBC_BF] = 1;
		else
#endif
#ifndef OPENSSL_NO_CAST
		if (strcmp(*argv, "cast-cbc") == 0)
			doit[D_CBC_CAST] = 1;
		else if (strcmp(*argv, "cast") == 0)
			doit[D_CBC_CAST] = 1;
		else if (strcmp(*argv, "cast5") == 0)
			doit[D_CBC_CAST] = 1;
		else
#endif
#ifndef OPENSSL_NO_DES
		if (strcmp(*argv, "des") == 0) {
			doit[D_CBC_DES] = 1;
			doit[D_EDE3_DES] = 1;
		} else
#endif
#ifndef OPENSSL_NO_AES
		if (strcmp(*argv, "aes") == 0) {
			doit[D_CBC_128_AES] = 1;
			doit[D_CBC_192_AES] = 1;
			doit[D_CBC_256_AES] = 1;
		} else if (strcmp(*argv, "ghash") == 0)
			doit[D_GHASH] = 1;
		else if (strcmp(*argv,"aes-128-gcm") == 0)
			doit[D_AES_128_GCM]=1;
		else if (strcmp(*argv,"aes-256-gcm") == 0)
			doit[D_AES_256_GCM]=1;
		else
#endif
#ifndef OPENSSL_NO_CAMELLIA
		if (strcmp(*argv, "camellia") == 0) {
			doit[D_CBC_128_CML] = 1;
			doit[D_CBC_192_CML] = 1;
			doit[D_CBC_256_CML] = 1;
		} else
#endif
#if !defined(OPENSSL_NO_CHACHA) && !defined(OPENSSL_NO_POLY1305)
		if (strcmp(*argv,"chacha20-poly1305") == 0)
			doit[D_CHACHA20_POLY1305]=1;
		else
#endif
		if (strcmp(*argv, "rsa") == 0) {
			rsa_doit[R_RSA_512] = 1;
			rsa_doit[R_RSA_1024] = 1;
			rsa_doit[R_RSA_2048] = 1;
			rsa_doit[R_RSA_4096] = 1;
		} else if (strcmp(*argv, "dsa") == 0) {
			dsa_doit[R_DSA_512] = 1;
			dsa_doit[R_DSA_1024] = 1;
			dsa_doit[R_DSA_2048] = 1;
		} else if (strcmp(*argv, "ecdsap160") == 0)
			ecdsa_doit[R_EC_P160] = 2;
		else if (strcmp(*argv, "ecdsap192") == 0)
			ecdsa_doit[R_EC_P192] = 2;
		else if (strcmp(*argv, "ecdsap224") == 0)
			ecdsa_doit[R_EC_P224] = 2;
		else if (strcmp(*argv, "ecdsap256") == 0)
			ecdsa_doit[R_EC_P256] = 2;
		else if (strcmp(*argv, "ecdsap384") == 0)
			ecdsa_doit[R_EC_P384] = 2;
		else if (strcmp(*argv, "ecdsap521") == 0)
			ecdsa_doit[R_EC_P521] = 2;
		else if (strcmp(*argv, "ecdsa") == 0) {
			for (i = 0; i < EC_NUM; i++)
				ecdsa_doit[i] = 1;
		} else if (strcmp(*argv, "ecdhp160") == 0)
			ecdh_doit[R_EC_P160] = 2;
		else if (strcmp(*argv, "ecdhp192") == 0)
			ecdh_doit[R_EC_P192] = 2;
		else if (strcmp(*argv, "ecdhp224") == 0)
			ecdh_doit[R_EC_P224] = 2;
		else if (strcmp(*argv, "ecdhp256") == 0)
			ecdh_doit[R_EC_P256] = 2;
		else if (strcmp(*argv, "ecdhp384") == 0)
			ecdh_doit[R_EC_P384] = 2;
		else if (strcmp(*argv, "ecdhp521") == 0)
			ecdh_doit[R_EC_P521] = 2;
		else if (strcmp(*argv, "ecdh") == 0) {
			for (i = 0; i < EC_NUM; i++)
				ecdh_doit[i] = 1;
		} else {
			BIO_printf(bio_err, "Error: bad option or value\n");
			BIO_printf(bio_err, "\n");
			BIO_printf(bio_err, "Available values:\n");
#ifndef OPENSSL_NO_MD4
			BIO_printf(bio_err, "md4      ");
#endif
#ifndef OPENSSL_NO_MD5
			BIO_printf(bio_err, "md5      ");
#ifndef OPENSSL_NO_HMAC
			BIO_printf(bio_err, "hmac     ");
#endif
#endif
#ifndef OPENSSL_NO_SHA1
			BIO_printf(bio_err, "sha1     ");
#endif
#ifndef OPENSSL_NO_SHA256
			BIO_printf(bio_err, "sha256   ");
#endif
#ifndef OPENSSL_NO_SHA512
			BIO_printf(bio_err, "sha512   ");
#endif
#ifndef OPENSSL_NO_WHIRLPOOL
			BIO_printf(bio_err, "whirlpool");
#endif
#ifndef OPENSSL_NO_RIPEMD160
			BIO_printf(bio_err, "rmd160");
#endif
#if !defined(OPENSSL_NO_MD2) || \
    !defined(OPENSSL_NO_MD4) || !defined(OPENSSL_NO_MD5) || \
    !defined(OPENSSL_NO_SHA1) || !defined(OPENSSL_NO_RIPEMD160) || \
    !defined(OPENSSL_NO_WHIRLPOOL)
			BIO_printf(bio_err, "\n");
#endif

#ifndef OPENSSL_NO_IDEA
			BIO_printf(bio_err, "idea-cbc ");
#endif
#ifndef OPENSSL_NO_RC2
			BIO_printf(bio_err, "rc2-cbc  ");
#endif
#ifndef OPENSSL_NO_BF
			BIO_printf(bio_err, "bf-cbc   ");
#endif
#ifndef OPENSSL_NO_DES
			BIO_printf(bio_err, "des-cbc  des-ede3\n");
#endif
#ifndef OPENSSL_NO_AES
			BIO_printf(bio_err, "aes-128-cbc aes-192-cbc aes-256-cbc ");
			BIO_printf(bio_err, "aes-128-ige aes-192-ige aes-256-ige\n");
			BIO_printf(bio_err, "aes-128-gcm aes-256-gcm ");
#endif
#ifndef OPENSSL_NO_CAMELLIA
			BIO_printf(bio_err, "\n");
			BIO_printf(bio_err, "camellia-128-cbc camellia-192-cbc camellia-256-cbc ");
#endif
#ifndef OPENSSL_NO_RC4
			BIO_printf(bio_err, "rc4");
#endif
#if !defined(OPENSSL_NO_CHACHA) && !defined(OPENSSL_NO_POLY1305)
			BIO_printf(bio_err," chacha20-poly1305");
#endif
			BIO_printf(bio_err, "\n");

			BIO_printf(bio_err, "rsa512   rsa1024  rsa2048  rsa4096\n");

			BIO_printf(bio_err, "dsa512   dsa1024  dsa2048\n");
			BIO_printf(bio_err, "ecdsap160 ecdsap192 ecdsap224 ecdsap256 ecdsap384 ecdsap521\n");
			BIO_printf(bio_err, "ecdhp160  ecdhp192  ecdhp224  ecdhp256  ecdhp384  ecdhp521\n");

#ifndef OPENSSL_NO_IDEA
			BIO_printf(bio_err, "idea     ");
#endif
#ifndef OPENSSL_NO_RC2
			BIO_printf(bio_err, "rc2      ");
#endif
#ifndef OPENSSL_NO_DES
			BIO_printf(bio_err, "des      ");
#endif
#ifndef OPENSSL_NO_AES
			BIO_printf(bio_err, "aes      ");
#endif
#ifndef OPENSSL_NO_CAMELLIA
			BIO_printf(bio_err, "camellia ");
#endif
			BIO_printf(bio_err, "rsa      ");
#ifndef OPENSSL_NO_BF
			BIO_printf(bio_err, "blowfish");
#endif
#if !defined(OPENSSL_NO_IDEA) || !defined(OPENSSL_NO_SEED) || \
    !defined(OPENSSL_NO_RC2) || !defined(OPENSSL_NO_DES) || \
    !defined(OPENSSL_NO_RSA) || !defined(OPENSSL_NO_BF) || \
    !defined(OPENSSL_NO_AES) || !defined(OPENSSL_NO_CAMELLIA)
			BIO_printf(bio_err, "\n");
#endif

			BIO_printf(bio_err, "\n");
			BIO_printf(bio_err, "Available options:\n");
			BIO_printf(bio_err, "-elapsed        measure time in real time instead of CPU user time.\n");
			BIO_printf(bio_err, "-evp e          use EVP e.\n");
			BIO_printf(bio_err, "-decrypt        time decryption instead of encryption (only EVP).\n");
			BIO_printf(bio_err, "-mr             produce machine readable output.\n");
#ifndef _WIN32
			BIO_printf(bio_err, "-multi n        run n benchmarks in parallel.\n");
#endif
			BIO_printf(bio_err, "-unaligned n    use buffers with offset n from proper alignment.\n");
			goto end;
		}
		argc--;
		argv++;
		j++;
	}

#ifndef _WIN32
	if (multi && do_multi(multi))
		goto show_res;
#endif

	if (j == 0) {
		for (i = 0; i < ALGOR_NUM; i++) {
			if (i != D_EVP)
				doit[i] = 1;
		}
		for (i = 0; i < RSA_NUM; i++)
			rsa_doit[i] = 1;
		for (i = 0; i < DSA_NUM; i++)
			dsa_doit[i] = 1;
		for (i = 0; i < EC_NUM; i++)
			ecdsa_doit[i] = 1;
		for (i = 0; i < EC_NUM; i++)
			ecdh_doit[i] = 1;
	}
	for (i = 0; i < ALGOR_NUM; i++)
		if (doit[i])
			pr_header++;

	if (usertime == 0 && !mr)
		BIO_printf(bio_err, "You have chosen to measure elapsed time instead of user CPU time.\n");

	for (i = 0; i < RSA_NUM; i++) {
		const unsigned char *p;

		p = rsa_data[i];
		rsa_key[i] = d2i_RSAPrivateKey(NULL, &p, rsa_data_length[i]);
		if (rsa_key[i] == NULL) {
			BIO_printf(bio_err, "internal error loading RSA key number %d\n", i);
			goto end;
		}
	}

	dsa_key[0] = get_dsa512();
	dsa_key[1] = get_dsa1024();
	dsa_key[2] = get_dsa2048();

#ifndef OPENSSL_NO_DES
	DES_set_key_unchecked(&key, &sch);
	DES_set_key_unchecked(&key2, &sch2);
	DES_set_key_unchecked(&key3, &sch3);
#endif
#ifndef OPENSSL_NO_AES
	AES_set_encrypt_key(key16, 128, &aes_ks1);
	AES_set_encrypt_key(key24, 192, &aes_ks2);
	AES_set_encrypt_key(key32, 256, &aes_ks3);
#endif
#ifndef OPENSSL_NO_CAMELLIA
	Camellia_set_key(key16, 128, &camellia_ks1);
	Camellia_set_key(ckey24, 192, &camellia_ks2);
	Camellia_set_key(ckey32, 256, &camellia_ks3);
#endif
#ifndef OPENSSL_NO_IDEA
	idea_set_encrypt_key(key16, &idea_ks);
#endif
#ifndef OPENSSL_NO_RC4
	RC4_set_key(&rc4_ks, 16, key16);
#endif
#ifndef OPENSSL_NO_RC2
	RC2_set_key(&rc2_ks, 16, key16, 128);
#endif
#ifndef OPENSSL_NO_BF
	BF_set_key(&bf_ks, 16, key16);
#endif
#ifndef OPENSSL_NO_CAST
	CAST_set_key(&cast_ks, 16, key16);
#endif
	memset(rsa_c, 0, sizeof(rsa_c));
#define COND(c)	(run && count<0x7fffffff)
#define COUNT(d) (count)

#ifndef _WIN32
	memset(&sa, 0, sizeof(sa));
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = sig_done;
	sigaction(SIGALRM, &sa, NULL);
#endif

#ifndef OPENSSL_NO_MD4
	if (doit[D_MD4]) {
		for (j = 0; j < SIZE_NUM; j++) {
			print_message(names[D_MD4], c[D_MD4][j], lengths[j]);
			Time_F(START);
			for (count = 0, run = 1; COND(c[D_MD4][j]); count++)
				EVP_Digest(&(buf[0]), (unsigned long) lengths[j], &(md4[0]), NULL, EVP_md4(), NULL);
			d = Time_F(STOP);
			print_result(D_MD4, j, count, d);
		}
	}
#endif

#ifndef OPENSSL_NO_MD5
	if (doit[D_MD5]) {
		for (j = 0; j < SIZE_NUM; j++) {
			print_message(names[D_MD5], c[D_MD5][j], lengths[j]);
			Time_F(START);
			for (count = 0, run = 1; COND(c[D_MD5][j]); count++)
				EVP_Digest(&(buf[0]), (unsigned long) lengths[j], &(md5[0]), NULL, EVP_get_digestbyname("md5"), NULL);
			d = Time_F(STOP);
			print_result(D_MD5, j, count, d);
		}
	}
#endif

#if !defined(OPENSSL_NO_MD5) && !defined(OPENSSL_NO_HMAC)
	if (doit[D_HMAC]) {
		HMAC_CTX *hctx;

		if ((hctx = HMAC_CTX_new()) == NULL) {
			BIO_printf(bio_err, "Failed to allocate HMAC context.\n");
			goto end;
		}

		HMAC_Init_ex(hctx, (unsigned char *) "This is a key...",
		    16, EVP_md5(), NULL);

		for (j = 0; j < SIZE_NUM; j++) {
			print_message(names[D_HMAC], c[D_HMAC][j], lengths[j]);
			Time_F(START);
			for (count = 0, run = 1; COND(c[D_HMAC][j]); count++) {
				if (!HMAC_Init_ex(hctx, NULL, 0, NULL, NULL)) {
					HMAC_CTX_free(hctx);
					goto end;
				}
				if (!HMAC_Update(hctx, buf, lengths[j])) {
					HMAC_CTX_free(hctx);
					goto end;
				}
				if (!HMAC_Final(hctx, &(hmac[0]), NULL)) {
					HMAC_CTX_free(hctx);
					goto end;
				}
			}
			d = Time_F(STOP);
			print_result(D_HMAC, j, count, d);
		}
		HMAC_CTX_free(hctx);
	}
#endif
#ifndef OPENSSL_NO_SHA
	if (doit[D_SHA1]) {
		for (j = 0; j < SIZE_NUM; j++) {
			print_message(names[D_SHA1], c[D_SHA1][j], lengths[j]);
			Time_F(START);
			for (count = 0, run = 1; COND(c[D_SHA1][j]); count++)
				EVP_Digest(buf, (unsigned long) lengths[j], &(sha[0]), NULL, EVP_sha1(), NULL);
			d = Time_F(STOP);
			print_result(D_SHA1, j, count, d);
		}
	}
#ifndef OPENSSL_NO_SHA256
	if (doit[D_SHA256]) {
		for (j = 0; j < SIZE_NUM; j++) {
			print_message(names[D_SHA256], c[D_SHA256][j], lengths[j]);
			Time_F(START);
			for (count = 0, run = 1; COND(c[D_SHA256][j]); count++)
				SHA256(buf, lengths[j], sha256);
			d = Time_F(STOP);
			print_result(D_SHA256, j, count, d);
		}
	}
#endif

#ifndef OPENSSL_NO_SHA512
	if (doit[D_SHA512]) {
		for (j = 0; j < SIZE_NUM; j++) {
			print_message(names[D_SHA512], c[D_SHA512][j], lengths[j]);
			Time_F(START);
			for (count = 0, run = 1; COND(c[D_SHA512][j]); count++)
				SHA512(buf, lengths[j], sha512);
			d = Time_F(STOP);
			print_result(D_SHA512, j, count, d);
		}
	}
#endif
#endif

#ifndef OPENSSL_NO_WHIRLPOOL
	if (doit[D_WHIRLPOOL]) {
		for (j = 0; j < SIZE_NUM; j++) {
			print_message(names[D_WHIRLPOOL], c[D_WHIRLPOOL][j], lengths[j]);
			Time_F(START);
			for (count = 0, run = 1; COND(c[D_WHIRLPOOL][j]); count++)
				WHIRLPOOL(buf, lengths[j], whirlpool);
			d = Time_F(STOP);
			print_result(D_WHIRLPOOL, j, count, d);
		}
	}
#endif

#ifndef OPENSSL_NO_RIPEMD
	if (doit[D_RMD160]) {
		for (j = 0; j < SIZE_NUM; j++) {
			print_message(names[D_RMD160], c[D_RMD160][j], lengths[j]);
			Time_F(START);
			for (count = 0, run = 1; COND(c[D_RMD160][j]); count++)
				EVP_Digest(buf, (unsigned long) lengths[j], &(rmd160[0]), NULL, EVP_ripemd160(), NULL);
			d = Time_F(STOP);
			print_result(D_RMD160, j, count, d);
		}
	}
#endif
#ifndef OPENSSL_NO_RC4
	if (doit[D_RC4]) {
		for (j = 0; j < SIZE_NUM; j++) {
			print_message(names[D_RC4], c[D_RC4][j], lengths[j]);
			Time_F(START);
			for (count = 0, run = 1; COND(c[D_RC4][j]); count++)
				RC4(&rc4_ks, (unsigned int) lengths[j],
				    buf, buf);
			d = Time_F(STOP);
			print_result(D_RC4, j, count, d);
		}
	}
#endif
#ifndef OPENSSL_NO_DES
	if (doit[D_CBC_DES]) {
		for (j = 0; j < SIZE_NUM; j++) {
			print_message(names[D_CBC_DES], c[D_CBC_DES][j], lengths[j]);
			Time_F(START);
			for (count = 0, run = 1; COND(c[D_CBC_DES][j]); count++)
				DES_ncbc_encrypt(buf, buf, lengths[j], &sch,
				    &DES_iv, DES_ENCRYPT);
			d = Time_F(STOP);
			print_result(D_CBC_DES, j, count, d);
		}
	}
	if (doit[D_EDE3_DES]) {
		for (j = 0; j < SIZE_NUM; j++) {
			print_message(names[D_EDE3_DES], c[D_EDE3_DES][j], lengths[j]);
			Time_F(START);
			for (count = 0, run = 1; COND(c[D_EDE3_DES][j]); count++)
				DES_ede3_cbc_encrypt(buf, buf, lengths[j],
				    &sch, &sch2, &sch3,
				    &DES_iv, DES_ENCRYPT);
			d = Time_F(STOP);
			print_result(D_EDE3_DES, j, count, d);
		}
	}
#endif
#ifndef OPENSSL_NO_AES
	if (doit[D_CBC_128_AES]) {
		for (j = 0; j < SIZE_NUM; j++) {
			print_message(names[D_CBC_128_AES], c[D_CBC_128_AES][j], lengths[j]);
			Time_F(START);
			for (count = 0, run = 1; COND(c[D_CBC_128_AES][j]); count++)
				AES_cbc_encrypt(buf, buf,
				    (unsigned long) lengths[j], &aes_ks1,
				    iv, AES_ENCRYPT);
			d = Time_F(STOP);
			print_result(D_CBC_128_AES, j, count, d);
		}
	}
	if (doit[D_CBC_192_AES]) {
		for (j = 0; j < SIZE_NUM; j++) {
			print_message(names[D_CBC_192_AES], c[D_CBC_192_AES][j], lengths[j]);
			Time_F(START);
			for (count = 0, run = 1; COND(c[D_CBC_192_AES][j]); count++)
				AES_cbc_encrypt(buf, buf,
				    (unsigned long) lengths[j], &aes_ks2,
				    iv, AES_ENCRYPT);
			d = Time_F(STOP);
			print_result(D_CBC_192_AES, j, count, d);
		}
	}
	if (doit[D_CBC_256_AES]) {
		for (j = 0; j < SIZE_NUM; j++) {
			print_message(names[D_CBC_256_AES], c[D_CBC_256_AES][j], lengths[j]);
			Time_F(START);
			for (count = 0, run = 1; COND(c[D_CBC_256_AES][j]); count++)
				AES_cbc_encrypt(buf, buf,
				    (unsigned long) lengths[j], &aes_ks3,
				    iv, AES_ENCRYPT);
			d = Time_F(STOP);
			print_result(D_CBC_256_AES, j, count, d);
		}
	}
	if (doit[D_IGE_128_AES]) {
		for (j = 0; j < SIZE_NUM; j++) {
			print_message(names[D_IGE_128_AES], c[D_IGE_128_AES][j], lengths[j]);
			Time_F(START);
			for (count = 0, run = 1; COND(c[D_IGE_128_AES][j]); count++)
				AES_ige_encrypt(buf, buf2,
				    (unsigned long) lengths[j], &aes_ks1,
				    iv, AES_ENCRYPT);
			d = Time_F(STOP);
			print_result(D_IGE_128_AES, j, count, d);
		}
	}
	if (doit[D_IGE_192_AES]) {
		for (j = 0; j < SIZE_NUM; j++) {
			print_message(names[D_IGE_192_AES], c[D_IGE_192_AES][j], lengths[j]);
			Time_F(START);
			for (count = 0, run = 1; COND(c[D_IGE_192_AES][j]); count++)
				AES_ige_encrypt(buf, buf2,
				    (unsigned long) lengths[j], &aes_ks2,
				    iv, AES_ENCRYPT);
			d = Time_F(STOP);
			print_result(D_IGE_192_AES, j, count, d);
		}
	}
	if (doit[D_IGE_256_AES]) {
		for (j = 0; j < SIZE_NUM; j++) {
			print_message(names[D_IGE_256_AES], c[D_IGE_256_AES][j], lengths[j]);
			Time_F(START);
			for (count = 0, run = 1; COND(c[D_IGE_256_AES][j]); count++)
				AES_ige_encrypt(buf, buf2,
				    (unsigned long) lengths[j], &aes_ks3,
				    iv, AES_ENCRYPT);
			d = Time_F(STOP);
			print_result(D_IGE_256_AES, j, count, d);
		}
	}
	if (doit[D_GHASH]) {
		GCM128_CONTEXT *ctx = CRYPTO_gcm128_new(&aes_ks1, (block128_f) AES_encrypt);
		CRYPTO_gcm128_setiv(ctx, (unsigned char *) "0123456789ab", 12);

		for (j = 0; j < SIZE_NUM; j++) {
			print_message(names[D_GHASH], c[D_GHASH][j], lengths[j]);
			Time_F(START);
			for (count = 0, run = 1; COND(c[D_GHASH][j]); count++)
				CRYPTO_gcm128_aad(ctx, buf, lengths[j]);
			d = Time_F(STOP);
			print_result(D_GHASH, j, count, d);
		}
		CRYPTO_gcm128_release(ctx);
	}
	if (doit[D_AES_128_GCM]) {
		const EVP_AEAD *aead = EVP_aead_aes_128_gcm();
		static const unsigned char nonce[32] = {0};
		size_t buf_len, nonce_len;
		EVP_AEAD_CTX *ctx;

		if ((ctx = EVP_AEAD_CTX_new()) == NULL) {
			BIO_printf(bio_err,
			    "Failed to allocate aead context.\n");
			goto end;
		}

		EVP_AEAD_CTX_init(ctx, aead, key32, EVP_AEAD_key_length(aead),
		    EVP_AEAD_DEFAULT_TAG_LENGTH, NULL);
		nonce_len = EVP_AEAD_nonce_length(aead);

		for (j = 0; j < SIZE_NUM; j++) {
			print_message(names[D_AES_128_GCM],c[D_AES_128_GCM][j],lengths[j]);
			Time_F(START);
			for (count = 0, run = 1; COND(c[D_AES_128_GCM][j]); count++)
				EVP_AEAD_CTX_seal(ctx, buf, &buf_len, BUFSIZE, nonce,
				    nonce_len, buf, lengths[j], NULL, 0);
			d=Time_F(STOP);
			print_result(D_AES_128_GCM,j,count,d);
		}
		EVP_AEAD_CTX_free(ctx);
	}

	if (doit[D_AES_256_GCM]) {
		const EVP_AEAD *aead = EVP_aead_aes_256_gcm();
		static const unsigned char nonce[32] = {0};
		size_t buf_len, nonce_len;
		EVP_AEAD_CTX *ctx;

		if ((ctx = EVP_AEAD_CTX_new()) == NULL) {
			BIO_printf(bio_err,
			    "Failed to allocate aead context.\n");
			goto end;
		}

		EVP_AEAD_CTX_init(ctx, aead, key32, EVP_AEAD_key_length(aead),
		EVP_AEAD_DEFAULT_TAG_LENGTH, NULL);
		nonce_len = EVP_AEAD_nonce_length(aead);

		for (j = 0; j < SIZE_NUM; j++) {
			print_message(names[D_AES_256_GCM],c[D_AES_256_GCM][j],lengths[j]);
			Time_F(START);
			for (count = 0, run = 1; COND(c[D_AES_256_GCM][j]); count++)
				EVP_AEAD_CTX_seal(ctx, buf, &buf_len, BUFSIZE, nonce,
				    nonce_len, buf, lengths[j], NULL, 0);
			d=Time_F(STOP);
			print_result(D_AES_256_GCM, j, count, d);
		}
		EVP_AEAD_CTX_free(ctx);
	}
#endif
#if !defined(OPENSSL_NO_CHACHA) && !defined(OPENSSL_NO_POLY1305)
	if (doit[D_CHACHA20_POLY1305]) {
		const EVP_AEAD *aead = EVP_aead_chacha20_poly1305();
		static const unsigned char nonce[32] = {0};
		size_t buf_len, nonce_len;
		EVP_AEAD_CTX *ctx;

		if ((ctx = EVP_AEAD_CTX_new()) == NULL) {
			BIO_printf(bio_err,
			    "Failed to allocate aead context.\n");
			goto end;
		}

		EVP_AEAD_CTX_init(ctx, aead, key32, EVP_AEAD_key_length(aead),
		    EVP_AEAD_DEFAULT_TAG_LENGTH, NULL);
		nonce_len = EVP_AEAD_nonce_length(aead);

		for (j = 0; j < SIZE_NUM; j++) {
			print_message(names[D_CHACHA20_POLY1305],
			    c[D_CHACHA20_POLY1305][j], lengths[j]);
			Time_F(START);
			for (count = 0, run = 1; COND(c[D_CHACHA20_POLY1305][j]); count++)
				EVP_AEAD_CTX_seal(ctx, buf, &buf_len, BUFSIZE, nonce,
				    nonce_len, buf, lengths[j], NULL, 0);
			d=Time_F(STOP);
			print_result(D_CHACHA20_POLY1305, j, count, d);
		}
		EVP_AEAD_CTX_free(ctx);
	}
#endif
#ifndef OPENSSL_NO_CAMELLIA
	if (doit[D_CBC_128_CML]) {
		for (j = 0; j < SIZE_NUM; j++) {
			print_message(names[D_CBC_128_CML], c[D_CBC_128_CML][j], lengths[j]);
			Time_F(START);
			for (count = 0, run = 1; COND(c[D_CBC_128_CML][j]); count++)
				Camellia_cbc_encrypt(buf, buf,
				    (unsigned long) lengths[j], &camellia_ks1,
				    iv, CAMELLIA_ENCRYPT);
			d = Time_F(STOP);
			print_result(D_CBC_128_CML, j, count, d);
		}
	}
	if (doit[D_CBC_192_CML]) {
		for (j = 0; j < SIZE_NUM; j++) {
			print_message(names[D_CBC_192_CML], c[D_CBC_192_CML][j], lengths[j]);
			Time_F(START);
			for (count = 0, run = 1; COND(c[D_CBC_192_CML][j]); count++)
				Camellia_cbc_encrypt(buf, buf,
				    (unsigned long) lengths[j], &camellia_ks2,
				    iv, CAMELLIA_ENCRYPT);
			d = Time_F(STOP);
			print_result(D_CBC_192_CML, j, count, d);
		}
	}
	if (doit[D_CBC_256_CML]) {
		for (j = 0; j < SIZE_NUM; j++) {
			print_message(names[D_CBC_256_CML], c[D_CBC_256_CML][j], lengths[j]);
			Time_F(START);
			for (count = 0, run = 1; COND(c[D_CBC_256_CML][j]); count++)
				Camellia_cbc_encrypt(buf, buf,
				    (unsigned long) lengths[j], &camellia_ks3,
				    iv, CAMELLIA_ENCRYPT);
			d = Time_F(STOP);
			print_result(D_CBC_256_CML, j, count, d);
		}
	}
#endif
#ifndef OPENSSL_NO_IDEA
	if (doit[D_CBC_IDEA]) {
		for (j = 0; j < SIZE_NUM; j++) {
			print_message(names[D_CBC_IDEA], c[D_CBC_IDEA][j], lengths[j]);
			Time_F(START);
			for (count = 0, run = 1; COND(c[D_CBC_IDEA][j]); count++)
				idea_cbc_encrypt(buf, buf,
				    (unsigned long) lengths[j], &idea_ks,
				    iv, IDEA_ENCRYPT);
			d = Time_F(STOP);
			print_result(D_CBC_IDEA, j, count, d);
		}
	}
#endif
#ifndef OPENSSL_NO_RC2
	if (doit[D_CBC_RC2]) {
		for (j = 0; j < SIZE_NUM; j++) {
			print_message(names[D_CBC_RC2], c[D_CBC_RC2][j], lengths[j]);
			Time_F(START);
			for (count = 0, run = 1; COND(c[D_CBC_RC2][j]); count++)
				RC2_cbc_encrypt(buf, buf,
				    (unsigned long) lengths[j], &rc2_ks,
				    iv, RC2_ENCRYPT);
			d = Time_F(STOP);
			print_result(D_CBC_RC2, j, count, d);
		}
	}
#endif
#ifndef OPENSSL_NO_BF
	if (doit[D_CBC_BF]) {
		for (j = 0; j < SIZE_NUM; j++) {
			print_message(names[D_CBC_BF], c[D_CBC_BF][j], lengths[j]);
			Time_F(START);
			for (count = 0, run = 1; COND(c[D_CBC_BF][j]); count++)
				BF_cbc_encrypt(buf, buf,
				    (unsigned long) lengths[j], &bf_ks,
				    iv, BF_ENCRYPT);
			d = Time_F(STOP);
			print_result(D_CBC_BF, j, count, d);
		}
	}
#endif
#ifndef OPENSSL_NO_CAST
	if (doit[D_CBC_CAST]) {
		for (j = 0; j < SIZE_NUM; j++) {
			print_message(names[D_CBC_CAST], c[D_CBC_CAST][j], lengths[j]);
			Time_F(START);
			for (count = 0, run = 1; COND(c[D_CBC_CAST][j]); count++)
				CAST_cbc_encrypt(buf, buf,
				    (unsigned long) lengths[j], &cast_ks,
				    iv, CAST_ENCRYPT);
			d = Time_F(STOP);
			print_result(D_CBC_CAST, j, count, d);
		}
	}
#endif

	if (doit[D_EVP]) {
		for (j = 0; j < SIZE_NUM; j++) {
			if (evp_cipher) {
				EVP_CIPHER_CTX *ctx;
				int outl;

				names[D_EVP] =
				    OBJ_nid2ln(EVP_CIPHER_nid(evp_cipher));
				/*
				 * -O3 -fschedule-insns messes up an
				 * optimization here!  names[D_EVP] somehow
				 * becomes NULL
				 */
				print_message(names[D_EVP], save_count,
				    lengths[j]);

				if ((ctx = EVP_CIPHER_CTX_new()) == NULL) {
					BIO_printf(bio_err, "Failed to "
					    "allocate cipher context.\n");
					goto end;
				}
				if (decrypt)
					EVP_DecryptInit_ex(ctx, evp_cipher, NULL, key16, iv);
				else
					EVP_EncryptInit_ex(ctx, evp_cipher, NULL, key16, iv);
				EVP_CIPHER_CTX_set_padding(ctx, 0);

				Time_F(START);
				if (decrypt)
					for (count = 0, run = 1; COND(save_count * 4 * lengths[0] / lengths[j]); count++)
						EVP_DecryptUpdate(ctx, buf, &outl, buf, lengths[j]);
				else
					for (count = 0, run = 1; COND(save_count * 4 * lengths[0] / lengths[j]); count++)
						EVP_EncryptUpdate(ctx, buf, &outl, buf, lengths[j]);
				if (decrypt)
					EVP_DecryptFinal_ex(ctx, buf, &outl);
				else
					EVP_EncryptFinal_ex(ctx, buf, &outl);
				d = Time_F(STOP);
				EVP_CIPHER_CTX_free(ctx);
			}
			if (evp_md) {
				names[D_EVP] = OBJ_nid2ln(EVP_MD_type(evp_md));
				print_message(names[D_EVP], save_count,
				    lengths[j]);

				Time_F(START);
				for (count = 0, run = 1; COND(save_count * 4 * lengths[0] / lengths[j]); count++)
					EVP_Digest(buf, lengths[j], &(md[0]), NULL, evp_md, NULL);

				d = Time_F(STOP);
			}
			print_result(D_EVP, j, count, d);
		}
	}
	arc4random_buf(buf, 36);
	for (j = 0; j < RSA_NUM; j++) {
		int ret;
		if (!rsa_doit[j])
			continue;
		ret = RSA_sign(NID_md5_sha1, buf, 36, buf2, &rsa_num, rsa_key[j]);
		if (ret == 0) {
			BIO_printf(bio_err, "RSA sign failure.  No RSA sign will be done.\n");
			ERR_print_errors(bio_err);
			rsa_count = 1;
		} else {
			pkey_print_message("private", "rsa",
			    rsa_c[j][0], rsa_bits[j],
			    RSA_SECONDS);
/*			RSA_blinding_on(rsa_key[j],NULL); */
			Time_F(START);
			for (count = 0, run = 1; COND(rsa_c[j][0]); count++) {
				ret = RSA_sign(NID_md5_sha1, buf, 36, buf2,
				    &rsa_num, rsa_key[j]);
				if (ret == 0) {
					BIO_printf(bio_err,
					    "RSA sign failure\n");
					ERR_print_errors(bio_err);
					count = 1;
					break;
				}
			}
			d = Time_F(STOP);
			BIO_printf(bio_err, mr ? "+R1:%ld:%d:%.2f\n"
			    : "%ld %d bit private RSA in %.2fs\n",
			    count, rsa_bits[j], d);
			rsa_results[j][0] = d / (double) count;
			rsa_count = count;
		}

		ret = RSA_verify(NID_md5_sha1, buf, 36, buf2, rsa_num, rsa_key[j]);
		if (ret <= 0) {
			BIO_printf(bio_err, "RSA verify failure.  No RSA verify will be done.\n");
			ERR_print_errors(bio_err);
			rsa_doit[j] = 0;
		} else {
			pkey_print_message("public", "rsa",
			    rsa_c[j][1], rsa_bits[j],
			    RSA_SECONDS);
			Time_F(START);
			for (count = 0, run = 1; COND(rsa_c[j][1]); count++) {
				ret = RSA_verify(NID_md5_sha1, buf, 36, buf2,
				    rsa_num, rsa_key[j]);
				if (ret <= 0) {
					BIO_printf(bio_err,
					    "RSA verify failure\n");
					ERR_print_errors(bio_err);
					count = 1;
					break;
				}
			}
			d = Time_F(STOP);
			BIO_printf(bio_err, mr ? "+R2:%ld:%d:%.2f\n"
			    : "%ld %d bit public RSA in %.2fs\n",
			    count, rsa_bits[j], d);
			rsa_results[j][1] = d / (double) count;
		}

		if (rsa_count <= 1) {
			/* if longer than 10s, don't do any more */
			for (j++; j < RSA_NUM; j++)
				rsa_doit[j] = 0;
		}
	}

	arc4random_buf(buf, 20);
	for (j = 0; j < DSA_NUM; j++) {
		unsigned int kk;
		int ret;

		if (!dsa_doit[j])
			continue;
/*		DSA_generate_key(dsa_key[j]); */
/*		DSA_sign_setup(dsa_key[j],NULL); */
		ret = DSA_sign(EVP_PKEY_DSA, buf, 20, buf2,
		    &kk, dsa_key[j]);
		if (ret == 0) {
			BIO_printf(bio_err, "DSA sign failure.  No DSA sign will be done.\n");
			ERR_print_errors(bio_err);
			rsa_count = 1;
		} else {
			pkey_print_message("sign", "dsa",
			    dsa_c[j][0], dsa_bits[j],
			    DSA_SECONDS);
			Time_F(START);
			for (count = 0, run = 1; COND(dsa_c[j][0]); count++) {
				ret = DSA_sign(EVP_PKEY_DSA, buf, 20, buf2,
				    &kk, dsa_key[j]);
				if (ret == 0) {
					BIO_printf(bio_err,
					    "DSA sign failure\n");
					ERR_print_errors(bio_err);
					count = 1;
					break;
				}
			}
			d = Time_F(STOP);
			BIO_printf(bio_err, mr ? "+R3:%ld:%d:%.2f\n"
			    : "%ld %d bit DSA signs in %.2fs\n",
			    count, dsa_bits[j], d);
			dsa_results[j][0] = d / (double) count;
			rsa_count = count;
		}

		ret = DSA_verify(EVP_PKEY_DSA, buf, 20, buf2,
		    kk, dsa_key[j]);
		if (ret <= 0) {
			BIO_printf(bio_err, "DSA verify failure.  No DSA verify will be done.\n");
			ERR_print_errors(bio_err);
			dsa_doit[j] = 0;
		} else {
			pkey_print_message("verify", "dsa",
			    dsa_c[j][1], dsa_bits[j],
			    DSA_SECONDS);
			Time_F(START);
			for (count = 0, run = 1; COND(dsa_c[j][1]); count++) {
				ret = DSA_verify(EVP_PKEY_DSA, buf, 20, buf2,
				    kk, dsa_key[j]);
				if (ret <= 0) {
					BIO_printf(bio_err,
					    "DSA verify failure\n");
					ERR_print_errors(bio_err);
					count = 1;
					break;
				}
			}
			d = Time_F(STOP);
			BIO_printf(bio_err, mr ? "+R4:%ld:%d:%.2f\n"
			    : "%ld %d bit DSA verify in %.2fs\n",
			    count, dsa_bits[j], d);
			dsa_results[j][1] = d / (double) count;
		}

		if (rsa_count <= 1) {
			/* if longer than 10s, don't do any more */
			for (j++; j < DSA_NUM; j++)
				dsa_doit[j] = 0;
		}
	}

	for (j = 0; j < EC_NUM; j++) {
		int ret;

		if (!ecdsa_doit[j])
			continue;	/* Ignore Curve */
		ecdsa[j] = EC_KEY_new_by_curve_name(test_curves[j]);
		if (ecdsa[j] == NULL) {
			BIO_printf(bio_err, "ECDSA failure.\n");
			ERR_print_errors(bio_err);
			rsa_count = 1;
		} else {
			EC_KEY_precompute_mult(ecdsa[j], NULL);

			/* Perform ECDSA signature test */
			EC_KEY_generate_key(ecdsa[j]);
			ret = ECDSA_sign(0, buf, 20, ecdsasig,
			    &ecdsasiglen, ecdsa[j]);
			if (ret == 0) {
				BIO_printf(bio_err, "ECDSA sign failure.  No ECDSA sign will be done.\n");
				ERR_print_errors(bio_err);
				rsa_count = 1;
			} else {
				pkey_print_message("sign", "ecdsa",
				    ecdsa_c[j][0],
				    test_curves_bits[j],
				    ECDSA_SECONDS);

				Time_F(START);
				for (count = 0, run = 1; COND(ecdsa_c[j][0]);
				    count++) {
					ret = ECDSA_sign(0, buf, 20,
					    ecdsasig, &ecdsasiglen,
					    ecdsa[j]);
					if (ret == 0) {
						BIO_printf(bio_err, "ECDSA sign failure\n");
						ERR_print_errors(bio_err);
						count = 1;
						break;
					}
				}
				d = Time_F(STOP);

				BIO_printf(bio_err, mr ? "+R5:%ld:%d:%.2f\n" :
				    "%ld %d bit ECDSA signs in %.2fs \n",
				    count, test_curves_bits[j], d);
				ecdsa_results[j][0] = d / (double) count;
				rsa_count = count;
			}

			/* Perform ECDSA verification test */
			ret = ECDSA_verify(0, buf, 20, ecdsasig,
			    ecdsasiglen, ecdsa[j]);
			if (ret != 1) {
				BIO_printf(bio_err, "ECDSA verify failure.  No ECDSA verify will be done.\n");
				ERR_print_errors(bio_err);
				ecdsa_doit[j] = 0;
			} else {
				pkey_print_message("verify", "ecdsa",
				    ecdsa_c[j][1],
				    test_curves_bits[j],
				    ECDSA_SECONDS);
				Time_F(START);
				for (count = 0, run = 1; COND(ecdsa_c[j][1]); count++) {
					ret = ECDSA_verify(0, buf, 20, ecdsasig, ecdsasiglen, ecdsa[j]);
					if (ret != 1) {
						BIO_printf(bio_err, "ECDSA verify failure\n");
						ERR_print_errors(bio_err);
						count = 1;
						break;
					}
				}
				d = Time_F(STOP);
				BIO_printf(bio_err, mr ? "+R6:%ld:%d:%.2f\n"
				    : "%ld %d bit ECDSA verify in %.2fs\n",
				    count, test_curves_bits[j], d);
				ecdsa_results[j][1] = d / (double) count;
			}

			if (rsa_count <= 1) {
				/* if longer than 10s, don't do any more */
				for (j++; j < EC_NUM; j++)
					ecdsa_doit[j] = 0;
			}
		}
	}

	for (j = 0; j < EC_NUM; j++) {
		if (!ecdh_doit[j])
			continue;
		ecdh_a[j] = EC_KEY_new_by_curve_name(test_curves[j]);
		ecdh_b[j] = EC_KEY_new_by_curve_name(test_curves[j]);
		if ((ecdh_a[j] == NULL) || (ecdh_b[j] == NULL)) {
			BIO_printf(bio_err, "ECDH failure.\n");
			ERR_print_errors(bio_err);
			rsa_count = 1;
		} else {
			/* generate two ECDH key pairs */
			if (!EC_KEY_generate_key(ecdh_a[j]) ||
			    !EC_KEY_generate_key(ecdh_b[j])) {
				BIO_printf(bio_err, "ECDH key generation failure.\n");
				ERR_print_errors(bio_err);
				rsa_count = 1;
			} else {
				/*
				 * If field size is not more than 24 octets,
				 * then use SHA-1 hash of result; otherwise,
				 * use result (see section 4.8 of
				 * draft-ietf-tls-ecc-03.txt).
				 */
				int field_size, outlen;
				void *(*kdf) (const void *in, size_t inlen, void *out, size_t * xoutlen);
				field_size = EC_GROUP_get_degree(EC_KEY_get0_group(ecdh_a[j]));
				if (field_size <= 24 * 8) {
					outlen = KDF1_SHA1_len;
					kdf = KDF1_SHA1;
				} else {
					outlen = (field_size + 7) / 8;
					kdf = NULL;
				}
				secret_size_a = ECDH_compute_key(secret_a, outlen,
				    EC_KEY_get0_public_key(ecdh_b[j]),
				    ecdh_a[j], kdf);
				secret_size_b = ECDH_compute_key(secret_b, outlen,
				    EC_KEY_get0_public_key(ecdh_a[j]),
				    ecdh_b[j], kdf);
				if (secret_size_a != secret_size_b)
					ecdh_checks = 0;
				else
					ecdh_checks = 1;

				for (secret_idx = 0;
				    (secret_idx < secret_size_a)
				    && (ecdh_checks == 1);
				    secret_idx++) {
					if (secret_a[secret_idx] != secret_b[secret_idx])
						ecdh_checks = 0;
				}

				if (ecdh_checks == 0) {
					BIO_printf(bio_err,
					    "ECDH computations don't match.\n");
					ERR_print_errors(bio_err);
					rsa_count = 1;
				} else {
					pkey_print_message("", "ecdh",
					    ecdh_c[j][0],
					    test_curves_bits[j],
					    ECDH_SECONDS);
					Time_F(START);
					for (count = 0, run = 1;
					     COND(ecdh_c[j][0]); count++) {
						ECDH_compute_key(secret_a,
						    outlen,
						    EC_KEY_get0_public_key(ecdh_b[j]),
						    ecdh_a[j], kdf);
					}
					d = Time_F(STOP);
					BIO_printf(bio_err, mr
					    ? "+R7:%ld:%d:%.2f\n"
					    : "%ld %d-bit ECDH ops in %.2fs\n",
					    count, test_curves_bits[j], d);
					ecdh_results[j][0] = d / (double) count;
					rsa_count = count;
				}
			}
		}


		if (rsa_count <= 1) {
			/* if longer than 10s, don't do any more */
			for (j++; j < EC_NUM; j++)
				ecdh_doit[j] = 0;
		}
	}
#ifndef _WIN32
show_res:
#endif
	if (!mr) {
		fprintf(stdout, "%s\n", SSLeay_version(SSLEAY_VERSION));
		fprintf(stdout, "%s\n", SSLeay_version(SSLEAY_BUILT_ON));
		fprintf(stdout, "%s\n", SSLeay_version(SSLEAY_CFLAGS));
	}
	if (pr_header) {
		if (mr)
			fprintf(stdout, "+H");
		else {
			fprintf(stdout, "The 'numbers' are in 1000s of bytes per second processed.\n");
			fprintf(stdout, "type        ");
		}
		for (j = 0; j < SIZE_NUM; j++)
			fprintf(stdout, mr ? ":%d" : "%7d bytes", lengths[j]);
		fprintf(stdout, "\n");
	}
	for (k = 0; k < ALGOR_NUM; k++) {
		if (!doit[k])
			continue;
		if (mr)
			fprintf(stdout, "+F:%d:%s", k, names[k]);
		else
			fprintf(stdout, "%-13s", names[k]);
		for (j = 0; j < SIZE_NUM; j++) {
			if (results[k][j] > 10000 && !mr)
				fprintf(stdout, " %11.2fk", results[k][j] / 1e3);
			else
				fprintf(stdout, mr ? ":%.2f" : " %11.2f ", results[k][j]);
		}
		fprintf(stdout, "\n");
	}
	j = 1;
	for (k = 0; k < RSA_NUM; k++) {
		if (!rsa_doit[k])
			continue;
		if (j && !mr) {
			printf("%18ssign    verify    sign/s verify/s\n", " ");
			j = 0;
		}
		if (mr)
			fprintf(stdout, "+F2:%u:%u:%f:%f\n",
			    k, rsa_bits[k], rsa_results[k][0],
			    rsa_results[k][1]);
		else
			fprintf(stdout, "rsa %4u bits %8.6fs %8.6fs %8.1f %8.1f\n",
			    rsa_bits[k], rsa_results[k][0], rsa_results[k][1],
			    1.0 / rsa_results[k][0], 1.0 / rsa_results[k][1]);
	}
	j = 1;
	for (k = 0; k < DSA_NUM; k++) {
		if (!dsa_doit[k])
			continue;
		if (j && !mr) {
			printf("%18ssign    verify    sign/s verify/s\n", " ");
			j = 0;
		}
		if (mr)
			fprintf(stdout, "+F3:%u:%u:%f:%f\n",
			    k, dsa_bits[k], dsa_results[k][0], dsa_results[k][1]);
		else
			fprintf(stdout, "dsa %4u bits %8.6fs %8.6fs %8.1f %8.1f\n",
			    dsa_bits[k], dsa_results[k][0], dsa_results[k][1],
			    1.0 / dsa_results[k][0], 1.0 / dsa_results[k][1]);
	}
	j = 1;
	for (k = 0; k < EC_NUM; k++) {
		if (!ecdsa_doit[k])
			continue;
		if (j && !mr) {
			printf("%30ssign    verify    sign/s verify/s\n", " ");
			j = 0;
		}
		if (mr)
			fprintf(stdout, "+F4:%u:%u:%f:%f\n",
			    k, test_curves_bits[k],
			    ecdsa_results[k][0], ecdsa_results[k][1]);
		else
			fprintf(stdout,
			    "%4u bit ecdsa (%s) %8.4fs %8.4fs %8.1f %8.1f\n",
			    test_curves_bits[k],
			    test_curves_names[k],
			    ecdsa_results[k][0], ecdsa_results[k][1],
			    1.0 / ecdsa_results[k][0], 1.0 / ecdsa_results[k][1]);
	}


	j = 1;
	for (k = 0; k < EC_NUM; k++) {
		if (!ecdh_doit[k])
			continue;
		if (j && !mr) {
			printf("%30sop      op/s\n", " ");
			j = 0;
		}
		if (mr)
			fprintf(stdout, "+F5:%u:%u:%f:%f\n",
			    k, test_curves_bits[k],
			    ecdh_results[k][0], 1.0 / ecdh_results[k][0]);

		else
			fprintf(stdout, "%4u bit ecdh (%s) %8.4fs %8.1f\n",
			    test_curves_bits[k],
			    test_curves_names[k],
			    ecdh_results[k][0], 1.0 / ecdh_results[k][0]);
	}

	mret = 0;

 end:
	ERR_print_errors(bio_err);
	free(real_buf);
	free(real_buf2);
	for (i = 0; i < RSA_NUM; i++)
		if (rsa_key[i] != NULL)
			RSA_free(rsa_key[i]);
	for (i = 0; i < DSA_NUM; i++)
		if (dsa_key[i] != NULL)
			DSA_free(dsa_key[i]);

	for (i = 0; i < EC_NUM; i++)
		if (ecdsa[i] != NULL)
			EC_KEY_free(ecdsa[i]);
	for (i = 0; i < EC_NUM; i++) {
		if (ecdh_a[i] != NULL)
			EC_KEY_free(ecdh_a[i]);
		if (ecdh_b[i] != NULL)
			EC_KEY_free(ecdh_b[i]);
	}


	return (mret);
}

static void
print_message(const char *s, long num, int length)
{
	BIO_printf(bio_err, mr ? "+DT:%s:%d:%d\n"
	    : "Doing %s for %ds on %d size blocks: ", s, SECONDS, length);
	(void) BIO_flush(bio_err);
	alarm(SECONDS);
}

static void
pkey_print_message(const char *str, const char *str2, long num,
    int bits, int tm)
{
	BIO_printf(bio_err, mr ? "+DTP:%d:%s:%s:%d\n"
	    : "Doing %d bit %s %s for %ds: ", bits, str, str2, tm);
	(void) BIO_flush(bio_err);
	alarm(tm);
}

static void
print_result(int alg, int run_no, int count, double time_used)
{
#ifdef _WIN32
	speed_alarm_free(run);
#endif
	BIO_printf(bio_err, mr ? "+R:%d:%s:%f\n"
	    : "%d %s in %.2fs\n", count, names[alg], time_used);
	results[alg][run_no] = ((double) count) / time_used * lengths[run_no];
}

#ifndef _WIN32
static char *
sstrsep(char **string, const char *delim)
{
	char isdelim[256];
	char *token = *string;

	if (**string == 0)
		return NULL;

	memset(isdelim, 0, sizeof isdelim);
	isdelim[0] = 1;

	while (*delim) {
		isdelim[(unsigned char) (*delim)] = 1;
		delim++;
	}

	while (!isdelim[(unsigned char) (**string)]) {
		(*string)++;
	}

	if (**string) {
		**string = 0;
		(*string)++;
	}
	return token;
}

static int
do_multi(int multi)
{
	int n;
	int fd[2];
	int *fds;
	static char sep[] = ":";
	const char *errstr = NULL;

	fds = reallocarray(NULL, multi, sizeof *fds);
	if (fds == NULL) {
		fprintf(stderr, "reallocarray failure\n");
		exit(1);
	}
	for (n = 0; n < multi; ++n) {
		if (pipe(fd) == -1) {
			fprintf(stderr, "pipe failure\n");
			exit(1);
		}
		fflush(stdout);
		fflush(stderr);
		if (fork()) {
			close(fd[1]);
			fds[n] = fd[0];
		} else {
			close(fd[0]);
			close(1);
			if (dup(fd[1]) == -1) {
				fprintf(stderr, "dup failed\n");
				exit(1);
			}
			close(fd[1]);
			mr = 1;
			usertime = 0;
			free(fds);
			return 0;
		}
		printf("Forked child %d\n", n);
	}

	/* for now, assume the pipe is long enough to take all the output */
	for (n = 0; n < multi; ++n) {
		FILE *f;
		char buf[1024];
		char *p;

		f = fdopen(fds[n], "r");
		while (fgets(buf, sizeof buf, f)) {
			p = strchr(buf, '\n');
			if (p)
				*p = '\0';
			if (buf[0] != '+') {
				fprintf(stderr, "Don't understand line '%s' from child %d\n",
				    buf, n);
				continue;
			}
			printf("Got: %s from %d\n", buf, n);
			if (!strncmp(buf, "+F:", 3)) {
				int alg;
				int j;

				p = buf + 3;
				alg = strtonum(sstrsep(&p, sep),
				    0, ALGOR_NUM - 1, &errstr);
				sstrsep(&p, sep);
				for (j = 0; j < SIZE_NUM; ++j)
					results[alg][j] += atof(sstrsep(&p, sep));
			} else if (!strncmp(buf, "+F2:", 4)) {
				int k;
				double d;

				p = buf + 4;
				k = strtonum(sstrsep(&p, sep),
				    0, ALGOR_NUM - 1, &errstr);
				sstrsep(&p, sep);

				d = atof(sstrsep(&p, sep));
				if (n)
					rsa_results[k][0] = 1 / (1 / rsa_results[k][0] + 1 / d);
				else
					rsa_results[k][0] = d;

				d = atof(sstrsep(&p, sep));
				if (n)
					rsa_results[k][1] = 1 / (1 / rsa_results[k][1] + 1 / d);
				else
					rsa_results[k][1] = d;
			} else if (!strncmp(buf, "+F2:", 4)) {
				int k;
				double d;

				p = buf + 4;
				k = strtonum(sstrsep(&p, sep),
				    0, ALGOR_NUM - 1, &errstr);
				sstrsep(&p, sep);

				d = atof(sstrsep(&p, sep));
				if (n)
					rsa_results[k][0] = 1 / (1 / rsa_results[k][0] + 1 / d);
				else
					rsa_results[k][0] = d;

				d = atof(sstrsep(&p, sep));
				if (n)
					rsa_results[k][1] = 1 / (1 / rsa_results[k][1] + 1 / d);
				else
					rsa_results[k][1] = d;
			} else if (!strncmp(buf, "+F3:", 4)) {
				int k;
				double d;

				p = buf + 4;
				k = strtonum(sstrsep(&p, sep),
				    0, ALGOR_NUM - 1, &errstr);
				sstrsep(&p, sep);

				d = atof(sstrsep(&p, sep));
				if (n)
					dsa_results[k][0] = 1 / (1 / dsa_results[k][0] + 1 / d);
				else
					dsa_results[k][0] = d;

				d = atof(sstrsep(&p, sep));
				if (n)
					dsa_results[k][1] = 1 / (1 / dsa_results[k][1] + 1 / d);
				else
					dsa_results[k][1] = d;
			} else if (!strncmp(buf, "+F4:", 4)) {
				int k;
				double d;

				p = buf + 4;
				k = strtonum(sstrsep(&p, sep),
				    0, ALGOR_NUM - 1, &errstr);
				sstrsep(&p, sep);

				d = atof(sstrsep(&p, sep));
				if (n)
					ecdsa_results[k][0] = 1 / (1 / ecdsa_results[k][0] + 1 / d);
				else
					ecdsa_results[k][0] = d;

				d = atof(sstrsep(&p, sep));
				if (n)
					ecdsa_results[k][1] = 1 / (1 / ecdsa_results[k][1] + 1 / d);
				else
					ecdsa_results[k][1] = d;
			} else if (!strncmp(buf, "+F5:", 4)) {
				int k;
				double d;

				p = buf + 4;
				k = strtonum(sstrsep(&p, sep),
				    0, ALGOR_NUM - 1, &errstr);
				sstrsep(&p, sep);

				d = atof(sstrsep(&p, sep));
				if (n)
					ecdh_results[k][0] = 1 / (1 / ecdh_results[k][0] + 1 / d);
				else
					ecdh_results[k][0] = d;

			} else if (!strncmp(buf, "+H:", 3)) {
			} else
				fprintf(stderr, "Unknown type '%s' from child %d\n", buf, n);
		}

		fclose(f);
	}
	free(fds);
	return 1;
}
#endif
#endif
