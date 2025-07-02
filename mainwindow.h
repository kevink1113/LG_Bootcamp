#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDialog>
#include <QSlider>
#include <QPushButton>
#include "gamewindow.h"
#include "rankingdialog.h"

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
    void on_settingsButton_clicked();
    void onVolumeChanged(int value);
    void showRankingDialog();  // 이름 변경

private:
    Ui::MainWindow *ui;
    GameWindow *gameWindow;
    QDialog *settingsDialog;
    QSlider *volumeSlider;
    QPushButton *rankingButton;
    RankingDialog *rankingDialog;
    
    // 게임 윈도우 생성 상태 관리
    bool isCreatingGameWindow;
    QTimer *gameWindowCreationTimer;
    
    void createSettingsDialog();
    void updateButtonPositions();
    void createNewGameWindow();  // 새 함수 추가
    void cleanupGameWindow();    // 게임 윈도우 정리 함수
};

#endif // MAINWINDOW_H
