/*
 * The code to implement the MD5 message-digest algorithm is moved
 * to lib/clplumbing/md5.c
 *
 * Cleaned up by Mitja Sarp <mitja@lysator.liu.se> for heartbeat
 *
 * Significant changed by Sun Jiang Dong
 * 	<sunjd@cn.ibm.com>
 *
 */

#include <lha_internal.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdio.h>		/* for sprintf() */
#include <stdlib.h>
#include <string.h>		/* for memcpy() */
#include <sys/types.h>		/* for stupid systems */
#include <netinet/in.h>		/* for ntohl() */
#include <clplumbing/md5.h>
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
,	const void * text, size_t textlen ,char * result, int resultlen);

static int md5_auth_needskey(void);

/* Authentication plugin operations */
static struct HBAuthOps md5ops =
{	md5_auth_calc
,	md5_auth_needskey
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
	,	&md5ops
	,	NULL		/*close */
	,	&OurInterface
	,	&OurImports
	,	interfprivate); 
}

/*
 *	Real work starts here ;-)
 */

#define MD5_DIGESTSIZE  16
#define md5byte unsigned char

static int
md5_auth_needskey(void) 
{ 
	return 1;	/* Yes, we require (need) a key */
}


#define byteSwap(buf,words)

static int
md5_auth_calc(const struct HBauth_info *t, const void * text
,	size_t textlen, char * result, int resultlen)
{

	unsigned char digest[MD5_DIGESTSIZE];
	const unsigned char * key = (unsigned char *)t->key;
	int i, key_len;


	if (resultlen <= (MD5_DIGESTSIZE+1) *2) {
		return 0;
	}
	key_len = strlen((const char *)key);
	
	HMAC(key, key_len, text, textlen, digest);

	/* And show the result in human-readable form */
	for (i = 0; i < MD5_DIGESTSIZE; i++) {
		sprintf(result, "%02x", digest[i]);
		result +=2;
	}
	return 1;
}
