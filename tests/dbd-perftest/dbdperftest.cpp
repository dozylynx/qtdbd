/**
 * Copyright (c) 2015 Assured Information Security, Inc. <pattersonc@ainfosec.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "dbdperftest.h"

DbdPerfTest::DbdPerfTest(QCoreApplication *app) : app(app), readInterval(10), writeInterval(1000), readIterations(100000), numberVms(25), printTimer(), elapsedTimer(), readerThread(NULL), reader(NULL), writerThread(NULL), writer(NULL)
{
}

DbdPerfTest::~DbdPerfTest()
{
}

void DbdPerfTest::exitCleanup()
{
    qDebug() << "exiting...";
    readerThread->quit();
    writerThread->quit();
    readerThread->wait();
    writerThread->wait();
    printSummary();
}

void DbdPerfTest::printSummary()
{
    reader->updateLock.lock();
    auto readSuccess = reader->readSuccessCount;
    auto readError = reader->readErrorCount;
    reader->updateLock.unlock();

    writer->updateLock.lock();
    auto writeSuccess = writer->writeSuccessCount;
    auto writeError = writer->writeErrorCount;
    writer->updateLock.unlock();

    auto elapsed = elapsedTimer.elapsed();

    qDebug() << "read success count:" << readSuccess;
    qDebug() << "read error count:" << readError;
    qDebug() << "read total count:" << readSuccess + readError;
    qDebug() << "write success count:" << writeSuccess;
    qDebug() << "write error count:" << writeError;
    qDebug() << "write total count:" << writeSuccess + writeError;
    qDebug() << "elapsed time:" << elapsed << "ms";
    qDebug() << "read rate:" << (readSuccess) / (elapsedTimer.elapsed() / 1000.0) << "per second";
}

void DbdPerfTest::parseCommandLine()
{
    QCommandLineParser parser;

    parser.setApplicationDescription("openxt simple db storage daemon");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption readIntervalOption(QStringList() << "r" << "read-interval",
                                          QCoreApplication::translate("main", "interval to set read timer to read"),
                                          QCoreApplication::translate("main", "milliseconds"));
    parser.addOption(readIntervalOption);

    QCommandLineOption writeIntervalOption(QStringList() << "w" << "write-interval",
                                           QCoreApplication::translate("main", "interval to set write timer to write"),
                                           QCoreApplication::translate("main", "milliseconds"));
    parser.addOption(writeIntervalOption);

    QCommandLineOption readIterationsOption(QStringList() << "i" << "read-iterations",
                                            QCoreApplication::translate("main", "number of read iterations to attempt"),
                                            QCoreApplication::translate("main", "iterations"));
    parser.addOption(readIterationsOption);

    QCommandLineOption numberVmsOption(QStringList() << "n" << "number-vms",
                                       QCoreApplication::translate("main", "number of vms to simulate"),
                                       QCoreApplication::translate("main", "vms"));
    parser.addOption(numberVmsOption);

    QCommandLineOption sessionBusOption(QStringList() << "x" << "use-session-bus",
                                        QCoreApplication::translate("main", "use session bus instead of system bus (useful for testing)"));
    parser.addOption(sessionBusOption);

    parser.process(*app);

    if (parser.isSet(readIntervalOption))
    {
        readInterval = parser.value(readIntervalOption).toDouble();
    }

    if (parser.isSet(writeIntervalOption))
    {
        writeInterval = parser.value(writeIntervalOption).toDouble();
    }

    if (parser.isSet(readIterationsOption))
    {
        readIterations = parser.value(readIterationsOption).toULongLong();
    }

    if (parser.isSet(numberVmsOption))
    {
        numberVms = parser.value(numberVmsOption).toULongLong();
    }

    qDebug() << "write interval:" << writeInterval;
    qDebug() << "read iterations:" << readIterations;
    qDebug() << "number vms:" << numberVms;
}

void DbdPerfTest::startTests()
{
    qDebug() << "starting tests...";

    QObject::connect(&printTimer, &QTimer::timeout, this, &DbdPerfTest::printSummary);
    printTimer.setInterval(1000);
    elapsedTimer.start();
    printTimer.start();

    readerThread = new QThread();
    writerThread = new QThread();

    reader = new DbdPerfTestReader();
    writer = new DbdPerfTestWriter();

    reader->readInterval = readInterval;
    reader->readIterations = readIterations;
    writer->writeInterval = writeInterval;

    reader->moveToThread(readerThread);
    writer->moveToThread(writerThread);

    connect(readerThread, &QThread::started, reader, &DbdPerfTestReader::setup);
    connect(writerThread, &QThread::started, writer, &DbdPerfTestWriter::setup);

    connect(readerThread, &QThread::finished, reader, &QObject::deleteLater);
    connect(writerThread, &QThread::finished, writer, &QObject::deleteLater);

    connect(reader, &DbdPerfTestReader::finished, reader, &QObject::deleteLater);
    connect(writer, &DbdPerfTestWriter::finished, writer, &QObject::deleteLater);

    connect(reader, &DbdPerfTestReader::finished, readerThread, &QThread::quit);
    connect(writer, &DbdPerfTestWriter::finished, writerThread, &QThread::quit);

    connect(app, &QCoreApplication::aboutToQuit, reader, &DbdPerfTestReader::stop);
    connect(app, &QCoreApplication::aboutToQuit, writer, &DbdPerfTestWriter::stop);

    readerThread->start();
    writerThread->start();

    qDebug() << "tests started...";
}

