AC_INIT(tuxbox-plugins,0.0.1)
AM_INIT_AUTOMAKE(tuxbox-plugins,0.0.1)

TUXBOX_APPS
TUXBOX_APPS_DIRECTORY

AC_PROG_CC
AC_PROG_CXX
AC_DISABLE_STATIC
AM_PROG_LIBTOOL

TUXBOX_BOXTYPE

if false; then
if test "$BOXTYPE" != "tripledragon"; then
# we neither have dvb headers nor driver sources nor the dbox capture api on TD ;)
TUXBOX_APPS_DVB
TUXBOX_APPS_DRIVER
TUXBOX_APPS_CAPTURE
fi
fi

# needed by enigma/movieplayer
TUXBOX_APPS_LIB_PKGCONFIG_CHECK(CURL,libcurl)

# needed by cooliTSclimax
TUXBOX_APPS_LIB_PKGCONFIG_CHECK(AVFORMAT,libavformat)
TUXBOX_APPS_LIB_PKGCONFIG_CHECK(AVCODEC,libavcodec)
TUXBOX_APPS_LIB_PKGCONFIG_CHECK(AVUTIL,libavutil)

# needed by tuxmail
TUXBOX_APPS_LIB_PKGCONFIG(FREETYPE,freetype2)
TUXBOX_APPS_LIB_PKGCONFIG(PNG,libpng)

# needed by enigma-demo
# TUXBOX_APPS_LIB_PKGCONFIG(SIGC,sigc++-1.2)
if test "$BOXTYPE" = "dbox2"; then
TUXBOX_APPS_LIB_PKGCONFIG(TUXBOX,tuxbox)
fi

# needed by enigma-demo
# TUXBOX_APPS_LIB_PKGCONFIG(XMLTREE,tuxbox-xmltree)

# needed by vncviewer
# TUXBOX_APPS_LIB_PKGCONFIG(CONFIGFILE,tuxbox-configfile)

# needed by sysinfo
# TUXBOX_APPS_LIB_PKGCONFIG(MD5SUM,tuxbox-md5sum)
# TUXBOX_APPS_LIB_PKGCONFIG(UCODES,tuxbox-ucodes)

# needed by tuxtxt
# TUXBOX_APPS_LIB_PKGCONFIG(TUXTXT,tuxbox-tuxtxt)

# CPPFLAGS="$CPPFLAGS $TUXTXT_CFLAGS"
# CFLAGS="$CFLAGS $TUXTXT_CFLAGS"

AC_OUTPUT([
Makefile
include/Makefile
msgbox/Makefile
input/Makefile
tuxmail/Makefile
tuxcom/Makefile
shellexec/Makefile
cooliTSclimax/Makefile
tuxbox-plugins.pc
])
