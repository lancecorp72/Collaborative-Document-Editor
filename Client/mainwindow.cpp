#include <QtWidgets>
#include <QDebug>
#include "mainwindow.h"
#include <QTimer>
#include <QtNetwork>
#define SERVER_PORT 5000
#define MAXBUFSIZE 2000

MainWindow::MainWindow()
    : textEdit(new QPlainTextEdit)
{
    numResponsesLeft = 0;
    prevResponseNum = 0;
    saveCursorPos = 1;
    currCursorPos = 0;
    prevCursorPos = 0;

    setCentralWidget(textEdit);
    createActions();
    createStatusBar();
    readSettings();

    connect(textEdit->document(), &QTextDocument::contentsChanged,
            this, &MainWindow::documentWasModified);
    connect(textEdit, &QPlainTextEdit::textChanged,
            this, &MainWindow::update);
#ifndef QT_NO_SESSIONMANAGER
    QGuiApplication::setFallbackSessionManagementEnabled(false);
    connect(qApp, &QGuiApplication::commitDataRequest,
            this, &MainWindow::commitData);
#endif

    port = (QInputDialog::getText(0, "Input dialog","Port:", QLineEdit::Normal,"",nullptr)).toInt();
    timer = new QTimer(this);
    connect(timer,&QTimer::timeout,this,&MainWindow::send);
    timer->start(100);

    textEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    setCurrentFile(QString());
    setUnifiedTitleAndToolBarOnMac(true);
    sockfd = new QTcpSocket(this);
    if(!(sockfd->bind(port)))
    {
        qInfo()<<"Bind Failed";
        return;
    }

    connect(sockfd,&QTcpSocket::connected,this,&MainWindow::isConnected);
    connect(sockfd,QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),this,&MainWindow::connectError);
    sockfd->connectToHost(QHostAddress::LocalHost,SERVER_PORT);
    connect(sockfd,&QTcpSocket::readyRead,this,&MainWindow::recv);
}

void MainWindow::isConnected()
{
    qInfo()<<"Connected";
}
void MainWindow::connectError()
{
    qInfo()<<"Connection Error";
}

void MainWindow::doEvent(QString event)
{
    QString temp;
    int i;
    for(i = 0; event[i] != ' '; i++)
        temp.append(event[i]);

    int curPos = temp.toInt();
    QTextCursor cur = textEdit->textCursor();
    if(event[++i] == '1')
    {
        int numChar = (event.mid(i+1)).toInt();
        temp = docLastState.left(curPos-numChar) + docLastState.mid(curPos);
        textEdit->setPlainText(temp);
        cur.setPosition(curPos-1);
        textEdit->setTextCursor(cur);
    }
    else
    {
        event = event.mid(i+1);
        temp = docLastState.left(curPos) + event + docLastState.mid(curPos);
        textEdit->setPlainText(temp);
        cur.setPosition(curPos+event.size());
        textEdit->setTextCursor(cur);
    }
    prevCursorPos = currCursorPos = cur.position();
}

void MainWindow::recv()
{
    if(!started)
    {
        QString msg(sockfd->read(MAXBUFSIZE)),temp;
        int i;
        for(i = 0; msg[i] != ' '; i++)
            temp.append(msg[i]);
        prevResponseNum = temp.toInt();
        textEdit->setPlainText(msg.mid(i+1));
        started = true;
        return;
    }

    // recv and update document
    QString temp(sockfd->read(1000));
    if(temp[0]=='1')
        numResponsesLeft--;

    QString sn;
    int i;
    for(i = 2; temp[i] != ' '; i++)
        sn.append(temp[i]);
    int seqNum = sn.toInt();

    QString operation(temp.mid(i+1));
    seqMap[seqNum] = operation;
    if(numResponsesLeft>0)
        return;

    while(seqMap.find(seqNum) != seqMap.end())
    {
        doEvent(seqMap[seqNum]);
        seqMap.erase(seqNum++);
        prevResponseNum++;
    }

}

void MainWindow::send()
{
    saveCursorPos = 1;
    toSendText = (textEdit->toPlainText());
    toSendCursor = prevCursorPos;
    if(currCursorPos < prevCursorPos)
    {
        numResponsesLeft++;
        toSendText = QString::number(toSendCursor) + " 1" + QString::number(prevCursorPos-currCursorPos);
        prevCursorPos = currCursorPos;
        //send cursor and numchars to delete
        if(sockfd->write(toSendText.toUtf8()) < 0)
        {
            qInfo()<<"Send Error";
            exit(0);
        }
    }
    else if(currCursorPos > prevCursorPos)
    {
        numResponsesLeft++;
        toSendText = QString::number(toSendCursor) + " 2" + toSendText.mid(prevCursorPos,currCursorPos-prevCursorPos);
        prevCursorPos = currCursorPos;
        //send cursor and text to insert
        if(sockfd->write(toSendText.toUtf8()) < 0)
        {
            qInfo()<<"Send Error";
            exit(0);
        }
    }
    else
    {
        toSendText = "";
        return;
    }
}
int MainWindow::getCursorPosition()
{
   return textEdit->textCursor().position();
}
void MainWindow::update()
{
    if(saveCursorPos == 1)
    {
        saveCursorPos = 0;
        textEdit->undo();
        if(numResponsesLeft == 0)
            docLastState = textEdit->toPlainText();

        prevCursorPos = getCursorPosition();
        textEdit->redo();
    }
    currCursorPos = getCursorPosition();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (maybeSave()) {
        writeSettings();
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::newFile()
{
    if (maybeSave()) {
        textEdit->clear();
        setCurrentFile(QString());
    }
}

void MainWindow::open()
{
    if (maybeSave()) {
        QString fileName = QFileDialog::getOpenFileName(this);
        if (!fileName.isEmpty())
            loadFile(fileName);
    }
}

bool MainWindow::save()
{
    if (curFile.isEmpty()) {
        return saveAs();
    } else {
        return saveFile(curFile);
    }
}

bool MainWindow::saveAs()
{
    QFileDialog dialog(this);
    dialog.setWindowModality(Qt::WindowModal);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    if (dialog.exec() != QDialog::Accepted)
        return false;
    return saveFile(dialog.selectedFiles().first());
}

void MainWindow::about()
{
   QMessageBox::about(this, tr("About Application"),
            tr("The <b>Application</b> example demonstrates how to "
               "write modern GUI applications using Qt, with a menu bar, "
               "toolbars, and a status bar."));
}

void MainWindow::documentWasModified()
{
    setWindowModified(textEdit->document()->isModified());
    //cout<<textEdit->toPlainText();
}

void MainWindow::createActions()
{

    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    QToolBar *fileToolBar = addToolBar(tr("File"));
    const QIcon newIcon = QIcon::fromTheme("document-new");
    QAction *newAct = new QAction(newIcon, tr("&New"), this);
    newAct->setShortcuts(QKeySequence::New);
    newAct->setStatusTip(tr("Create a new file"));
    connect(newAct, &QAction::triggered, this, &MainWindow::newFile);
    fileMenu->addAction(newAct);
    fileToolBar->addAction(newAct);

    const QIcon openIcon = QIcon::fromTheme("document-open");
    QAction *openAct = new QAction(openIcon, tr("&Open..."), this);
    openAct->setShortcuts(QKeySequence::Open);
    openAct->setStatusTip(tr("Open an existing file"));
    connect(openAct, &QAction::triggered, this, &MainWindow::open);
    fileMenu->addAction(openAct);
    fileToolBar->addAction(openAct);

    const QIcon saveIcon = QIcon::fromTheme("document-save");
    QAction *saveAct = new QAction(saveIcon, tr("&Save"), this);
    saveAct->setShortcuts(QKeySequence::Save);
    saveAct->setStatusTip(tr("Save the document to disk"));
    connect(saveAct, &QAction::triggered, this, &MainWindow::save);
    fileMenu->addAction(saveAct);
    fileToolBar->addAction(saveAct);

    const QIcon saveAsIcon = QIcon::fromTheme("document-save-as");
    QAction *saveAsAct = fileMenu->addAction(saveAsIcon, tr("Save &As..."), this, &MainWindow::saveAs);
    saveAsAct->setShortcuts(QKeySequence::SaveAs);
    saveAsAct->setStatusTip(tr("Save the document under a new name"));


    fileMenu->addSeparator();

    const QIcon exitIcon = QIcon::fromTheme("application-exit");
    QAction *exitAct = fileMenu->addAction(exitIcon, tr("E&xit"), this, &QWidget::close);
    exitAct->setShortcuts(QKeySequence::Quit);
    exitAct->setStatusTip(tr("Exit the application"));

    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    QToolBar *editToolBar = addToolBar(tr("Edit"));
#ifndef QT_NO_CLIPBOARD
    const QIcon cutIcon = QIcon::fromTheme("edit-cut");
    QAction *cutAct = new QAction(cutIcon, tr("Cu&t"), this);
    cutAct->setShortcuts(QKeySequence::Cut);
    cutAct->setStatusTip(tr("Cut the current selection's contents to the "
                            "clipboard"));
    connect(cutAct, &QAction::triggered, textEdit, &QPlainTextEdit::cut);
    editMenu->addAction(cutAct);
    editToolBar->addAction(cutAct);

    const QIcon copyIcon = QIcon::fromTheme("edit-copy");
    QAction *copyAct = new QAction(copyIcon, tr("&Copy"), this);
    copyAct->setShortcuts(QKeySequence::Copy);
    copyAct->setStatusTip(tr("Copy the current selection's contents to the "
                             "clipboard"));
    connect(copyAct, &QAction::triggered, textEdit, &QPlainTextEdit::copy);
    editMenu->addAction(copyAct);
    editToolBar->addAction(copyAct);

    const QIcon pasteIcon = QIcon::fromTheme("edit-paste");
    QAction *pasteAct = new QAction(pasteIcon, tr("&Paste"), this);
    pasteAct->setShortcuts(QKeySequence::Paste);
    pasteAct->setStatusTip(tr("Paste the clipboard's contents into the current "
                              "selection"));
    connect(pasteAct, &QAction::triggered, textEdit, &QPlainTextEdit::paste);
    editMenu->addAction(pasteAct);
    editToolBar->addAction(pasteAct);

    menuBar()->addSeparator();

#endif // !QT_NO_CLIPBOARD

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction *aboutAct = helpMenu->addAction(tr("&About"), this, &MainWindow::about);
    aboutAct->setStatusTip(tr("Show the application's About box"));


    QAction *aboutQtAct = helpMenu->addAction(tr("About &Qt"), qApp, &QApplication::aboutQt);
    aboutQtAct->setStatusTip(tr("Show the Qt library's About box"));

#ifndef QT_NO_CLIPBOARD
    cutAct->setEnabled(false);
    copyAct->setEnabled(false);
    connect(textEdit, &QPlainTextEdit::copyAvailable, cutAct, &QAction::setEnabled);
    connect(textEdit, &QPlainTextEdit::copyAvailable, copyAct, &QAction::setEnabled);
#endif // !QT_NO_CLIPBOARD
}

void MainWindow::createStatusBar()
{
    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::readSettings()
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    const QByteArray geometry = settings.value("geometry", QByteArray()).toByteArray();
    if (geometry.isEmpty()) {
        const QRect availableGeometry = QApplication::desktop()->availableGeometry(this);
        resize(availableGeometry.width() / 3, availableGeometry.height() / 2);
        move((availableGeometry.width() - width()) / 2,
             (availableGeometry.height() - height()) / 2);
    } else {
        restoreGeometry(geometry);
    }
}

void MainWindow::writeSettings()
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.setValue("geometry", saveGeometry());
}

bool MainWindow::maybeSave()
{
    if (!textEdit->document()->isModified())
        return true;
    const QMessageBox::StandardButton ret
        = QMessageBox::warning(this, tr("Application"),
                               tr("The document has been modified.\n"
                                  "Do you want to save your changes?"),
                               QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    switch (ret) {
    case QMessageBox::Save:
        return save();
    case QMessageBox::Cancel:
        return false;
    default:
        break;
    }
    return true;
}

void MainWindow::loadFile(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        QMessageBox::warning(this, tr("Application"),
                             tr("Cannot read file %1:\n%2.")
                             .arg(QDir::toNativeSeparators(fileName), file.errorString()));
        return;
    }

    QTextStream in(&file);
#ifndef QT_NO_CURSOR
    QApplication::setOverrideCursor(Qt::WaitCursor);
#endif
    textEdit->setPlainText(in.readAll());
#ifndef QT_NO_CURSOR
    QApplication::restoreOverrideCursor();
#endif

    setCurrentFile(fileName);
    statusBar()->showMessage(tr("File loaded"), 2000);
}

bool MainWindow::saveFile(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        QMessageBox::warning(this, tr("Application"),
                             tr("Cannot write file %1:\n%2.")
                             .arg(QDir::toNativeSeparators(fileName),
                                  file.errorString()));
        return false;
    }

    QTextStream out(&file);
#ifndef QT_NO_CURSOR
    QApplication::setOverrideCursor(Qt::WaitCursor);
#endif
    out << textEdit->toPlainText();
#ifndef QT_NO_CURSOR
    QApplication::restoreOverrideCursor();
#endif

    setCurrentFile(fileName);
    statusBar()->showMessage(tr("File saved"), 2000);
    return true;
}

void MainWindow::setCurrentFile(const QString &fileName)
{
    curFile = fileName;
    textEdit->document()->setModified(false);
    setWindowModified(false);

    QString shownName = curFile;
    if (curFile.isEmpty())
        shownName = "untitled.txt";
    setWindowFilePath(shownName);
}

QString MainWindow::strippedName(const QString &fullFileName)
{
    return QFileInfo(fullFileName).fileName();
}
#ifndef QT_NO_SESSIONMANAGER
void MainWindow::commitData(QSessionManager &manager)
{
    if (manager.allowsInteraction()) {
        if (!maybeSave())
            manager.cancel();
    } else {
        // Non-interactive: save without asking
        if (textEdit->document()->isModified())
            save();
    }
}
#endif
