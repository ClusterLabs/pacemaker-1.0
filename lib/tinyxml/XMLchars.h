/*
 * XML special character map
 */
struct XML_SpecialCharMap {
	const gchar *	escseq;
	gchar		special;
};

/* There are many more... But we probably don't care ;-)*/
static const struct XML_SpecialCharMap replacements[] =
{	{"nbsp",	' '}	
,	{"amp",		'&'}
,	{"lt",		'<'}
,	{"gt",		'>'}
,	{"quot",	'"'}
#if 0
/* I'm don't think we need all these... */
,	{"lsquo",	'`'}
,	{"rsquo",	'\''}
,	{"iexcl",	'\241'}
,	{"cent",	'\242'}
,	{"pound",	'\243'}
,	{"curren",	'\244'}
,	{"yen",		'\245'}
,	{"brvbar",	'\246'}
,	{"sect",	'\247'}
,	{"uml",		'\250'}
,	{"copy",	'\251'}
,	{"ordf",	'\252'}
,	{"laquo",	'\253'}
,	{"not",		'\254'}
,	{"shy",		'\255'}
,	{"reg",		'\256'}
,	{"macr",	'\257'}
,	{"deg",		'\260'}
,	{"plusmn",	'\261'}
,	{"sup2",	'\262'}
,	{"sup3",	'\263'}
,	{"acute",	'\264'}
,	{"micro",	'\265'}
,	{"para",	'\266'}
,	{"middot",	'\267'}
,	{"cedil",	'\270'}
,	{"sup1",	'\271'}
#endif
};
