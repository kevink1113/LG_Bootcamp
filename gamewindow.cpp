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

    // paintEvent에서는 별 생성하지 않음 - spawnObstacles에서 처리

    // 별 그리기 (얼굴 포함)
    painter.setPen(QPen(Qt::black, 2));
    for (Star& star : stars) {
        if (!star.active) continue;
        
        // 단순한 별 그리기
        painter.setBrush(Qt::yellow);
        painter.setPen(Qt::yellow);
        QRectF starRect(star.pos.x() - starSize/2, star.pos.y() - starSize/2, 
                       starSize, starSize);
        painter.drawEllipse(starRect);
    }
    
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
    painter.drawText(10, 65, QString("Volume: %1").arg(QString::number(currentVolume, 'f', 2)));
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
    QRectF playerBounds(player.x() - 20, player.y() - 20, 40, 40);
    
    for (Star &star : stars) {
        if (!star.active) continue;
        
        star.pos.setX(star.pos.x() - 3);  // 장애물과 같은 속도로 이동
        
        // 별과 플레이어의 충돌 검사
        QRectF starRect(star.pos.x() - starSize/2, star.pos.y() - starSize/2, starSize, starSize);
        if (playerBounds.intersects(starRect)) {
            star.active = false;
            score += 3;  // 별 획득 시 3점 추가
            continue;
        }
        
        // 화면 밖으로 나간 별 제거
        if (star.pos.x() + starSize < 0) {
            star.active = false;
        }
    }
    
    // 충돌 검사
    if (checkCollision()) {
        gameOver();
        return;
    }
    
    // 비활성 별 주기적으로 정리 (매 10프레임마다)
    static int cleanupCounter = 0;
    if (++cleanupCounter >= 10) {
        cleanupCounter = 0;
        if (stars.size() > 20) {
            for (int i = stars.size() - 1; i >= 0; --i) {
                if (!stars[i].active) {
                    stars.removeAt(i);
                }
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

    // 50% 확률로 별 생성
    if (QRandomGenerator::global()->bounded(2) == 0) {
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