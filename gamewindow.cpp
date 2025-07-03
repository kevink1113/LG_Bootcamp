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
    
    // 게임 상태 정지
    gameRunning = false;
    
    // 타이머들 먼저 정지
    if (gameTimer) {
        gameTimer->stop();
        gameTimer->deleteLater();
        gameTimer = nullptr;
    }
    if (obstacleTimer) {
        obstacleTimer->stop();
        obstacleTimer->deleteLater();
        obstacleTimer = nullptr;
    }
    if (pitchTimer) {
        pitchTimer->stop();
        pitchTimer->deleteLater();
        pitchTimer = nullptr;
    }
    if (countdownTimer) {
        countdownTimer->stop();
        countdownTimer->deleteLater();
        countdownTimer = nullptr;
    }
    
    // 멀티플레이어 정리
    stopMultiplayer();
    
    // 마이크 프로세스 정리
    stopMicProcess();
    
    // 버튼 정리
    if (backButton) {
        backButton->deleteLater();
        backButton = nullptr;
    }
    
    qDebug() << "GameWindow destructor completed";
}

void GameWindow::setupGame()
{
    qDebug() << "Setting up game window...";
    
    // 현재 화면 크기 가져오기
    QScreen *screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    
    // 게임 윈도우 설정 - 표시되기 전에 모든 설정 완료
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    
    // 창 크기와 위치를 화면에 맞게 설정
    setGeometry(screenGeometry);
    
    // 전체화면 상태로 미리 설정
    setWindowState(Qt::WindowFullScreen);
    
    // 기본 설정 사용
    
    // 모든 설정이 완료된 후 윈도우 표시
    show();
    raise();
    activateWindow();
    
    // 플레이어 초기화 (화면 크기에 맞게 조정)
    player = QRect(50, height()/2 - PLAYER_SIZE/2, PLAYER_SIZE, PLAYER_SIZE);
    targetY = height()/2 - PLAYER_SIZE/2;
    
    // 타이머 설정
    gameTimer = new QTimer(this);
    if (gameTimer) {
        connect(gameTimer, &QTimer::timeout, this, &GameWindow::updateGame);
        gameTimer->start(16); // 약 60 FPS
    }
    
    obstacleTimer = new QTimer(this);
    if (obstacleTimer) {
        connect(obstacleTimer, &QTimer::timeout, this, &GameWindow::spawnObstacles);
        // 싱글플레이어 모드이거나 멀티플레이어 호스트일 때만 타이머 시작
        if (!isMultiplayerMode) {
            obstacleTimer->start(2000); // 2초마다 장애물 생성
        }
        // 멀티플레이어 모드에서는 호스트가 게임 시작 후에 타이머를 시작함
    }
    
    // 피치 읽기 타이머
    pitchTimer = new QTimer(this);
    if (pitchTimer) {
        connect(pitchTimer, &QTimer::timeout, this, &GameWindow::readPitchData);
        pitchTimer->start(50); // 20Hz로 피치 읽기
    }
    
    gameRunning = true;
    score = 0;
    obstacles.clear();
    stars.clear();
    
    // 별 모양 초기화 - 둥근 모서리와 부드러운 곡선
    starPath = QPainterPath();
    const qreal angleStep = M_PI / STAR_POINTS;
    const qreal controlDist = CORNER_SMOOTHNESS; // 제어점 거리 비율
    
    for (int i = 0; i < STAR_POINTS * 2; ++i) {
        qreal radius = (i % 2 == 0) ? starSize * OUTER_RADIUS / 2 : starSize * INNER_RADIUS / 2;
        qreal angle = i * angleStep;
        qreal nextAngle = (i + 1) * angleStep;
        
        QPointF point(radius * sin(angle), -radius * cos(angle));
        qreal nextRadius = ((i + 1) % 2 == 0) ? starSize * OUTER_RADIUS / 2 : starSize * INNER_RADIUS / 2;
        QPointF nextPoint(nextRadius * sin(nextAngle), -nextRadius * cos(nextAngle));
        
        if (i == 0) {
            starPath.moveTo(point);
        }
        
        // 현재 점과 다음 점 사이에 제어점을 추가하여 둥근 모서리 생성
        QPointF ctrl1 = point + QPointF(radius * controlDist * cos(angle), radius * controlDist * sin(angle));
        QPointF ctrl2 = nextPoint - QPointF(nextRadius * controlDist * cos(nextAngle), nextRadius * controlDist * sin(nextAngle));
        
        starPath.cubicTo(ctrl1, ctrl2, nextPoint);
    }
    starPath.closeSubpath();
    
    // 마이크 프로세스 시작
    startMicProcess();
    
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
    
    if (pitchFile) {
        pitchFile->close();
        delete pitchFile;
        pitchFile = nullptr;
    }
}

void GameWindow::readPitchData()
{
    if (!gameRunning) return;
    
    // 파일에서 피치 데이터 읽기
    if (!pitchFile) {
        pitchFile = new QFile("/tmp/pitch_score");
    }
    
    if (pitchFile->open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(pitchFile);
        QString line = in.readLine();
        pitchFile->close();
        
        if (!line.isEmpty()) {
            QStringList parts = line.split(" ");
            if (parts.size() >= 2) {
                bool ok1, ok2;
                int pitch = parts[0].toInt(&ok1);
                float volume = parts[1].toFloat(&ok2);
                
                if (ok1 && ok2) {
                    currentPitch = pitch;
                    currentVolume = volume;
                    
                    // 피치 값을 Y 좌표로 변환 (2옥타브 A=1 ~ 5옥타브 A=37)
                    // 낮은 피치 = 위쪽, 높은 피치 = 아래쪽
                    if (currentPitch > 0 && currentVolume > 0.1f) { // 볼륨이 일정 이상일 때만
                        int pitchRange = 37 - 1; // 1~37 범위
                        float normalizedPitch = (currentPitch - 1.0f) / pitchRange;
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
    
    // 배경 그리기
    painter.fillRect(rect(), Qt::black);
    
    // 별 그리기 (얼굴 포함)
    for (Star& star : stars) {
        if (!star.active) continue;
        
        painter.save();
        painter.translate(star.pos);
        
        // 별 그리기
        painter.setBrush(QColor(255, 223, 0));  // 밝은 노란색
        painter.setPen(Qt::NoPen);
        painter.drawPath(starPath);
        
        // 얼굴 그리기
        painter.setPen(QPen(Qt::black, 2));  // 선 굵기 감소
        
        // 귀여운 점 형태의 눈
        painter.setBrush(Qt::black);
        painter.drawEllipse(QPointF(-starSize/8, -starSize/8), 2.5, 2.5);
        painter.drawEllipse(QPointF(starSize/8, -starSize/8), 2.5, 2.5);
        
        // 간단한 U자형 미소
        painter.setPen(QPen(Qt::black, 2));
        QPainterPath smilePath;
        const qreal smileWidth = starSize/5;
        const qreal smileHeight = starSize/8;
        smilePath.moveTo(-smileWidth, 0);  // Y 위치를 0으로 조정
        smilePath.quadTo(0, smileHeight, smileWidth, 0);  // 제어점도 함께 조정
        painter.drawPath(smilePath);
        
        painter.restore();
    }
    
    // 플레이어 그리기 (원형)
    painter.setBrush(Qt::white);
    painter.setPen(Qt::white);
    painter.drawEllipse(player);
    
    // 장애물 그리기
    painter.setBrush(Qt::red);
    painter.setPen(Qt::red);
    for (const QRect &obstacle : obstacles) {
        painter.drawRect(obstacle);
    }
    
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
    QFont font("Arial", 12);
    painter.setFont(font);
    
    // 텍스트 너비 계산을 위한 QFontMetrics 사용
    QFontMetrics fm(font);
    QString scoreText = QString("Score: %1").arg(score);
    QString pitchText = QString("Pitch: %1").arg(currentPitch);
    QString volumeText = QString("Volume: %1").arg(QString::number(currentVolume, 'f', 2));
    
    // 오른쪽 여백 설정
    int rightMargin = 10;
    int topMargin = 25;
    int lineSpacing = 20;
    
    // 오른쪽 정렬하여 텍스트 그리기
    painter.drawText(width() - fm.horizontalAdvance(scoreText) - rightMargin, topMargin, scoreText);
    painter.drawText(width() - fm.horizontalAdvance(pitchText) - rightMargin, topMargin + lineSpacing, pitchText);
    painter.drawText(width() - fm.horizontalAdvance(volumeText) - rightMargin, topMargin + lineSpacing * 2, volumeText);
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
    
    // 장애물 이동
    for (int i = obstacles.size() - 1; i >= 0; --i) {
        if (i < obstacles.size()) {
            QRect &obstacle = obstacles[i];
            obstacle.translate(-3, 0); // 장애물이 왼쪽으로 이동
            
            // 화면 밖으로 나간 장애물 제거
            if (obstacle.x() + obstacle.width() < 0) {
                obstacles.removeAt(i);
                score++;
            }
        }
    }
    
    // 별 이동 및 충돌 검사
    const QRectF playerBounds(player.x() - 15, player.y() - 15, 30, 30);
    const int halfStarSize = starSize / 2;  // 미리 계산
    
    for (Star &star : stars) {
        if (!star.active) continue;
        
        // 화면 밖으로 나간 별은 즉시 비활성화
        if (star.pos.x() + halfStarSize < 0) {
            star.active = false;
            continue;
        }
        
        star.pos.setX(star.pos.x() - 3);  // 장애물과 같은 속도로 이동
        
        // 충돌 검사 최적화: 대략적인 거리 체크 먼저
        const qreal dx = qAbs(star.pos.x() - player.x());
        const qreal dy = qAbs(star.pos.y() - player.y());
        if (dx > starSize || dy > starSize) continue;  // 충돌 불가능
        
        // 정확한 충돌 검사
        QRectF starRect(star.pos.x() - halfStarSize, star.pos.y() - halfStarSize, starSize, starSize);
        if (playerBounds.intersects(starRect)) {
            star.active = false;
            score += 3;  // 별 획득 시 3점 추가
        }
    }
    
    // 충돌 검사
    if (checkCollision()) {
        gameOver();
        return;
    }
    
    // 비활성 별 정리 (stars 크기가 임계값을 초과할 때만)
    const int MAX_STARS = 25;  // 최대 별 개수
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
    
    int gapY = fixedGenerator.bounded(100, height() - 100);
    
    // 위쪽 장애물
    QRect topObstacle(width(), 0, OBSTACLE_WIDTH, gapY - OBSTACLE_GAP/2);
    obstacles.append(topObstacle);
    
    // 아래쪽 장애물
    QRect bottomObstacle(width(), gapY + OBSTACLE_GAP/2, OBSTACLE_WIDTH, height() - (gapY + OBSTACLE_GAP/2));
    obstacles.append(bottomObstacle);

    // 30% 확률로 별 생성 (고정된 시드값 사용)
    if (fixedGenerator.bounded(100) < 30) {
        // 별을 장애물 사이 공간의 중앙에 배치
        int starX = width() + OBSTACLE_WIDTH/2;
        int starY = gapY;
        stars.append(Star(QPointF(starX, starY)));
    }
    
    // 게임 상태 전송은 updateGame에서 주기적으로 처리
}

bool GameWindow::checkCollision()
{
    for (const QRect &obstacle : obstacles) {
        if (player.intersects(obstacle)) {
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
    
    GameOverDialog *dialog = new GameOverDialog(score, this);
    
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
    
    dialog->exec();
    dialog->deleteLater();
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