# This file is part of Contacts daemon
#
# Copyright (c) 2010-2011 Nokia Corporation and/or its subsidiary(-ies).
#
# Contact:  Nokia Corporation (info@qt.nokia.com)
#
# GNU Lesser General Public License Usage
# This file may be used under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation and appearing in the
# file LICENSE.LGPL included in the packaging of this file.  Please review the
# following information to ensure the GNU Lesser General Public License version
# 2.1 requirements will be met:
# http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
#
# In addition, as a special exception, Nokia gives you certain additional rights.
# These rights are described in the Nokia Qt LGPL Exception version 1.1, included
# in the file LGPL_EXCEPTION.txt in this package.
#
# Other Usage
# Alternatively, this file may be used in accordance with the terms and
# conditions contained in a signed written agreement between you and Nokia.

check_daemon.target = check-with-daemon.sh
check_daemon.depends = $$PWD/with-daemon.sh.in
check_daemon.commands = \
    sed -e "s,@BINDIR@,$$TOP_BUILDDIR/src,g" \
        -e "s,@PLUGINDIR@,$$TOP_BUILDDIR/plugins/telepathy,g" \
    $< > $@ && chmod +x $@ || rm -f $@

check_wrapper.target = check-ut_telepathyplugin-wrapper.sh
check_wrapper.depends = $$PWD/ut_telepathyplugin-wrapper.sh.in
check_wrapper.commands = \
    sed -e "s,@SCRIPTDIR@,$$PWD/..,g" \
        -e "s,@BINDIR@,$$PWD,g" \
        -e "s,@WITH_DAEMON@,$$check_daemon.target,g" \
    $< > $@ && chmod +x $@ || rm -f $@
 
check.depends = $$TARGET check_wrapper check_daemon
check.commands = sh $$check_wrapper.target

QMAKE_EXTRA_TARGETS += check_wrapper check_daemon check
QMAKE_CLEAN += $$check_daemon.target $$check_wrapper.target
