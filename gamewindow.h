#ifndef GAMEWINDOW_H
#define GAMEWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QKeyEvent>
#include <QPainter>
#include <QList>
#include <QRect>
#include <QFont>
#include <QPainterPath>
#include <QProcess>
#include <QFile>
#include <QTextStream>
#include <QScreen>
#include <QStyle>
#include <QApplication>
#include "gameoverdialog.h"
#include <QPushButton>

class GameWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit GameWindow(QWidget *parent = nullptr);
    ~GameWindow();
    
    // 플레이어 이름 설정 메서드 추가
    void setCurrentPlayer(const QString &playerName);

signals:
    void requestMainWindow();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void updateGame();
    void spawnObstacles();
    void readPitchData();
    void goBackToMainWindow();

private:
    void setupGame();
    void gameOver();
    bool checkCollision();
    void startMicProcess();
    void stopMicProcess();
    void setupBackButton();
    void playSound(const QString &soundFile);  // 사운드 재생 도우미 함수

    QTimer *gameTimer;
    QTimer *obstacleTimer;
    QTimer *pitchTimer;
    QProcess *micProcess;
    QProcess *soundProcess; // 사운드 효과를 위한 프로세스
    QFile *pitchFile;
    QPushButton *backButton;
    
    QRect player;
    QVector<QRect> obstacles;  // QList 대신 QVector 사용
    struct Star {
        QPointF pos;
        bool active;
        Star(QPointF p) : pos(p), active(true) {}
    };
    QVector<Star> stars;  // QList 대신 QVector 사용 (연속 메모리 구조로 성능 향상)
    int starSize = 60;     // 별 크기
    QPainterPath starPath; // 캐시된 별 모양
    
    // 상수 정의
    static constexpr int STAR_POINTS = 5;    // 별의 꼭지점 수
    static constexpr float OUTER_RADIUS = 1.0f;  // 외부 반지름 비율
    static constexpr float INNER_RADIUS = 0.38f;  // 내부 반지름 비율 (더 뾰족하게)
    static constexpr float CORNER_SMOOTHNESS = 0.0f; // 모서리 둥글기 제거
    
    int playerSpeed;
    int score;
    bool gameRunning;
    bool moveUp;
    bool moveDown;
    
    // 마이크 입력 관련 변수
    int currentPitch;
    float currentVolume;
    int targetY;
    
    // 플레이어 정보
    QString currentPlayerName;  // 현재 플레이어 이름 저장
    
    // 게임 요소 크기
    static const int PLAYER_SIZE = 30;  // 플레이어 크기
    static const int OBSTACLE_WIDTH = 40;  // 장애물 너비
    static const int OBSTACLE_GAP = 200;  // 장애물 사이 간격

    QPixmap playerImage; // 플레이어 이미지
};

#endif // GAMEWINDOW_H