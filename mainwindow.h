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
#include "songgame.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void setTitleLabelY(int y); // Melody Game 제목 라벨 Y좌표 설정

protected:
    virtual void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override; // 추가된 부분

private slots:
    void on_menuButton1_clicked();
    void on_menuButton2_clicked();
    void on_menuButton3_clicked();
    void onVolumeChanged(int value);
    void showRankingDialog();  // 이름 변경
    void showPlayerDialog();   // 플레이어 설정 다이얼로그

private:
    Ui::MainWindow *ui;
    GameWindow *gameWindow;
    SongGame *songGame;
    QDialog *settingsDialog;
    QSlider *volumeSlider;
    QPushButton *rankingButton;
    QPushButton *playerButton;  // 플레이어 설정 버튼 추가
    QPushButton *menuButton1;   // 새로운 메뉴 버튼 1
    QPushButton *menuButton2;   // 새로운 메뉴 버튼 2
    QPushButton *menuButton3;   // 새로운 메뉴 버튼 3
    RankingDialog *rankingDialog;
    PlayerDialog *playerDialog;
    QLabel *currentPlayerLabel;  // 현재 플레이어 이름 표시 라벨
    
    // 배경 음악 관련
    bool backgroundMusicEnabled;
    QProcess *backgroundMusicProcess;
    int volumeLevel;
    
    // 게임 윈도우 생성 상태 관리
    bool isCreatingGameWindow;
    bool isCreatingSongGame;
    QTimer *gameWindowCreationTimer;
    
    // 중복 클릭 방지를 위한 추가 변수들
    QTimer *buttonCooldownTimer;
    bool isButtonCooldownActive;
    
    void createSettingsDialog();
    void updateButtonPositions();
    void updateCurrentPlayerDisplay();  // 현재 플레이어 표시 업데이트
    void createNewGameWindow();  // 새 함수 추가
    void cleanupGameWindow();    // 게임 윈도우 정리 함수

    
    // 오디오 관련 메서드
    void initAudio();  // 오디오 초기화
    void controlBackgroundMusicProcess(bool start); // 리눅스 명령어로 배경 음악 제어

    QPixmap backgroundPixmap; // 메인윈도우 배경 이미지

    int titleLabelY; // Melody Game 제목 라벨 Y좌표
    QLabel *titleLabel;


};

#endif // MAINWINDOW_H
