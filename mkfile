</$objtype/mkfile

$O.irccloudfs: net.$O main.$O fs.$O
	$O^l -o $O.irccloudfs net.$O main.$O fs.$O

main.$O: main.c
	$O^c main.c

net.$O: net.c
	$O^c net.c

fs.$O: fs.c
	$O^c fs.c

clean:V:
	rm -f *.[$OS] [$OS].irccloudfs

install: $O.irccloudfs
	cp $O.irccloudfs $home/bin/$objtype/irccloudfs
