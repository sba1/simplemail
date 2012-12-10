#
# Simple perl script to extract the test function calls. All test
# function calls are marked with @Test comment
#

use strict;

local $/=undef;
my $string;
my $file_contents;

print "struct unit_test unit_tests[] = {\n";

$file_contents = <STDIN>;
while ($file_contents =~ m/\@Test\s*\*\/\s*.*void\s+([^\(]+)/g)
{
	my$test_funcname;
	$test_funcname = $1;
	print "\t\"Testing $test_funcname()\",$test_funcname,\n";
}

print "\tNULL,NULL\n};\n";