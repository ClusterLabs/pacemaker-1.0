#include <lha_internal.h> 
#include <stdio.h> 
#include <string.h> 
#include <HBauth.h>

#define PIL_PLUGINTYPE		HB_AUTH_TYPE
#define PIL_PLUGIN		crc
#define PIL_PLUGINTYPE_S	"HBauth"
#define PIL_PLUGIN_S		"crc"
#define PIL_PLUGINLICENSE	LICENSE_LGPL
#define PIL_PLUGINLICENSEURL	URL_LGPL
#include <pils/plugin.h>

static int crc_auth_calc(const struct HBauth_info *
,	const void* text, size_t text_len, char * result, int resultlen);
static int crc_auth_needskey (void);

static struct HBAuthOps crcOps =
{	crc_auth_calc
,	crc_auth_needskey
};

static unsigned long const crctab[256] =
{
	0x0ul,
	0x04C11DB7ul, 0x09823B6Eul, 0x0D4326D9ul, 0x130476DCul, 0x17C56B6Bul,
	0x1A864DB2ul, 0x1E475005ul, 0x2608EDB8ul, 0x22C9F00Ful, 0x2F8AD6D6ul,
	0x2B4BCB61ul, 0x350C9B64ul, 0x31CD86D3ul, 0x3C8EA00Aul, 0x384FBDBDul,
	0x4C11DB70ul, 0x48D0C6C7ul, 0x4593E01Eul, 0x4152FDA9ul, 0x5F15ADACul,
	0x5BD4B01Bul, 0x569796C2ul, 0x52568B75ul, 0x6A1936C8ul, 0x6ED82B7Ful,
	0x639B0DA6ul, 0x675A1011ul, 0x791D4014ul, 0x7DDC5DA3ul, 0x709F7B7Aul,
	0x745E66CDul, 0x9823B6E0ul, 0x9CE2AB57ul, 0x91A18D8Eul, 0x95609039ul,
	0x8B27C03Cul, 0x8FE6DD8Bul, 0x82A5FB52ul, 0x8664E6E5ul, 0xBE2B5B58ul,
	0xBAEA46EFul, 0xB7A96036ul, 0xB3687D81ul, 0xAD2F2D84ul, 0xA9EE3033ul,
	0xA4AD16EAul, 0xA06C0B5Dul, 0xD4326D90ul, 0xD0F37027ul, 0xDDB056FEul,
	0xD9714B49ul, 0xC7361B4Cul, 0xC3F706FBul, 0xCEB42022ul, 0xCA753D95ul,
	0xF23A8028ul, 0xF6FB9D9Ful, 0xFBB8BB46ul, 0xFF79A6F1ul, 0xE13EF6F4ul,
	0xE5FFEB43ul, 0xE8BCCD9Aul, 0xEC7DD02Dul, 0x34867077ul, 0x30476DC0ul,
	0x3D044B19ul, 0x39C556AEul, 0x278206ABul, 0x23431B1Cul, 0x2E003DC5ul,
	0x2AC12072ul, 0x128E9DCFul, 0x164F8078ul, 0x1B0CA6A1ul, 0x1FCDBB16ul,
	0x018AEB13ul, 0x054BF6A4ul, 0x0808D07Dul, 0x0CC9CDCAul, 0x7897AB07ul,
	0x7C56B6B0ul, 0x71159069ul, 0x75D48DDEul, 0x6B93DDDBul, 0x6F52C06Cul,
	0x6211E6B5ul, 0x66D0FB02ul, 0x5E9F46BFul, 0x5A5E5B08ul, 0x571D7DD1ul,
	0x53DC6066ul, 0x4D9B3063ul, 0x495A2DD4ul, 0x44190B0Dul, 0x40D816BAul,
	0xACA5C697ul, 0xA864DB20ul, 0xA527FDF9ul, 0xA1E6E04Eul, 0xBFA1B04Bul,
	0xBB60ADFCul, 0xB6238B25ul, 0xB2E29692ul, 0x8AAD2B2Ful, 0x8E6C3698ul,
	0x832F1041ul, 0x87EE0DF6ul, 0x99A95DF3ul, 0x9D684044ul, 0x902B669Dul,
	0x94EA7B2Aul, 0xE0B41DE7ul, 0xE4750050ul, 0xE9362689ul, 0xEDF73B3Eul,
	0xF3B06B3Bul, 0xF771768Cul, 0xFA325055ul, 0xFEF34DE2ul, 0xC6BCF05Ful,
	0xC27DEDE8ul, 0xCF3ECB31ul, 0xCBFFD686ul, 0xD5B88683ul, 0xD1799B34ul,
	0xDC3ABDEDul, 0xD8FBA05Aul, 0x690CE0EEul, 0x6DCDFD59ul, 0x608EDB80ul,
	0x644FC637ul, 0x7A089632ul, 0x7EC98B85ul, 0x738AAD5Cul, 0x774BB0EBul,
	0x4F040D56ul, 0x4BC510E1ul, 0x46863638ul, 0x42472B8Ful, 0x5C007B8Aul,
	0x58C1663Dul, 0x558240E4ul, 0x51435D53ul, 0x251D3B9Eul, 0x21DC2629ul,
	0x2C9F00F0ul, 0x285E1D47ul, 0x36194D42ul, 0x32D850F5ul, 0x3F9B762Cul,
	0x3B5A6B9Bul, 0x0315D626ul, 0x07D4CB91ul, 0x0A97ED48ul, 0x0E56F0FFul,
	0x1011A0FAul, 0x14D0BD4Dul, 0x19939B94ul, 0x1D528623ul, 0xF12F560Eul,
	0xF5EE4BB9ul, 0xF8AD6D60ul, 0xFC6C70D7ul, 0xE22B20D2ul, 0xE6EA3D65ul,
	0xEBA91BBCul, 0xEF68060Bul, 0xD727BBB6ul, 0xD3E6A601ul, 0xDEA580D8ul,
	0xDA649D6Ful, 0xC423CD6Aul, 0xC0E2D0DDul, 0xCDA1F604ul, 0xC960EBB3ul,
	0xBD3E8D7Eul, 0xB9FF90C9ul, 0xB4BCB610ul, 0xB07DABA7ul, 0xAE3AFBA2ul,
	0xAAFBE615ul, 0xA7B8C0CCul, 0xA379DD7Bul, 0x9B3660C6ul, 0x9FF77D71ul,
	0x92B45BA8ul, 0x9675461Ful, 0x8832161Aul, 0x8CF30BADul, 0x81B02D74ul,
	0x857130C3ul, 0x5D8A9099ul, 0x594B8D2Eul, 0x5408ABF7ul, 0x50C9B640ul,
	0x4E8EE645ul, 0x4A4FFBF2ul, 0x470CDD2Bul, 0x43CDC09Cul, 0x7B827D21ul,
	0x7F436096ul, 0x7200464Ful, 0x76C15BF8ul, 0x68860BFDul, 0x6C47164Aul,
	0x61043093ul, 0x65C52D24ul, 0x119B4BE9ul, 0x155A565Eul, 0x18197087ul,
	0x1CD86D30ul, 0x029F3D35ul, 0x065E2082ul, 0x0B1D065Bul, 0x0FDC1BECul,
	0x3793A651ul, 0x3352BBE6ul, 0x3E119D3Ful, 0x3AD08088ul, 0x2497D08Dul,
	0x2056CD3Aul, 0x2D15EBE3ul, 0x29D4F654ul, 0xC5A92679ul, 0xC1683BCEul,
	0xCC2B1D17ul, 0xC8EA00A0ul, 0xD6AD50A5ul, 0xD26C4D12ul, 0xDF2F6BCBul,
	0xDBEE767Cul, 0xE3A1CBC1ul, 0xE760D676ul, 0xEA23F0AFul, 0xEEE2ED18ul,
	0xF0A5BD1Dul, 0xF464A0AAul, 0xF9278673ul, 0xFDE69BC4ul, 0x89B8FD09ul,
	0x8D79E0BEul, 0x803AC667ul, 0x84FBDBD0ul, 0x9ABC8BD5ul, 0x9E7D9662ul,
	0x933EB0BBul, 0x97FFAD0Cul, 0xAFB010B1ul, 0xAB710D06ul, 0xA6322BDFul,
	0xA2F33668ul, 0xBCB4666Dul, 0xB8757BDAul, 0xB5365D03ul, 0xB1F740B4ul
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
	,	&crcOps
	,	NULL		/*close */
	,	&OurInterface
	,	&OurImports
	,	interfprivate); 
}

static int
crc_auth_needskey (void)
{
	return 0;
}
                        
static int crc_auth_calc (const struct HBauth_info * info
,	const void * value, size_t valuelen, char * result, int resultlen)
{
	const char* valuechar=value;
	unsigned long crc = 0;
	(void)info;
	while(valuelen--)
		crc = (crc << 8) ^ crctab[((crc >> 24) ^ *(valuechar++)) & 0xFF];

	crc = ~crc & 0xFFFFFFFFul;
	snprintf(result, resultlen, "%lx", crc);
	return 1;
}
