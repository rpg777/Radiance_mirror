#!/bin/csh -f
# SCCSid "$SunId$ LBL"
# Create four standard views of scene and present as line drawings
#
set oblqxf="-rz 45 -ry 45"
set d=/usr/tmp/ol$$
onintr quit
mkdir $d
if ($#argv) then
	set origf=""
	set oblqf=""
	foreach f ($argv)
		xform -e $f > $d/$f.orig
		rad2mgf $d/$f.orig > $d/$f:r.orig.mgf
		set origf=($origf $f:r.orig.mgf)
		(echo xf $oblqxf; cat $d/$f:r.orig.mgf; echo xf) \
				> $d/$f:r.oblq.mgf
		set oblqf=($oblqf $f:r.oblq.mgf)
	end
else
	set origf=stdin.orig.mgf
	set oblqf=stdin.oblq.mgf
	xform -e > $d/stdin.orig
	rad2mgf $d/stdin.orig > $d/stdin.orig.mgf
	(echo xf $oblqxf; cat $d/stdin.orig.mgf; echo xf) \
			> $d/stdin.oblq.mgf
endif
cd $d
set rce='xm=($1+$2)/2;ym=($3+$4)/2;zm=($5+$6)/2;\
max(a,b):if(a-b,a,b);r=max(max($2-$1,$4-$3),$6-$5)*.52;\
$1=xm-r;$2=xm+r;$3=ym-r;$4=ym+r;$5=zm-r;$6=zm+r'
set origdim=`getbbox -h *.orig | rcalc -e $rce:q`
set oblqdim=`xform $oblqxf *.orig | getbbox -h | rcalc -e $rce:q`
mgf2meta -t .005 x $origdim $origf > x.mta
mgf2meta -t .005 y $origdim $origf > y.mta
mgf2meta -t .005 z $origdim $origf > z.mta
mgf2meta -t .005 x $oblqdim $oblqf > o.mta
plot4 {x,y,z,o}.mta
quit:
cd
exec rm -rf $d
