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
    , playerSpeed(5)
    , score(0)
    , gameRunning(false)
    , moveUp(false)
    , moveDown(false)
    , currentPitch(0)
    , currentVolume(0.0f)
    , targetY(300)  // 기본값으로 설정
    , gameOverDialog(nullptr)

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
                gameTimer->start(16); // 약 60 FPS로 변경 (33에서 16으로)
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
    static QFont infoFont("Arial", 12);  // 정적 폰트 객체
    painter.setFont(infoFont);

    // 멀티플레이어 모드에서 다른 플레이어들 그리기
    if (isMultiplayerMode) {
        painter.setBrush(Qt::blue);
        painter.setPen(Qt::blue);
        for (const PlayerData &otherPlayer : otherPlayers) {
            // 다른 플레이어를 파란색 원으로 그리기
            painter.drawEllipse(otherPlayer.x, otherPlayer.y, PLAYER_SIZE, PLAYER_SIZE);
            
            // 플레이어 ID를 흰색으로 표시
            painter.setPen(Qt::white);
            painter.drawText(otherPlayer.x, otherPlayer.y - 5, otherPlayer.playerId);
            painter.setPen(Qt::blue);
        }
        
        // 대기실 화면 그리기
        if (isInLobby && !isGameStarted) {
            painter.setPen(Qt::white);
            static QFont lobbyFont("Arial", 24, QFont::Bold);
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
            static QFont countdownFont("Arial", 48, QFont::Bold);
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
    gameOverDialog->show();
    gameOverDialog->raise();
    gameOverDialog->activateWindow();
    
    // 다이얼로그가 닫힐 때 자동으로 삭제되도록 설정
    gameOverDialog->setAttribute(Qt::WA_DeleteOnClose, true);
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
    
    // 소켓 바인딩 시도
    if (!udpSocket->bind(BROADCAST_PORT, QUdpSocket::ShareAddress)) {
        qDebug() << "Failed to bind UDP socket to port" << BROADCAST_PORT;
        udpSocket->deleteLater();
        udpSocket = nullptr;
        return;
    }
    
    // 브로드캐스트 주소로 바인딩 (실패해도 계속 진행)
    if (!udpSocket->joinMulticastGroup(QHostAddress("192.168.10.255"))) {
        qDebug() << "Failed to join multicast group, continuing with unicast";
    }
    
    // 데이터그램 읽기 시그널 연결
    connect(udpSocket, &QUdpSocket::readyRead, this, &GameWindow::readPendingDatagrams);
    
    // 브로드캐스트 타이머 설정
    broadcastTimer = new QTimer(this);
    if (broadcastTimer) {
        connect(broadcastTimer, &QTimer::timeout, this, &GameWindow::broadcastPlayerData);
        broadcastTimer->start(BROADCAST_INTERVAL);
    }
    
    // 클린업 타이머 설정
    cleanupTimer = new QTimer(this);
    if (cleanupTimer) {
        connect(cleanupTimer, &QTimer::timeout, this, &GameWindow::cleanupInactivePlayers);
        cleanupTimer->start(CLEANUP_INTERVAL);
    }
    
    qDebug() << "Multiplayer mode started successfully";
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
        QJsonObject data;
        data["type"] = "player_update";
        data["playerId"] = playerId;
        data["x"] = x;
        data["y"] = y;
        data["score"] = score;
        data["gameOver"] = gameOver;
        data["timestamp"] = QDateTime::currentMSecsSinceEpoch();
        
        QJsonDocument doc(data);
        QByteArray datagram = doc.toJson();
        
        if (datagram.isEmpty()) {
            qDebug() << "Empty datagram generated";
            return;
        }
        
        // 192.168.10.3~8 범위로 브로드캐스트
        for (int i = 3; i <= 8; ++i) {
            QHostAddress address(QString("192.168.10.%1").arg(i));
            qint64 bytesSent = udpSocket->writeDatagram(datagram, address, BROADCAST_PORT);
            
            if (bytesSent != datagram.size()) {
                qDebug() << "Failed to send datagram to" << address.toString();
            }
        }
    } catch (...) {
        qDebug() << "Exception in updatePlayerPosition";
    }
}

void GameWindow::readPendingDatagrams()
{
    if (!udpSocket) return;
    
    try {
        while (udpSocket->hasPendingDatagrams()) {
            QByteArray datagram;
            datagram.resize(udpSocket->pendingDatagramSize());
            QHostAddress sender;
            quint16 senderPort;
            
            qint64 bytesRead = udpSocket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
            
            if (bytesRead > 0 && !datagram.isEmpty()) {
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
            QString playerId = obj["playerId"].toString();
            
            // 자신의 데이터는 무시
            if (playerId == this->playerId) return;
            
            PlayerData playerData;
            playerData.playerId = playerId;
            playerData.x = obj["x"].toInt();
            playerData.y = obj["y"].toInt();
            playerData.score = obj["score"].toInt();
            playerData.gameOver = obj["gameOver"].toBool();
            playerData.address = sender;
            playerData.port = port;
            playerData.lastSeen = QDateTime::currentMSecsSinceEpoch();
            
            // 기존 플레이어 업데이트 또는 새 플레이어 추가
            bool found = false;
            for (int i = 0; i < otherPlayers.size(); ++i) {
                if (otherPlayers[i].playerId == playerId) {
                    otherPlayers[i] = playerData;
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                otherPlayers.append(playerData);
                qDebug() << "New player joined:" << playerId;
                
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
                
                // 클라이언트는 obstacleTimer를 시작하지 않음 (호스트의 게임 상태를 받아서 동기화)
            }
        }
        else if (type == "game_state") {
            processGameState(obj);
        }
    } catch (...) {
        qDebug() << "Exception in processIncomingData";
    }
}

void GameWindow::broadcastPlayerData()
{
    if (!isMultiplayerMode || !gameRunning) return;
    
    updatePlayerPosition(player.x(), player.y(), score, false);
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
    
    // 최소 2명 이상이고 모든 플레이어가 준비되었을 때 게임 시작
    int totalPlayers = otherPlayers.size() + 1;
    if (totalPlayers >= 2) {
        // 호스트가 게임 시작 카운트다운 시작
        if (isHost) {
            qDebug() << "Starting game countdown...";
            countdownValue = 3;
            
            countdownTimer = new QTimer(this);
            connect(countdownTimer, &QTimer::timeout, this, &GameWindow::startGameCountdown);
            countdownTimer->start(1000); // 1초마다
            
            // 게임 시작 메시지 전송
            QJsonObject startMsg;
            startMsg["type"] = "game_start";
            startMsg["countdown"] = countdownValue;
            
            QJsonDocument doc(startMsg);
            QByteArray datagram = doc.toJson();
            
            for (int i = 3; i <= 8; ++i) {
                QHostAddress address(QString("192.168.10.%1").arg(i));
                udpSocket->writeDatagram(datagram, address, BROADCAST_PORT);
            }
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
        
        for (int i = 3; i <= 8; ++i) {
            QHostAddress address(QString("192.168.10.%1").arg(i));
            udpSocket->writeDatagram(datagram, address, BROADCAST_PORT);
        }
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
        
        for (int i = 3; i <= 8; ++i) {
            QHostAddress address(QString("192.168.10.%1").arg(i));
            udpSocket->writeDatagram(datagram, address, BROADCAST_PORT);
        }
        
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
    
    // 모든 클라이언트에게 전송
    for (int i = 3; i <= 8; ++i) {
        QHostAddress address(QString("192.168.10.%1").arg(i));
        udpSocket->writeDatagram(datagram, address, BROADCAST_PORT);
    }
    
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
