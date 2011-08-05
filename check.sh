#! /bin/sh

[ -x dcc ] || { echo "Error: dcc not executable"; exit 1; }

DIR="samples"

LIST=
if [ "$#" = "0" ]; then
	LIST=`ls $DIR/*.out`
else
	for test in "$@"; do
		LIST="$LIST $DIR/$test.out"
	done
fi

for file in $LIST; do
	base=`echo $file | sed 's/\(.*\)\.out/\1/'`

	ext=''
	if [ -r $base.frag ]; then
		ext='frag'
	elif [ -r $base.decaf ]; then
		ext='decaf'
	else
		echo "Error: Input file for base: $base not found"
		exit 1
	fi

	tmp=${TMP:-"/tmp"}/check.tmp
	sh run $base.$ext 1>$tmp 2>&1

	printf "Checking %-27s: " $file
	if ! cmp -s $tmp $file; then
		echo "FAIL <--"
		diff $tmp $file
	else
		echo "PASS"
	fi
done
