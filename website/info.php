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

		if (preg_match('%MSIE  *(([1-9][0-9])*\.[0-9.]+)%', $ua, $match)) {
			$Browser="MSIE";
			$Version=$match[1];
			$MajorVers=$match[2];
		}elseif (preg_match('%^([A-Za-z][A-Za-z]*)/(([1-9][0-9]*)\.[0-9.]+)%m', $ua, $match)) {
			$Browser=$match[1];
			$Version=$match[2];
			$MajorVers=$match[3];
		}
	}
	return array($Browser, $Version, intval($MajorVers));
}

function browser_compatibility() {

	$T=browser_type();
	if (strcasecmp($T[0], "Mozilla") == 0 && $T[2] >= 5) {
		return 2;
	}
	if (strcasecmp($T[0], "Opera") == 0) {
		if ($T[2] > 6) {
			return 2;
		}elseif ($T[2] == 6) {
			return 1;
		}
	}
	return 0;
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
