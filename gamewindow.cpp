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
    , targetY(WINDOW_HEIGHT/2 - PLAYER_SIZE/2)
{
    qDebug() << "GameWindow constructor called" << (isMultiplayer ? "(Multiplayer)" : "(Single Player)");
    
    // 초기화 과정에서 창이 보이지 않도록 숨김
    hide();

    // 생성자에서 바로 초기화하지 않고 이벤트 루프가 시작된 후 초기화
    QTimer::singleShot(50, this, &GameWindow::setupGame);
}


GameWindow::~GameWindow()
{
    qDebug() << "GameWindow destructor called";
    gameRunning = false;
    try {
        // 타이머 안전 정리
        auto safeDeleteTimer = [](QTimer*& timer) {
            if (timer) {
                timer->stop();
                timer->deleteLater();
                timer = nullptr;
            }
        };
        safeDeleteTimer(gameTimer);
        safeDeleteTimer(obstacleTimer);
        safeDeleteTimer(pitchTimer);
        safeDeleteTimer(countdownTimer);
        safeDeleteTimer(broadcastTimer);
        safeDeleteTimer(cleanupTimer);

        // 멀티플레이어 정리 (stopMultiplayer 내부에서 deleteLater 중복 호출 안 하도록 주의)
        if (udpSocket || broadcastTimer || cleanupTimer || !otherPlayers.isEmpty()) {
            stopMultiplayer();
        }

        // 마이크 프로세스 안전 정리
        if (micProcess) {
            if (micProcess->state() != QProcess::NotRunning) {
                micProcess->terminate();
                if (!micProcess->waitForFinished(2000)) {
                    micProcess->kill();
                    micProcess->waitForFinished(1000);
                }
            }
            micProcess->deleteLater();
            micProcess = nullptr;
        }

        // 사운드 프로세스 안전 정리
        if (soundProcess) {
            if (soundProcess->state() != QProcess::NotRunning) {
                soundProcess->terminate();
                soundProcess->waitForFinished(1000);
            }
            soundProcess->deleteLater();
            soundProcess = nullptr;
        }

        // 버튼 안전 정리
        if (backButton) {
            backButton->deleteLater();
            backButton = nullptr;
        }
    } catch (...) {
        qDebug() << "Exception in GameWindow destructor (ignored)";
    }
    qDebug() << "GameWindow destructor completed";
}


void GameWindow::setupGame()
{
    qDebug() << "Setting up game window...";
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
    player = QRect(50, height()/2 - PLAYER_SIZE/2, PLAYER_SIZE, PLAYER_SIZE);
    targetY = height()/2 - PLAYER_SIZE/2;
    
    // 타이머는 이미 QObject(parent)로 관리되므로 중복 생성 방지
    if (!gameTimer) {
        gameTimer = new QTimer(this);
        connect(gameTimer, &QTimer::timeout, this, &GameWindow::updateGame);
    }
    gameTimer->start(16); // 약 60 FPS
    
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
    

    // 뒤로가기 버튼 설정 (중복 생성 방지)
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
    
    // 뒤로가기 버튼 설정 (멀티플레이어 모드에서만)
    if (isMultiplayerMode) {
        setupBackButton();
    }

    
    // 초기 화면 그리기
    update();
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
    if (micProcess) {
        micProcess->terminate();
        if (!micProcess->waitForFinished(3000)) {
            micProcess->kill();
        }
        micProcess->deleteLater();
        micProcess = nullptr;
    }
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
    painter.setRenderHint(QPainter::Antialiasing, false); // 성능: 안티앨리어싱 OFF
    // 배경 그리기 (이미지 최적화)
    static QPixmap bgPixmap;
    static QSize lastBgSize;
    if (bgPixmap.isNull() || lastBgSize != size()) {
        QPixmap rawBg;
        if (rawBg.load("/mnt/nfs/background.png")) {
            bgPixmap = rawBg.scaled(size(), Qt::IgnoreAspectRatio, Qt::FastTransformation); // 성능: FastTransformation
            lastBgSize = size();
        } else {
            bgPixmap = QPixmap();
        }
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
    
    // 장애물 그리기 (brick_pillar.png의 중앙 기둥 부분만 세로만 스케일, 가로는 원본 비율)
    static QPixmap pillarPixmap;
    static QMap<int, QPixmap> pillarVCache; // height별 캐시
    static int lastMaxPillarHeight = 0;
    static QPixmap croppedPillar;
    if (pillarPixmap.isNull())
        pillarPixmap.load("/mnt/nfs/brick_pillar.png");
    const int REAL_PILLAR_WIDTH = 60;
    // 1. 현재 장애물 중 가장 높은 높이 계산 (장애물 없으면 0)
    int maxPillarHeight = 0;
    for (const QRect &obstacle : obstacles) {
        if (obstacle.height() > maxPillarHeight)
            maxPillarHeight = obstacle.height();
    }
    // 2. 가장 높은 기둥 높이가 바뀌었을 때만 크롭 (불필요한 연산 방지)
    if (!pillarPixmap.isNull() && maxPillarHeight > 0 && maxPillarHeight != lastMaxPillarHeight) {
        int imgW = pillarPixmap.width();
        int imgH = pillarPixmap.height();
        int srcX = (imgW - REAL_PILLAR_WIDTH) / 2;
        int cropH = qMin(maxPillarHeight, imgH); // 이미지보다 높으면 이미지 끝까지만
        croppedPillar = pillarPixmap.copy(srcX, 0, REAL_PILLAR_WIDTH, cropH);
        pillarVCache.clear(); // 높이별 캐시도 무효화
        lastMaxPillarHeight = maxPillarHeight;
    }
    for (int i = 0; i < obstacles.size(); ++i) {
        const QRect &obstacle = obstacles[i];
        int h = obstacle.height();
        int x = obstacle.x() + (obstacle.width() - REAL_PILLAR_WIDTH) / 2;
        int y = obstacle.y();
        if (!croppedPillar.isNull()) {
            QPixmap scaled;
            if (pillarVCache.contains(h)) {
                scaled = pillarVCache.value(h);
            } else {
                // 3. 크롭된 이미지를 각 기둥 높이에 맞게 세로로만 스케일
                scaled = croppedPillar.scaled(REAL_PILLAR_WIDTH, h, Qt::IgnoreAspectRatio, Qt::FastTransformation); // FastTransformation로 성능 향상
                pillarVCache.insert(h, scaled);
            }
            painter.drawPixmap(x, y, REAL_PILLAR_WIDTH, h, scaled);
        } else {
            painter.setBrush(Qt::red);
            painter.setPen(Qt::NoPen);
            painter.drawRect(x, y, REAL_PILLAR_WIDTH, h);
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
    if (currentVolume > 0.1f) {
        int currentY = player.y();
        int dy = targetY - currentY;
        if (qAbs(dy) > 0) {
            player.translate(0, qBound(-playerSpeed, dy, playerSpeed));
        }
    }
    
    // 키보드 입력
    if (moveUp && player.y() > 0) {
        player.translate(0, -playerSpeed);
    }
    if (moveDown && player.y() < height() - PLAYER_SIZE) {
        player.translate(0, playerSpeed);
    }
    
    // 장애물 이동 및 제거 (역순 루프, reserve)
    const int leftBoundary = 0;
    const int obstacleSpeed = 3;
    for (int i = obstacles.size() - 1; i >= 0; --i) {
        QRect &obstacle = obstacles[i];
        obstacle.translate(-obstacleSpeed, 0);
        if (obstacle.x() + obstacle.width() < leftBoundary) {
            obstacles.removeAt(i);
            score++;
        }
    }
    
    // 별 이동 및 충돌 검사 (역순 루프, reserve)
    const QRectF playerBounds(player.x() - 15, player.y() - 15, player.width() + 30, player.height() + 30);
    const int halfStarSize = starSize / 2;
    const int starSpeed = 3;
    for (int i = stars.size() - 1; i >= 0; --i) {
        Star &star = stars[i];
        if (!star.active) continue;
        if (star.pos.x() + halfStarSize < leftBoundary) {
            star.active = false;
            continue;
        }
        star.pos.setX(star.pos.x() - starSpeed);
        const qreal dx = qAbs(star.pos.x() - player.x());
        const qreal dy = qAbs(star.pos.y() - player.y());
        if (dx > starSize || dy > starSize) continue;
        QRectF starRect(star.pos.x() - halfStarSize, star.pos.y() - halfStarSize, starSize, starSize);
        if (playerBounds.intersects(starRect)) {
            star.active = false;
            score += 3;
            playSound("/mnt/nfs/wav/item.wav");
        }
    }
    
    // 충돌 검사
    if (checkCollision()) {
        gameOver();
        return;
    }
    
    // 비활성 별 정리 (reserve)
    const int MAX_STARS = 25;
    if (stars.size() > MAX_STARS) {
        for (int i = stars.size() - 1; i >= 0; --i) {
            if (!stars[i].active) {
                stars.removeAt(i);
            }
        }
    }
    

    // 멀티플레이어 모드에서 네트워크 업데이트
    if (isMultiplayerMode) {
        updatePlayerPosition(player.x(), player.y(), score, false);
        
        // 호스트가 주기적으로 게임 상태 전송 (2초마다)
        static int frameCount = 0;
        if (isHost && isGameStarted && frameCount % 120 == 0) { // 120프레임마다 (약 2초)
            sendGameState();
        }
        frameCount++;
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
    // 완전 투명 배경, 그림자/테두리 없음
    QString buttonStyle = 
        "QPushButton {"
        "   background-color: transparent;"
        "   border: none;"
        "   border-radius: 0px;"
        "   padding: 0px;"
        "}"
        "QPushButton:hover {"
        "   background-color: rgba(255,255,255,40);"
        "}"
        "QPushButton:pressed {"
        "   background-color: rgba(0,0,0,30);"
        "}";
    backButton->setStyleSheet(buttonStyle);
    // 아이콘 크게 (40x40)
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
    
    // 멀티플레이어 정리
    stopMultiplayer();
    
    // 마이크 프로세스 정리
    stopMicProcess();
    
    // 타이머들 정지
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
    
    // 메인 윈도우로 돌아가라는 시그널 발생
    emit requestMainWindow();
    
    // 게임 창 닫기
    close();
}


void GameWindow::setCurrentPlayer(const QString &playerName)
{
    currentPlayerName = playerName;
}


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
    qDebug() << "Stopping multiplayer mode...";
    
    // 멀티플레이어 상태 초기화
    isInLobby = false;
    isGameStarted = false;
    isHost = false;
    countdownValue = 0;
    
    // 타이머들 정리
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
    
    // UDP 소켓 정리
    if (udpSocket) {
        udpSocket->close();
        udpSocket->deleteLater();
        udpSocket = nullptr;
    }
    
    // 플레이어 목록 정리
    otherPlayers.clear();
    
    qDebug() << "Multiplayer mode stopped";
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
