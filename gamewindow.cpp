#include "gamewindow.h"
#include <QMessageBox>
#include <QPainter>
#include <QRandomGenerator>
#include <QApplication>
#include <QDebug>

GameWindow::GameWindow(QWidget *parent)
    : QMainWindow(parent)
    , gameTimer(nullptr)
    , obstacleTimer(nullptr)
    , pitchTimer(nullptr)
    , micProcess(nullptr)
    , pitchFile(nullptr)
    , playerSpeed(5)
    , score(0)
    , gameRunning(false)
    , moveUp(false)
    , moveDown(false)
    , currentPitch(0)
    , currentVolume(0.0f)
    , targetY(WINDOW_HEIGHT/2 - PLAYER_SIZE/2)
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
    // 윈도우 설정
    setFixedSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    setWindowTitle("Voice Control Game");
    
    // 플레이어 초기화
    player = QRect(50, WINDOW_HEIGHT/2 - PLAYER_SIZE/2, PLAYER_SIZE, PLAYER_SIZE);
    targetY = WINDOW_HEIGHT/2 - PLAYER_SIZE/2;
    
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
    
    // 마이크 프로세스 시작
    startMicProcess();
    
    // 초기 화면 그리기
    update();
}

void GameWindow::startMicProcess()
{
    if (micProcess) {
        stopMicProcess();
    }
    
    micProcess = new QProcess(this);
    micProcess->setWorkingDirectory("/mnt/nfs");
    micProcess->start("./mic");
    
    if (micProcess->waitForStarted()) {
        qDebug() << "Mic process started successfully";
    } else {
        qDebug() << "Failed to start mic process:" << micProcess->errorString();
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
                        targetY = (1.0f - normalizedPitch) * (WINDOW_HEIGHT - PLAYER_SIZE);
                        targetY = qBound(0, targetY, WINDOW_HEIGHT - PLAYER_SIZE);
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
    
    // 플레이어 그리기
    painter.setBrush(Qt::white);
    painter.setPen(Qt::white);
    painter.drawEllipse(player);
    
    // 장애물들 그리기
    painter.setBrush(Qt::red);
    painter.setPen(Qt::red);
    for (const QRect &obstacle : obstacles) {
        painter.drawRect(obstacle);
    }
    
    // 점수와 피치 정보 표시
    painter.setPen(Qt::white);
    QFont font("Arial", 12);
    painter.setFont(font);
    painter.drawText(10, 25, QString("Score: %1").arg(score));
    painter.drawText(10, 45, QString("Pitch: %1").arg(currentPitch));
    painter.drawText(10, 65, QString("Volume: %.2f").arg(currentVolume));
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
    if (moveDown && player.y() < WINDOW_HEIGHT - PLAYER_SIZE) {
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
    
    // 충돌 검사
    if (checkCollision()) {
        gameOver();
        return;
    }
    
    // 화면 갱신
    update();
}

void GameWindow::spawnObstacles()
{
    if (!gameRunning) return;
    
    int gapY = QRandomGenerator::global()->bounded(100, WINDOW_HEIGHT - 100);
    
    // 위쪽 장애물
    QRect topObstacle(WINDOW_WIDTH, 0, OBSTACLE_WIDTH, gapY - OBSTACLE_GAP/2);
    obstacles.append(topObstacle);
    
    // 아래쪽 장애물
    QRect bottomObstacle(WINDOW_WIDTH, gapY + OBSTACLE_GAP/2, OBSTACLE_WIDTH, WINDOW_HEIGHT - (gapY + OBSTACLE_GAP/2));
    obstacles.append(bottomObstacle);
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
    
    QMessageBox::information(this, "Game Over", 
                           QString("Game Over!\nScore: %1").arg(score));
    
    close();
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