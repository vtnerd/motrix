AC_INIT([motrix], [0.1], [code@leeclagett.com], [motrix], [https://code.leeclagett.com/motrix])
AM_INIT_AUTOMAKE([foreign subdir-objects dist-bzip2 dist-xz])

#LT_INIT
AC_PROG_CXX
AC_LANG(C++)

AC_MSG_CHECKING([c++11 enable-if type traits])
AC_COMPILE_IFELSE(
  [AC_LANG_SOURCE([[#include <type_traits> typename std::enable_if<true>::type foo();]])], AC_MSG_RESULT([yes]), AC_MSG_ERROR([failed])
)

AC_MSG_CHECKING([c++11 variadic template support])
AC_COMPILE_IFELSE(
  [AC_LANG_SOURCE([[template<typename... T> void foo(T...) {}]])], AC_MSG_RESULT([yes]), AC_MSG_ERROR([failed])
)

AC_CHECK_HEADER([zmq.h], [], AC_MSG_ERROR([Unable to find ZeroMQ header]))
AC_CHECK_HEADER([ncurses.h], [], AC_MSG_ERROR([Unable to find ncurses header]))

AC_SEARCH_LIBS([zmq_z85_encode], [zmq], [], AC_MSG_ERROR([Unable to find ZeroMQ lib with z85 functions]))
AC_SEARCH_LIBS([curs_set], [tinfo ncurses], [], AC_MSG_ERROR([Unable to find tinfo compatible ilb]))
AC_SEARCH_LIBS([newwin], [ncurses], [], AC_MSG_ERROR([Unable to find ncurses compatible lib]))

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
