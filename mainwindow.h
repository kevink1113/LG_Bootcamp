#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDialog>
#include <QSlider>
#include <QPushButton>
#include <QLineEdit>
#include <QTimer>
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
    void on_settingsButton_clicked();
    void onVolumeChanged(int value);
    void showRankingDialog();  // 이름 변경
    void showPlayerDialog();   // 플레이어 설정 다이얼로그

private:
    Ui::MainWindow *ui;
    GameWindow *gameWindow;
    QDialog *settingsDialog;
    QSlider *volumeSlider;
    QPushButton *rankingButton;
    QPushButton *playerButton;  // 플레이어 설정 버튼 추가
    RankingDialog *rankingDialog;
    PlayerDialog *playerDialog;
    QLabel *currentPlayerLabel;  // 현재 플레이어 이름 표시 라벨
    
    // 배경 음악 관련
    bool backgroundMusicEnabled;
    QProcess *backgroundMusicProcess;
    int volumeLevel;
    
    // 게임 윈도우 생성 상태 관리
    bool isCreatingGameWindow;
    QTimer *gameWindowCreationTimer;
    
    void createSettingsDialog();
    void updateButtonPositions();
    void updateCurrentPlayerDisplay();  // 현재 플레이어 표시 업데이트
    void createNewGameWindow();  // 새 함수 추가
    void cleanupGameWindow();    // 게임 윈도우 정리 함수
    void initAudio();            // 오디오 초기화
    void controlBackgroundMusicProcess(bool start);  // 배경 음악 제어
};

#endif // MAINWINDOW_H
