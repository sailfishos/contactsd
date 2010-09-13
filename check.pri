check.commands += \
    sh $$TOP_SOURCEDIR/tools/with-session-bus.sh \
        --config-file=$$TOP_BUILDDIR/tests/dbus-1/session.conf -- \
        sh $$TOP_SOURCEDIR/tests/runtest.sh ./$(QMAKE_TARGET)
