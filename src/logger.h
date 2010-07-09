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

#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QFile>
#include <QString>
#include <QStringList>
#include <QTextStream>
#include <QMutex>

/**
 * @brief Simple Qt logging message handler class.
 *
 * This class is used to prefix normal Qt debug log messages with a date and log level.
 * Logger also writes messages to a file if one is given.
 **/
class Logger : public QObject
{
Q_OBJECT

public:

    /**
     * Public static Logger installer, for creating singleton instance of Logger.
     *
     * @param filename the file to log to.
     * @param size the minimum size in KB of each logfile.
     * @param file_count the number of rotated log files to save.
     * @return Logger instance or NULL in case of logger already installed or memory allocation error.
     */
    static Logger* installLogger(const QString & filename = QString(), uint size = 100, uint file_count = 5);

    /**
     * Destructor, closes the log file and installs original message handler.
     */
    ~Logger ();

    /**
     * Sets console logging enabled / disabled. By default console logging could be disabled.
     *
     * @param enabled flag to .
     */
    void setConsoleLoggingEnabled(bool enabled);

    /**
     * Sets logging level, i.e. show only messages of given output level or above.
     *
     * @param new logging level debug|warning|critical.
     */
    void setLoggingLevel(const QString & level);

    /**
     * Limit logging output to given whitelist or blacklist. Note that only one can be active.
     *
     * @param whitelist show only messages that start with the prefix from whitelist.
     * @param blacklist shot only messages that do not start with prefix from blacklist.
     */
    void setLoggingPrefixes(const QStringList &whitelist, const QStringList blacklist);

private:
    /**
     * Creates the logger for prefixing and logging into the file @p filename if one is given.
     * The file is opened for appendeing so no old data is lost. The given @p filename must be a
     * fully qualified path if needed.
     *
     * @param filename the file to log to.
     * @param size the minimum size in KB of each logfile.
     * @param file_count the number of rotated log files to save.
     */
    Logger (const QString & filename, uint size, uint file_count);

    /**
     * The actual message handler method. This method will get called for each logged @p msg.
     * The log @p type is mapped to a string and prefixed, along with the current time.
     *
     * @param type the log level type.
     * @param msg the message to be logged.
     */
    static void messageHandler (QtMsgType type, const char *msg);

    /**
     * Performs log file rotation. If a the size of the log file exceeds a specific size (in @e
     * min_size) then a full rotation is performed. The current log file is renamed with an
     * additional @e .0 suffix and all old files have their suffix incremented by one. If we
     * have @e store_count old files then the last one is removed.
     */
    void rotate ();

    /**
     * @brief Worker method that does the actual writing of log message.
     *
     * @param type the type of log message (debug, warning etc).
     * @param msg the actual message to be logged.
     */
    void writeMessage(QtMsgType type, const char *msg);

    //! an instance pointer to the class
    static Logger * instance;

    //! the stream we're writing to
    QTextStream stream;

    //! the stderr stream
    QTextStream err;

    //! an open file where the above stream writes
    QFile file;

    //! a list of log level strings
    QStringList log_levels;

    //! the old msg handler
    QtMsgHandler old_msg_handler;

    //! mutex for making operations thread safe
    QMutex mutex;

    //! logging level used (show messages of this level or above)
    QtMsgType logging_level;

    //! the current number of logged lines
    int logged_lines;

    //! interval for how often to check for rotations
    int rotation_check_interval;

    //! the number of rotated log files to save
    int log_file_count;

    //! the minimum size in bytes of each logfile
    int min_size;

    //! flag indicating whether logging to file is enabled
    bool log_to_file;

    //! flag indicating whether logging to console is enabled
    bool log_to_console;

    //! flag indicating whether there is whitelist for logging prefixes
    bool has_whitelist;

    //! flag indicating whether there is blacklist for logging prefixes
    bool has_blacklist;

    //! white or blaclisted logging message prefixes
    QStringList prefixes;
};

#endif // LOGGER_H
