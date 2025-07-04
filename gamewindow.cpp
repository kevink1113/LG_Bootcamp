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

GameWindow::GameWindow(QWidget *parent)
    : QMainWindow(parent)
    , gameTimer(nullptr)
    , obstacleTimer(nullptr)
    , pitchTimer(nullptr)
    , micProcess(nullptr)
    , soundProcess(nullptr)  // 사운드 프로세스 초기화
    , pitchFile(nullptr)
    , backButton(nullptr)
    , playerSpeed(5)
    , score(0)
    , gameRunning(false)
    , moveUp(false)
    , moveDown(false)
    , currentPitch(0)
    , currentVolume(0.0f)
    , targetY(0)
{
    qDebug() << "GameWindow constructor called";
    // 초기화 과정에서 창이 보이지 않도록 숨김 (블랙화면 방지 위해 주석처리)
    // hide();
    // 생성자에서 바로 초기화하지 않고 이벤트 루프가 시작된 후 초기화
    QTimer::singleShot(50, this, &GameWindow::setupGame);
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
    
    update();
}

void GameWindow::spawnObstacles()
{
    if (!gameRunning) return;
    
    int gapY = QRandomGenerator::global()->bounded(100, height() - 100);
    
    // 위쪽 장애물
    QRect topObstacle(width(), 0, OBSTACLE_WIDTH, gapY - OBSTACLE_GAP/2);
    obstacles.append(topObstacle);
    
    // 아래쪽 장애물
    QRect bottomObstacle(width(), gapY + OBSTACLE_GAP/2, OBSTACLE_WIDTH, height() - (gapY + OBSTACLE_GAP/2));
    obstacles.append(bottomObstacle);

    // 30% 확률로 별 생성
    if (QRandomGenerator::global()->bounded(100) < 30) {
        // 별을 장애물 사이 공간의 중앙에 배치
        int starX = width() + OBSTACLE_WIDTH/2;
        int starY = gapY;
        stars.append(Star(QPointF(starX, starY)));
    }
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
    // 게임 중지
    gameRunning = false;
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
    
    // 메인 윈도우로 돌아가라는 시그널 발생
    emit requestMainWindow();
    
    // 게임 창 닫기
    close();
}

void GameWindow::setCurrentPlayer(const QString &playerName)
{
    currentPlayerName = playerName;
}

GameWindow::~GameWindow() {
    // Clean up resources if needed
    // All child QObjects with 'this' as parent are deleted automatically,
    // but we ensure any manual allocations are cleaned up.
    // (Most members are parented to 'this', so explicit deletion is not strictly necessary.)
    // If you add any new raw pointers, clean them up here.
}