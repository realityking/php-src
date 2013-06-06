dnl $Id$
dnl config.m4 for extension utf8

  old_CPPFLAGS=$CPPFLAGS
  CPPFLAGS="$INCLUDES -I$abs_srcdir $CPPFLAGS"
  AC_DEFINE(HAVE_UTF8, 1, [Whether UTF-8 support is enabled])
  PHP_NEW_EXTENSION(utf8, utf8.c utf8tools.c, no)
  PHP_INSTALL_HEADERS([ext/utf8], [php_utf8.h utf8.h utf8decode.h])
