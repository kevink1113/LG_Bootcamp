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

GameWindow::GameWindow(QWidget *parent)
    : QMainWindow(parent)
    , gameTimer(nullptr)
    , obstacleTimer(nullptr)
    , pitchTimer(nullptr)
    , micProcess(nullptr)
    , pitchFile(nullptr)
    , backButton(nullptr)
    , playerSpeed(5)
    , score(0)
    , gameRunning(false)
    , moveUp(false)
    , moveDown(false)
    , currentPitch(0)
    , currentVolume(0.0f)
    , targetY(0)  // setupGame에서 올바른 값으로 설정됨
{
    // 부모 위젯이 설정된 후에 게임 초기화
    QTimer::singleShot(0, this, &GameWindow::setupGame);
}

GameWindow::~GameWindow()
{
    gameRunning = false;
    
    stopMicProcess();
    
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
}

void GameWindow::setupGame()
{
    qDebug() << "Setting up game window...";
    
    // 현재 화면 크기 가져오기
    QScreen *screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    
    // 게임 윈도우 설정
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    
    // 창 크기와 위치를 화면에 맞게 설정
    setGeometry(screenGeometry);
    
    // 기본 설정 사용
    
    // 윈도우를 보이게 하고 전체화면으로 설정
    show();
    setWindowState(Qt::WindowFullScreen);
    raise();
    activateWindow();
    
    // 창 크기를 화면 크기로 설정
    setGeometry(screenGeometry);
    
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
        obstacleTimer->start(2000); // 2초마다 장애물 생성
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
    
    // 뒤로가기 버튼 설정
    setupBackButton();
    
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
    
    // 화면 갱신
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
    
    GameOverDialog *dialog = new GameOverDialog(score, this);
    
    connect(dialog, &GameOverDialog::mainMenuRequested, this, [this]() {
        QWidget* mainWindow = parentWidget();
        if (mainWindow) {
            mainWindow->showNormal();
            mainWindow->activateWindow();
            mainWindow->raise();
        }
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
    
    // MainWindow로 돌아가기
    QWidget* mainWindow = parentWidget();
    if (mainWindow) {
        mainWindow->showNormal();  // 창 모드로 표시
        mainWindow->activateWindow();  // 창을 활성화
        mainWindow->raise();  // 창을 맨 앞으로
    }
    close();  // 게임 창 닫기
}