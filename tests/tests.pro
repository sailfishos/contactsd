TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += libtelepathy ut_telepathyplugin

UNIT_TESTS += ut_telepathyplugin

testxml.target = tests.xml
testxml.commands = sh $$PWD/mktests.sh $$UNIT_TESTS >$@ || rm -f $@
testxml.depends = $$UNIT_TESTS

install_testxml.files = $$testxml.target
install_testxml.path = $$PREFIX/share/contactsd-tests
install_testxml.depends = $$testxml.target
install_testxml.CONFIG = no_check_exist

INSTALLS += install_testxml

QMAKE_EXTRA_TARGETS += testxml
QMAKE_DISTCLEAN += $$testxml.target

POST_TARGETDEPS += $$testxml.target
