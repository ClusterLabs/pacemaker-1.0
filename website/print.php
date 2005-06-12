<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">
<?php include("trick.php");
function PRLinkFix($total, $href, $linktext)
{
	$Root="http://wwnew.linux-ha.org";
	global $XrefList, $URLs;
	if (substr($href, 0, 1) == "/") {
		$href = $Root . $href;
	}
	if (strcasecmp(substr($href, 0, strlen("mailto:")), "mailto:") == 0
	||	$href[0] == '#') {
		return "<SPAN CLASS=\"i_linktext\">${total}</SPAN>";
	}
	if (isset($URLs) && array_key_exists($href, $URLs)) {
		$num = $URLs[$href];
	}else{
		$XrefList[] = $href;
		$URLs[$href] = count($XrefList);
		$num = count($XrefList);
	}
	return "<SPAN CLASS=\"i_linktext\">${total}</SPAN><SPAN CLASS=\"i_xref\">[${num}]</SPAN>";
}
function PRXrefs()
{
	global $XrefList;
	if (count($XrefList) == 0) {
		return "";
	}
	$ret="<DIV ID=\"_references\"><HR><H2>References</H2>\n";
	$ret.="<TABLE BORDER=0>\n";
	for ($j=0; $j < count($XrefList); ++$j) {
		$k=$j+1;
		$ref=$XrefList[$j];
		$ret .= "<TR><TD><SPAN CLASS=\"i_refno\">[$k]</SPAN></TD><TD><SPAN CLASS=\"_href\">$ref</SPAN><BR></TD></TR>\n";
	}
	$ret .= "</TABLE></DIV><BR>\n";
	return $ret;
}

function PRPageFix($text)
{
	global $XrefList;
	$PRallSearch[] = "'(< *a [^>]*href=\"([^\"]*)\"[^>]*>([^<]*)</ *a[^>]*>)'ie";
	$PRallReplace[] = "PRLinkFix('\$1', '\$2', '\$3')";
	$PRallSearch[] = "'(< *a [^>]*href=\"([^\"]*)\"[^>]*>([^<]*<img[^>]*>[^>]*)</ *a[^>]*>)'ie";
	$PRallReplace[] = "PRLinkFix('\$1', '\$2', '\$3')";
	$PRallSearch[] = '/\\\"/';
	$PRallReplace[] = '"';

	return preg_replace ($PRallSearch, $PRallReplace, $text);
}
?>
<html>
<head>
 <meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1">
 <meta name="author" content="www.linux-ha.org">
 <title><?php echo "Print $pagetitle: from $sitename"; ?></title>
 <link rel="stylesheet" href="/print.css" type="text/css">
</head>
<body>
<img alt="Linux-HA Logo" src="/_cache/TopLogo__linux-ha.gif"><br>
<?php echo PRPageFix($content); echo PRXrefs(); ?>
<div id="_printattribution"><?php echo MoinMoin("PrintAttribution"); ?></div>
</body>
</html>
