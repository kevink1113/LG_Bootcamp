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
#include <QFontDatabase> // QFontDatabase 추가

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
    
    // 게임 오버 상태 표시를 위한 필드
    qint64 gameOverTime; // 게임 오버된 시간
    QString playerName; // 플레이어 이름
};

struct GameState {
    QList<QRect> obstacles;
    QList<QPointF> starPositions;
    int currentScore;
    qint64 timestamp;
};

struct GameFeedbackData {
    QString message;
    QPointF position;
    double startTime;
    double duration;
    QColor color;
    int fontSize;
    bool active;
    
    GameFeedbackData(const QString &msg, const QPointF &pos, const QColor &col = Qt::yellow, int size = 24)
        : message(msg), position(pos), startTime(0.0), duration(1.5), color(col), fontSize(size), active(true) {}
};

class GameWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit GameWindow(QWidget *parent = nullptr, bool isMultiplayer = false);
    ~GameWindow();
    
    // 플레이어 이름 설정 메서드 추가
    void setCurrentPlayer(const QString &playerName);

signals:
    void requestMainWindow();
    void gameOverSignal(); // 게임 오버 시그널 추가
    void restartRequested(); // 게임 재시작 시그널 추가

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
    void cleanupInactivePlayers();

private:
    void setupGame();
    void gameOver();
    bool checkCollision();
    void startMicProcess();
    void stopMicProcess();
    void setupBackButton();

    void playSound(const QString &soundFile);  // 사운드 재생 도우미 함수

    // 폰트 로딩 도우미 함수
    QFont loadSystemFont(const QString &fontName, int size, QFont::Weight weight = QFont::Normal);
    
    // 피드백 시스템 관련 함수들
    void addFeedback(const QString &message, const QPointF &position, const QColor &color = Qt::yellow, int fontSize = 24);
    void updateFeedbacks();
    void checkPitchAccuracy();
    void clearFeedbacks();
    
    // 멀티플레이어 관련 함수들
    void startMultiplayer();
    void stopMultiplayer();
    void updatePlayerPosition(int x, int y, int score, bool gameOver);
    void processIncomingData(const QByteArray &data, const QHostAddress &sender, quint16 port);
    void processPositionPacket(QDataStream &stream, const QHostAddress &sender, quint16 port);
    void processJsonData(const QByteArray &data, const QHostAddress &sender, quint16 port);
    void sendGameState();
    void processGameState(const QJsonObject &gameState);
    void startLobby();
    void leaveLobby();
    void checkGameStart();
    void calculateRankings(); // 순위 계산 함수 추가
    void showMultiplayerResults(); // 멀티플레이어 결과 표시 함수 추가
    void determineHost(); // 호스트 선정 함수 추가


    QTimer *gameTimer;
    QTimer *obstacleTimer;
    QTimer *pitchTimer;
    QProcess *micProcess;
    QProcess *soundProcess; // 사운드 효과를 위한 프로세스
    QFile *pitchFile;
    QPushButton *backButton;
    
    // 멀티플레이어 관련 멤버들
    QUdpSocket *udpSocket;
    QTimer *broadcastTimer;
    QTimer *cleanupTimer;
    QTimer *countdownTimer;
    QString playerId;
    QVector<PlayerData> otherPlayers;
    QVector<PlayerData> finishedPlayers;
    QVector<GameFeedbackData> feedbacks; // 피드백 메시지들
    
    // 멀티플레이어 상태
    bool isMultiplayerMode;
    bool isInLobby;
    bool isGameStarted;
    bool isHost;
    int countdownValue;
    GameState sharedGameState;
    qint64 lastGameStateUpdate;
    qint64 gameStartTime; // 게임 시작 시간 (동기화용)
    
    // 멀티플레이어 순위 관련 멤버들
    bool isGameFinished; // 전체 게임이 끝났는지 여부
    int myRank; // 내 순위
    int obstacleShowed; // 장애물 생성 카운터 (멀티플레이어 동기화용)
    
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
    double lastSoundTime; // 마지막 사운드 재생 시간
    
    // 피드백 시스템 관련 변수들
    int consecutivePerfect; // 연속 Perfect 횟수
    double lastFeedbackTime; // 마지막 피드백 시간
    
    // 게임 상수
    static const int PLAYER_SIZE = 30;  // 플레이어 크기
    static const int OBSTACLE_WIDTH = 60;  // 장애물 너비 (160에서 60으로 줄임)


    static const int OBSTACLE_GAP = 300;  // 장애물 사이 간격 (300으로 증가)


    QPixmap playerImage; // 플레이어 이미지
    GameOverDialog *gameOverDialog; // 게임 오버 다이얼로그 포인터

//    static const int OBSTACLE_GAP = 300;  // 장애물 사이 간격 (300으로 증가)

    static const int WINDOW_WIDTH = 800;  // 윈도우 너비
    static const int WINDOW_HEIGHT = 600;  // 윈도우 높이
    
    // 멀티플레이어 상수들
    static const quint16 BROADCAST_PORT = 12345;
    static const int BROADCAST_INTERVAL = 16; // 16ms (60fps로 증가)
    static const int CLEANUP_INTERVAL = 2000; // 2초
    static const int PLAYER_TIMEOUT = 3000; // 3초
    static const quint32 FIXED_SEED = 0xDEADBEEF; // 더 복잡한 고정된 랜덤 시드값

};

#endif // GAMEWINDOW_H
