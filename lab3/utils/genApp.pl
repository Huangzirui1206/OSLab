#!/usr/bin/perl

open(SIG, $ARGV[0]) || die "open $ARGV[0]: $!";

$n = sysread(SIG, $buf, 30000);

if($n > 512 *30){
	print STDERR "ERROR: App too large: $n bytes (max 15360)\n";
	exit 1;
}

print STDERR "OK: App block is $n bytes (max 512 * 30)\n";

$buf .= "\0" x (15360-$n);

open(SIG, ">$ARGV[0]") || die "open >$ARGV[0]: $!";
print SIG $buf;
close SIG;
