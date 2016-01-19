#!/usr/bin/perl -w
# Spit out all the mkstr options
# Intended to be used something like:
# ./_mkstr_allstr.pl | xargs -0 ../run.sh ../mkstr/cidr_mkstr
# just to run a bunch of stuff through it (dmalloc testing, etc)

use strict;

# Grab our lists of tests to run
@INC = ( "../regression", @INC );
require "arr.pl";

# Top level is test programs we work with
our %TESTS;
foreach my $inp ( keys %{$TESTS{'mkstr'}} )
{
	printf("$inp%c", 0);
}
