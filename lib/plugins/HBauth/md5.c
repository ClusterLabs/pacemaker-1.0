/*
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * MD5Context structure, pass it to MD5Init, call MD5Update as
 * needed on buffers full of bytes, and then call MD5Final, which
 * will fill a supplied 16-byte array with the digest.
 *
 * Cleaned up by Mitja Sarp <mitja@lysator.liu.se> for heartbeat
 *
 */

#include <portability.h>
#include <stdint.h>
#include <stdio.h>		/* for sprintf() */
#include <string.h>		/* for memcpy() */
#include <sys/types.h>		/* for stupid systems */
#include <netinet/in.h>		/* for ntohl() */
#include <HBauth.h>

#define PIL_PLUGINTYPE		HB_AUTH_TYPE
#define PIL_PLUGIN		md5
#define PIL_PLUGINTYPE_S	"HBauth"
#define PIL_PLUGIN_S		"md5"
#define PIL_PLUGINLICENSE	LICENSE_PUBDOM
#define PIL_PLUGINLICENSEURL	URL_PUBDOM
#include <pils/plugin.h>


/* The Ops we export to the world... */
static int md5_auth_calc(const struct HBauth_info *t
,	const char * text, char * result, int resultlen);

static int md5_auth_needskey(void);

/* Authentication plugin operations */
static struct HBAuthOps md5ops =
{	md5_auth_calc
,	md5_auth_needskey
};

/*
 * The following two functions are only exported to the plugin infrastructure.
 */

/*
 * md5close is called as part of shutting down the md5 HBauth plugin.
 * If there was any global data allocated, or file descriptors opened, etc.
 * which is associated with the plugin, and not a single interface
 * in particular, here's our chance to clean it up.
 */
static void md5closepi(PILPlugin* pi)
{
}

/*
 * md5closeintf called as part of shutting down the md5 HBauth interface.
 * If there was any global data allocated, or file descriptors opened, etc.
 * which is associated with the md5 implementation, here's our chance
 * to clean it up.
 */
static PIL_rc md5closeintf(PILInterface* pi, void* pd)
{
	return PIL_OK;
}

PIL_PLUGIN_BOILERPLATE("1.0", Debug, md5closepi);

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
	,	&md5ops
	,	md5closeintf		/*close */
	,	&OurInterface
	,	&OurImports
	,	interfprivate); 
}

/*
 *	Real work starts here ;-)
 */

#define MD5_DIGESTSIZE  16
#define MD5_BLOCKSIZE   64

typedef struct MD5Context_st {
	uint32_t buf[4];
	uint32_t bytes[2];
	uint32_t in[16];
}MD5Context;

#define md5byte unsigned char

struct MD5Context {
	uint32_t buf[4];
	uint32_t bytes[2];
	uint32_t in[16];
};

static void MD5Init(MD5Context *context);
static void MD5Update(MD5Context *context, md5byte const *buf, unsigned len);
static void MD5Final(unsigned char digest[16], MD5Context *context);
static void MD5Transform(uint32_t buf[4], uint32_t const in[16]);

static int
md5_auth_needskey(void) 
{ 
	return 1;	/* Yes, we require (need) a key */
}


#define byteSwap(buf,words)

/*
 * Start MD5 accumulation.  Set bit count to 0 and buffer to mysterious
 * initialization constants.
 */
static void
MD5Init(MD5Context *ctx)
{
	ctx->buf[0] = 0x67452301ul;
	ctx->buf[1] = 0xefcdab89ul;
	ctx->buf[2] = 0x98badcfeul;
	ctx->buf[3] = 0x10325476ul;

	ctx->bytes[0] = 0;
	ctx->bytes[1] = 0;
}

/*
 * Update context to reflect the concatenation of another buffer full
 * of bytes.
 */
static void
MD5Update(MD5Context *ctx, md5byte const *buf, unsigned len)
{
	uint32_t t;

	/* Update byte count */

	t = ctx->bytes[0];
	if ((ctx->bytes[0] = t + len) < t)
		ctx->bytes[1]++;	/* Carry from low to high */

	t = 64 - (t & 0x3f);	/* Space available in ctx->in (at least 1) */
	if (t > len) {
		memcpy((md5byte *)ctx->in + 64 - t, buf, len);
		return;
	}
	/* First chunk is an odd size */
	memcpy((md5byte *)ctx->in + 64 - t, buf, t);
	byteSwap(ctx->in, 16);
	MD5Transform(ctx->buf, ctx->in);
	buf += t;
	len -= t;

	/* Process data in 64-byte chunks */
	while (len >= 64) {
		memcpy(ctx->in, buf, 64);
		byteSwap(ctx->in, 16);
		MD5Transform(ctx->buf, ctx->in);
		buf += 64;
		len -= 64;
	}

	/* Handle any remaining bytes of data. */
	memcpy(ctx->in, buf, len);
}

/*
 * Final wrapup - pad to 64-byte boundary with the bit pattern 
 * 1 0* (64-bit count of bits processed, MSB-first)
 */
static void
MD5Final(md5byte digest[16], MD5Context *ctx)
{
	int count = ctx->bytes[0] & 0x3f;	/* Number of bytes in ctx->in */
	md5byte *p = (md5byte *)ctx->in + count;

	/* Set the first char of padding to 0x80.  There is always room. */
	*p++ = 0x80;

	/* Bytes of padding needed to make 56 bytes (-8..55) */
	count = 56 - 1 - count;

	if (count < 0) {	/* Padding forces an extra block */
		memset(p, 0, count + 8);
		byteSwap(ctx->in, 16);
		MD5Transform(ctx->buf, ctx->in);
		p = (md5byte *)ctx->in;
		count = 56;
	}
	memset(p, 0, count);
	byteSwap(ctx->in, 14);

	/* Append length in bits and transform */
	ctx->in[14] = ctx->bytes[0] << 3;
	ctx->in[15] = ctx->bytes[1] << 3 | ctx->bytes[0] >> 29;
	MD5Transform(ctx->buf, ctx->in);

	byteSwap(ctx->buf, 4);
	memcpy(digest, ctx->buf, 16);
	memset(ctx, 0, sizeof(ctx));	/* In case it's sensitive */
}

/* The four core functions - F1 is optimized somewhat */

/* #define F1(x, y, z) (x & y | ~x & z) */
#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

/* This is the central step in the MD5 algorithm. */
#define MD5STEP(f,w,x,y,z,in,s) \
	 (w += f(x,y,z) + in, w = (w<<s | w>>(32-s)) + x)

/*
 * The core of the MD5 algorithm, this alters an existing MD5 hash to
 * reflect the addition of 16 longwords of new data.  MD5Update blocks
 * the data and converts bytes into longwords for this routine.
 */
static void
MD5Transform(uint32_t buf[4], uint32_t const in[16])
{
	register uint32_t a, b, c, d;

	a = buf[0];
	b = buf[1];
	c = buf[2];
	d = buf[3];

	MD5STEP(F1, a, b, c, d, in[0] + 0xd76aa478ul, 7);
	MD5STEP(F1, d, a, b, c, in[1] + 0xe8c7b756ul, 12);
	MD5STEP(F1, c, d, a, b, in[2] + 0x242070dbul, 17);
	MD5STEP(F1, b, c, d, a, in[3] + 0xc1bdceeeul, 22);
	MD5STEP(F1, a, b, c, d, in[4] + 0xf57c0faful, 7);
	MD5STEP(F1, d, a, b, c, in[5] + 0x4787c62aul, 12);
	MD5STEP(F1, c, d, a, b, in[6] + 0xa8304613ul, 17);
	MD5STEP(F1, b, c, d, a, in[7] + 0xfd469501ul, 22);
	MD5STEP(F1, a, b, c, d, in[8] + 0x698098d8ul, 7);
	MD5STEP(F1, d, a, b, c, in[9] + 0x8b44f7aful, 12);
	MD5STEP(F1, c, d, a, b, in[10] + 0xffff5bb1ul, 17);
	MD5STEP(F1, b, c, d, a, in[11] + 0x895cd7beul, 22);
	MD5STEP(F1, a, b, c, d, in[12] + 0x6b901122ul, 7);
	MD5STEP(F1, d, a, b, c, in[13] + 0xfd987193ul, 12);
	MD5STEP(F1, c, d, a, b, in[14] + 0xa679438eul, 17);
	MD5STEP(F1, b, c, d, a, in[15] + 0x49b40821ul, 22);

	MD5STEP(F2, a, b, c, d, in[1] + 0xf61e2562ul, 5);
	MD5STEP(F2, d, a, b, c, in[6] + 0xc040b340ul, 9);
	MD5STEP(F2, c, d, a, b, in[11] + 0x265e5a51ul, 14);
	MD5STEP(F2, b, c, d, a, in[0] + 0xe9b6c7aaul, 20);
	MD5STEP(F2, a, b, c, d, in[5] + 0xd62f105dul, 5);
	MD5STEP(F2, d, a, b, c, in[10] + 0x02441453ul, 9);
	MD5STEP(F2, c, d, a, b, in[15] + 0xd8a1e681ul, 14);
	MD5STEP(F2, b, c, d, a, in[4] + 0xe7d3fbc8ul, 20);
	MD5STEP(F2, a, b, c, d, in[9] + 0x21e1cde6ul, 5);
	MD5STEP(F2, d, a, b, c, in[14] + 0xc33707d6ul, 9);
	MD5STEP(F2, c, d, a, b, in[3] + 0xf4d50d87ul, 14);
	MD5STEP(F2, b, c, d, a, in[8] + 0x455a14edul, 20);
	MD5STEP(F2, a, b, c, d, in[13] + 0xa9e3e905ul, 5);
	MD5STEP(F2, d, a, b, c, in[2] + 0xfcefa3f8ul, 9);
	MD5STEP(F2, c, d, a, b, in[7] + 0x676f02d9ul, 14);
	MD5STEP(F2, b, c, d, a, in[12] + 0x8d2a4c8aul, 20);

	MD5STEP(F3, a, b, c, d, in[5] + 0xfffa3942ul, 4);
	MD5STEP(F3, d, a, b, c, in[8] + 0x8771f681ul, 11);
	MD5STEP(F3, c, d, a, b, in[11] + 0x6d9d6122ul, 16);
	MD5STEP(F3, b, c, d, a, in[14] + 0xfde5380cul, 23);
	MD5STEP(F3, a, b, c, d, in[1] + 0xa4beea44ul, 4);
	MD5STEP(F3, d, a, b, c, in[4] + 0x4bdecfa9ul, 11);
	MD5STEP(F3, c, d, a, b, in[7] + 0xf6bb4b60ul, 16);
	MD5STEP(F3, b, c, d, a, in[10] + 0xbebfbc70ul, 23);
	MD5STEP(F3, a, b, c, d, in[13] + 0x289b7ec6ul, 4);
	MD5STEP(F3, d, a, b, c, in[0] + 0xeaa127faul, 11);
	MD5STEP(F3, c, d, a, b, in[3] + 0xd4ef3085ul, 16);
	MD5STEP(F3, b, c, d, a, in[6] + 0x04881d05ul, 23);
	MD5STEP(F3, a, b, c, d, in[9] + 0xd9d4d039ul, 4);
	MD5STEP(F3, d, a, b, c, in[12] + 0xe6db99e5ul, 11);
	MD5STEP(F3, c, d, a, b, in[15] + 0x1fa27cf8ul, 16);
	MD5STEP(F3, b, c, d, a, in[2] + 0xc4ac5665ul, 23);

	MD5STEP(F4, a, b, c, d, in[0] + 0xf4292244ul, 6);
	MD5STEP(F4, d, a, b, c, in[7] + 0x432aff97ul, 10);
	MD5STEP(F4, c, d, a, b, in[14] + 0xab9423a7ul, 15);
	MD5STEP(F4, b, c, d, a, in[5] + 0xfc93a039ul, 21);
	MD5STEP(F4, a, b, c, d, in[12] + 0x655b59c3ul, 6);
	MD5STEP(F4, d, a, b, c, in[3] + 0x8f0ccc92ul, 10);
	MD5STEP(F4, c, d, a, b, in[10] + 0xffeff47dul, 15);
	MD5STEP(F4, b, c, d, a, in[1] + 0x85845dd1ul, 21);
	MD5STEP(F4, a, b, c, d, in[8] + 0x6fa87e4ful, 6);
	MD5STEP(F4, d, a, b, c, in[15] + 0xfe2ce6e0ul, 10);
	MD5STEP(F4, c, d, a, b, in[6] + 0xa3014314ul, 15);
	MD5STEP(F4, b, c, d, a, in[13] + 0x4e0811a1ul, 21);
	MD5STEP(F4, a, b, c, d, in[4] + 0xf7537e82ul, 6);
	MD5STEP(F4, d, a, b, c, in[11] + 0xbd3af235ul, 10);
	MD5STEP(F4, c, d, a, b, in[2] + 0x2ad7d2bbul, 15);
	MD5STEP(F4, b, c, d, a, in[9] + 0xeb86d391ul, 21);

	buf[0] += a;
	buf[1] += b;
	buf[2] += c;
	buf[3] += d;
}

static int
md5_auth_calc(const struct HBauth_info *t, const char * text
,	char * result, int resultlen)
{

	MD5Context context;
	unsigned char digest[MD5_DIGESTSIZE];
	const char * key = t->key;
	/* inner padding - key XORd with ipad */
	unsigned char k_ipad[65];    
	/* outer padding - * key XORd with opad */
	unsigned char k_opad[65];    
	unsigned char tk[MD5_DIGESTSIZE];
	int i, text_len, key_len;


	if (resultlen <= (MD5_DIGESTSIZE+1) *2) {
		return 0;
	}
	text_len = strlen(text);
	key_len = strlen(key);
	
	/* if key is longer than MD5_BLOCKSIZE bytes reset it to key=MD5(key) */
	if (key_len > MD5_BLOCKSIZE) { 
		MD5Context      tctx;   
		MD5Init(&tctx);
		MD5Update(&tctx, key, key_len);
		MD5Final(tk, &tctx); 

		key = tk;
		key_len = MD5_DIGESTSIZE;
	}       
	/* start out by storing key in pads */
	memset(k_ipad, 0, sizeof k_ipad);
	memset(k_opad, 0, sizeof k_opad);
	memcpy(k_ipad, key, key_len);
	memcpy(k_opad, key, key_len);

	/* XOR key with ipad and opad values */
	for (i=0; i<MD5_BLOCKSIZE; i++) {
		k_ipad[i] ^= 0x36;
		k_opad[i] ^= 0x5c;
	}       
	/* perform inner MD5 */
	MD5Init(&context);                   /* init context for 1st pass */
	MD5Update(&context, k_ipad, MD5_BLOCKSIZE);     /* start with inner pad */
	MD5Update(&context, text, text_len); /* then text of datagram */
	MD5Final(digest, &context);          /* finish up 1st pass */
	/* perform outer MD5 */
	MD5Init(&context);                   /* init context for 2nd pass */
	MD5Update(&context, k_opad, MD5_BLOCKSIZE);     /* start with outer pad */
	MD5Update(&context, digest, MD5_DIGESTSIZE);     /* then results of 1st hash */

	MD5Final(digest, &context);          /* finish up 2nd pass */

	/* And show the result in human-readable form */
	for (i = 0; i < MD5_DIGESTSIZE; i++) {
		sprintf(result, "%02x", digest[i]);
		result +=2;
	}
	return 1;
}
