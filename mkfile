</$objtype/mkfile

TARG=irccloudfs
BIN=$home/bin/$objtype

FSFILES=\
	net.$O\
	main.$O\
	fs.$O\

</sys/src/cmd/mkmany

$O.irccloudfs: $FSFILES
	$LD -o $target $prereq
