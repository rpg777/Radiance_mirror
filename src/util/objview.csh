#!/bin/csh -f
# SCCSid "$SunId$ SGI"
#
# Make a nice view of an object
# Arguments are scene input files
#
set tmpdir=/usr/tmp
set octree=$tmpdir/ov$$.oct
set lights=$tmpdir/lt$$.rad
set rif=$tmpdir/ov$$.rif
set tmpfiles="$octree $lights $rif"
set up="Z"
set vw="XYZ"
set opts=""
while ($#argv > 0)
	switch ($argv[1])
	case -g:
		set usegl
		breaksw
	case -u:
		shift argv
		set up=$argv[1]
		breaksw
	case -s:
	case -w:
		set opts=($opts $argv[1])
		breaksw
	case -v:
		shift argv
		set vw=$argv[1]
		breaksw
	case -o:
		shift argv
		set opts=($opts -o $argv[1])
		set radopt
		breaksw
	case -V:
	case -e:
		set opts=($opts $argv[1])
		set radopt
		breaksw
	case -S:
	case -b:
		set opts=($opts $argv[1])
		set glradopt
		breaksw
	case -*:
		echo "Bad option: $argv[1]"
		exit 1
		breaksw
	default:
		break
	endsw
	shift argv
end
if ( $#argv == 0 ) then
	echo "No input files specified"
	exit 1
endif
if ( $?usegl ) then
	if ( $?radopt ) then
		echo "bad option for glrad"
		glrad
		exit 1
	endif
else
	if ( $?glradopt ) then
		echo "bad option for rad"
		rad
		exit 1
	endif
endif

onintr quit

cat > $lights <<_EOF_
void glow dim 0 0 4 .1 .1 .15 0
dim source background 0 0 4 0 0 1 360
void light bright 0 0 3 1000 1000 1000
bright source sun1 0 0 4 1 .2 1 5
bright source sun2 0 0 4 .3 1 1 5
bright source sun3 0 0 4 -1 -.7 1 5
_EOF_

cat > $rif <<_EOF_
scene= $argv[*]:q $lights
EXPOSURE= .5
UP= $up
view= $vw
OCTREE= $octree
oconv= -f
_EOF_

if ( $?usegl ) then
	glrad $opts $rif
else
	rad -o x11 $opts $rif
endif

quit:
rm -f $tmpfiles
exit 0
