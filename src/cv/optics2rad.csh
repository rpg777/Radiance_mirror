#!/bin/csh -f
# RCSid $Id: optics2rad.csh,v 2.1 2006/05/19 03:50:13 greg Exp $
#
# Convert Optics 5 output to correct Radiance input
#
if ($#argv < 1) then
	echo "Usage: $0 optics.mat .."
	exit 1
endif
cat > /tmp/opt.fmt << '_EOF_'
void glass $(name)_glass
0
0
3 ${Rtn} ${Gtn} ${Btn}

void BRTDfunc $(name)_front
10
	${fRrho} ${fGrho} ${fBrho}
	${Rtau} ${Gtau} ${Btau}
     0 0 0
     .
0
9 0 0 0 0 0 0 0 0 0

void BRTDfunc $(name)_back
10
	${bRrho} ${bGrho} ${bBrho}
	${Rtau} ${Gtau} ${Btau}
     0 0 0
     .
0
9 0 0 0 0 0 0 0 0 0
'_EOF_'

echo "# Output generated by $0 from $*"

cat > /tmp/out$$.fmt << '_EOF_'

void glass $(name)
0
0
3 ${Rtn} ${Gtn} ${Btn}
'_EOF_'

rcalc -e 'abs(x):if(x,x,-x);and(a,b):if(a,b,a);not(x):if(x,-1,1)' \
	-e 'lum(r,g,b):.265*r+.670*g+.065*b' \
	-e 'trans=lum(Rtau,Gtau,Btau)' \
	-e 'rfront=lum(fRrho,fGrho,fBrho);rback=lum(bRrho,bGrho,bBrho)' \
	-e 'cond=0.005-abs(rfront-rback)' \
	-i /tmp/opt.fmt -o /tmp/out$$.fmt $*

cat > /tmp/out$$.fmt << '_EOF_'

void BRTDfunc $(name)
10
	rR_clear rG_clear rB_clear
	${Rtau}*tR_clear ${Gtau}*tG_clear ${Btau}*tB_clear
     0 0 0
     window.cal
0
15 0 0 0 0 0 0 0 0 0
	${fRrho} ${fGrho} ${fBrho}
	${bRrho} ${bGrho} ${bBrho}
'_EOF_'

rcalc -e 'abs(x):if(x,x,-x);and(a,b):if(a,b,a);not(x):if(x,-1,1)' \
	-e 'lum(r,g,b):.265*r+.670*g+.065*b' \
	-e 'trans=lum(Rtau,Gtau,Btau)' \
	-e 'rfront=lum(fRrho,fGrho,fBrho);rback=lum(bRrho,bGrho,bBrho)' \
	-e 'cond=and(trans-.645, not(0.005-abs(rfront-rback)))' \
	-i /tmp/opt.fmt -o /tmp/out$$.fmt $*

cat > /tmp/out$$.fmt << '_EOF_'

void BRTDfunc $(name)
10
	rR_bronze rG_bronze rB_bronze
	${Rtau}*tR_bronze ${Gtau}*tG_bronze ${Btau}*tB_bronze
     0 0 0
     window.cal
0
15 0 0 0 0 0 0 0 0 0
	${fRrho} ${fGrho} ${fBrho}
	${bRrho} ${bGrho} ${bBrho}
'_EOF_'

rcalc -e 'abs(x):if(x,x,-x);and(a,b):if(a,b,a);not(x):if(x,-1,1)' \
	-e 'lum(r,g,b):.265*r+.670*g+.065*b' \
	-e 'trans=lum(Rtau,Gtau,Btau)' \
	-e 'rfront=lum(fRrho,fGrho,fBrho);rback=lum(bRrho,bGrho,bBrho)' \
	-e 'cond=and(not(trans-.645), not(0.005-abs(rfront-rback)))' \
	-i /tmp/opt.fmt -o /tmp/out$$.fmt $*

rm -f /tmp/opt.fmt /tmp/out$$.fmt
