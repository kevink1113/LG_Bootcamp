#include "gamewindow.h"
#include <QMessageBox>
#include <QPainter>
#include <QRandomGenerator>
#include <QApplication>
#include <QDebug>
#include <QScreen>
#include <QStyle>
#include <QFile>
#include <QTextStream>
#include <cmath>

#include <QVector>  // QVector 추가
#include <QDir>     // QDir 추가

#include <QDateTime>
#include <QUdpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHostAddress>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QFrame>
#include <algorithm>
#include <QDataStream>
#include <QThread>
#include <QFontDatabase> // QFontDatabase 추가


GameWindow::GameWindow(QWidget *parent, bool isMultiplayer)
    : QMainWindow(parent)
    , gameTimer(nullptr)
    , obstacleTimer(nullptr)
    , pitchTimer(nullptr)
    , micProcess(nullptr)
    , soundProcess(nullptr)  // 사운드 프로세스 초기화
    , pitchFile(nullptr)
    , backButton(nullptr)
    , udpSocket(nullptr)
    , broadcastTimer(nullptr)
    , cleanupTimer(nullptr)
    , countdownTimer(nullptr)
    , playerId(QString::number(QDateTime::currentMSecsSinceEpoch()))
    , isMultiplayerMode(isMultiplayer)
    , isInLobby(false)
    , isGameStarted(false)
    , isHost(false)
    , countdownValue(0)
    , lastGameStateUpdate(0)
    , isGameFinished(false) // 게임 종료 상태 초기화
    , myRank(0) // 내 순위 초기화
    , playerSpeed(5)
    , score(0)
    , gameRunning(false)
    , moveUp(false)
    , moveDown(false)
    , currentPitch(0)
    , currentVolume(0.0f)
    , targetY(300)  // 기본값으로 설정
    , gameOverDialog(nullptr)

    , lastSoundTime(0.0) // 마지막 사운드 재생 시간 초기화
    , consecutivePerfect(0) // 연속 Perfect 횟수 초기화
    , lastFeedbackTime(0.0) // 마지막 피드백 시간 초기화
{
    qDebug() << "GameWindow constructor called" << (isMultiplayer ? "(Multiplayer)" : "(Single Player)");
    
    // 중복 생성 방지를 위한 정적 플래그
    static bool isInitializing = false;
    if (isInitializing) {
        qDebug() << "GameWindow initialization already in progress, skipping...";
        return;
    }
    isInitializing = true;
    
    // 초기화 과정에서 창이 보이지 않도록 숨김
    hide();
    
    // 생성자에서 바로 초기화하지 않고 이벤트 루프가 시작된 후 초기화
    QTimer::singleShot(100, this, [this]() {
        setupGame();
        isInitializing = false;
    });
}


GameWindow::~GameWindow()
{
    qDebug() << "GameWindow destructor called";
    
    // 게임 상태 정지
    gameRunning = false;
    

    // 모든 시그널 연결 해제
    disconnect();
    
    // 타이머들 먼저 정지 및 정리 (nullptr 체크 추가)

    if (gameTimer) {
        qDebug() << "Deleting gameTimer...";
        gameTimer->stop();
        gameTimer->disconnect();

        delete gameTimer;

        gameTimer = nullptr;
        qDebug() << "gameTimer deleted.";
    }
    if (obstacleTimer) {
        qDebug() << "Deleting obstacleTimer...";
        obstacleTimer->stop();
        obstacleTimer->disconnect();

        delete obstacleTimer;

        obstacleTimer = nullptr;
        qDebug() << "obstacleTimer deleted.";
    }
    if (pitchTimer) {
        qDebug() << "Deleting pitchTimer...";
        pitchTimer->stop();
        pitchTimer->disconnect();

        delete pitchTimer;

        pitchTimer = nullptr;
        qDebug() << "pitchTimer deleted.";
    }
    if (countdownTimer) {
        qDebug() << "Deleting countdownTimer...";
        countdownTimer->stop();
        countdownTimer->disconnect();

        delete countdownTimer;

        countdownTimer = nullptr;
        qDebug() << "countdownTimer deleted.";
    }

    if (broadcastTimer) {
        qDebug() << "Deleting broadcastTimer...";
        broadcastTimer->stop();
        broadcastTimer->disconnect();
        broadcastTimer->deleteLater();
        broadcastTimer = nullptr;
        qDebug() << "broadcastTimer deleted.";
    }
    if (cleanupTimer) {
        qDebug() << "Deleting cleanupTimer...";
        cleanupTimer->stop();
        cleanupTimer->disconnect();
        cleanupTimer->deleteLater();
        cleanupTimer = nullptr;
        qDebug() << "cleanupTimer deleted.";
    }
    if (broadcastTimer) {
        broadcastTimer->stop();
        broadcastTimer->disconnect();
        delete broadcastTimer;
        broadcastTimer = nullptr;
    }
    if (cleanupTimer) {
        cleanupTimer->stop();
        cleanupTimer->disconnect();
        delete cleanupTimer;
        cleanupTimer = nullptr;
    }
    
    // 멀티플레이어 정리
    qDebug() << "Calling stopMultiplayer()...";
    stopMultiplayer();
    qDebug() << "stopMultiplayer() finished.";
    
    // 마이크 프로세스 정리
    qDebug() << "Calling stopMicProcess()...";
    stopMicProcess();
    qDebug() << "stopMicProcess() finished.";
    
    // 사운드 프로세스 정리
    if (soundProcess) {
        qDebug() << "Deleting soundProcess...";
        soundProcess->terminate();
        soundProcess->waitForFinished(1000);
        soundProcess->deleteLater();
        soundProcess = nullptr;
        qDebug() << "soundProcess deleted.";
    }
    
    // 사운드 프로세스 정리 (nullptr 체크 추가)
    if (soundProcess) {
        soundProcess->terminate();
        soundProcess->waitForFinished(1000);
        delete soundProcess;
        soundProcess = nullptr;
    }
    
    // 버튼 정리 (nullptr 체크 추가)
    if (backButton) {

        backButton->disconnect();
        delete backButton;

        backButton = nullptr;
        qDebug() << "backButton deleted.";
    }
    
    // 이벤트 루프 처리
    QApplication::processEvents();
    
    qDebug() << "GameWindow destructor completed";
}


void GameWindow::setupGame()
{
    qDebug() << "Setting up game window...";
    
    // 중복 실행 방지
    static bool isSetupInProgress = false;
    if (isSetupInProgress) {
        qDebug() << "Game setup already in progress, skipping...";
        return;
    }
    isSetupInProgress = true;

    QScreen *screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setGeometry(screenGeometry);
    setWindowState(Qt::WindowFullScreen);
    show();
    raise();
    activateWindow();
    player = QRect(50, height()/2 - PLAYER_SIZE/2, PLAYER_SIZE, PLAYER_SIZE);
    targetY = height()/2 - PLAYER_SIZE/2;
    
    // 타이머 생성 및 연결
    if (!gameTimer) {
        gameTimer = new QTimer(this);
        if (gameTimer) {
            connect(gameTimer, &QTimer::timeout, this, &GameWindow::updateGame);
            gameTimer->start(16); // 약 60 FPS로 변경 (33에서 16으로)
        }
    }

    gameTimer->start(8); // 약 120 FPS (더 부드러운 움직임)

    
    if (!obstacleTimer) {
        obstacleTimer = new QTimer(this);
        if (obstacleTimer) {
            connect(obstacleTimer, &QTimer::timeout, this, &GameWindow::spawnObstacles);
            obstacleTimer->start(1800); // 1.8초마다 장애물 생성 (1.5초에서 변경)
        }
    }
    
    if (!pitchTimer) {
        pitchTimer = new QTimer(this);
        if (pitchTimer) {
            connect(pitchTimer, &QTimer::timeout, this, &GameWindow::readPitchData);
            pitchTimer->start(50); // 20Hz로 피치 읽기 (100에서 50으로 변경)
        }
    }
    
    gameRunning = true;
    score = 0;
    obstacles.clear();
    stars.clear();
    
    // 피드백 시스템 초기화
    consecutivePerfect = 0;
    lastFeedbackTime = 0.0;
    clearFeedbacks();
    
    try {
        // 1. 전체화면/geometry/flags를 show() 전에 설정
        QScreen *screen = QApplication::primaryScreen();
        if (!screen) {
            qDebug() << "No primary screen found!";
            return;
        }
        
        QRect screenGeometry = screen->geometry();
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
        setGeometry(screenGeometry);
        setWindowState(Qt::WindowFullScreen);
        
        // 2. show()는 마지막에 호출
        show();
        QCoreApplication::processEvents(); // 즉시 화면 갱신
        
        // 3. raise/activateWindow는 show() 이후
        raise();
        activateWindow();
        
        qDebug() << "GameWindow shown. Size:" << size();
        
        // 플레이어 위치 초기화
        player = QRect(50, height()/2 - PLAYER_SIZE/2, PLAYER_SIZE, PLAYER_SIZE);
        targetY = height()/2 - PLAYER_SIZE/2;
        
        // 타이머 생성 및 연결
        if (!gameTimer) {
            gameTimer = new QTimer(this);
            if (gameTimer) {
                connect(gameTimer, &QTimer::timeout, this, &GameWindow::updateGame);

                gameTimer->start(8); // 약 120 FPS (더 부드러운 움직임)

            }
        }
        
        if (!obstacleTimer) {
            obstacleTimer = new QTimer(this);
            if (obstacleTimer) {
                connect(obstacleTimer, &QTimer::timeout, this, &GameWindow::spawnObstacles);
                obstacleTimer->start(1800); // 1.8초마다 장애물 생성 (1.5초에서 변경)
            }
        }
        
        if (!pitchTimer) {
            pitchTimer = new QTimer(this);
            if (pitchTimer) {
                connect(pitchTimer, &QTimer::timeout, this, &GameWindow::readPitchData);
                pitchTimer->start(50); // 20Hz로 피치 읽기 (100에서 50으로 변경)
            }
        }
        
        // 게임 상태 초기화
        gameRunning = true;
        score = 0;
        obstacles.clear();
        stars.clear();
        
        // 별 모양 초기화 - 둥근 모서리와 부드러운 곡선 (캐싱)
        if (starPath.isEmpty()) {
            starPath = QPainterPath();
            const qreal angleStep = M_PI / STAR_POINTS;
            const qreal controlDist = CORNER_SMOOTHNESS;
            for (int i = 0; i < STAR_POINTS * 2; ++i) {
                qreal radius = (i % 2 == 0) ? starSize * OUTER_RADIUS / 2 : starSize * INNER_RADIUS / 2;
                qreal angle = i * angleStep;
                qreal nextAngle = (i + 1) * angleStep;
                QPointF point(radius * sin(angle), -radius * cos(angle));
                qreal nextRadius = ((i + 1) % 2 == 0) ? starSize * OUTER_RADIUS / 2 : starSize * INNER_RADIUS / 2;
                QPointF nextPoint(nextRadius * sin(nextAngle), -nextRadius * cos(nextAngle));
                if (i == 0) starPath.moveTo(point);
                QPointF ctrl1 = point + QPointF(radius * controlDist * cos(angle), radius * controlDist * sin(angle));
                QPointF ctrl2 = nextPoint - QPointF(nextRadius * controlDist * cos(nextAngle), nextRadius * controlDist * sin(nextAngle));
                starPath.cubicTo(ctrl1, ctrl2, nextPoint);
            }
            starPath.closeSubpath();
        }
        
        // 마이크 프로세스 시작
        startMicProcess();
        
        // 뒤로가기 버튼 설정
        if (!backButton) {
            setupBackButton();
        }
        
        // player2.png 미리 스케일링 (성능 최적화)
        static QPixmap cachedPlayerPixmap;
        static bool playerImageLoaded = false;
        const int PLAYER_DISPLAY_SIZE = PLAYER_SIZE * 3; // 기존보다 3배 크게
        if (!playerImageLoaded) {
            QPixmap rawPixmap;
            if (rawPixmap.load("/mnt/nfs/player2.png")) {
                cachedPlayerPixmap = rawPixmap.scaled(PLAYER_DISPLAY_SIZE, PLAYER_DISPLAY_SIZE, Qt::KeepAspectRatio, Qt::FastTransformation);
                qDebug() << "Player image loaded and cached from /mnt/nfs/player2.png (3x size)";
            } else {
                qDebug() << "Failed to load player image from /mnt/nfs/player2.png";
            }
            playerImageLoaded = true;
        }
        playerImage = cachedPlayerPixmap;
        
        // 멀티플레이어 모드인 경우 네트워크 초기화
        if (isMultiplayerMode) {
            startMultiplayer();
            startLobby();
        }
        
        // 초기 화면 그리기
        update();
        
    } catch (const std::exception& e) {
        qDebug() << "Exception in setupGame:" << e.what();
    } catch (...) {
        qDebug() << "Unknown exception in setupGame";
    }
    
    // 설정 완료 후 플래그 리셋
    isSetupInProgress = false;
}

void GameWindow::startMicProcess()
{
    if (micProcess) {
        stopMicProcess();
    }
    
    micProcess = new QProcess(this);
    QString workingDir = QApplication::applicationDirPath();
    micProcess->setWorkingDirectory(workingDir);
    qDebug() << "Starting mic process in directory:" << workingDir;
    micProcess->start("./mic", QStringList(), QIODevice::ReadWrite);
    
    // mic 프로세스 시작 실패 시 기본값 생성
    if (!micProcess->waitForStarted(1000)) {
        qDebug() << "Creating default pitch_score file...";
        QFile defaultFile("/tmp/pitch_score");
        if (defaultFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&defaultFile);
            stream << "15 500.0\n";  // 중간 높이와 적절한 볼륨으로 설정
            defaultFile.close();
        }
    }
    
    if (micProcess->waitForStarted()) {
        qDebug() << "Mic process started successfully";
    } else {
        qDebug() << "Mic not available, game will run with default values";
        // 마이크가 없어도 게임이 실행되도록 프로세스 유지
        QFile file("/tmp/pitch_score");
        if (!file.exists()) {
            QFile defaultFile("/tmp/pitch_score");
            if (defaultFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream stream(&defaultFile);
                stream << "15 500.0\n";  // 중간 높이와 적절한 볼륨으로 설정
                defaultFile.close();
            }
        }
    }
}

void GameWindow::stopMicProcess()
{
    qDebug() << "[stopMicProcess] called";
    if (micProcess) {
        qDebug() << "[stopMicProcess] Deleting micProcess...";
        micProcess->terminate();
        if (!micProcess->waitForFinished(3000)) {
            micProcess->kill();
        }
        delete micProcess;
        micProcess = nullptr;
        qDebug() << "[stopMicProcess] micProcess deleted.";
    }
    qDebug() << "[stopMicProcess] finished";
}

void GameWindow::readPitchData()
{
    if (!gameRunning) return;
    
    // 파일에서 피치 데이터 읽기
    static QFile pitchFile("/tmp/pitch_score"); // 정적 파일 객체 사용
    
    if (pitchFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&pitchFile);
        QString line = in.readLine();
        pitchFile.close();
        
        if (!line.isEmpty()) {
            QStringList parts = line.split(' ');
            if (parts.size() >= 2) {
                bool ok1, ok2;
                int pitch = parts[0].toInt(&ok1);
                float volume = parts[1].toFloat(&ok2);
                
                if (ok1 && ok2) {
                    currentPitch = pitch;
                    currentVolume = volume;
                    
                    // 볼륨이 일정 이상일 때만 계산
                    if (currentPitch > 0 && currentVolume > 0.1f) {
                        // 정적 변수로 캐싱
                        static const int pitchRange = 37 - 1; // 1~37 범위
                        static const int screenHeight = height();
                        const float normalizedPitch = (currentPitch - 1.0f) / pitchRange;
                        targetY = (1.0f - normalizedPitch) * (screenHeight - PLAYER_SIZE);
                        targetY = qBound(0, targetY, screenHeight - PLAYER_SIZE);
                    }
                }
            }
        }
    }
}

void GameWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false); // 안티앨리어싱 비활성화로 성능 향상
    
    // 배경 그리기 (이미지 최적화 예시)
    static QPixmap bgPixmap;
    static bool bgLoaded = false;
    if (!bgLoaded) {
        bgPixmap.load("/mnt/nfs/background.png");
        if (!bgPixmap.isNull()) {
            bgPixmap = bgPixmap.scaled(size(), Qt::IgnoreAspectRatio, Qt::FastTransformation);
        }
        bgLoaded = true;
    }
    if (!bgPixmap.isNull()) {
        painter.drawPixmap(rect(), bgPixmap);
    } else {
        painter.fillRect(rect(), Qt::black);
    }
    
    // 별 그리기 - 활성 별만 그리기
    painter.setBrush(QColor(255, 223, 0));  // 밝은 노란색
    painter.setPen(Qt::NoPen);
    
    // 눈과 미소 미리 생성 (정적 객체로 캐싱)
    static QPainterPath smilePath;
    static bool smilePathCreated = false;
    if (!smilePathCreated) {
        const qreal smileWidth = starSize/5;
        const qreal smileHeight = starSize/8;
        smilePath.moveTo(-smileWidth, 0);
        smilePath.quadTo(0, smileHeight, smileWidth, 0);
        smilePathCreated = true;
    }
    
    for (const Star& star : stars) {
        if (!star.active) continue;
        
        painter.save();
        painter.translate(star.pos);
        
        // 별 모양 그리기
        painter.drawPath(starPath);
        
        // 얼굴 그리기
        painter.setPen(QPen(Qt::black, 2));
        
        // 눈
        painter.setBrush(Qt::black);
        painter.drawEllipse(QPointF(-starSize/8, -starSize/8), 2.5, 2.5);
        painter.drawEllipse(QPointF(starSize/8, -starSize/8), 2.5, 2.5);
        
        // 미소
        painter.drawPath(smilePath);
        
        painter.restore();
    }
    
    // 장애물 그리기 (상단/하단 이미지로 대체)
    static QPixmap obstacleTopPixmap, obstacleBottomPixmap;
    static bool obstaclesLoaded = false;
    if (!obstaclesLoaded) {
        obstacleTopPixmap.load("/mnt/nfs/obstacle_top.png");
        obstacleBottomPixmap.load("/mnt/nfs/obstacle_bottom.png");
        obstaclesLoaded = true;
    }
    
    for (int i = 0; i < obstacles.size(); ++i) {
        const QRect &obstacle = obstacles[i];
        // 상단 장애물: y==0, 하단 장애물: y>0
        if (obstacle.y() == 0 && !obstacleTopPixmap.isNull()) {
            painter.drawPixmap(obstacle, obstacleTopPixmap);
        } else if (obstacle.y() > 0 && !obstacleBottomPixmap.isNull()) {
            painter.drawPixmap(obstacle, obstacleBottomPixmap);
        } else {
            painter.setBrush(Qt::red);
            painter.setPen(Qt::NoPen);
            painter.drawRect(obstacle);
        }
    }
    
    // 플레이어 그리기 (이미지)
    if (!playerImage.isNull()) {
        int px = player.x() + (player.width() - playerImage.width())/2;
        int py = player.y() + (player.height() - playerImage.height())/2;
        painter.drawPixmap(px, py, playerImage.width(), playerImage.height(), playerImage);
    } else {
        painter.setBrush(Qt::white);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(player);
    }
    
    // 텍스트 정보 표시 - 캐싱 및 최적화
    static QFont infoFont = loadSystemFont("CookieRun Regular", 12);  // CookieRun Regular 폰트 사용
    painter.setFont(infoFont);

    // 멀티플레이어 모드에서 다른 플레이어들 그리기
    if (isMultiplayerMode) {
        for (const PlayerData &otherPlayer : otherPlayers) {
            // 게임 오버된 플레이어는 빨간색, 활성 플레이어는 파란색
            if (otherPlayer.gameOver) {
                painter.setBrush(Qt::red);
                painter.setPen(Qt::red);
            } else {
                painter.setBrush(Qt::blue);
                painter.setPen(Qt::blue);
            }
            
            // 다른 플레이어를 원으로 그리기
            painter.drawEllipse(otherPlayer.x, otherPlayer.y, PLAYER_SIZE, PLAYER_SIZE);
            
            // 플레이어 정보 표시
            painter.setPen(Qt::white);
            QString playerInfo = otherPlayer.playerName.isEmpty() ? 
                otherPlayer.playerId : otherPlayer.playerName;
            
            if (otherPlayer.gameOver) {
                playerInfo += " (GAME OVER)";
                painter.setPen(Qt::red);
            }
            
            painter.drawText(otherPlayer.x, otherPlayer.y - 5, playerInfo);
            
            // 게임 오버된 플레이어의 점수도 표시
            if (otherPlayer.gameOver) {
                QString scoreText = QString("Score: %1").arg(otherPlayer.score);
                painter.drawText(otherPlayer.x, otherPlayer.y + PLAYER_SIZE + 15, scoreText);
            }
        }
        
        // 대기실 화면 그리기
        if (isInLobby && !isGameStarted) {
            painter.setPen(Qt::white);


            static QFont lobbyFont = loadSystemFont("CookieRun Bold", 24, QFont::Bold);

            painter.setFont(lobbyFont);
            
            QString lobbyText = "Waiting for players...";
            QFontMetrics fm(lobbyFont);
            int textX = (width() - fm.horizontalAdvance(lobbyText)) / 2;
            painter.drawText(textX, height() / 2 - 50, lobbyText);
            
            QString playerCountText = QString("Players: %1/4").arg(otherPlayers.size() + 1);
            int countX = (width() - fm.horizontalAdvance(playerCountText)) / 2;
            painter.drawText(countX, height() / 2, playerCountText);
            
            // 호스트 표시
            if (isHost) {
                QString hostText = "You are the host";
                int hostX = (width() - fm.horizontalAdvance(hostText)) / 2;
                painter.drawText(hostX, height() / 2 + 50, hostText);
            }
        }
        
        // 게임 시작 카운트다운 그리기
        if (countdownValue > 0) {
            painter.setPen(Qt::yellow);

            static QFont countdownFont = loadSystemFont("CookieRun Bold", 48, QFont::Bold);

            painter.setFont(countdownFont);
            
            QString countdownText = QString::number(countdownValue);
            QFontMetrics fm(countdownFont);
            int textX = (width() - fm.horizontalAdvance(countdownText)) / 2;
            int textY = (height() + fm.height()) / 2;
            painter.drawText(textX, textY, countdownText);
        }
        
        // 멀티플레이어 모드임을 표시
        painter.setPen(Qt::white);
        painter.drawText(10, 25, QString("Multiplayer Mode - Players: %1").arg(otherPlayers.size() + 1));
        
        // 게임 중일 때 현재 순위 표시
        if (isGameStarted && gameRunning) {
            calculateRankings(); // 실시간 순위 계산
            QString rankText = QString("Your Rank: %1/%2").arg(myRank).arg(otherPlayers.size() + 1);
            painter.drawText(10, 45, rankText);
        }
    }
    
    // 점수와 플레이어 정보 표시 (오른쪽 상단)
    painter.setPen(Qt::white);
    static QFont scoreFont("Arial", 12, QFont::Bold);
    painter.setFont(scoreFont);
    
    // 텍스트 위치 계산
    QFontMetrics fm(painter.font());
    const int rightMargin = 10;
    const int topMargin = 25;
    const int lineSpacing = 20;
    
    // 필요한 문자열만 생성
    QString scoreText = QString("Score: %1").arg(score);
    QString playerText = QString("Player: %1").arg(currentPlayerName.isEmpty() ? "No Player" : currentPlayerName);
    QString pitchText = QString("Pitch: %1").arg(currentPitch);
    QString volumeText = QString("Volume: %1").arg(QString::number(currentVolume, 'f', 2));
    
    // 오른쪽 정렬 텍스트
    int rightEdge = width() - rightMargin;
    painter.drawText(rightEdge - fm.horizontalAdvance(scoreText), topMargin, scoreText);

    painter.drawText(rightEdge - fm.horizontalAdvance(playerText), topMargin + lineSpacing * 3, playerText);
    
    // 점수 표시에 고정폰트 폰트 사용
    static QFont scoreFont = loadSystemFont("CookieRun Bold", 14);
    painter.setFont(scoreFont);
    painter.drawText(rightEdge - 100, topMargin + lineSpacing * 4, QString("SCORE: %1").arg(score));
    
    // 다시 기본 폰트로 복원
    painter.setFont(infoFont);
    
    // 피드백 메시지들 그리기
    for (const GameFeedbackData &feedback : feedbacks) {
        if (!feedback.active) continue;
        
        // 시간에 따른 투명도 계산
        double currentTime = QDateTime::currentMSecsSinceEpoch() / 1000.0;
        double elapsed = currentTime - feedback.startTime;
        double alpha = 1.0 - (elapsed / feedback.duration);
        alpha = qBound(0.0, alpha, 1.0);
        
        // 색상에 투명도 적용
        QColor textColor = feedback.color;
        textColor.setAlphaF(alpha);
        painter.setPen(textColor);
        
        // 폰트 설정
        QFont feedbackFont = loadSystemFont("CookieRun Bold", feedback.fontSize, QFont::Bold);
        painter.setFont(feedbackFont);
        
        // 텍스트 그림자 효과
        painter.setPen(QPen(Qt::black, 3));
        painter.drawText(feedback.position + QPointF(2, 2), feedback.message);
        
        // 메인 텍스트
        painter.setPen(textColor);
        painter.drawText(feedback.position, feedback.message);
    }
    
    // 연속 Perfect 카운터 표시
    if (consecutivePerfect > 0) {
        painter.setPen(Qt::white);
        QFont counterFont = loadSystemFont("CookieRun Bold", 14, QFont::Bold);
        painter.setFont(counterFont);
        
        QString counterText = QString("Perfect x%1").arg(consecutivePerfect);
        painter.setPen(QColor(255, 255, 0)); // 노란색
        
        painter.drawText(10, 60, counterText);
    }
    
    // 디버그 정보는 조건부로 표시 (성능에 영향 줄이기)
#ifdef QT_DEBUG
    QString pitchText = QString("Pitch: %1").arg(currentPitch);
    QString volumeText = QString("Volume: %1").arg(QString::number(currentVolume, 'f', 2));

    painter.drawText(rightEdge - fm.horizontalAdvance(pitchText), topMargin + lineSpacing, pitchText);
    painter.drawText(rightEdge - fm.horizontalAdvance(volumeText), topMargin + lineSpacing * 2, volumeText);
    painter.drawText(rightEdge - fm.horizontalAdvance(playerText), topMargin + lineSpacing * 3, playerText);
}

void GameWindow::updateGame()
{
    if (!gameRunning) return;
    
    // 멀티플레이어 모드에서만 게임 시작 상태 체크
    if (isMultiplayerMode && !isGameStarted) return;
    
    // 마이크 입력에 따른 플레이어 이동
    if (currentVolume > 0.1f) { // 볼륨이 일정 이상일 때만
        int currentY = player.y();
        if (currentY < targetY) {
            player.translate(0, qMin(playerSpeed, targetY - currentY));
        } else if (currentY > targetY) {
            player.translate(0, -qMin(playerSpeed, currentY - targetY));
        }
    }
    
    // 키보드 입력도 여전히 지원 (디버깅용)
    if (moveUp && player.y() > 0) {
        player.translate(0, -playerSpeed);
    }
    if (moveDown && player.y() < height() - PLAYER_SIZE) {
        player.translate(0, playerSpeed);
    }
    
    // 장애물 이동 및 제거 - 성능 최적화
    const int leftBoundary = 0;
    const int obstacleSpeed = 3;
    
    for (int i = obstacles.size() - 1; i >= 0; --i) {
        QRect &obstacle = obstacles[i];
        obstacle.translate(-obstacleSpeed, 0); // 장애물이 왼쪽으로 이동
        
        // 화면 밖으로 나간 장애물 제거
        if (obstacle.x() + obstacle.width() < leftBoundary) {
            obstacles.removeAt(i);
            score++;
        }
    }
    
    // 별 이동 및 충돌 검사 최적화
    const QRectF playerBounds(player.x() - 15, player.y() - 15, player.width() + 30, player.height() + 30);
    const int halfStarSize = starSize / 2;
    const int starSpeed = 3; // 장애물과 동일한 속도
    
    for (int i = stars.size() - 1; i >= 0; --i) {
        Star &star = stars[i];
        if (!star.active) continue;
        
        // 화면 밖으로 나간 별은 즉시 비활성화
        if (star.pos.x() + halfStarSize < leftBoundary) {
            star.active = false;
            continue;
        }
        
        star.pos.setX(star.pos.x() - starSpeed);
        
        // 충돌 검사 최적화: 대략적인 거리 체크 먼저 (빠른 거부)
        const qreal dx = qAbs(star.pos.x() - player.x());
        const qreal dy = qAbs(star.pos.y() - player.y());
        if (dx > starSize || dy > starSize) continue;  // 충돌 불가능
        
        // 정확한 충돌 검사
        QRectF starRect(star.pos.x() - halfStarSize, star.pos.y() - halfStarSize, starSize, starSize);
        if (playerBounds.intersects(starRect)) {
            star.active = false;
            score += 3;  // 별 획득 시 3점 추가
            
            // 별 획득 사운드 재생 - QProcess 재사용 패턴
            playSound("/mnt/nfs/wav/item.wav");
        }
    }
    
    // 충돌 검사
    if (checkCollision()) {
        gameOver();
        return;
    }
    

    // 비활성 별 정리 (성능 최적화: 10초마다만 처리)
    static int cleanupCounter = 0;
    cleanupCounter++;
    if (cleanupCounter >= 600) { // 60FPS 기준 10초마다
        cleanupCounter = 0;
        const int MAX_STARS = 25;  // 최대 별 개수
        if (stars.size() > MAX_STARS) {
            for (int i = stars.size() - 1; i >= 0; --i) {
                if (!stars[i].active) {
                    stars.removeAt(i);
                }
            }
        }
    }
    
    // 멀티플레이어 모드에서 네트워크 업데이트 (성능 최적화: 3초마다)
    if (isMultiplayerMode) {
        static int frameCount = 0;

    // 피드백 시스템 업데이트
    updateFeedbacks();
    checkPitchAccuracy();
    
    // 멀티플레이어 모드에서 네트워크 업데이트 - 매 프레임마다 직접 전송
    if (isMultiplayerMode) {
        // 매 프레임마다 위치 전송 (실시간)
        updatePlayerPosition(player.x(), player.y(), score, false);
        
        // 호스트가 주기적으로 게임 상태 전송 (1초마다)
        static int frameCount = 0;
        if (isHost && isGameStarted && frameCount % 120 == 0) { // 120프레임마다 (약 1초)
            sendGameState();
        }

        frameCount++;
        
        // 3초마다만 업데이트 (60FPS 기준 180프레임)
        if (frameCount % 180 == 0) {
            updatePlayerPosition(player.x(), player.y(), score, false);
            
            // 호스트가 주기적으로 게임 상태 전송
            if (isHost && isGameStarted) {
                sendGameState();
            }
        }
    }
    
    // 점수에 따른 장애물 생성 빈도 조절 (최적화: 20점마다 체크)
    static int lastDifficultyCheck = 0;
    if (score >= lastDifficultyCheck + 20) {
        lastDifficultyCheck = score;
        int difficultyLevel = score / 50; // 50점마다 난이도 증가
        int spawnInterval = qMax(1200, 2500 - difficultyLevel * 150); // 2500ms -> 1200ms까지 감소
        
        if (obstacleTimer && obstacleTimer->isActive()) {
            obstacleTimer->setInterval(spawnInterval);
        }
    }
    
    // 화면 갱신
    update();
}

void GameWindow::calculateRankings()
{
    if (!isMultiplayerMode) return;
    
    // 모든 플레이어 정보를 수집 (자신 포함)
    QVector<PlayerData> allPlayers;
    
    // 자신의 정보 추가
    PlayerData myData;
    myData.playerId = playerId;
    myData.playerName = currentPlayerName;
    myData.score = score;
    myData.gameOver = !gameRunning;
    myData.gameOverTime = !gameRunning ? QDateTime::currentMSecsSinceEpoch() : 0;
    allPlayers.append(myData);
    
    // 다른 플레이어들 추가
    allPlayers.append(otherPlayers);
    
    // 점수와 게임 오버 시간으로 정렬 (높은 점수 우선, 같은 점수면 먼저 끝난 사람이 높은 순위)
    std::sort(allPlayers.begin(), allPlayers.end(), [](const PlayerData &a, const PlayerData &b) {
        if (a.score != b.score) {
            return a.score > b.score; // 높은 점수 우선
        }
        // 같은 점수면 게임 오버 시간이 빠른 사람이 높은 순위
        if (a.gameOver && b.gameOver) {
            return a.gameOverTime < b.gameOverTime;
        }
        // 아직 게임 중인 사람이 더 높은 순위
        return !a.gameOver && b.gameOver;
    });
    
    // 내 순위 찾기
    for (int i = 0; i < allPlayers.size(); ++i) {
        if (allPlayers[i].playerId == playerId) {
            myRank = i + 1;
            break;
        }
    }
    
    // 게임이 끝난 플레이어들 저장
    finishedPlayers.clear();
    for (const PlayerData &player : allPlayers) {
        if (player.gameOver) {
            finishedPlayers.append(player);
        }
    }
    
    qDebug() << "Rankings calculated. My rank:" << myRank << "out of" << allPlayers.size();
}

void GameWindow::showMultiplayerResults()
{
    if (!isMultiplayerMode) return;
    
    // 순위 계산
    calculateRankings();
    
    // 결과 다이얼로그 생성
    QDialog *resultDialog = new QDialog(this);
    resultDialog->setWindowTitle("Multiplayer Results");
    resultDialog->setFixedSize(400, 300);
    resultDialog->setModal(true);
    
    QVBoxLayout *layout = new QVBoxLayout(resultDialog);
    
    // 제목
    QLabel *titleLabel = new QLabel("Game Results", resultDialog);
    titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = loadSystemFont("CookieRun Bold", 16, QFont::Bold);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);
    
    // 내 순위 표시
    QString rankText = QString("Your Rank: %1/%2").arg(myRank).arg(finishedPlayers.size() + 1);
    QLabel *rankLabel = new QLabel(rankText, resultDialog);
    rankLabel->setAlignment(Qt::AlignCenter);
    QFont rankFont = loadSystemFont("CookieRun Bold", 14, QFont::Bold);
    rankLabel->setFont(rankFont);
    rankLabel->setStyleSheet("color: #FFD700;"); // 금색
    layout->addWidget(rankLabel);
    
    // 내 점수
    QString scoreText = QString("Your Score: %1").arg(score);
    QLabel *scoreLabel = new QLabel(scoreText, resultDialog);
    scoreLabel->setAlignment(Qt::AlignCenter);
    scoreLabel->setFont(loadSystemFont("CookieRun Regular", 12));
    layout->addWidget(scoreLabel);
    
    // 구분선
    QFrame *line = new QFrame(resultDialog);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    layout->addWidget(line);
    
    // 전체 순위표
    QLabel *rankingTitle = new QLabel("Final Rankings:", resultDialog);
    rankingTitle->setFont(loadSystemFont("CookieRun Bold", 12, QFont::Bold));
    layout->addWidget(rankingTitle);
    
    QTextEdit *rankingText = new QTextEdit(resultDialog);
    rankingText->setReadOnly(true);
    rankingText->setMaximumHeight(150);
    
    QString rankingString;
    int rank = 1;
    
    // 모든 플레이어 정보를 수집
    QVector<PlayerData> allPlayers;
    PlayerData myData;
    myData.playerId = playerId;
    myData.playerName = currentPlayerName;
    myData.score = score;
    myData.gameOver = !gameRunning;
    allPlayers.append(myData);
    allPlayers.append(otherPlayers);
    
    // 정렬
    std::sort(allPlayers.begin(), allPlayers.end(), [](const PlayerData &a, const PlayerData &b) {
        if (a.score != b.score) {
            return a.score > b.score;
        }
        if (a.gameOver && b.gameOver) {
            return a.gameOverTime < b.gameOverTime;
        }
        return !a.gameOver && b.gameOver;
    });
    
    for (const PlayerData &player : allPlayers) {
        QString playerName = player.playerName.isEmpty() ? player.playerId : player.playerName;
        if (player.playerId == playerId) {
            playerName += " (You)";
        }
        
        rankingString += QString("%1. %2 - Score: %3\n")
                        .arg(rank)
                        .arg(playerName)
                        .arg(player.score);
        rank++;
    }
    
    rankingText->setPlainText(rankingString);
    layout->addWidget(rankingText);
    
    // 버튼들
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    
    QPushButton *mainMenuButton = new QPushButton("Main Menu", resultDialog);
    QPushButton *restartButton = new QPushButton("Play Again", resultDialog);
    
    // 버튼 폰트 설정
    mainMenuButton->setFont(loadSystemFont("CookieRun Bold", 10, QFont::Bold));
    restartButton->setFont(loadSystemFont("CookieRun Bold", 10, QFont::Bold));
    
    buttonLayout->addWidget(mainMenuButton);
    buttonLayout->addWidget(restartButton);
    layout->addLayout(buttonLayout);
    
    // 버튼 연결
    connect(mainMenuButton, &QPushButton::clicked, this, [this, resultDialog]() {
        resultDialog->accept();
        emit requestMainWindow();
        close();
    });
    
    connect(restartButton, &QPushButton::clicked, this, [this, resultDialog]() {
        resultDialog->accept();
        // 게임 재시작
        gameRunning = true;
        score = 0;
        obstacles.clear();
        stars.clear();
        isGameFinished = false;
        myRank = 0;
        finishedPlayers.clear();
        
        // 플레이어 위치 초기화
        player.moveTop(height()/2 - PLAYER_SIZE/2);
        
        // 타이머 재시작
        if (gameTimer) gameTimer->start();
        if (obstacleTimer) obstacleTimer->start();
        if (pitchTimer) pitchTimer->start();
        
        // 마이크 프로세스 재시작
        startMicProcess();
        
        update();
    });
    
    resultDialog->exec();
    resultDialog->deleteLater();
}

void GameWindow::spawnObstacles()
{
    if (!gameRunning) return;
    
    // 멀티플레이어 모드에서만 게임 시작 상태와 호스트 조건 체크
    if (isMultiplayerMode) {
        if (!isGameStarted) return;
        if (!isHost) return;
    }
    
    static QRandomGenerator fixedGenerator(FIXED_SEED);
    int obstacleCount = obstacles.size() / 2;
    fixedGenerator.seed(QDateTime::currentMSecsSinceEpoch() + obstacleCount);

    // 점수에 따른 난이도 조절
    int difficultyLevel = score / 30; // 30점마다 난이도 증가 (50에서 30으로 변경)
    int minGap = qMax(40, 120 - difficultyLevel * 20); // 최소 간격 더 작게 (60에서 40으로, 150에서 120으로)
    int maxGap = qMax(60, 140 - difficultyLevel * 25); // 최대 간격 더 작게 (100에서 60으로, 180에서 140으로)
    
    // 범위가 유효한지 확인
    if (minGap >= maxGap) {
        minGap = 50;
        maxGap = 80;
    }
    
    // 장애물 사이의 통과 공간을 랜덤하게 설정
    int randomGap = fixedGenerator.bounded(minGap, maxGap + 1);
    
    // gapY 계산 (장애물 사이 통과 위치)
    int minGapY = randomGap/2 + PLAYER_SIZE + 30;
    int maxGapY = height() - randomGap/2 - PLAYER_SIZE - 30;
    if (minGapY >= maxGapY) {
        minGapY = 60;
        maxGapY = height() - 60;
    }
    int gapY = fixedGenerator.bounded(minGapY, maxGapY);

    // 위쪽 장애물
    QRect topObstacle(width(), 0, OBSTACLE_WIDTH, gapY - randomGap/2);
    obstacles.append(topObstacle);
    // 아래쪽 장애물
    QRect bottomObstacle(width(), gapY + randomGap/2, OBSTACLE_WIDTH, height() - (gapY + randomGap/2));
    obstacles.append(bottomObstacle);

    // 별 생성 확률은 그대로
    if (fixedGenerator.bounded(100) < 20) {
        int starX = width() + OBSTACLE_WIDTH/2;
        int starY = gapY;
        stars.append(Star(QPointF(starX, starY)));
    }
    
    // 게임 상태 전송은 updateGame에서 주기적으로 처리
}

bool GameWindow::checkCollision()
{

    for (int i = 0; i < obstacles.size(); i += 2) {
        // 상단 장애물 (아래쪽 부분만 충돌 감지 줄임)
        QRect topObstacle = obstacles[i];
        // 장애물 높이에 비례해서 여유 계산 (40x81 기준 7픽셀)
        int topMargin = qRound(topObstacle.height() * 7.0 / 81.0);
        QRect topCollisionRect = topObstacle.adjusted(0, 0, 0, -topMargin);
        
        // 하단 장애물 (위쪽 부분만 충돌 감지 줄임)
        QRect bottomObstacle = obstacles[i + 1];
        int bottomMargin = qRound(bottomObstacle.height() * 7.0 / 81.0);
        QRect bottomCollisionRect = bottomObstacle.adjusted(0, bottomMargin, 0, 0);
        
        if (player.intersects(topCollisionRect) || player.intersects(bottomCollisionRect)) {

    for (const QRect &obstacle : obstacles) {
        if (player.intersects(obstacle)) {
            // 연속 Perfect 카운터 리셋
            consecutivePerfect = 0;
            
            // 기존 피드백 메시지 모두 제거
            clearFeedbacks();
            
            // 충돌 피드백 표시
            addFeedback("MISS!", QPointF(player.x() + 50, player.y() - 30), QColor(255, 0, 0), 20);
            

            // 충돌 소리 재생
            playSound("/mnt/nfs/wav/scratch.wav");
            return true;
        }
    }
    return false;
}

void GameWindow::gameOver()
{
    gameRunning = false;
    
    // 게임 오버 시그널 발생 (배경 음악 중지를 위해)
    emit gameOverSignal();
    
    // 멀티플레이어 모드에서 게임 오버 상태 전송
    if (isMultiplayerMode) {
        updatePlayerPosition(player.x(), player.y(), score, true);
    }
    
    stopMicProcess();
    
    // 피드백 시스템 정리
    clearFeedbacks();
    
    if (gameTimer) {
        gameTimer->stop();
    }
    if (obstacleTimer) {
        obstacleTimer->stop();
    }
    if (pitchTimer) {
        pitchTimer->stop();
    }
    

    gameOverDialog = new GameOverDialog(score, currentPlayerName, this);
    
    connect(gameOverDialog, &GameOverDialog::mainMenuRequested, this, [this]() {
        // 메인 윈도우로 돌아가라는 시그널 발생
        emit requestMainWindow();
        // 게임 윈도우 닫기
        close();
    });
    
    connect(gameOverDialog, &GameOverDialog::rankingRequested, this, []() {
        // Ranking 기능은 나중에 구현
    });
    
    connect(gameOverDialog, &GameOverDialog::restartRequested, this, [this]() {
        // 게임 재시작 시그널 발생 (배경 음악 재시작을 위해)
        emit restartRequested();
        
        // 게임 오버 다이얼로그 정리
        if (gameOverDialog) {
            gameOverDialog->disconnect();
            gameOverDialog->close();
            gameOverDialog = nullptr;
        }
        
        // 게임 재시작
        gameRunning = true;
        score = 0;
        obstacles.clear();
        stars.clear();

    // 멀티플레이어 모드에서는 멀티플레이어 결과 표시
    if (isMultiplayerMode) {
        // 잠시 대기하여 다른 플레이어들의 게임 오버 상태를 받을 시간을 줌
        QTimer::singleShot(2000, this, &GameWindow::showMultiplayerResults);
    } else {
        // 싱글플레이어 모드에서는 기존 GameOverDialog 사용
        GameOverDialog *dialog = new GameOverDialog(score, currentPlayerName, this);

        
        connect(dialog, &GameOverDialog::mainMenuRequested, this, [this]() {
            // 메인 윈도우로 돌아가라는 시그널 발생
            emit requestMainWindow();
            // 게임 윈도우 닫기
            close();
        });
        
        connect(dialog, &GameOverDialog::rankingRequested, this, []() {
            // Ranking 기능은 나중에 구현
        });
        
        connect(dialog, &GameOverDialog::restartRequested, this, [this]() {
            // 게임 재시작
            gameRunning = true;
            score = 0;
            obstacles.clear();
            stars.clear();
            
            // 플레이어 위치 초기화
            player.moveTop(height()/2 - PLAYER_SIZE/2);
            
            // 타이머 재시작
            if (gameTimer) gameTimer->start();
            if (obstacleTimer) obstacleTimer->start();
            if (pitchTimer) pitchTimer->start();
            
            // 마이크 프로세스 재시작
            startMicProcess();
            
            update();
        });
        

        update();
    });
    
    // 비모달로 표시 (show() 사용, exec() 대신)
    gameOverDialog->show();
    gameOverDialog->raise();
    gameOverDialog->activateWindow();
    
    // 다이얼로그가 닫힐 때 자동으로 삭제되도록 설정
    gameOverDialog->setAttribute(Qt::WA_DeleteOnClose, true);
        // 비모달로 표시 (show() 사용, exec() 대신)
        dialog->show();
        dialog->raise();
        dialog->activateWindow();
        
        // 다이얼로그가 닫힐 때 자동으로 삭제되도록 설정
        dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    }

}

void GameWindow::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Up:
    case Qt::Key_W:
        moveUp = true;
        break;
    case Qt::Key_Down:
    case Qt::Key_S:
        moveDown = true;
        break;
    case Qt::Key_Escape:
        close();
        break;
    }
}

void GameWindow::keyReleaseEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Up:
    case Qt::Key_W:
        moveUp = false;
        break;
    case Qt::Key_Down:
    case Qt::Key_S:
        moveDown = false;
        break;
    }
}

void GameWindow::setupBackButton()
{
    backButton = new QPushButton(this);
    backButton->setFixedSize(50, 50);
    backButton->move(10, 10);
    
    // 스타일 설정 (배경 제거)
    QString buttonStyle = 
        "QPushButton {"
        "   background-color: transparent;"
        "   border: none;"
        "   padding: 5px;"
        "}"
        "QPushButton:hover {"
        "   background-color: rgba(255, 255, 255, 50);"
        "}"
        "QPushButton:pressed {"
        "   background-color: rgba(255, 255, 255, 100);"
        "}";
    backButton->setStyleSheet(buttonStyle);
    
    // 뒤로가기 아이콘 설정
    QStyle *style = QApplication::style();
    QIcon backIcon = style->standardIcon(QStyle::SP_ArrowBack);
    backButton->setIcon(backIcon);
    backButton->setIconSize(QSize(40, 40));
    
    connect(backButton, &QPushButton::clicked, this, &GameWindow::goBackToMainWindow);
    backButton->show();
    backButton->raise();
}

// 사운드 재생을 위한 도우미 함수
void GameWindow::playSound(const QString &soundFile)
{
    // 이전 사운드 프로세스 정리
    if (soundProcess) {
        if (soundProcess->state() == QProcess::Running) {
            soundProcess->terminate();
            soundProcess->waitForFinished(100);
        }
        delete soundProcess;
    }
    
    // 새 프로세스 시작
    soundProcess = new QProcess(this);
    soundProcess->start("./aplay", QStringList() << "-Dhw:0,0" << soundFile);
    
    // 시작 실패 시 절대 경로로 재시도
    if (!soundProcess->waitForStarted(300)) {
        qDebug() << "Failed to play sound. Trying absolute path...";
        delete soundProcess;
        
        soundProcess = new QProcess(this);
        soundProcess->start("/usr/bin/aplay", QStringList() << "-Dhw:0,0" << soundFile);
        
        if (!soundProcess->waitForStarted(300)) {
            qDebug() << "Failed to play sound with absolute path too.";
        }
    }
}

// 폰트 로딩을 위한 도우미 함수
QFont GameWindow::loadSystemFont(const QString &fontName, int size, QFont::Weight weight)
{
    QFont font;
    
    // 시스템 폰트 디렉토리에서 폰트 로드 시도
    QString fontPath = QString("/usr/lib/fonts/%1.ttf").arg(fontName);
    int fontId = QFontDatabase::addApplicationFont(fontPath);
    
    if (fontId != -1) {
        // 폰트 로드 성공
        QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);
        if (!fontFamilies.isEmpty()) {
            font = QFont(fontFamilies.first(), size, weight);
            qDebug() << "Loaded font:" << fontName << "from" << fontPath;
        }
    } else {
        // 폰트 로드 실패 시 시스템 폰트 사용
        font = QFont(fontName, size, weight);
        qDebug() << "Using system font:" << fontName;
    }
    
    return font;
}

void GameWindow::goBackToMainWindow()
{
    qDebug() << "Going back to main window";
    
    // 게임 상태 정지
    gameRunning = false;
    
    // 모든 타이머 정지
    if (gameTimer) {
        gameTimer->stop();
    }
    if (obstacleTimer) {
        obstacleTimer->stop();
    }
    if (pitchTimer) {
        pitchTimer->stop();
    }
    if (countdownTimer) {
        countdownTimer->stop();
    }
    
    // 게임 오버 다이얼로그가 열려있으면 닫기
    if (gameOverDialog && gameOverDialog->isVisible()) {
        gameOverDialog->disconnect();
        gameOverDialog->close();
        gameOverDialog = nullptr;
    }
    
    // 마이크 프로세스 정리
    stopMicProcess();
    
    // 메인 윈도우로 돌아가는 시그널 발생 (배경 음악 중지를 위해)
    emit requestMainWindow();
    
    // 잠시 대기 후 게임 창 닫기
    QTimer::singleShot(100, this, [this]() {
        close();
    });
}


void GameWindow::setCurrentPlayer(const QString &playerName)
{
    currentPlayerName = playerName;
}


//GameWindow::~GameWindow() {

    // Clean up resources if needed
    // All child QObjects with 'this' as parent are deleted automatically,
    // but we ensure any manual allocations are cleaned up.
    // (Most members are parented to 'this', so explicit deletion is not strictly necessary.)
    // If you add any new raw pointers, clean them up here.


// 멀티플레이어 관련 함수들
void GameWindow::startMultiplayer()
{
    qDebug() << "Starting multiplayer mode...";
    
    // 기존 소켓이 있다면 정리
    if (udpSocket) {
        udpSocket->close();
        udpSocket->deleteLater();
        udpSocket = nullptr;
    }
    
    // 기존 타이머들 정리
    if (broadcastTimer) {
        broadcastTimer->stop();
        broadcastTimer->deleteLater();
        broadcastTimer = nullptr;
    }
    
    if (cleanupTimer) {
        qDebug() << "[stopMultiplayer] Deleting cleanupTimer...";
        cleanupTimer->stop();
        cleanupTimer->deleteLater();
        cleanupTimer = nullptr;
    }
    // UDP 소켓 생성
    udpSocket = new QUdpSocket(this);
    if (!udpSocket) {
        qDebug() << "Failed to create UDP socket";
        return;
    }
    
    // 소켓 버퍼 크기 증가로 네트워크 성능 개선
    udpSocket->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption, 65536);
    udpSocket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, 65536);
    
    // 소켓 우선순위 설정 (가능한 경우)
    udpSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    
    // 소켓 바인딩 시도 - 모든 인터페이스에서 수신
    if (!udpSocket->bind(QHostAddress::Any, BROADCAST_PORT, QUdpSocket::ShareAddress)) {
        qDebug() << "Failed to bind UDP socket to port" << BROADCAST_PORT;
        udpSocket->deleteLater();
        udpSocket = nullptr;
        return;
    }
    
    // 데이터그램 읽기 시그널 연결
    connect(udpSocket, &QUdpSocket::readyRead, this, &GameWindow::readPendingDatagrams);
    
    // 소켓 에러 시그널 연결
    connect(udpSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
            this, [this](QAbstractSocket::SocketError error) {
        qDebug() << "UDP Socket error:" << error << udpSocket->errorString();
    });
    
    // 클린업 타이머 설정
    cleanupTimer = new QTimer(this);
    if (cleanupTimer) {
        connect(cleanupTimer, &QTimer::timeout, this, &GameWindow::cleanupInactivePlayers);
        cleanupTimer->start(CLEANUP_INTERVAL);
    }
    
    qDebug() << "Multiplayer mode started successfully";
    qDebug() << "UDP Socket bound to port" << BROADCAST_PORT;
    qDebug() << "Local address:" << udpSocket->localAddress().toString();
    qDebug() << "Local port:" << udpSocket->localPort();
}

void GameWindow::stopMultiplayer()
{
    qDebug() << "[stopMultiplayer] called";
    qDebug() << "[stopMultiplayer] isInLobby:" << isInLobby << ", isGameStarted:" << isGameStarted << ", isHost:" << isHost;
    // 멀티플레이어 상태 초기화
    isInLobby = false;
    isGameStarted = false;
    isHost = false;
    countdownValue = 0;
    // 타이머들 정리
    if (broadcastTimer) {
        qDebug() << "[stopMultiplayer] Deleting broadcastTimer...";
        broadcastTimer->stop();
        broadcastTimer->deleteLater();
        broadcastTimer = nullptr;
    }
    if (cleanupTimer) {
        qDebug() << "[stopMultiplayer] Deleting cleanupTimer...";
        cleanupTimer->stop();
        cleanupTimer->deleteLater();
        cleanupTimer = nullptr;
    }
    // UDP 소켓 정리
    if (udpSocket) {
        qDebug() << "[stopMultiplayer] Deleting udpSocket...";
        udpSocket->close();
        udpSocket->deleteLater();
        udpSocket = nullptr;
    }
    // 플레이어 목록 정리
    otherPlayers.clear();
    qDebug() << "[stopMultiplayer] finished";
}

void GameWindow::updatePlayerPosition(int x, int y, int score, bool gameOver)
{
    if (!isMultiplayerMode || !udpSocket) return;
    
    try {
        // 바이너리 프로토콜로 최적화 (JSON 대신)
        QByteArray datagram;
        QDataStream stream(&datagram, QIODevice::WriteOnly);
        
        // 프로토콜 헤더: "POS" (3바이트)
        stream.writeRawData("POS", 3);
        
        // 플레이어 ID 길이와 ID
        QByteArray idBytes = playerId.toUtf8();
        quint8 idLength = idBytes.size();
        stream << idLength;
        stream.writeRawData(idBytes.constData(), idLength);
        
        // 위치 및 게임 상태 데이터
        stream << (qint32)x << (qint32)y << (qint32)score << (bool)gameOver;
        
        // 타임스탬프
        qint64 sendTime = QDateTime::currentMSecsSinceEpoch();
        stream << sendTime;
        
        if (datagram.isEmpty()) {
            qDebug() << "Empty datagram generated";
            return;
        }
        
        // 디버그: 전송 시간 로그 (10번에 한 번만)
        static int sendCount = 0;
        if (++sendCount % 10 == 0) {
            qDebug() << "Sending position at" << sendTime << "x:" << x << "y:" << y;
        }
        
        // 브로드캐스트 주소로 전송
        QHostAddress broadcastAddress("192.168.10.255");
        qint64 bytesSent = udpSocket->writeDatagram(datagram, broadcastAddress, BROADCAST_PORT);
        
        if (bytesSent != datagram.size()) {
            qDebug() << "Failed to send datagram to broadcast address:" << udpSocket->errorString();
        }
    } catch (...) {
        qDebug() << "Exception in updatePlayerPosition";
    }
}

void GameWindow::readPendingDatagrams()
{
    if (!udpSocket) return;
    
    try {
        // 즉시 처리하도록 우선순위 설정
        QThread::currentThread()->setPriority(QThread::HighPriority);
        
        while (udpSocket->hasPendingDatagrams()) {
            QByteArray datagram;
            datagram.resize(udpSocket->pendingDatagramSize());
            QHostAddress sender;
            quint16 senderPort;
            
            qint64 bytesRead = udpSocket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
            
            if (bytesRead > 0 && !datagram.isEmpty()) {
                // 즉시 처리
                processIncomingData(datagram, sender, senderPort);
            }
        }
    } catch (...) {
        qDebug() << "Exception in readPendingDatagrams";
    }
}

void GameWindow::processIncomingData(const QByteArray &data, const QHostAddress &sender, quint16 port)
{
    if (data.isEmpty()) return;
    
    try {
        // 바이너리 프로토콜 처리
        QDataStream stream(data);
        
        // 프로토콜 헤더 확인
        char header[3];
        stream.readRawData(header, 3);
        
        if (strncmp(header, "POS", 3) == 0) {
            // 위치 업데이트 패킷
            processPositionPacket(stream, sender, port);
        } else {
            // 기존 JSON 프로토콜 처리 (하위 호환성)
            processJsonData(data, sender, port);
        }
    } catch (...) {
        qDebug() << "Exception in processIncomingData";
    }
}

void GameWindow::processPositionPacket(QDataStream &stream, const QHostAddress &sender, quint16 port)
{
    // 플레이어 ID 읽기
    quint8 idLength;
    stream >> idLength;
    
    QByteArray idBytes;
    idBytes.resize(idLength);
    stream.readRawData(idBytes.data(), idLength);
    QString playerId = QString::fromUtf8(idBytes);
    
    // 자신의 데이터는 무시
    if (playerId == this->playerId) return;
    
    // 위치 및 게임 상태 데이터 읽기
    qint32 x, y, score;
    bool gameOver;
    qint64 sendTime;
    
    stream >> x >> y >> score >> gameOver >> sendTime;
    
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    qint64 latency = currentTime - sendTime;
    
    // 디버그: 수신 시간과 지연 로그 (10번에 한 번만)
    static int recvCount = 0;
    if (++recvCount % 10 == 0) {
        qDebug() << "Received position from" << playerId << "latency:" << latency << "ms x:" << x << "y:" << y;
    }
    
    // 기존 플레이어 업데이트 또는 새 플레이어 추가
    bool found = false;
    for (int i = 0; i < otherPlayers.size(); ++i) {
        if (otherPlayers[i].playerId == playerId) {
            // 위치를 바로 갱신
            otherPlayers[i].x = x;
            otherPlayers[i].y = y;
            otherPlayers[i].score = score;
            
            bool wasGameOver = otherPlayers[i].gameOver;
            otherPlayers[i].gameOver = gameOver;
            
            // 게임 오버 상태가 변경되었을 때 시간 기록
            if (!wasGameOver && otherPlayers[i].gameOver) {
                otherPlayers[i].gameOverTime = currentTime;
                qDebug() << "Player" << otherPlayers[i].playerId << "game over with score:" << otherPlayers[i].score;
            }
            
            otherPlayers[i].address = sender;
            otherPlayers[i].port = port;
            otherPlayers[i].lastSeen = currentTime;
            
            // 위치 업데이트 후 즉시 화면 갱신
            update();
            
            found = true;
            break;
        }
    }
    
    if (!found) {
        PlayerData playerData;
        playerData.playerId = playerId;
        playerData.x = x;
        playerData.y = y;
        playerData.score = score;
        playerData.gameOver = gameOver;
        
        // 게임 오버 시간 초기화
        if (playerData.gameOver) {
            playerData.gameOverTime = currentTime;
        } else {
            playerData.gameOverTime = 0;
        }
        
        playerData.playerName = "";
        playerData.address = sender;
        playerData.port = port;
        playerData.lastSeen = currentTime;
        
        otherPlayers.append(playerData);
        qDebug() << "New player joined:" << playerId;
        
        // 새 플레이어 추가 후 즉시 화면 갱신
        update();
        
        // 대기실에서 새 플레이어가 들어오면 게임 시작 조건 확인
        if (isInLobby && !isGameStarted) {
            checkGameStart();
        }
    }
}

void GameWindow::processJsonData(const QByteArray &data, const QHostAddress &sender, quint16 port)
{
    // 기존 JSON 처리 로직 (하위 호환성)
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        qDebug() << "JSON parse error:" << error.errorString();
        return;
    }
    
    if (!doc.isObject()) {
        qDebug() << "JSON document is not an object";
        return;
    }
    
    QJsonObject obj = doc.object();
    QString type = obj["type"].toString();
    
    if (type == "player_update") {
        // 기존 JSON 위치 업데이트 처리
        QString playerId = obj["playerId"].toString();
        
        // 자신의 데이터는 무시
        if (playerId == this->playerId) return;
        
        qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        
        // 기존 플레이어 업데이트 또는 새 플레이어 추가
        bool found = false;
        for (int i = 0; i < otherPlayers.size(); ++i) {
            if (otherPlayers[i].playerId == playerId) {
                // 위치를 바로 갱신
                otherPlayers[i].x = obj["x"].toInt();
                otherPlayers[i].y = obj["y"].toInt();
                
                otherPlayers[i].score = obj["score"].toInt();
                bool wasGameOver = otherPlayers[i].gameOver;
                otherPlayers[i].gameOver = obj["gameOver"].toBool();
                
                // 게임 오버 상태가 변경되었을 때 시간 기록
                if (!wasGameOver && otherPlayers[i].gameOver) {
                    otherPlayers[i].gameOverTime = currentTime;
                    qDebug() << "Player" << otherPlayers[i].playerId << "game over with score:" << otherPlayers[i].score;
                }
                
                // 플레이어 이름 업데이트 (있는 경우)
                if (obj.contains("playerName")) {
                    otherPlayers[i].playerName = obj["playerName"].toString();
                }
                
                otherPlayers[i].address = sender;
                otherPlayers[i].port = port;
                otherPlayers[i].lastSeen = currentTime;
                
                // 위치 업데이트 후 즉시 화면 갱신
                update();
                
                found = true;
                break;
            }
        }
        
        if (!found) {
            PlayerData playerData;
            playerData.playerId = playerId;
            playerData.x = obj["x"].toInt();
            playerData.y = obj["y"].toInt();
            playerData.score = obj["score"].toInt();
            playerData.gameOver = obj["gameOver"].toBool();
            
            // 게임 오버 시간 초기화
            if (playerData.gameOver) {
                playerData.gameOverTime = currentTime;
            } else {
                playerData.gameOverTime = 0;
            }
            
            // 플레이어 이름 설정 (있는 경우)
            if (obj.contains("playerName")) {
                playerData.playerName = obj["playerName"].toString();
            } else {
                playerData.playerName = "";
            }
            
            playerData.address = sender;
            playerData.port = port;
            playerData.lastSeen = currentTime;
            otherPlayers.append(playerData);
            qDebug() << "New player joined:" << playerId;
            
            // 새 플레이어 추가 후 즉시 화면 갱신
            update();
            
            // 대기실에서 새 플레이어가 들어오면 게임 시작 조건 확인
            if (isInLobby && !isGameStarted) {
                checkGameStart();
            }
        }
    }
    else if (type == "game_start") {
        if (!isHost) {
            countdownValue = obj["countdown"].toInt();
            qDebug() << "Game countdown started:" << countdownValue;
        }
    }
    else if (type == "countdown") {
        if (!isHost) {
            countdownValue = obj["value"].toInt();
            qDebug() << "Countdown:" << countdownValue;
        }
    }
    else if (type == "game_started") {
        if (!isHost) {
            qDebug() << "Game started by host!";
            isGameStarted = true;
            isInLobby = false;
        }
    }
    else if (type == "game_state") {
        processGameState(obj);
    }
}

void GameWindow::cleanupInactivePlayers()
{
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    
    for (int i = otherPlayers.size() - 1; i >= 0; --i) {
        if (currentTime - otherPlayers[i].lastSeen > PLAYER_TIMEOUT) {
            qDebug() << "Player timed out:" << otherPlayers[i].playerId;
            otherPlayers.removeAt(i);
        }
    }
    
    // 대기실에서 플레이어가 나가면 게임 시작 조건 재확인
    if (isInLobby && !isGameStarted) {
        checkGameStart();
    }
}

// 대기실 시스템 관련 함수들
void GameWindow::startLobby()
{
    qDebug() << "Starting lobby...";
    isInLobby = true;
    isGameStarted = false;
    
    // 첫 번째 플레이어가 호스트가 됨
    if (otherPlayers.isEmpty()) {
        isHost = true;
        qDebug() << "You are the host";
    }
    
    // 준비 상태 전송
    updatePlayerPosition(player.x(), player.y(), score, false);
}

void GameWindow::leaveLobby()
{
    qDebug() << "Leaving lobby...";
    isInLobby = false;
    isGameStarted = false;
    isHost = false;
    
    // 게임 종료 상태 전송
    updatePlayerPosition(player.x(), player.y(), score, true);
}

void GameWindow::checkGameStart()
{
    if (!isInLobby || isGameStarted) return;
    
    // 이미 카운트다운이 진행 중이면 중복 실행 방지
    if (countdownTimer && countdownTimer->isActive()) return;
    
    // 최소 2명 이상이고 모든 플레이어가 준비되었을 때 게임 시작
    int totalPlayers = otherPlayers.size() + 1;
    if (totalPlayers >= 2) {
        // 호스트가 게임 시작 카운트다운 시작
        if (isHost) {
            qDebug() << "Starting game countdown...";
            countdownValue = 3;
            
            // 기존 타이머가 있다면 정리
            if (countdownTimer) {
                countdownTimer->stop();
                countdownTimer->deleteLater();
            }
            
            countdownTimer = new QTimer(this);
            connect(countdownTimer, &QTimer::timeout, this, &GameWindow::startGameCountdown);
            countdownTimer->start(1000); // 1초마다
            
            // 게임 시작 메시지 전송
            QJsonObject startMsg;
            startMsg["type"] = "game_start";
            startMsg["countdown"] = countdownValue;
            
            QJsonDocument doc(startMsg);
            QByteArray datagram = doc.toJson();
            
            QHostAddress broadcastAddress("192.168.10.255");
            udpSocket->writeDatagram(datagram, broadcastAddress, BROADCAST_PORT);
        }
    }
}

void GameWindow::startGameCountdown()
{
    countdownValue--;
    
    if (countdownValue > 0) {
        // 카운트다운 계속
        QJsonObject countdownMsg;
        countdownMsg["type"] = "countdown";
        countdownMsg["value"] = countdownValue;
        
        QJsonDocument doc(countdownMsg);
        QByteArray datagram = doc.toJson();
        
        QHostAddress broadcastAddress("192.168.10.255");
        udpSocket->writeDatagram(datagram, broadcastAddress, BROADCAST_PORT);
    } else {
        // 게임 시작
        qDebug() << "Game started!";
        isGameStarted = true;
        isInLobby = false;
        
        if (countdownTimer) {
            countdownTimer->stop();
            countdownTimer->deleteLater();
            countdownTimer = nullptr;
        }
        
        // 게임 시작 메시지 전송
        QJsonObject startMsg;
        startMsg["type"] = "game_started";
        
        QJsonDocument doc(startMsg);
        QByteArray datagram = doc.toJson();
        
        QHostAddress broadcastAddress("192.168.10.255");
        udpSocket->writeDatagram(datagram, broadcastAddress, BROADCAST_PORT);
        
        // 호스트가 첫 번째 장애물 생성
        if (isHost) {
            // 호스트의 obstacleTimer 시작
            if (obstacleTimer && !obstacleTimer->isActive()) {
                obstacleTimer->start(1800);
            }
            spawnObstacles();
        }
        
        // 클라이언트는 obstacleTimer를 시작하지 않음 (호스트의 게임 상태를 받아서 동기화)
    }
}

void GameWindow::sendGameState()
{
    if (!isMultiplayerMode || !udpSocket || !isHost) return;
    
    QJsonObject gameState;
    gameState["type"] = "game_state";
    gameState["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    
    // 장애물 정보
    QJsonArray obstaclesArray;
    for (const QRect &obstacle : obstacles) {
        QJsonObject obstacleObj;
        obstacleObj["x"] = obstacle.x();
        obstacleObj["y"] = obstacle.y();
        obstacleObj["width"] = obstacle.width();
        obstacleObj["height"] = obstacle.height();
        obstaclesArray.append(obstacleObj);
    }
    gameState["obstacles"] = obstaclesArray;
    
    // 별 정보
    QJsonArray starsArray;
    for (const Star &star : stars) {
        if (star.active) {
            QJsonObject starObj;
            starObj["x"] = star.pos.x();
            starObj["y"] = star.pos.y();
            starsArray.append(starObj);
        }
    }
    gameState["stars"] = starsArray;
    
    QJsonDocument doc(gameState);
    QByteArray datagram = doc.toJson();
    
    // 브로드캐스트 주소로 전송
    QHostAddress broadcastAddress("192.168.10.255");
    udpSocket->writeDatagram(datagram, broadcastAddress, BROADCAST_PORT);
    
    // 디버그 로그는 10번에 한 번만 출력
    static int logCount = 0;
    if (++logCount % 10 == 0) {
        qDebug() << "Game state sent - Obstacles:" << obstacles.size() << "Stars:" << stars.size();
    }
}

void GameWindow::processGameState(const QJsonObject &gameState)
{
    if (!isMultiplayerMode || isHost) return; // 호스트는 자신의 상태를 받지 않음
    
    qint64 timestamp = gameState["timestamp"].toVariant().toLongLong();
    if (timestamp <= lastGameStateUpdate) return; // 오래된 상태는 무시
    
    lastGameStateUpdate = timestamp;
    
    // 장애물 동기화
    obstacles.clear();
    QJsonArray obstaclesArray = gameState["obstacles"].toArray();
    for (const QJsonValue &value : obstaclesArray) {
        QJsonObject obstacleObj = value.toObject();
        QRect obstacle(
            obstacleObj["x"].toInt(),
            obstacleObj["y"].toInt(),
            obstacleObj["width"].toInt(),
            obstacleObj["height"].toInt()
        );
        obstacles.append(obstacle);
    }
    
    // 별 동기화
    stars.clear();
    QJsonArray starsArray = gameState["stars"].toArray();
    for (const QJsonValue &value : starsArray) {
        QJsonObject starObj = value.toObject();
        QPointF pos(starObj["x"].toDouble(), starObj["y"].toDouble());
        stars.append(Star(pos));
    }
    
    // 디버그 로그는 10번에 한 번만 출력
    static int logCount = 0;
    if (++logCount % 10 == 0) {
        qDebug() << "Game state received - Obstacles:" << obstacles.size() << "Stars:" << stars.size();
    }

}

void GameWindow::addFeedback(const QString &message, const QPointF &position, const QColor &color, int fontSize)
{
    GameFeedbackData feedback(message, position, color, fontSize);
    feedback.startTime = QDateTime::currentMSecsSinceEpoch() / 1000.0; // 현재 시간을 초 단위로
    feedbacks.append(feedback);
    
    // 최대 5개의 피드백만 유지
    if (feedbacks.size() > 5) {
        feedbacks.removeFirst();
    }
}

void GameWindow::updateFeedbacks()
{
    double currentTime = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    
    // 시간이 지난 피드백 제거
    for (int i = feedbacks.size() - 1; i >= 0; --i) {
        if (currentTime - feedbacks[i].startTime > feedbacks[i].duration) {
            feedbacks.removeAt(i);
        }
    }
}

void GameWindow::checkPitchAccuracy()
{
    if (!gameRunning) return;
    
    // 플레이어가 장애물을 통과하는 시점인지 확인
    bool passingObstacle = false;
    for (const QRect &obstacle : obstacles) {
        // 플레이어가 장애물을 통과하는 순간 (플레이어가 장애물의 오른쪽 끝에 도달했을 때)
        if (player.x() >= obstacle.x() + obstacle.width() && 
            player.x() <= obstacle.x() + obstacle.width() + 10) {
            passingObstacle = true;
            break;
        }
    }
    
    if (!passingObstacle) return;
    
    // 장애물 통과 성공! (충돌하지 않고 통과했다는 것은 성공)
    consecutivePerfect++;
    
    // 기존 피드백 메시지 모두 제거 (겹침 방지)
    clearFeedbacks();
    
    // 연속 Perfect에 따른 피드백
    QString message;
    QColor color;
    int fontSize;
    
    if (consecutivePerfect >= 10) {
        message = "LEGENDARY!";
        color = QColor(255, 0, 255); // 마젠타
        fontSize = 32;
    } else if (consecutivePerfect >= 7) {
        message = "AMAZING!";
        color = QColor(255, 165, 0); // 주황색
        fontSize = 30;
    } else if (consecutivePerfect >= 5) {
        message = "FANTASTIC!";
        color = QColor(0, 255, 255); // 시안
        fontSize = 28;
    } else if (consecutivePerfect >= 3) {
        message = "EXCELLENT!";
        color = QColor(0, 255, 0); // 초록색
        fontSize = 26;
    } else {
        message = "PERFECT!";
        color = QColor(255, 255, 0); // 노란색
        fontSize = 24;
    }
    
    addFeedback(message, QPointF(player.x() + 50, player.y() - 30), color, fontSize);
    lastFeedbackTime = QDateTime::currentMSecsSinceEpoch() / 1000.0;
}

void GameWindow::clearFeedbacks()
{
    feedbacks.clear();
}
