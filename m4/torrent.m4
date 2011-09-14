#                                               -*- Autoconf -*-
# This file is part of the aMule Project.
#
# Copyright (c) 2003-2011 aMule Team ( admin@amule.org / http://www.amule.org )
#
# Any parts of this program derived from the xMule, lMule or eMule project,
# or contributed by third-party developers are copyrighted by their
# respective authors.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA
#

dnl ---------------------------------------------------------------------------
dnl MULE_CHECK_TORRENT
dnl
dnl Checks if the Torrent library is requested, exists, and whether it should and
dnl could be linked statically.
dnl ---------------------------------------------------------------------------
AC_DEFUN([MULE_CHECK_TORRENT],
[
	m4_define([MIN_LIBTORRENT_VERSION], [m4_ifval([$1], [$1], [0.16.0])])dnl
	MULE_ARG_ENABLE([torrent], [no], [compile with libtorrent to join swarms and speed up downloads])

	MULE_IF_ENABLED([torrent], [
		AC_MSG_CHECKING([for libtorrent version >= MIN_LIBTORRENT_VERSION])
		PKG_CHECK_MODULES(libtorrent, libtorrent-rasterbar >= MIN_LIBTORRENT_VERSION,
                		  CXXFLAGS="$CXXFLAGS $libtorrent_CFLAGS";
		                  LIBS="$LIBS $libtorrent_LIBS")
		AS_IF([$PKG_CONFIG libtorrent-rasterbar --exists], [LIBTORRENT_VERSION=`$PKG_CONFIG libtorrent-rasterbar --modversion`])
		AC_DEFINE([SUPPORT_LIBTORRENT], [1], [Define if you want libTorrent support.])
	])	
])
AC_SUBST([TORRENT_CPPFLAGS])dnl
AC_SUBST([TORRENT_LDFLAGS])dnl
AC_SUBST([TORRENT_LIBS])dnl
