/* * This file is part of contacts/contactui *
 * Copyright © 2009 Nokia Corporation and/or its subsidiary(-ies). All rights reserved.
 * Contact: Aleksandar Stojiljkovic <aleksandar.stojiljkovic@nokia.com>
 * This software, including documentation, is protected by copyright controlled by
 * Nokia Corporation. All rights are reserved. Copying, including reproducing, storing,
 * adapting or translating, any or all of this material requires the prior written consent
 * of Nokia Corporation. This material also contains confidential information which may
 * not be disclosed to others without the prior written consent of Nokia. */

// Qt includes
#include <QDir>
#include <QFileInfo>
#include <QTime>
#include <QMutexLocker>

// People & Contacts includes
#include "logger.h"

// static initialization
Logger * Logger::instance = 0;

Logger* Logger::installLogger(const QString & filename, uint size, uint file_count)
{
    // do we have an instance installed?
    if ( Logger::instance != 0 ) {
        return 0;
    }

    try{
        Logger::instance = new Logger(filename, size, file_count);
    }catch( std::bad_alloc & ){
        // Catch memory allocation error which might be caused by
        qCritical("Logger::installLogger: memory allocation error");
        Logger::instance = 0;
    }

    return Logger::instance;
}

Logger::Logger (const QString & filename, uint size, uint file_count)
    : err(stderr),
    logging_level(QtDebugMsg),
    logged_lines(0),
    rotation_check_interval(400),
    log_file_count(file_count),
    min_size(size * 1024),
    has_whitelist(false),
    has_blacklist(false)
{
    // log levels
    log_levels << "debug" << "warn " << "crit " << "fatal";

    // should we log to stdout?
    log_to_console = true;

    if (filename.isEmpty() == true) {
        log_to_file = false;
    }
    else {
        // check if given filename was absolute, otherwise lets use home dir
        if (QDir::isAbsolutePath(filename)) {
            file.setFileName( filename );
        }
        else {
            file.setFileName(QDir::homePath() + '/' + filename);
        }

        // check if directories need to be created
        QFileInfo info(file.fileName());
        if (info.dir().exists() == false) {
            QDir dir;
            dir.mkpath(info.absolutePath ());
        }

        // open the file for appending
        if ( file.open( QIODevice::WriteOnly | QIODevice::Append ) ) {
            // log file successfully opened
            log_to_file = true;

            // set a stream to the file too
            stream.setDevice( &file );
        }
        else {
            // failed to open...
            log_to_file = false;

            writeMessage(QtDebugMsg,
                         qPrintable(QString("Logger::Logger: failed to open log file: %1").arg(filename)));

            // still continue, at least we can make nice output if loggin to console gets enabled
        }
    }

    // register ourselves as a debug message handler
    old_msg_handler = qInstallMsgHandler( Logger::messageHandler );

    writeMessage(QtDebugMsg,
                 qPrintable(QString("Logger::Logger: logger installed with output to %1").arg(filename)));
}

Logger::~Logger () {
    writeMessage(QtDebugMsg,
                 "Logger::~Logger: taken old message handler into use");

    Logger::instance = 0;

    // we can install old_msg_handler even if it is 0 (then Qt default will be used)
    qInstallMsgHandler(old_msg_handler);

    if ( file.isOpen() ) {
        writeMessage(QtDebugMsg,
                     qPrintable(QString("Logger::~Logger: closing file %1").arg(file.fileName())));
        file.close();
    }
}

void Logger::messageHandler (QtMsgType type, const char *msg) {
    QMutexLocker locker( &Logger::instance->mutex );

    if (!Logger::instance)
        return;

    Logger::instance->writeMessage(type, msg);

    if (Logger::instance->log_to_file) {
        // enough lines logged for a potential rotation?
        if ( Logger::instance->logged_lines++ >= Logger::instance->rotation_check_interval ) {
            // time to at least check for a rotation
            Logger::instance->rotate ();
        }
    }
}

void Logger::writeMessage(QtMsgType type, const char *msg)
{
    // check if message type exceeds logging level set
    if (type < logging_level) {
        return;
    }

    // check if message is whitelisted or blacklisted
    if (has_whitelist) {
        // check against whitelist

        // pass through messages more serious than QtWarningMsg
        if (type == QtDebugMsg || type == QtWarningMsg) {
            QString msg_string(msg);
            bool passed(false);

            foreach (const QString & prefix, prefixes) {
                if (msg_string.startsWith(prefix)) {
                    passed = true;
                    break;
                }
            }

            if (!passed) {
                return;
            }
        }
    }
    else if (has_blacklist) {
        // check against blacklist

        // pass through messages more serious than QtWarningMsg
        if (type == QtDebugMsg || type == QtWarningMsg) {
            QString msg_string(msg);
            foreach (const QString & prefix, prefixes) {
                if (msg_string.startsWith(prefix)) {
                    return;
                }
            }
        }
    }

    if (log_to_console) {
        // write to the stderr
        err << QTime::currentTime().toString("hh:mm:ss.zzz")
                << " [" << log_levels[type] << ']'
                << ' '
                << msg << endl;
    }

    if (log_to_file) {
        // write to the log file
        stream << QTime::currentTime().toString("hh:mm:ss.zzz")
                << " [" << log_levels[type] << ']'
                << ' '
                << msg << endl;
    }
}

void Logger::rotate () {
    // always reset the logged lines so that we again log some lines before redoing this check
    logged_lines = 0;

    QFileInfo info (file.fileName());

    // big enough for rotation?
    if ( info.size() < min_size ) {
        return;
    }

    writeMessage(QtDebugMsg,
                 "Logger::rotate: maximum log file size exceeded, performing log file rotation");

    // get the directory where we store logfiles and the actual filename without any paths
    QDir dir         = info.dir ();
    QString filename = info.fileName();

    // now find all files that match the filename pattern
    QStringList old_files = QDir ( dir.path(), filename + ".?" ).entryList ( QDir::Files,
                                                                             QDir::Name );

    // any old files at all?
    if ( old_files.size() > 0 ) {
        for ( int index = old_files.size() - 1; index >= 0; index-- ) {
            // have we reached file maximum? this file is no longer rotated but should be nuked
            if ( index == log_file_count-1 ) {
                // yup,
                QFile::remove ( info.absolutePath () + '/' + old_files[index] );
                continue;
            }

            // a file 0-8, so create a new filename with the suffix += 1
            QString rename_to = old_files[index];
            rename_to.truncate ( rename_to.size() - 1 );
            rename_to += QString::number ( index + 1 );

            // perform the renaming
            QFile::rename ( info.absolutePath () + '/' + old_files[index],
                            info.absolutePath () + '/' + rename_to );
        }
    }

    // flush stream and file
    stream.flush();
    file.flush();

    // now finally rename our open file
    file.rename( file.fileName() + ".0" );

    // try to create open a new file
    file.close();
    file.setFileName ( info.absoluteFilePath() );
    if ( file.open(QFile::WriteOnly | QIODevice::Append) ) {
        // open a strem to the file
        stream.setDevice ( &file );
    }

    else {
        // failed to open the new log file
        log_to_file = false;
        writeMessage(QtWarningMsg,
                     qPrintable(QString("Logger::rotate: failed to open the log file %1 for writing!")
                                .arg(file.fileName())));
    }
}

void Logger::setConsoleLoggingEnabled(bool enabled)
{
    writeMessage(QtDebugMsg,
                 qPrintable(QString("Logger::setConsoleLoggingEnabled: %1.").arg(enabled)));
    if (enabled == false) {
        writeMessage(QtDebugMsg,
                     qPrintable(QString("Logger::setConsoleLoggingEnabled: Launch with -log-console to enable console logging.")));
    }

    log_to_console = enabled;
}

void Logger::setLoggingLevel(const QString & levelString)
{
    if (levelString.isEmpty()) {
        return;
    }
    if (levelString == "debug") {
        logging_level = QtDebugMsg;
    }
    else if (levelString == "warning") {
        logging_level = QtWarningMsg;
    }
    else if (levelString == "critical") {
        logging_level = QtCriticalMsg;
    }
    else {
        writeMessage(QtWarningMsg,
                     qPrintable(QString("Logger::setLoggingLevel: invalid logging level given %1")
                                .arg(levelString)));
    }

    writeMessage(QtDebugMsg,
                 qPrintable(QString("Logger::setLoggingLevel: logging level set to %1").arg(levelString)));
}

void Logger::setLoggingPrefixes(const QStringList &whitelist, const QStringList blacklist)
{
    if (whitelist.isEmpty() == false && blacklist.isEmpty() == false) {
        writeMessage(QtWarningMsg,
                     "Logger::setLoggingPrefixes: can not give both whitelist and blacklist!");
        return;
    }

    has_whitelist = !whitelist.isEmpty();
    has_blacklist = !blacklist.isEmpty();

    if (whitelist.isEmpty() == false) {
        prefixes = whitelist;

        writeMessage(QtDebugMsg,
                     qPrintable(QString("Logger::setLoggingPrefixes: whitelisted following prefixes: %1")
                                .arg(prefixes.join(", "))));
    }
    else if (blacklist.isEmpty() == false) {
        prefixes = blacklist;

        writeMessage(QtDebugMsg,
                     qPrintable(QString("Logger::setLoggingPrefixes: blacklisted following prefixes: %1")
                                .arg(prefixes.join(", "))));
    }
}
