#ifndef SONGGAME_H
#define SONGGAME_H

#include <QMainWindow>
#include <QTimer>
#include <QPainter>
#include <QKeyEvent>
#include <QProcess>
#include <QFile>
#include <QTextStream>
#include <QVector>
#include <QPointF>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QUdpSocket>
#include <QHostAddress>
#include <QPushButton>
#include <QMessageBox>
#include <QScreen>
#include <QApplication>
#include <QDebug>
#include <cmath>
#include <QSet>

struct NoteData {
    QString lyric;      // 가사
    double beat;        // 박자
    int octave;         // 옥타브
    QString note;       // 음 (C, D, E, F, G, A, B)
    double startTime;   // 시작 시간 (초)
    double endTime;     // 끝 시간 (초)
};

struct ObstacleData {
    QRect rect;
    QString lyric;
    QString note;
    int octave;
};

class SongGame : public QMainWindow
{
    Q_OBJECT

public:
    explicit SongGame(QWidget *parent = nullptr);
    ~SongGame();
    
    void setCurrentPlayer(const QString &playerName);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private slots:
    void updateGame();
    void readPitchData();
    void goBackToMainWindow();
    void setupBackButton();

private:
    // 게임 상수
    static const int PLAYER_SIZE = 30;
    static const int OBSTACLE_WIDTH = 120; // 두께 증가
    static const int OBSTACLE_GAP = 150;
    static const int WINDOW_WIDTH = 1024;
    static const int WINDOW_HEIGHT = 600;
    static const int PLAYER_SPEED = 10;
    static const int OBSTACLE_SPEED = 10; // 속도 증가
    static const int INITIAL_SCORE = 100;
    static const int PENALTY_PER_HIT = 5;

    // 게임 상태
    QRect player;
    QVector<ObstacleData> obstacles;
    QSet<int> collidedObstacles; // 충돌한 장애물 추적
    QVector<NoteData> songNotes;
    QTimer *gameTimer;
    QTimer *pitchTimer;
    QTimer *countdownTimer;
    QProcess *micProcess;
    QProcess *soundProcess;
    QPushButton *backButton;
    
    bool gameRunning;
    bool moveUp;
    bool moveDown;
    int score;
    int currentNoteIndex;
    double gameTime;
    double targetY;
    int currentPitch;
    float currentVolume;
    QString currentPlayerName;
    double lastSoundTime; // 마지막 사운드 재생 시간
    
    // 노래 데이터
    void setupGame();
    void loadSongData();
    void createObstacleFromNote(const NoteData &note);
    void gameOver();
    void startMicProcess();
    void stopMicProcess();
    void playSound(const QString &soundFile);
    
    // 음정을 Y 좌표로 변환
    int noteToYPosition(const QString &note, int octave);
    
    // CSV 파싱
    QVector<NoteData> parseCSV(const QString &filename);
};

#endif // SONGGAME_H 