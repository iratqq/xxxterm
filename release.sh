#!/bin/sh
#
# $xxxterm: release.sh,v 1.3 2011/02/25 18:10:04 todd Exp $
# XXX This sucks, I know.  send me diffs to make it better
#

td=`mktemp -d /tmp/release.XXXXXXXXXX`
cd $td
cvs -d $(cat $1/CVS/Root) -Q export -D tomorrow xxxterm || exit 1
REL=$(awk '/ .xxxterm: xxxterm.c,v/{print $4;}' xxxterm/xxxterm.c)
mv xxxterm xxxterm-$REL
tar zcf xxxterm-$REL.tgz xxxterm-$REL
echo $td/xxxterm-$REL.tgz
