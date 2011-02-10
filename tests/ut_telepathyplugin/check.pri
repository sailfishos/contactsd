check_daemon.target = check-with-daemon.sh
check_daemon.depends = $$PWD/with-daemon.sh.in
check_daemon.commands = \
    sed -e "s,@BINDIR@,$$TOP_BUILDDIR/src,g" \
        -e "s,@PLUGINDIR@,$$TOP_BUILDDIR/plugins/telepathy,g" \
    $< > $@ && chmod +x $@ || rm -f $@

check_wrapper.target = check-ut_telepathyplugin-wrapper.sh
check_wrapper.depends = $$PWD/ut_telepathyplugin-wrapper.sh.in
check_wrapper.commands = \
    sed -e "s,@SCRIPTDIR@,$$PWD,g" \
        -e "s,@BINDIR@,$$PWD,g" \
        -e "s,@WITH_DAEMON@,$$check_daemon.target,g" \
    $< > $@ && chmod +x $@ || rm -f $@
 
check.depends = $$TARGET check_wrapper check_daemon
check.commands = sh $$check_wrapper.target

QMAKE_EXTRA_TARGETS += check_wrapper check_daemon check
QMAKE_CLEAN += $$check_daemon.target $$check_wrapper.target
