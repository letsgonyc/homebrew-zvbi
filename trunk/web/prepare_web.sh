#!/bin/sh
#$Id: prepare_web.sh,v 1.3 2002-04-02 22:44:08 mschimek Exp $

cd /home/groups/z/za/zapping
chmod ug=rwX,o-rwx . -R
umask 007
cvs -z3 update -dP
cvs -z3 -d:pserver:anonymous@cvs.zapping.sourceforge.net:/cvsroot/zapping co zapping/ChangeLog
mv zapping/ChangeLog htdocs/
chmod a+rX htdocs/ChangeLog
rm -fR zapping
chmod a+rX . htdocs
cd htdocs
chmod a+r *.php *.inc *.html *.jpeg *.gif *.png bookmark.ico rescd.zip
for i in images_* screenshots; do
  find $i -name "CVS" -prune -o -exec chmod a+rX '{}' ';'
done
cd -
