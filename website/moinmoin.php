<?php

# WikiWare - Standard MoinMoin Transclusion Function
# Dmytri Kleiner -- dmytrik@trickmedia.com
# Copyleft 2003 Idiosyntactix. All rights detourned.	

# Usage Examples:
#   print MoinMoin("FrontPage");
#   print MoinMoin("FrontPage","costomregs.php");
#   print MoinMoin("FrontPage","costomregs.php",$cachesuffix);

# General Configuration
# ... moved into trick.php

#############################################################
#############################################################
#############################################################

/*
 *	All of these get set when a user presses shift-reload in mozilla
 *	Apache Environment:
 *	HTTP_CACHE_CONTROL:					"no-cache"
 *	HTTP_PRAGMA:						"no-cache"
 *	PHP Variables:		_SERVER["HTTP_CACHE_CONTROL"]:	"no-cache"
 *	HTTP request headers:	Cache-Control			"no-cache"
 *
 *
 *	All of these get set when a user presses reload in mozilla
 *	HTTP_CACHE_CONTROL:					"max-age=0"
 *	PHP Variables:		_SERVER["HTTP_CACHE_CONTROL"]:	"max-age=0"
 *	HTTP request headers:	Cache-Control			"max-age=0"
 *
 *	None of these are available when a "normal" page load is performed.
 *
 *	For now, we only pay attention to the shift-reload sequence.
 *
 */

function MoinMoinNoCache($cachefile)
{
	global $MOINMOINCacheLimit, $PageTitle, $MOINMOINurl, $MOINMOINfetched;

	if (isset($MOINMOINfetched[$cachefile])) {
		return false;
	}
	if (isset($MOINMOINCacheLimit[$PageTitle])) {
		$cachelimit = $MOINMOINCacheLimit[$PageTitle];
	}elseif (isset($MOINMOINCacheLimit['*'])) {
		$cachelimit = $MOINMOINCacheLimit['*'];
	}else{
		$cachelimit = -1;
	}
	if ($cachelimit >= 0) {
		$now = intval(date("U"));
		if (file_exists($cachefile)) {
			clearstatcache();
			$mtime = filemtime($cachefile);
		}else{
			$mtime = $now;
		}
		if (($now-$mtime) >= $cachelimit) {
			return true;
		}
	}
	if (isset($_SERVER["HTTP_CACHE_CONTROL"])) {
		return (0 == strcasecmp($_SERVER["HTTP_CACHE_CONTROL"], "no-cache"));
	}
	return false;
}

function MoinMoin($ptitle, $INCLUDEPHP = false, $CACHESUFFIX = "")
{
	global $MOINMOINurl, $MOINMOINalias, $MOINMOINcachedir, $MOINMOINfilemod, $MOINMOINstandardsearch;
	global $MOINMOINstandardreplace, $current_cache_prefix, $current_cache_relprefix, $PageTitle;
	global $MOINMOINfetched;

	$PageTitle = str_replace("/","_", $ptitle);
	$filename = "$MOINMOINurl/$PageTitle";
	$cachefile = "$MOINMOINcachedir/$MOINMOINalias$PageTitle$CACHESUFFIX.html";
	# for attachments and the like
	$current_cache_prefix = "$MOINMOINcachedir/${MOINMOINalias}${PageTitle}__";
	$current_cache_relprefix = "${MOINMOINalias}${PageTitle}__";
	set_time_limit(30);
	umask(077);
	
	if (MoinMoinNoCache($cachefile) && file_exists($cachefile)) {
		unlink($cachefile);
	}
	if (!file_exists($cachefile))
	{

		$content = implode("",file($filename));

		if (!$GLOBALS["MOINMOINstandardregsloaded"])
		{
			MOINMOINloadstandardregs();
		}

		if ($INCLUDEPHP)
		{
			include($INCLUDEPHP);
			
			$MOINMOINallsearch = array_merge($MOINMOINstandardsearch, $MOINMOINsearch);
			$MOINMOINallreplace = array_merge($MOINMOINstandardreplace, $MOINMOINreplace);
			
			unset($MOINMOINsearch);
			unset($MOINMOINreplace);
		} else {
			$MOINMOINallsearch = $MOINMOINstandardsearch;
			$MOINMOINallreplace = $MOINMOINstandardreplace;
		}
		$body = preg_replace ($MOINMOINallsearch, $MOINMOINallreplace, $content);

		$fd = fopen($cachefile, "w");
		fwrite($fd, $body);
		fclose ($fd);
		chmod($cachefile, $MOINMOINfilemod);
		$msg=sprintf("EXPANDED %d bytes into %s"
		,	filesize($cachefile), $cachefile);
		$MOINMOINfetched[$cachefile] = true;

	} else {
		$body = implode("",file($cachefile));
		$msg=sprintf("READ %d bytes from %s"
		,	filesize($cachefile), $cachefile);
		LogIt($msg);
	}

	return $body;
}


function MOINMOINloadstandardregs()
{
	global $MOINMOINstandardsearch, $MOINMOINstandardreplace, $MOINMOINalias
	,	$local_cache_url_prefix, $MOINMOINExtraneousImages;
	
	if (!isset($MOINMOINstandardsearch)) { $MOINMOINstandardsearch = array(); }
	if (!isset($MOINMOINstandardreplace)) { $MOINMOINstandardreplace = array(); }
	set_time_limit(60);
		
	# Trap Pages Not In Wiki
	# FIXME
	$MOINMOINstandardsearch[] = "'^.*<a href=\"[^\">]*?\?action=edit\">Create this page</a>.*$'s";
	$MOINMOINstandardreplace[] = "<b>Not Found.</b>";
	
	# Eliminate Goto Link from Include Macro
	$MOINMOINstandardsearch[] = "'<div class=\"include-link\"><a [^>]*>.*?</a></div>'s";
	$MOINMOINstandardreplace[] = "";

	# Get Content Area
	$MOINMOINstandardsearch[] = "'^.*?<div id=\"content\"[^>]*>'s";
	$MOINMOINstandardreplace[] = "";
	$MOINMOINstandardsearch[] = "'</div>\s*<div id=\"footer\".*'s";
	$MOINMOINstandardreplace[] = "";
	
	# Strip Wiki Class Tags
	$MOINMOINstandardsearch[] = "'(<[^>]*) class=\"[^\"]*\"([^>]*>)'U";
	$MOINMOINstandardreplace[] = "\\1\\2";
	
	# Clean Table Tags
	$MOINMOINstandardsearch[] = "'(<table\s)[^>]*(>)'Ui";
	$MOINMOINstandardreplace[] = "\\1\\2";

	# Fix Internal Links
	$MOINMOINstandardsearch[] = "'(<a\s[^>]*href=\")/$MOINMOINalias([^\">]*\">)'iU";
	$MOINMOINstandardreplace[] = "\\1$local_cache_url_prefix\\2";

	# Strip out [WWW] [FTP] images, etc.
	foreach ($MOINMOINExtraneousImages as $im) {
		$MOINMOINstandardsearch[] = "'< *img +src=\"${im}\"[^>]*>'i";
		$MOINMOINstandardreplace[] = '';
	}

	# Cache MoinMoin Images Locally
	$MOINMOINstandardsearch[]  = "'src=\"(/[^\"]*wiki[^\"]*img/([^\"]*))\"'ie";
	$MOINMOINstandardreplace[] = "MOINMOINcacheimages('\\1','\\2')";

	# Cache inline images (Attachments) Locally
	$MOINMOINstandardsearch[] = "'(<\s*img\s[^>]*src=\")/$MOINMOINalias([^\">]*?action=AttachFile&[^\">]*target=([^\">]*))(\"[^>]*>)'iUe";
	$MOINMOINstandardreplace[] = "stripslashes('\\1') . MOINMOINcacheattachments('\\2','\\3') . stripslashes('\\4')";

	# Cache MoinMoin Attachments Localy
	$MOINMOINstandardsearch[] = "'(<a\s*[^>]*href=\")/$MOINMOINalias([^\">]*?action=AttachFile&[^\">]*target=([^\">]*))(\"[^>]*>)'iUe";
	$MOINMOINstandardreplace[] = "stripslashes('\\1') . MOINMOINcacheattachments('\\2','\\3') . stripslashes('\\4')";

	$GLOBALS["MOINMOINstandardregsloaded"] = true;
}

function browser_type() {
	$Browser="unknown";
	$Version="0.0";
	$MajorVers="0";
	if (isset($_SERVER["HTTP_USER_AGENT"])) {
		$ua = $_SERVER["HTTP_USER_AGENT"];

		$BrowserPats = array('%(MSIE|Opera) +(([1-9][0-9]*)\.[0-9.]+)%'
		,	'%; +(Konqueror|Netscape)/(([1-9][0-9]*)\.[0-9.]+)%i'
		,	'%(Mozilla)/(([1-9][0-9]*)\.[0-9.]+)%i');

		foreach ($BrowserPats as $pat) {
			if (preg_match($pat, $ua, $match)) {
				$Browser=$match[1];
				$Version=$match[2];
				$MajorVers=$match[3];
				break;
			}
		}
	}
	return array($Browser, $Version, intval($MajorVers));
}

function browser_compatibility() {

	$T=browser_type();
	if (strcasecmp($T[0], "Mozilla") == 0 && $T[2] >= 5) {
		return 2;
	}
	if (strcasecmp($T[0], "MSIE") == 0) {
		if ($T[2] >= 7) {
			return 2;
		}else{
			return 0;
		}
	}
	if (strcasecmp($T[0], "Konqueror") == 0) {
		if ($T[2] >= 3) {
			return 2;
		}else{
			return 1;
		}
	}
	if (strcasecmp($T[0], "Netscape") == 0) {
		if ($T[2] >= 7) {
			return 2;
		}elseif ($T[2] >= 5) {
			return 1;
		}
	}
	if (strcasecmp($T[0], "Opera") == 0) {
		if ($T[2] >= 6) {
			return 2;
		}elseif ($T[2] == 5) {
			return 1;
		}
	}
	return 0;
}

function browser_compatibility_messages() {
	$c = browser_compatibility();
	$ffurl="http://www.mozilla.org/products/firefox/";
	$ff="<a href=\"$ffurl\">Firefox </a>";
	$imgdir="http://sfx-images.mozilla.org/affiliates/Buttons";
	$ffbut1="<img border=\"0\" alt=\"Get Firefox!\" src=\"$imgdir/80x15/white_1.gif\"/>";
	$ffbut2="<img border=\"0\" alt=\"Get Firefox!\" height=\"24\" WIDTH=\"83\" src=\"$imgdir/110x32/trust.gif\"/>";
	$ff1="<a href=\"$ffurl\">Firefox $ffbut1</a>";
	$ff2="<a href=\"$ffurl\">Firefox $ffbut2</a>";
	if ($c >= 2) {
		return;
	}
	if ($c == 1) {
		echo '<font size="-2">This site best when viewed with a CSS-compatible browser. '
		.	"We recommend $ff1.</font>\n";
	}else{
		echo '<font size="-2">This site best when viewed with a modern standards-compliant browser. '
		.	"We recommend $ff2.</font>\n";
	}
}

function search_box_html($ncols)
{
echo "<TABLE><TR ALIGN=CENTER><TD>
<FORM method=GET action=\"http://www.google.com/search\">
<input type=hidden name=ie value=UTF-8><input type=hidden name=oe value=UTF-8>
<INPUT TYPE=text name=q size=$ncols maxlength=255 value=\"\"><BR>
<INPUT type=submit name=btnG VALUE=\"Site Search\"><BR>
<input type=hidden name=domains value=\"http://wwnew.linux-ha.org\">
<input type=hidden name=sitesearch value=\"wwnew.linux-ha.org\">
</FORM>
</TD></TR></TABLE>";
}
#	return array($Browser, $Version, intval($MajorVers));
function stylesheet_link()
{
	$T=browser_type();
	if (strcasecmp($T[0], "MSIE") == 0 && $T[2] < 7) {
		$ss="/linuxhaIE6.css";
	}else{
		$ss="/linuxha.css";
	}
 	echo "<link rel=\"stylesheet\" href=\"$ss\" type=\"text/css\">\n";
}

function URLtoCacheFile($urlsuffix, $cacheprefix)
{
	global	$MOINMOINcachedir;
	/* FIXME:  Clean up $urlsuffix to make sure it's safe */
	# SECURITY ALERT
	# need to clean up sanitize $argTar, in a specially crafted wiki page
	# may be a special file name
	# hope this is enough, just in case:
	$urlsuffix = str_replace("/","_", "${cacheprefix}${urlsuffix}");
	return "${MOINMOINcachedir}/${urlsuffix}";
}

function CleanURL($urlprefix, $urlsuffix)
{
	/* FIXME:  Clean up $url to make sure it's safe */
	return $urlprefix . $urlsuffix;
}

function CacheURL($urlprefix, $urlsuffix, $cacheprefix)
{
	$cachefile = URLtoCacheFile($urlsuffix, $cacheprefix);
	$url = CleanURL($urlprefix , $urlsuffix);
	return CacheURL_ll($url, $cachefile);
}

function LogIt($message)
{
	$logfile="/tmp/linux-ha.web";
	$datestamp=date("Y/m/d_H:i:s");
	if (file_exists($logfile) && filesize($logfile) > 1000000) {
		rename($logfile, "$logfile.OLD");
		touch($logfile);
		chmod($logfile, 0644);
	}
	error_log("$datestamp	$message\n", 3, $logfile);
	chmod($logfile, 0644);
}
function ReportError($errorcode, $descr, $file, $line, $symtab)
{
	global $cachetmp;
	if (!isset($file)) {
		$file = "unknown";
	}
	if (!isset($line)) {
		$file = "?";
	}
	echo "ERROR: $errorcode $descr [$file:$line]";
	LogIt("ERROR: $errorcode $descr [$file:$line]");
	error_log("ERROR: $errorcode $descr [$file:$line]", 0);
	if (isset($cachetmp) && file_exists($cachetmp)) {
		unlink($cachetmp);
	}
	exit(1);
}

function wget($url, $file) {
	
	if (file_exists($file)) {
		unlink($file);
	}
	$WGET='/usr/bin/wget';
	$url = preg_replace('/\\\\/', '\\\\', $url);
	$url = preg_replace('/\'/', '\\\'', $url);
	$CMD="$WGET -q -U 'Mozilla/5.0' -S -nd -O '$file' '" . $url . '\'';
	system($CMD, $rc);
	if ($rc != 0 && file_exists($file)) {
		unlink($file);
	}
	return $rc;
}

#	Inputs are the results from microtime()
function subtimes($lhs, $rhs)
{
	$lhsa = explode(" ", $lhs, 2);
	$lhsusec=intval(substr($lhsa[0], 2, 6));
	$lhssec=intval($lhsa[1]);

	$rhsa = explode(" ", $rhs, 2);
	$rhsusec=intval(substr($rhsa[0], 2, 6));
	$rhssec=intval($rhsa[1]);

	if ($lhsusec < $rhsusec) {
		$lhssec -= 1;
		$lhsusec += 1000000;
	}
	$usecs = ($lhssec - $rhssec) * 1000000;
	$usecs += ($lhsusec - $rhsusec);
	return (doubleval($usecs)/ 1000000.0);
}

function CacheURL_ll($url, $cachefile)
{
	global $MOINMOINfilemod, $MOINMOINcachedir, $MOINMOINurl;
	global $cachetmp,  $MOINMOINfetched;
	$cachetmp = tempnam($MOINMOINcachedir, 'TEMP_');
	set_time_limit(60);
	set_error_handler("ReportError");
	if (substr($url, 0, 1) == "/") {
		$url = "${MOINMOINurl}/${url}";
	}

	$start=microtime();
	if (wget($url, $cachetmp) != 0) {
		return false;
	}
	$end=microtime();
	$elapsed = subtimes($end, $start);
	chmod($cachetmp, $MOINMOINfilemod);
	
	$msg=sprintf("CACHED %d bytes in %.3f secs into %s"
	,	filesize($cachetmp), $elapsed, $cachefile);
	LogIt($msg);

	if (file_exists($cachefile)) {
		unlink($cachefile);
	}
	rename($cachetmp, $cachefile);
	if (file_exists($cachetmp)) {
		unlink($cachetmp);
		unset($cachetmp);
	}
	$MOINMOINfetched[$cachefile] = true;
	return file_exists($cachefile);
}


function MOINMOINcacheattachments($argSrc, $argTar)
{
	global $MOINMOINurl, $current_cache_prefix, $local_cache_url_prefix;

	# SECURITY ALERT
	# need to clean up sanitize $argTar, in a specially crafted wiki page
	# may be a special file name
	# hope this is enough, just in case:
	$argTar = str_replace("/","_", $argTar);

	$cachefile = "$current_cache_prefix$argTar";
	if (MoinMoinNoCache($cachefile) || !file_exists($cachefile)) {
		CacheURL_ll("$MOINMOINurl/$argSrc", $cachefile);
	}
	return $local_cache_url_prefix . $cachefile;
}

function MOINMOINcacheimages($argSrc, $argTar)
{
	global $MOINMOINserver, $MOINMOINcachedir, $MOINMOINfilemod, $local_cache_url_prefix, $MOINMOINurl;
	
	# SECURITY ALERT
	# need to clean up sanitize $argTar, in a specially crafted wiki page
	# may be a special file name
	# hope this is enough, just in case:

	$argTar = str_replace("/","_", $argTar);

	$cachefile = URLtoCacheFile($argSrc, "");
	if (MoinMoinNoCache($cachefile) || !file_exists($cachefile)) {
		CacheURL($MOINMOINurl, $argSrc, "");
	}
	return "src=\"$local_cache_url_prefix$cachefile\"";
}
?>
