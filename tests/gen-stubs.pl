my %hash = ();

while(defined($line = <STDIN>))
{
	if ($line =~ m/undefined reference to `(.+)'/)
	{
		$hash{$1} = 1;
	}
}

print "#include <stdlib.h>\n#include <stdio.h>\n";

while ( my ($key, $value) = each(%hash) ) {
	print 'void '.$key.'(void){printf("Function '.$key.'() is not implemented!\n");exit(-1);}'."\n"
}
