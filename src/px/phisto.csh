#!/bin/csh -f
# SCCSid "$SunId$ SGI"
#
# Compute foveal histogram for picture set
#
set tf=/usr/tmp/ph$$
onintr quit
if ( $#argv == 0 ) then
	pfilt -1 -x 128 -y 128 -p 1 \
			| pvalue -o -h -H -d -b > $tf.dat
else
	rm -f $tf.dat
	foreach i ( $* )
		pfilt -1 -x 128 -y 128 -p 1 $i \
				| pvalue -o -h -H -d -b >> $tf.dat
		if ( $status ) exit 1
	end
endif
set Lmin=`total -l $tf.dat | rcalc -e 'L=$1*179;$1=if(L-1e-7,log10(L),-7)'`
set Lmax=`total -u $tf.dat | rcalc -e '$1=log10($1*179)'`
rcalc -e 'L=$1*179;cond=L-1e-7;$1=log10(L)' $tf.dat \
	| histo $Lmin $Lmax 100
quit:
rm -f $tf.dat
