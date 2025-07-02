#ifndef GAMEWINDOW_H
#define GAMEWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QKeyEvent>
#include <QPainter>
#include <QList>
#include <QRect>
#include <QFont>
#include <QProcess>
#include <QFile>
#include <QTextStream>
#include <QScreen>
#include <QStyle>
#include <QApplication>

class GameWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit GameWindow(QWidget *parent = nullptr);
    ~GameWindow();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void updateGame();
    void spawnObstacles();
    void readPitchData();

private:
    void setupGame();
    void gameOver();
    bool checkCollision();
    void startMicProcess();
    void stopMicProcess();

    QTimer *gameTimer;
    QTimer *obstacleTimer;
    QTimer *pitchTimer;
    QProcess *micProcess;
    QFile *pitchFile;
    
    QRect player;
    QList<QRect> obstacles;
    QList<QPointF> stars;  // 별 위치 목록
    int starSize = 20;     // 별의 크기
    
    // 상수 정의
    static constexpr int STAR_POINTS = 5;  // 별의 꼭지점 수
    static constexpr float STAR_INNER_RATIO = 0.4f;  // 별의 내부 반지름 비율
    
    int playerSpeed;
    int score;
    bool gameRunning;
    bool moveUp;
    bool moveDown;
    
    // 마이크 입력 관련 변수
    int currentPitch;
    float currentVolume;
    int targetY;
    
    // 게임 요소 크기
    static const int PLAYER_SIZE = 30;  // 플레이어 크기 약간 증가
    static const int OBSTACLE_WIDTH = 40;  // 장애물 너비 약간 증가
    static const int OBSTACLE_GAP = 200;  // 장애물 사이 간격 증가
};

#endif // GAMEWINDOW_H