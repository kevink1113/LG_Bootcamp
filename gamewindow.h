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

// 멀티플레이어 관련 헤더들
#include <QUdpSocket>
#include <QHostAddress>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>

struct PlayerData {
    QString playerId;
    int x;
    int y;
    int score;
    bool gameOver;
    bool isReady;
    QHostAddress address;
    quint16 port;
    qint64 lastSeen;
};

struct GameState {
    QList<QRect> obstacles;
    QList<QPointF> starPositions;
    int currentScore;
    qint64 timestamp;
};

class GameWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit GameWindow(QWidget *parent = nullptr, bool isMultiplayer = false);
    ~GameWindow();

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
    
    // 멀티플레이어 관련 슬롯들
    void readPendingDatagrams();
    void broadcastPlayerData();
    void cleanupInactivePlayers();
    void startGameCountdown();

private:
    void setupGame();
    void gameOver();
    bool checkCollision();
    void startMicProcess();
    void stopMicProcess();
    void setupBackButton();
    
    // 멀티플레이어 관련 함수들
    void startMultiplayer();
    void stopMultiplayer();
    void updatePlayerPosition(int x, int y, int score, bool gameOver);
    void sendPlayerData();
    void processIncomingData(const QByteArray &data, const QHostAddress &sender, quint16 port);
    void sendGameState();
    void processGameState(const QJsonObject &gameState);
    void startLobby();
    void leaveLobby();
    void checkGameStart();

    QTimer *gameTimer;
    QTimer *obstacleTimer;
    QTimer *pitchTimer;
    QProcess *micProcess;
    QFile *pitchFile;
    QPushButton *backButton;
    
    // 멀티플레이어 관련 멤버들
    QUdpSocket *udpSocket;
    QTimer *broadcastTimer;
    QTimer *cleanupTimer;
    QTimer *countdownTimer;
    QString playerId;
    QList<PlayerData> otherPlayers;
    bool isMultiplayerMode;
    bool isInLobby;
    bool isGameStarted;
    bool isHost;
    int countdownValue;
    GameState sharedGameState;
    qint64 lastGameStateUpdate;
    
    QRect player;
    QList<QRect> obstacles;
    struct Star {
        QPointF pos;
        bool active;
        Star(QPointF p) : pos(p), active(true) {}
    };
    QList<Star> stars;  // 별 위치와 상태 목록
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
    
    // 게임 요소 크기
    static const int PLAYER_SIZE = 30;  // 플레이어 크기
    static const int OBSTACLE_WIDTH = 40;  // 장애물 너비
    static const int OBSTACLE_GAP = 300;  // 장애물 사이 간격 (300으로 증가)
    static const int WINDOW_WIDTH = 800;  // 윈도우 너비
    static const int WINDOW_HEIGHT = 600;  // 윈도우 높이
    
    // 멀티플레이어 상수들
    static const quint16 BROADCAST_PORT = 12345;
    static const int BROADCAST_INTERVAL = 100; // 100ms
    static const int CLEANUP_INTERVAL = 2000; // 2초
    static const int PLAYER_TIMEOUT = 3000; // 3초
    static const quint32 FIXED_SEED = 0xDEADBEEF; // 더 복잡한 고정된 랜덤 시드값
};

#endif // GAMEWINDOW_H