</$objtype/mkfile

TARG=irccloudfs irccloud
BIN=$home/bin/$objtype
RCBIN=$home/bin/rc

FSFILES=\
	net.$O\
	main.$O\
	fs.$O\

</sys/src/cmd/mkmany

$O.irccloudfs: $FSFILES
	$LD -o $target $prereq

irccloud.install: $O.irccloud
	cp $O.irccloud $RCBIN/irccloud

$O.irccloud: irccloud.$O
	cp irccloud.$O $O.irccloud

irccloud.$O: irccloud.rc
	cp irccloud.rc irccloud.$O
