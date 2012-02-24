#!/usr/bin/perl 

# This script filters out all lines containing
# "Value SLData"
# from <model>.rtw before the tlc post processing begins.
# TLC chokes on these lines

$in = shift;
$out = shift; 

open IN, $in or die "can't open $in for reading";
open OUT, ">$out" and select OUT;

while ( <IN> ) {
    next if /^\s+Value\s+SLData\(\d+\)/;
    print; 
}
