</$objtype/mkfile

TARG=irccloudfs irccloud
BIN=/$objtype/bin

FSFILES=\
	net.$O\
	main.$O\
	fs.$O\

</sys/src/cmd/mkmany

$O.irccloudfs: $FSFILES
	$LD -o $target $prereq

$O.irccloud: irccloud.$O
	$LD -o $target $prereq
