/***************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (people-users@projects.maemo.org)
**
** This file is part of contactsd.
**
** If you have questions regarding the use of this file, please contact
** Nokia at people-users@projects.maemo.org.
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation
** and appearing in the file LICENSE.LGPL included in the packaging
** of this file.
**
****************************************************************************/

#include "logger.h"

#include <QDir>
#include <QFileInfo>
#include <QTime>
#include <QMutexLocker>

// static initialization
Logger *Logger::instance = 0;

Logger *Logger::installLogger(const QString &fileName, uint size,
        uint fileCount)
{
    if (!instance) {
        instance = new Logger(fileName, size, fileCount);
    }
    return instance;
}

Logger::Logger(const QString &fileName, uint size, uint fileCount)
    : err(stderr),
      loggingLevel(QtDebugMsg),
      loggedLines(0),
      rotationCheckInterval(400),
      logFileCount(fileCount),
      minSize(size * 1024),
      logToConsole(true),
      hasWhiteList(false),
      hasBlackList(false)
{
    // default log levels
    logLevels << "debug" << "warning " << "critical " << "fatal";

    if (fileName.isEmpty() == true) {
        logToFile = false;
    } else {
        // check if given fileName was absolute, otherwise lets use home dir
        if (QDir::isAbsolutePath(fileName)) {
            file.setFileName(fileName);
        } else {
            file.setFileName(QDir::homePath() + '/' + fileName);
        }

        // check if directories need to be created
        QFileInfo info(file.fileName());
        if (info.dir().exists() == false) {
            QDir dir;
            dir.mkpath(info.absolutePath());
        }

        // open the file for appending
        if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
            // log file successfully opened
            logToFile = true;

            // set a stream to the file too
            stream.setDevice(&file);
        }
        else {
            // failed to open...
            logToFile = false;

            writeMessage(QtDebugMsg,
                    qPrintable(QString("Failed to open log file %1 for writing")
                        .arg(fileName)));

            // still continue, at least we can make nice output if loggin to
            // console is enabled
        }
    }

    // register ourselves as a debug message handler
    oldMsgHandler = qInstallMsgHandler(Logger::messageHandler);

    writeMessage(QtDebugMsg,
            qPrintable(QString("Logger installed with log file %1")
                .arg(fileName)));
}

Logger::~Logger()
{
    instance = 0;

    // restore previous message handler
    qInstallMsgHandler(oldMsgHandler);

    if (file.isOpen()) {
        writeMessage(QtDebugMsg,
                qPrintable(QString("Closing log file %1")
                    .arg(file.fileName())));
        file.close();
    }
}

void Logger::messageHandler(QtMsgType type, const char *msg)
{
    QMutexLocker locker(&instance->mutex);

    if (!instance) {
        return;
    }

    instance->writeMessage(type, msg);

    if (instance->logToFile) {
        // enough lines logged for a potential rotation?
        if (Logger::instance->loggedLines++ >=
                Logger::instance->rotationCheckInterval) {
            // time to at least check for a rotation
            Logger::instance->rotate();
        }
    }
}

void Logger::writeMessage(QtMsgType type, const char *msg)
{
    // check if message type exceeds logging level set
    if (type < loggingLevel) {
        return;
    }

    // check if message is whitelisted or blacklisted
    if (hasWhiteList) {
        // check against whitelist

        // pass through messages more serious than QtWarningMsg
        if (type == QtDebugMsg || type == QtWarningMsg) {
            QString msgString(msg);
            bool passed = false;
            foreach (const QString &prefix, prefixes) {
                if (msgString.startsWith(prefix)) {
                    passed = true;
                    break;
                }
            }

            if (!passed) {
                return;
            }
        }
    } else if (hasBlackList) {
        // check against blacklist

        // pass through messages more serious than QtWarningMsg
        if (type == QtDebugMsg || type == QtWarningMsg) {
            QString msgString(msg);
            foreach (const QString &prefix, prefixes) {
                if (msgString.startsWith(prefix)) {
                    return;
                }
            }
        }
    }

    QString currentTime = QTime::currentTime().toString("hh:mm:ss.zzz");
    if (logToConsole) {
        // write to the stderr
        err << currentTime <<
            " [" << logLevels[type] << ']' <<
            ' ' << msg << endl;
    }

    if (logToFile) {
        // write to the log file
        stream << currentTime <<
            " [" << logLevels[type] << ']' <<
            ' ' << msg << endl;
    }
}

void Logger::rotate()
{
    // always reset the logged lines so that we again log some lines before
    // redoing this check
    loggedLines = 0;

    QFileInfo info(file.fileName());

    // big enough for rotation?
    if (info.size() < minSize) {
        return;
    }

    writeMessage(QtDebugMsg,
            "Maximum log file size exceeded, performing log file rotation");

    // get the directory where we store logfiles and the actual fileName without any paths
    QDir dir = info.dir();
    QString fileName = info.fileName();

    // now find all files that match the fileName pattern
    QStringList oldFiles = QDir(dir.path(), fileName + ".?").entryList(
            QDir::Files, QDir::Name);

    // any old files at all?
    if (oldFiles.size() > 0) {
        for (int index = oldFiles.size() - 1; index >= 0; index--) {
            // have we reached file maximum? this file is no longer rotated but should be nuked
            if (index == logFileCount - 1) {
                // yup,
                QFile::remove(info.absolutePath() + '/' + oldFiles[index]);
                continue;
            }

            // a file 0-8, so create a new fileName with the suffix += 1
            QString renameTo = oldFiles[index];
            renameTo.truncate(renameTo.size() - 1);
            renameTo += QString::number(index + 1);

            // perform the renaming
            QFile::rename(info.absolutePath() + '/' + oldFiles[index],
                          info.absolutePath() + '/' + renameTo);
        }
    }

    // flush stream and file
    stream.flush();
    file.flush();

    // now finally rename our open file
    file.rename(file.fileName() + ".0");

    // try to create open a new file
    file.close();
    file.setFileName(info.absoluteFilePath());
    if (file.open(QFile::WriteOnly | QIODevice::Append)) {
        // open a strem to the file
        stream.setDevice(&file);
    } else {
        // failed to open the new log file
        logToFile = false;
        writeMessage(QtWarningMsg,
                qPrintable(QString("Failed to open the log file %1 for writing!")
                    .arg(file.fileName())));
    }
}

void Logger::setConsoleLoggingEnabled(bool enabled)
{
    if (logToConsole == enabled) {
        return;
    }

    if (enabled) {
        writeMessage(QtDebugMsg,
                qPrintable(QString("Enabling console logging")));
    } else {
        writeMessage(QtDebugMsg,
                qPrintable(QString("Disabling console logging")));
    }

    logToConsole = enabled;
}

void Logger::setLoggingLevel(const QString &levelString)
{
    if (levelString.isEmpty()) {
        return;
    }

    if (levelString == "debug") {
        loggingLevel = QtDebugMsg;
    } else if (levelString == "warning") {
        loggingLevel = QtWarningMsg;
    } else if (levelString == "critical") {
        loggingLevel = QtCriticalMsg;
    } else {
        writeMessage(QtWarningMsg,
                qPrintable(QString("Trying to set an invalid logging level given %1")
                    .arg(levelString)));
        return;
    }

    writeMessage(QtDebugMsg,
            qPrintable(QString("Logging level set to %1").arg(levelString)));
}

void Logger::setLoggingPrefixes(const QStringList &whitelist,
        const QStringList &blacklist)
{
    if (whitelist.isEmpty() == false && blacklist.isEmpty() == false) {
        writeMessage(QtWarningMsg,
                "Cannot set both logging whitelist and blacklist!");
        return;
    }

    hasWhiteList = !whitelist.isEmpty();
    hasBlackList = !blacklist.isEmpty();

    if (whitelist.isEmpty() == false) {
        prefixes = whitelist;
        writeMessage(QtDebugMsg,
                qPrintable(QString("Whitelisted following logging prefixes: %1")
                    .arg(prefixes.join(", "))));
    }
    else if (blacklist.isEmpty() == false) {
        prefixes = blacklist;
        writeMessage(QtDebugMsg,
                qPrintable(QString("Blacklisted following logging prefixes: %1")
                    .arg(prefixes.join(", "))));
    }
}
