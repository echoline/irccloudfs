</$objtype/mkfile

TARG=irccloudfs irccloud
BIN=$home/bin/$objtype

FSFILES=\
	net.$O\
	main.$O\
	fs.$O\

</sys/src/cmd/mkmany

$O.irccloudfs: $FSFILES
	$LD -o $target $prereq

$O.irccloud: irccloud.$O
	$LD -o $target $prereq
