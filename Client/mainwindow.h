#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QAction;
class QMenu;
class QPlainTextEdit;
class QSessionManager;
class QTimer;
class QTcpSocket;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow();

    void loadFile(const QString &fileName);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void send();
    void recv();
    void newFile();
    void open();
    bool save();
    bool saveAs();
    void about();
    void documentWasModified();
    void update();
    void isConnected();
    void connectError();
#ifndef QT_NO_SESSIONMANAGER
    void commitData(QSessionManager &);
#endif

private:
    int getCursorPosition();
    void createActions();
    void createStatusBar();
    void readSettings();
    void writeSettings();
    bool maybeSave();
    bool saveFile(const QString &fileName);
    void setCurrentFile(const QString &fileName);
    QString strippedName(const QString &fullFileName);
    void doEvent(QString event);
    QPlainTextEdit *textEdit;
    QString curFile;
    QTimer *timer;
    int saveCursorPos;
    int prevCursorPos;
    int currCursorPos;
    QString toSendText;
    int toSendCursor;
    QTcpSocket *sockfd;
    int numResponsesLeft;
    int prevResponseNum;
    QString docLastState;
    std::map<int,QString> seqMap;
    bool started;
    int port;
};

#endif
