#!/bin/sh
#
#
# Update LiVES.pot file with messages from perl Plugins
#
#
# Erase & Make original potfile
# (c) G. Finch (salsaman@xs4all.nl)

# released under the GNU GPL 2
# see file COPYING or www.gnu.org for details

#syntax ./update_with_plugins.sh $(PACKAGE) $(prefix)

pwd=`pwd`

cd po

rm $2.pot
make $2.pot
#
#
# Compile plugin list $(DESTDIR) $(prefix)
./make_rfx_builtin_list.pl $4 $3

if ! test -s POTFILES_PLUGINS ; then
 echo no rendered effect plugins found.; exit 0;
fi

#
# Add message data from plugins and smogrify
#
./pxgettext $3 $2.pot `cat POTFILES_PLUGINS` >> $2.pot

# LiVES must match with $GUI_NAME in smogrify
./update_with_smogrify smogrify $2.pot LiVES >> $2.pot
#
#
# Update $LANG.po
#

echo make update-gmo
make update-gmo

if [ "$1" = "install" ]; then
    echo make DESTDIR=$4 install
    make DESTDIR=$4 install
fi

echo "Added messages from RFX plugins and smogrify"

cd $pwd
