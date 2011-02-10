daemon.target = with-daemon.sh
daemon.depends = $$PWD/with-daemon.sh.in
daemon.path = $$PREFIX/share/contactsd-tests
daemon.commands = \
    sed -e \"s,@BINDIR@,$$BINDIR,g\" \
        -e \"s,@PLUGINDIR@,$$LIBDIR/contactsd-1.0/plugins,g\" \
    $< > $@ && chmod +x $@ || rm -f $@

wrapper.target = ut_telepathyplugin-wrapper.sh
wrapper.depends = $$PWD/ut_telepathyplugin-wrapper.sh.in
wrapper.path = $$PREFIX/share/contactsd-tests
wrapper.commands = \
    sed -e \"s,@SCRIPTDIR@,$$PREFIX/share/contactsd-tests,g\" \
        -e \"s,@BINDIR@,$$BINDIR,g\" \
        -e \"s,@WITH_DAEMON@,$$daemon.target,g\" \
    $< > $@ && chmod +x $@ || rm -f $@

install_extrascripts.files = $$wrapper.target $$daemon.target with-session-bus.sh session.conf
install_extrascripts.path = $$PREFIX/share/contactsd-tests
install_extrascripts.depends = daemon wrapper
install_extrascripts.CONFIG = no_check_exist

QMAKE_INSTALL_FILE = cp -p
QMAKE_EXTRA_TARGETS += daemon wrapper
QMAKE_CLEAN += $$daemon.target $$wrapper.target

PRE_TARGETDEPS += $$daemon.target $$wrapper.target
INSTALLS += install_extrascripts

