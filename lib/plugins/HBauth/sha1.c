/*
SHA-1 in C
By Steve Reid <steve@edmweb.com>
100% Public Domain

Test Vectors (from FIPS PUB 180-1)
"abc"
  A9993E36 4706816A BA3E2571 7850C26C 9CD0D89D
"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
  84983E44 1C3BD26E BAAE4AA1 F95129E5 E54670F1
A million repetitions of "a"
  34AA973C D4C4DAA4 F61EEB2B DBAD2731 6534016F

  Cleaned up by Mitja Sarp <mitja@lysator.liu.se> for heartbeat
*/

/* #define LITTLE_ENDIAN * This should be #define'd if true. */
/* #define SHA1HANDSOFF * Copies data before messing with it. */
#define SHA1HANDSOFF 1

#include <lha_internal.h>
#include <stdio.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <string.h>
#include <sys/types.h>
#include <HBauth.h>

#define PIL_PLUGINTYPE		HB_AUTH_TYPE
#define PIL_PLUGINTYPE_S	"HBauth"
#define PIL_PLUGIN		sha1
#define PIL_PLUGIN_S		"sha1"
#define PIL_PLUGINLICENSE	LICENSE_PUBDOM
#define PIL_PLUGINLICENSEURL	URL_PUBDOM
#include <pils/plugin.h>


#define SHA_DIGESTSIZE  20
#define SHA_BLOCKSIZE   64

typedef struct SHA1Context_st{
	    uint32_t state[5];
	    uint32_t count[2];
	    unsigned char buffer[64];
} SHA1_CTX;

void SHA1Transform(uint32_t state[5], const unsigned char buffer[64]);
void SHA1Init(SHA1_CTX* context);
void SHA1Update(SHA1_CTX* context, const unsigned char* data, unsigned int len);
void SHA1Final(unsigned char digest[20], SHA1_CTX* context);

static int sha1_auth_calc (const struct HBauth_info *info
,			    const void * text, size_t textlen, char * result, int resultlen);

static int sha1_auth_needskey(void);

static struct HBAuthOps sha1Ops =
{	sha1_auth_calc
,	sha1_auth_needskey
};

PIL_PLUGIN_BOILERPLATE2("1.0", Debug)
static const PILPluginImports*  PluginImports;
static PILPlugin*               OurPlugin;
static PILInterface*		OurInterface;
static void*			OurImports;
static void*			interfprivate;

/*
 *
 * Our plugin initialization and registration function
 * It gets called when the plugin gets loaded.
 */
PIL_rc
PIL_PLUGIN_INIT(PILPlugin*us, const PILPluginImports* imports);

PIL_rc
PIL_PLUGIN_INIT(PILPlugin*us, const PILPluginImports* imports)
{
	/* Force the compiler to do a little type checking */
	(void)(PILPluginInitFun)PIL_PLUGIN_INIT;

	PluginImports = imports;
	OurPlugin = us;

	/* Register ourself as a plugin */
	imports->register_plugin(us, &OurPIExports);  

	/*  Register our interfaces */
 	return imports->register_interface(us, PIL_PLUGINTYPE_S,  PIL_PLUGIN_S
	,	&sha1Ops
	,	NULL		/*close */
	,	&OurInterface
	,	&OurImports
	,	interfprivate); 
}

static int
sha1_auth_needskey(void) 
{ 
	return 1;
}


#define rol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

/* blk0() and blk() perform the initial expand. */
/* I got the idea of expanding during the round function from SSLeay */
#ifdef LITTLE_ENDIAN
#define blk0(i) (block->l[i] = (rol(block->l[i],24)&0xFF00FF00U) \
    |(rol(block->l[i],8)&0x00FF00FFU))
#else
#define blk0(i) block->l[i]
#endif
#define blk(i) (block->l[i&15] = rol(block->l[(i+13)&15]^block->l[(i+8)&15] \
    ^block->l[(i+2)&15]^block->l[i&15],1))

/* (R0+R1), R2, R3, R4 are the different operations used in SHA1 */
#define R0(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk0(i)+0x5A827999u+rol(v,5);w=rol(w,30);
#define R1(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk(i)+0x5A827999u+rol(v,5);w=rol(w,30);
#define R2(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0x6ED9EBA1u+rol(v,5);w=rol(w,30);
#define R3(v,w,x,y,z,i) z+=(((w|x)&y)|(w&x))+blk(i)+0x8F1BBCDCu+rol(v,5);w=rol(w,30);
#define R4(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0xCA62C1D6u+rol(v,5);w=rol(w,30);


/* Hash a single 512-bit block. This is the core of the algorithm. */

void SHA1Transform(uint32_t state[5], const unsigned char buffer[64])
{
uint32_t a, b, c, d, e;
typedef union {
    unsigned char c[64];
    uint32_t l[16];
} CHAR64LONG16;
CHAR64LONG16* block;
#ifdef SHA1HANDSOFF
CHAR64LONG16 workspace;
    block = &workspace;
    memcpy(block, buffer, 64);
#else
    block = (CHAR64LONG16*)buffer;
#endif
    /* Copy context->state[] to working vars */
    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];
    /* 4 rounds of 20 operations each. Loop unrolled. */
    R0(a,b,c,d,e, 0); R0(e,a,b,c,d, 1); R0(d,e,a,b,c, 2); R0(c,d,e,a,b, 3);
    R0(b,c,d,e,a, 4); R0(a,b,c,d,e, 5); R0(e,a,b,c,d, 6); R0(d,e,a,b,c, 7);
    R0(c,d,e,a,b, 8); R0(b,c,d,e,a, 9); R0(a,b,c,d,e,10); R0(e,a,b,c,d,11);
    R0(d,e,a,b,c,12); R0(c,d,e,a,b,13); R0(b,c,d,e,a,14); R0(a,b,c,d,e,15);
    R1(e,a,b,c,d,16); R1(d,e,a,b,c,17); R1(c,d,e,a,b,18); R1(b,c,d,e,a,19);
    R2(a,b,c,d,e,20); R2(e,a,b,c,d,21); R2(d,e,a,b,c,22); R2(c,d,e,a,b,23);
    R2(b,c,d,e,a,24); R2(a,b,c,d,e,25); R2(e,a,b,c,d,26); R2(d,e,a,b,c,27);
    R2(c,d,e,a,b,28); R2(b,c,d,e,a,29); R2(a,b,c,d,e,30); R2(e,a,b,c,d,31);
    R2(d,e,a,b,c,32); R2(c,d,e,a,b,33); R2(b,c,d,e,a,34); R2(a,b,c,d,e,35);
    R2(e,a,b,c,d,36); R2(d,e,a,b,c,37); R2(c,d,e,a,b,38); R2(b,c,d,e,a,39);
    R3(a,b,c,d,e,40); R3(e,a,b,c,d,41); R3(d,e,a,b,c,42); R3(c,d,e,a,b,43);
    R3(b,c,d,e,a,44); R3(a,b,c,d,e,45); R3(e,a,b,c,d,46); R3(d,e,a,b,c,47);
    R3(c,d,e,a,b,48); R3(b,c,d,e,a,49); R3(a,b,c,d,e,50); R3(e,a,b,c,d,51);
    R3(d,e,a,b,c,52); R3(c,d,e,a,b,53); R3(b,c,d,e,a,54); R3(a,b,c,d,e,55);
    R3(e,a,b,c,d,56); R3(d,e,a,b,c,57); R3(c,d,e,a,b,58); R3(b,c,d,e,a,59);
    R4(a,b,c,d,e,60); R4(e,a,b,c,d,61); R4(d,e,a,b,c,62); R4(c,d,e,a,b,63);
    R4(b,c,d,e,a,64); R4(a,b,c,d,e,65); R4(e,a,b,c,d,66); R4(d,e,a,b,c,67);
    R4(c,d,e,a,b,68); R4(b,c,d,e,a,69); R4(a,b,c,d,e,70); R4(e,a,b,c,d,71);
    R4(d,e,a,b,c,72); R4(c,d,e,a,b,73); R4(b,c,d,e,a,74); R4(a,b,c,d,e,75);
    R4(e,a,b,c,d,76); R4(d,e,a,b,c,77); R4(c,d,e,a,b,78); R4(b,c,d,e,a,79);
    /* Add the working vars back into context.state[] */
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    /* Wipe variables */
    a = b = c = d = e = 0;
}


/* SHA1Init - Initialize new context */

void SHA1Init(SHA1_CTX* context)
{
    /* SHA1 initialization constants */
    context->state[0] = 0x67452301u;
    context->state[1] = 0xEFCDAB89u;
    context->state[2] = 0x98BADCFEu;
    context->state[3] = 0x10325476u;
    context->state[4] = 0xC3D2E1F0u;
    context->count[0] = context->count[1] = 0;
}


/* Run your data through this. */

void SHA1Update(SHA1_CTX* context, const unsigned char* data, unsigned int len)
{
unsigned int i, j;

    j = (context->count[0] >> 3) & 63;
    if ((context->count[0] += len << 3) < (len << 3)) {
	context->count[1]++;
    }
    context->count[1] += (len >> 29);
    if ((j + len) > 63) {
        memcpy(&context->buffer[j], data, (i = 64-j));
        SHA1Transform(context->state, context->buffer);
        for ( ; i + 63 < len; i += 64) {
            SHA1Transform(context->state, &data[i]);
        }
        j = 0;
    } else {
	i = 0;
    }
    memcpy(&context->buffer[j], &data[i], len - i);
}


/* Add padding and return the message digest. */

void SHA1Final(unsigned char digest[20], SHA1_CTX* context)
{
    uint32_t i, j;
    unsigned char finalcount[8];
    unsigned char twohundred [] = "\200";
    unsigned char twozeroes [] = "\00";

    for (i = 0; i < 8; i++) {
        finalcount[i] = (unsigned char)((context->count[(i >= 4 ? 0 : 1)]
         >> ((3-(i & 3)) * 8) ) & 255);  /* Endian independent */
    }
    SHA1Update(context, twohundred, 1);
    while ((context->count[0] & 504) != 448) {
        SHA1Update(context, twozeroes, 1);
    }
    SHA1Update(context, finalcount, 8);  /* Should cause a SHA1Transform() */
    for (i = 0; i < 20; i++) {
        digest[i] = (unsigned char)
         ((context->state[i>>2] >> ((3-(i & 3)) * 8) ) & 255);
    }
    /* Wipe variables */
    i = j = 0;
    memset(context->buffer, 0, 64);
    memset(context->state, 0, 20);
    memset(context->count, 0, 8);
    memset(&finalcount, 0, 8);
#ifdef SHA1HANDSOFF  /* make SHA1Transform overwrite it's own static vars */
    SHA1Transform(context->state, context->buffer);
#endif
}


static int
sha1_auth_calc (const struct HBauth_info *info
,	    const void * text, size_t textlen, char * result, int resultlen)
{
	SHA1_CTX ictx, octx ;
	unsigned char   isha[SHA_DIGESTSIZE]; 
	unsigned char 	osha[SHA_DIGESTSIZE];
	unsigned char   tk[SHA_DIGESTSIZE];
	unsigned char   buf[SHA_BLOCKSIZE];
	int	i, key_len;
	unsigned char * key;

	if (resultlen <= SHA_DIGESTSIZE) {
		return FALSE;
	}

	key = (unsigned char *)g_strdup(info->key);

	key_len = strlen((char *)key);

	if (key_len > SHA_BLOCKSIZE) {
		SHA1_CTX         tctx ;
		SHA1Init(&tctx);
		SHA1Update(&tctx, key, key_len);
		SHA1Final(tk, &tctx);
		g_free(key);
		key = tk;
		key_len = SHA_DIGESTSIZE;
	}

	/**** Inner Digest ****/

	SHA1Init(&ictx) ;

	/* Pad the key for inner digest */
	for (i = 0 ; i < key_len ; ++i) { buf[i] = key[i] ^ 0x36;};
	/* Should this be a call to to memset? */
	for (i = key_len ; i < SHA_BLOCKSIZE ; ++i) { buf[i] = 0x36;};

	SHA1Update(&ictx, buf, SHA_BLOCKSIZE) ;
	SHA1Update(&ictx, (const unsigned char *)text, textlen) ;

	SHA1Final(isha, &ictx) ;

	/**** Outer Digest ****/

	SHA1Init(&octx) ;

	/* Pad the key for outer digest */

	for (i = 0 ; i < key_len ; ++i) {buf[i] = key[i] ^ 0x5C;};
	/* Should this be a call to memset? */
	for (i = key_len ; i < SHA_BLOCKSIZE ; ++i) { buf[i] = 0x5C;};

	SHA1Update(&octx, buf, SHA_BLOCKSIZE) ;
	SHA1Update(&octx, isha, SHA_DIGESTSIZE) ;
	SHA1Final(osha, &octx) ;

	result[0] = '\0';
	for (i = 0; i < SHA_DIGESTSIZE; i++) {
		sprintf((char *)tk, "%02x", osha[i]);
		strcat(result, (char *)tk);
	}
	if (key != tk) {
		g_free(key);
	}

	return TRUE;
}
