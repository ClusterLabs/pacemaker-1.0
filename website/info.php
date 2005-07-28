<!--
Author:       Alan Robertson
Support: linux-ha@lists.linux-ha.org
License:      GNU General Public License (GPL)
Copyright:    (C) 2005 International Business Machines, Inc.
-->
<?php
/*braries and defines some variables
 */


 /**
  * Displays PHP information
   */
       phpinfo();

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

function wget($url, $file) {
	
	if (file_exists($file)) {
		unlink($file);
	}
	$WGET="/usr/bin/wget";
	$url = preg_replace('/\\\\/', '\\\\', $url);
	$url = preg_replace('/\'/', '\\\'', $url);
	$CMD="$WGET -q -U 'Mozilla/5.0' -S -nd -O '$file' '" . $url . '\'';
	$start = microtime();
	system($CMD, $rc);
	$end = microtime();
	if ($rc != 0 && file_exists($file)) {
		unlink($file);
	}
	$elapsed = subtimes($end, $start);
	printf("<BR>RC=%d elapsed: %.3f secs<br>", $rc, $elapsed);
	return $rc;
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
	echo "<BR><STRONG>$Browser, $Version, $MajorVers</STRONG><BR>\n";
	return array($Browser, $Version, intval($MajorVers));
}

function browser_compatibility() {

	$T=browser_type();
	if (strcasecmp($T[0], "Mozilla") == 0 && $T[2] >= 5) {
		return 2;
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
		}elseif ($T[2] >= 6) {
			return 1;
		}
	}
	if (strcasecmp($T[0], "Opera") == 0) {
		if ($T[2] >= 7) {
			return 2;
		}elseif ($T[2] == 6) {
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
	$ffbut2="<img border=\"0\" alt=\"Get Firefox!\" src=\"$imgdir/110x32/trust.gif\"/>";
	$ff1="<a href=\"$ffurl\">Firefox $ffbut1</a>";
	$ff2="<a href=\"$ffurl\">Firefox $ffbut2</a>";
	if ($c >= 2) {
		return;
	}
	echo '<font size="+1">';
	if ($c == 1) {
		echo "<p>This site best when viewed with a modern CSS-compatible browser. "
		.	"We recommend $ff1</p>";
	}else{
		echo "<p>Your browser will likely have trouble with this site. "
		.	"This site best when viewed with a modern browser<BR> "
		.	"We recommend $ff2</p>";
	}
	echo "</font>";
}

	print_r(browser_type());
	echo "<BR> Compatibility: " . browser_compatibility() . "<BR>\n";

	$URL="http://wiki.linux-ha.org/Talks?action=AttachFile&do=get&target=HighAssurance.zip";
	$URL="http://www.linux-ha.org/index.html";
	$file="/tmp/HighAssurance.zip";

	echo `date`;
	$utimes=microtime();
	echo "TIME: $utimes at start<br>";
	echo "Starting wget.<BR>";
	$rc = wget($URL, $file);
	echo "$out<BR>RC = $rc <BR>";
	$utimes=microtime();
	echo "TIME: $utimes at end<br>";
	echo `date`;
	echo "<BR>";
	$size = filesize($file);
	echo "Wget finished: size = $size<br>";
	echo "Finished<br>";
	unlink($file);
?>
