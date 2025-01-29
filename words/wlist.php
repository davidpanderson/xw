#! /usr/bin/env php

<?php

// wlist.php infile outfile
//
// infile has e.g.
// WORD;50
//
// output words above a certain score, in lowercase.
// skip words that aren't entirely alphabetic

define('MIN_SCORE', 50);

function main($infile, $outfile) {
    $wds = file($infile);
    $out = '';
    foreach ($wds as $wd) {
        $x = explode(';', $wd);
        if (count($x) != 2) die("bad line $wd\n");
        if ((int)$x[1] < MIN_SCORE) continue;
        $w = $x[0];
        if (!ctype_alpha($w)) continue;
        $out .= strtolower($w)."\n";
    }
    file_put_contents($outfile, $out);
}

main($argv[1], $argv[2]);

?>
