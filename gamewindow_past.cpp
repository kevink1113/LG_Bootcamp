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

{
    qDebug() << "GameWindow constructor called" << (isMultiplayer ? "(Multiplayer)" : "(Single Player)");
    
    // 초기화 과정에서 창이 보이지 않도록 숨김
    hide();
    
    // 생성자에서 바로 초기화하지 않고 이벤트 루프가 시작된 후 초기화
    QTimer::singleShot(100, this, &GameWindow::setupGame);
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
    
    // 타이머는 이미 QObject(parent)로 관리되므로 중복 생성 방지
    if (!gameTimer) {
        gameTimer = new QTimer(this);
        connect(gameTimer, &QTimer::timeout, this, &GameWindow::updateGame);
    }
    gameTimer->start(8); // 약 120 FPS (더 부드러운 움직임)
    
    if (!obstacleTimer) {
        obstacleTimer = new QTimer(this);
        connect(obstacleTimer, &QTimer::timeout, this, &GameWindow::spawnObstacles);

        // 싱글플레이어 모드이거나 멀티플레이어 호스트일 때만 타이머 시작
        if (!isMultiplayerMode) {
            obstacleTimer->start(2000); // 2초마다 장애물 생성
        }
        // 멀티플레이어 모드에서는 호스트가 게임 시작 후에 타이머를 시작함

    }
    obstacleTimer->start(2000); // 2초마다 장애물 생성
    
    if (!pitchTimer) {
        pitchTimer = new QTimer(this);
        connect(pitchTimer, &QTimer::timeout, this, &GameWindow::readPitchData);
    }
    pitchTimer->start(50); // 20Hz로 피치 읽기
    
    gameRunning = true;
    score = 0;
    obstacles.clear();
    stars.clear();

    
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
                obstacleTimer->start(2000); // 2초마다 장애물 생성
            }
        }
        
        if (!pitchTimer) {
            pitchTimer = new QTimer(this);
            if (pitchTimer) {
                connect(pitchTimer, &QTimer::timeout, this, &GameWindow::readPitchData);
                pitchTimer->start(50); // 20Hz로 피치 읽기
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
        const int PLAYER_DISPLAY_SIZE = PLAYER_SIZE * 3; // 기존보다 3배 크게
        if (cachedPlayerPixmap.isNull()) {
            QPixmap rawPixmap;
            if (rawPixmap.load("/mnt/nfs/player2.png")) {
                cachedPlayerPixmap = rawPixmap.scaled(PLAYER_DISPLAY_SIZE, PLAYER_DISPLAY_SIZE, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                qDebug() << "Player image loaded and cached from /mnt/nfs/player2.png (3x size)";
            } else {
                qDebug() << "Failed to load player image from /mnt/nfs/player2.png";
            }
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
                        const float normalizedPitch = (currentPitch - 1.0f) / pitchRange;
                        targetY = (1.0f - normalizedPitch) * (height() - PLAYER_SIZE);
                        targetY = qBound(0, targetY, height() - PLAYER_SIZE);
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
    painter.setRenderHint(QPainter::Antialiasing);
    // 배경 그리기 (이미지 최적화 예시)
    static QPixmap bgPixmap;
    if (bgPixmap.isNull()) {
        bgPixmap.load("/mnt/nfs/background.png");
        if (!bgPixmap.isNull())
            bgPixmap = bgPixmap.scaled(size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    if (!bgPixmap.isNull()) {
        painter.drawPixmap(rect(), bgPixmap);
    } else {
        painter.fillRect(rect(), Qt::black);
    }
    
    // 별 그리기 - 활성 별만 그리기
    painter.setBrush(QColor(255, 223, 0));  // 밝은 노란색
    painter.setPen(Qt::NoPen);
    
    // 눈과 미소 미리 생성
    QPainterPath smilePath;
    const qreal smileWidth = starSize/5;
    const qreal smileHeight = starSize/8;
    smilePath.moveTo(-smileWidth, 0);
    smilePath.quadTo(0, smileHeight, smileWidth, 0);
    
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
    if (obstacleTopPixmap.isNull())
        obstacleTopPixmap.load("/mnt/nfs/obstacle_top.png");
    if (obstacleBottomPixmap.isNull())
        obstacleBottomPixmap.load("/mnt/nfs/obstacle_bottom.png");
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
    static QFont infoFont("Arial", 12);  // 정적 폰트 객체
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
            QFont lobbyFont("Arial", 24, QFont::Bold);
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
            QFont countdownFont("Arial", 48, QFont::Bold);
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
    
    // 점수와 피치 정보 표시 (오른쪽 상단)

    painter.setPen(Qt::white);
    
    // 텍스트 위치 계산 (매 프레임마다 계산하지 않도록 최적화 가능)
    static QFontMetrics fm(infoFont);
    const int rightMargin = 10;
    const int topMargin = 25;
    const int lineSpacing = 20;
    
    // 필요한 문자열만 생성
    QString scoreText = QString("Score: %1").arg(score);
    QString playerText = QString("Player: %1").arg(currentPlayerName.isEmpty() ? "No Player" : currentPlayerName);
    
    // 오른쪽 정렬 텍스트
    int rightEdge = width() - rightMargin;
    painter.drawText(rightEdge - fm.horizontalAdvance(scoreText), topMargin, scoreText);
    painter.drawText(rightEdge - fm.horizontalAdvance(playerText), topMargin + lineSpacing * 3, playerText);
    
    // 디버그 정보는 조건부로 표시 (성능에 영향 줄이기)
#ifdef QT_DEBUG
    QString pitchText = QString("Pitch: %1").arg(currentPitch);
    QString volumeText = QString("Volume: %1").arg(QString::number(currentVolume, 'f', 2));
    painter.drawText(rightEdge - fm.horizontalAdvance(pitchText), topMargin + lineSpacing, pitchText);
    painter.drawText(rightEdge - fm.horizontalAdvance(volumeText), topMargin + lineSpacing * 2, volumeText);
#endif
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
    
    // 비활성 별 정리 (필요할 때만 처리)
    const int MAX_STARS = 25;  // 최대 별 개수
    if (stars.size() > MAX_STARS) {
        for (int i = stars.size() - 1; i >= 0; --i) {
            if (!stars[i].active) {
                stars.removeAt(i);
            }
        }
    }
    

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
    }
    
    // 화면 갱신
    update();
}

void GameWindow::calculateRankings()
{
    if (!isMultiplayerMode) return;
    
    // 모든 플레이어 정보를 수집 (자신 포함)
    QList<PlayerData> allPlayers;
    
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
    QFont titleFont("Arial", 16, QFont::Bold);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);
    
    // 내 순위 표시
    QString rankText = QString("Your Rank: %1/%2").arg(myRank).arg(finishedPlayers.size() + 1);
    QLabel *rankLabel = new QLabel(rankText, resultDialog);
    rankLabel->setAlignment(Qt::AlignCenter);
    QFont rankFont("Arial", 14, QFont::Bold);
    rankLabel->setFont(rankFont);
    rankLabel->setStyleSheet("color: #FFD700;"); // 금색
    layout->addWidget(rankLabel);
    
    // 내 점수
    QString scoreText = QString("Your Score: %1").arg(score);
    QLabel *scoreLabel = new QLabel(scoreText, resultDialog);
    scoreLabel->setAlignment(Qt::AlignCenter);
    scoreLabel->setFont(QFont("Arial", 12));
    layout->addWidget(scoreLabel);
    
    // 구분선
    QFrame *line = new QFrame(resultDialog);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    layout->addWidget(line);
    
    // 전체 순위표
    QLabel *rankingTitle = new QLabel("Final Rankings:", resultDialog);
    rankingTitle->setFont(QFont("Arial", 12, QFont::Bold));
    layout->addWidget(rankingTitle);
    
    QTextEdit *rankingText = new QTextEdit(resultDialog);
    rankingText->setReadOnly(true);
    rankingText->setMaximumHeight(150);
    
    QString rankingString;
    int rank = 1;
    
    // 모든 플레이어 정보를 수집
    QList<PlayerData> allPlayers;
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
    
    // 고정된 시드값 사용 (모든 보드에서 동일한 랜덤 시퀀스 생성)
    static QRandomGenerator fixedGenerator(FIXED_SEED); // 고정된 시드값
    
    // 장애물 개수에 따라 시드값 조정 (시간에 따른 변화)
    int obstacleCount = obstacles.size() / 2; // 장애물 쌍의 개수
    fixedGenerator.seed(FIXED_SEED + obstacleCount);
    
    // 장애물 간격을 플레이어가 통과할 수 있도록 조정
    // 최소 간격: OBSTACLE_GAP/2 + PLAYER_SIZE + 여유공간
    int minGapY = OBSTACLE_GAP/2 + PLAYER_SIZE + 50; // 최소 간격
    int maxGapY = height() - OBSTACLE_GAP/2 - PLAYER_SIZE - 50; // 최대 간격
    
    // 범위가 유효한지 확인
    if (minGapY >= maxGapY) {
        minGapY = 150;
        maxGapY = height() - 150;
    }
    
    int gapY = fixedGenerator.bounded(minGapY, maxGapY);
    
    // 위쪽 장애물
    QRect topObstacle(width(), 0, OBSTACLE_WIDTH, gapY - OBSTACLE_GAP/2);
    obstacles.append(topObstacle);
    
    // 아래쪽 장애물
    QRect bottomObstacle(width(), gapY + OBSTACLE_GAP/2, OBSTACLE_WIDTH, height() - (gapY + OBSTACLE_GAP/2));
    obstacles.append(bottomObstacle);

    // 30% 확률로 별 생성 (고정된 시드값 사용)
    if (fixedGenerator.bounded(100) < 30) {
        // 별을 장애물 사이 통과 가능한 공간의 중앙에 배치
        int starX = width() + OBSTACLE_WIDTH/2;
        int starY = gapY; // 장애물 사이 공간의 중앙
        stars.append(Star(QPointF(starX, starY)));
    }
    
    // 게임 상태 전송은 updateGame에서 주기적으로 처리
}

bool GameWindow::checkCollision()
{
    for (const QRect &obstacle : obstacles) {
        if (player.intersects(obstacle)) {
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
    
    // 멀티플레이어 모드에서 게임 오버 상태 전송
    if (isMultiplayerMode) {
        updatePlayerPosition(player.x(), player.y(), score, true);
    }
    
    stopMicProcess();
    
    if (gameTimer) {
        gameTimer->stop();
    }
    if (obstacleTimer) {
        obstacleTimer->stop();
    }
    if (pitchTimer) {
        pitchTimer->stop();
    }
    
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
    
    // 스타일 설정
    QString buttonStyle = 
        "QPushButton {"
        "   background-color: rgba(255, 255, 255, 180);"
        "   border: none;"
        "   border-radius: 10px;"
        "   padding: 5px;"
        "}"
        "QPushButton:hover {"
        "   background-color: rgba(255, 255, 255, 220);"
        "}"
        "QPushButton:pressed {"
        "   background-color: rgba(200, 200, 200, 220);"
        "}";
    backButton->setStyleSheet(buttonStyle);
    
    // 뒤로가기 아이콘 설정
    QStyle *style = QApplication::style();
    QIcon backIcon = style->standardIcon(QStyle::SP_ArrowBack);
    backButton->setIcon(backIcon);
    backButton->setIconSize(QSize(30, 30));
    
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
    
    // 게임 창 닫기 (시그널 발생 없이)
    close();
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
                obstacleTimer->start(2000);
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