#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDialog>
#include <QPushButton>
#include <QLineEdit>
#include <QTimer>
#include <QProcess>
#include "gamewindow.h"
#include "rankingdialog.h"
#include "playerdialog.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

protected:
    virtual void resizeEvent(QResizeEvent *event) override;

private slots:
    void on_menuButton1_clicked();
    void on_menuButton2_clicked();
    void on_menuButton3_clicked();
    void showRankingDialog();
    void showPlayerDialog();

private:
    Ui::MainWindow *ui;
    GameWindow *gameWindow;
    QPushButton *rankingButton;
    QPushButton *playerButton;
    RankingDialog *rankingDialog;
    PlayerDialog *playerDialog;
    QLabel *currentPlayerLabel;
    bool backgroundMusicEnabled;
    QProcess *backgroundMusicProcess;
    int volumeLevel;
    bool isCreatingGameWindow;
    QTimer *gameWindowCreationTimer;
    void updateButtonPositions();
    void updateCurrentPlayerDisplay();
    void createNewGameWindow();
    void cleanupGameWindow();
    void initAudio();
    void controlBackgroundMusicProcess(bool start);
};

#endif // MAINWINDOW_H
