#include <stdio.h>
#include "gXMLparse.h"

const char * testxml[] =
{
/* These simple test cases work... */
"<aarray><elem name=\"foo\">bar</elem></aarray>",
"<SLL><elem>A</elem><elem>B</elem></sll>",
"<dll><elem><dll><elem>a</elem></dll></elem></dll>",
"<dll><elem>A</elem><elem>B</elem></dll>",
"<sll><elem><aarray><elem name=\"foo\">A</elem></aarray></elem></sll>",
"<sll><elem>&nbsp;Four	  score&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;and seven years ago, our fathers brought forth onto this continent...    </elem></sll>",
"<aarray><elem name=\"foo\"><sll><elem>&lt;A&gt;</elem></sll></elem></aarray>",
"<aarray><elem name=\"foo\"><sll><elem>A</elem><elem><aarray><elem name=\"foo\">BAR</elem></aarray></elem></sll></elem></aarray>",
};

void test_string(const char * s);
int
main(int argc, char ** argv)
{
	int	j;
	for (j=0; j < DIMOF(testxml); ++j) {
		test_string(testxml[j]);
	}
	return 0;
}
void
test_string(const char * s)
{
	GString * gxml = g_string_new(s);
	gXML_wrapper*	ret = NULL;

	fprintf(stderr,  "PARSING this string...\n%s\n", s);
	if (gXML_struct(&ret, gxml)) {
		GString*	out;
		GString*	secondout;
		gXML_wrapper*	secondret = NULL;
		out = gXML_out(ret);
		fprintf(stderr, "Returned XML from struct:\n%s\n"
		,	out->str);
		if (strcmp(s, out->str) == 0) {
			fprintf(stderr, "Result is identical to original!\n");
		}else{
			fprintf(stderr
			,	"Note: result not identical to original.\n");
			/* This does not necessarily indicate a bug */
		}
		fprintf(stderr, "Hurray!  [%s] is valid XML!\n"
		,	s);
		if (!gXML_struct(&secondret, out)) {
			/* This always indicates a problem... */
			fprintf(stderr
			,	"UhOh!  Generated XML [%s] is bad XML!\n"
			,	s);
		}else{
			secondout = gXML_out(secondret);
			if (strcmp(out->str, secondout->str) != 0) {
				/* This always indicates a bug ... */
				fprintf(stderr
				,	"UhOh!  Second-Generation"
				" generated XML "
				"[%s] doesn't match original generated"
				" XML [%s]\n"
				,	secondout->str, out->str);
			}else{
				fprintf(stderr, "2nd Generation check OK!\n");
				fprintf(stderr, "\n\n");
			}
			g_string_free(secondout, TRUE); secondout = NULL;
		}
		g_string_free(out, TRUE); out = NULL;
	}else{
		fprintf(stderr, "BOO!  [%s] is bad XML!\n"
		,	s);
	}
	g_string_free(gxml, TRUE); gxml = NULL;
}
