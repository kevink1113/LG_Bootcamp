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
#include <QColor>

struct NoteData {
    QString lyric;      // 가사
    double beat;        // 박자
    int octave;         // 옥타브
    QString note;       // 음 (C, D, E, F, G, A, B)
    double startTime;   // 시작 시간 (초)
    double endTime;     // 끝 시간 (초)
};

struct SongInfo {
    QString name;       // 노래 이름
    QString filename;   // CSV 파일명
    QString description; // 노래 설명
};

struct ObstacleData {
    QRect rect;
    QString lyric;
    QString note;
    int octave;
};

struct FeedbackData {
    QString message;
    QPointF position;
    double startTime;
    double duration;
    QColor color;
    int fontSize;
    bool active;
    
    FeedbackData(const QString &msg, const QPointF &pos, const QColor &col = Qt::yellow, int size = 24)
        : message(msg), position(pos), startTime(0.0), duration(1.5), color(col), fontSize(size), active(true) {}
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
    static const int OBSTACLE_WIDTH = 80; // 두께 증가
    static const int OBSTACLE_GAP = 120; // 갭을 더 크게 조정 (기존 80에서 120으로)
    static const int WINDOW_WIDTH = 1024;
    static const int WINDOW_HEIGHT = 600;
    static const int PLAYER_SPEED = 10;
    static const int OBSTACLE_SPEED = 10; // 속도를 더 빠르게 조정 (기존 3에서 8로)
    static const int INITIAL_SCORE = 100;
    static const int PENALTY_PER_HIT = 10; // 감점을 10점으로 조정
    static const int PERFECT_BONUS = 5; // Perfect 시 보너스 점수

    // 게임 상태
    QRect player;
    QVector<ObstacleData> obstacles;
    QSet<int> collidedObstacles; // 충돌한 장애물 추적
    QVector<NoteData> songNotes;
    QVector<SongInfo> availableSongs; // 사용 가능한 노래 목록
    int selectedSongIndex; // 선택된 노래 인덱스
    QVector<FeedbackData> feedbacks; // 피드백 메시지들
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
    
    // 피드백 시스템 관련 변수들
    int consecutivePerfect; // 연속 Perfect 횟수
    double lastFeedbackTime; // 마지막 피드백 시간
    
    // 노래 데이터
    void setupGame();
    void loadSongData();
    void createObstacleFromNote(const NoteData &note);
    void gameOver();
    void startMicProcess();
    void stopMicProcess();
    void playSound(const QString &soundFile);
    
    // 노래 선택 관련 함수들
    void initializeSongs();
    void showSongSelectionDialog();
    void selectSong(int index);
    
    // 피드백 시스템 관련 함수들
    void addFeedback(const QString &message, const QPointF &position, const QColor &color = Qt::yellow, int fontSize = 24);
    void updateFeedbacks();
    void checkPitchAccuracy();
    void clearFeedbacks();
    
    // 폰트 로딩을 위한 도우미 함수
    QFont loadSystemFont(const QString &fontName, int size, QFont::Weight weight = QFont::Normal);
    
    // 음정을 Y 좌표로 변환
    int noteToYPosition(const QString &note, int octave);
    
    // CSV 파싱
    QVector<NoteData> parseCSV(const QString &filename);
};

#endif // SONGGAME_H 