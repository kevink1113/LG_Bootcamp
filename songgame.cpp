#include "songgame.h"
#include <QMessageBox>
#include <QPainter>
#include <QApplication>
#include <QDebug>
#include <QScreen>
#include <QStyle>
#include <QFile>
#include <QTextStream>
#include <cmath>

SongGame::SongGame(QWidget *parent)
    : QMainWindow(parent)
    , gameTimer(nullptr)
    , pitchTimer(nullptr)
    , countdownTimer(nullptr)
    , micProcess(nullptr)
    , soundProcess(nullptr)
    , backButton(nullptr)
    , gameRunning(false)
    , moveUp(false)
    , moveDown(false)
    , score(INITIAL_SCORE)
    , currentNoteIndex(0)
    , gameTime(0.0)
    , targetY(WINDOW_HEIGHT/2 - PLAYER_SIZE/2)
    , currentPitch(0)
    , currentVolume(0.0f)
    , currentPlayerName("")
    , lastSoundTime(0.0)
{
    qDebug() << "SongGame constructor called";
    
    // 초기화 과정에서 창이 보이지 않도록 숨김
    hide();
    
    // 생성자에서 바로 초기화하지 않고 이벤트 루프가 시작된 후 초기화
    QTimer::singleShot(100, this, [this]() {
        setupGame();
    });
}

SongGame::~SongGame()
{
    qDebug() << "SongGame destructor called";
    
    // 게임 상태 정지
    gameRunning = false;
    
    // 모든 시그널 연결 해제
    disconnect();
    
    // 타이머들 정리
    if (gameTimer) {
        gameTimer->stop();
        gameTimer->disconnect();
        delete gameTimer;
        gameTimer = nullptr;
    }
    if (pitchTimer) {
        pitchTimer->stop();
        pitchTimer->disconnect();
        delete pitchTimer;
        pitchTimer = nullptr;
    }
    if (countdownTimer) {
        countdownTimer->stop();
        countdownTimer->disconnect();
        delete countdownTimer;
        countdownTimer = nullptr;
    }
    
    // 마이크 프로세스 정리
    stopMicProcess();
    
    // 사운드 프로세스 정리
    if (soundProcess) {
        if (soundProcess->state() == QProcess::Running) {
            soundProcess->terminate();
            if (!soundProcess->waitForFinished(1000)) {
                soundProcess->kill();
                soundProcess->waitForFinished(500);
            }
        }
        delete soundProcess;
        soundProcess = nullptr;
    }
    
    // 버튼 정리
    if (backButton) {
        backButton->disconnect();
        delete backButton;
        backButton = nullptr;
    }
    
    // 이벤트 루프 처리
    QApplication::processEvents();
    
    qDebug() << "SongGame destructor completed";
}

void SongGame::setupGame()
{
    qDebug() << "Setting up song game window...";
    
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
        QCoreApplication::processEvents();
        
        // 3. raise/activateWindow는 show() 이후
        raise();
        activateWindow();
        
        qDebug() << "SongGame shown. Size:" << size();
        
        // 플레이어 위치 초기화
        player = QRect(50, height()/2 - PLAYER_SIZE/2, PLAYER_SIZE, PLAYER_SIZE);
        targetY = height()/2 - PLAYER_SIZE/2;
        
        // 노래 데이터 로드
        loadSongData();
        
        // 타이머 생성 및 연결
        if (!gameTimer) {
            gameTimer = new QTimer(this);
            if (gameTimer) {
                connect(gameTimer, &QTimer::timeout, this, &SongGame::updateGame);
                gameTimer->start(16); // 약 60 FPS
            }
        }
        
        if (!pitchTimer) {
            pitchTimer = new QTimer(this);
            if (pitchTimer) {
                connect(pitchTimer, &QTimer::timeout, this, &SongGame::readPitchData);
                pitchTimer->start(50); // 20Hz로 피치 읽기
            }
        }
        
        // 게임 상태 초기화
        gameRunning = true;
        score = INITIAL_SCORE;
        obstacles.clear();
        currentNoteIndex = 0;
        gameTime = 0.0;
        
        // 마이크 프로세스 시작
        startMicProcess();
        
        // 뒤로가기 버튼 설정
        if (!backButton) {
            setupBackButton();
        }
        
        // 초기 화면 그리기
        update();
        
    } catch (const std::exception& e) {
        qDebug() << "Exception in setupGame:" << e.what();
    } catch (...) {
        qDebug() << "Unknown exception in setupGame";
    }
}

void SongGame::loadSongData()
{
    songNotes = parseCSV("/mnt/nfs/애국가.csv");
    qDebug() << "Loaded" << songNotes.size() << "notes from song data";
    
    // 노트의 시작/끝 시간 계산
    double currentTime = 0.0;
    for (int i = 0; i < songNotes.size(); ++i) {
        songNotes[i].startTime = currentTime;
        currentTime += songNotes[i].beat * 0.3; // 박자를 0.3초 단위로 변환 (더 빠른 간격)
        songNotes[i].endTime = currentTime;
    }
}

QVector<NoteData> SongGame::parseCSV(const QString &filename)
{
    QVector<NoteData> notes;
    QFile file(filename);
    
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open CSV file:" << filename;
        return notes;
    }
    
    QTextStream in(&file);
    QString line = in.readLine(); // 헤더 라인 스킵
    
    while (!in.atEnd()) {
        line = in.readLine();
        QStringList parts = line.split(',');
        
        if (parts.size() >= 4) {
            NoteData note;
            note.lyric = parts[0].trimmed();
            note.beat = parts[1].toDouble();
            note.octave = parts[2].toInt();
            note.note = parts[3].trimmed();
            
            if (!note.note.isEmpty() && note.note != "-") {
                notes.append(note);
            }
        }
    }
    
    file.close();
    return notes;
}

int SongGame::noteToYPosition(const QString &note, int octave)
{
    // 애국가 CSV 파일의 음정을 직접 사용
    // 예: D2, G2, A2, B2, C3, D3, E3, F3, G3, A3, B3, C4, D4, E4, F4, G4, A4, B4, C5, D5, E5, F5, G5, A5
    
    // 음정과 옥타브를 점수로 변환 (간단한 방식)
    QMap<QString, int> noteValues;
    noteValues["C"] = 0;
    noteValues["C#"] = 1;
    noteValues["D"] = 2;
    noteValues["D#"] = 3;
    noteValues["E"] = 4;
    noteValues["F"] = 5;
    noteValues["F#"] = 6;
    noteValues["G"] = 7;
    noteValues["G#"] = 8;
    noteValues["A"] = 9;
    noteValues["A#"] = 10;
    noteValues["B"] = 11;
    
    // 옥타브별 오프셋
    int octaveOffset = (octave - 2) * 12; // 2옥타브를 기준으로
    int noteValue = noteValues.value(note, 0);
    int totalValue = octaveOffset + noteValue;
    
    // 전체 범위를 화면 높이로 매핑
    int maxValue = 36; // C5까지
    int minValue = 0;  // C2부터
    
    if (totalValue > maxValue) totalValue = maxValue;
    if (totalValue < minValue) totalValue = minValue;
    
    double normalizedValue = (double)(totalValue - minValue) / (maxValue - minValue);
    int y = (int)((1.0 - normalizedValue) * (height() - PLAYER_SIZE));
    
    // 디버그 출력 추가
    qDebug() << "Note:" << note << "Octave:" << octave << "TotalValue:" << totalValue << "Normalized:" << normalizedValue << "Y:" << y;
    
    return qBound(0, y, height() - PLAYER_SIZE);
}

void SongGame::createObstacleFromNote(const NoteData &note)
{
    int noteY = noteToYPosition(note.note, note.octave);
    
    // 장애물을 노트의 Y 위치에 맞춰 생성
    int gapY = noteY;
    int minGapY = OBSTACLE_GAP/2 + PLAYER_SIZE + 20;
    int maxGapY = height() - OBSTACLE_GAP/2 - PLAYER_SIZE - 20;
    
    // 범위 조정
    if (gapY < minGapY) gapY = minGapY;
    if (gapY > maxGapY) gapY = maxGapY;
    
    // 위쪽 장애물
    ObstacleData topObstacle;
    topObstacle.rect = QRect(width(), 0, OBSTACLE_WIDTH, gapY - OBSTACLE_GAP/2);
    topObstacle.lyric = note.lyric;
    topObstacle.note = note.note;
    topObstacle.octave = note.octave;
    obstacles.append(topObstacle);
    
    // 아래쪽 장애물
    ObstacleData bottomObstacle;
    bottomObstacle.rect = QRect(width(), gapY + OBSTACLE_GAP/2, OBSTACLE_WIDTH, height() - (gapY + OBSTACLE_GAP/2));
    bottomObstacle.lyric = note.lyric;
    bottomObstacle.note = note.note;
    bottomObstacle.octave = note.octave;
    obstacles.append(bottomObstacle);
    
    qDebug() << "Created obstacle for note:" << note.note << note.octave << "at Y:" << gapY << "Lyric:" << note.lyric;
}

void SongGame::updateGame()
{
    if (!gameRunning) return;
    
    // 게임 시간 업데이트
    gameTime += 0.016; // 16ms = 0.016초
    
    // 현재 시간에 맞는 노트 확인 및 장애물 생성
    while (currentNoteIndex < songNotes.size() && 
           songNotes[currentNoteIndex].startTime <= gameTime) {
        createObstacleFromNote(songNotes[currentNoteIndex]);
        currentNoteIndex++;
    }
    
    // 마이크 입력에 따른 플레이어 이동
    if (currentVolume > 0.1f) {
        int currentY = player.y();
        int dy = targetY - currentY;
        if (qAbs(dy) > 0) {
            player.translate(0, qBound(-PLAYER_SPEED, dy, PLAYER_SPEED));
        }
    }
    
    // 키보드 입력
    if (moveUp && player.y() > 0) {
        player.translate(0, -PLAYER_SPEED);
    }
    if (moveDown && player.y() < height() - PLAYER_SIZE) {
        player.translate(0, PLAYER_SPEED);
    }
    
    // 장애물 이동 및 제거
    const int leftBoundary = 0;
    for (int i = obstacles.size() - 1; i >= 0; --i) {
        ObstacleData &obstacle = obstacles[i];
        obstacle.rect.translate(-OBSTACLE_SPEED, 0);
        if (obstacle.rect.x() + obstacle.rect.width() < leftBoundary) {
            // 장애물이 화면에서 사라질 때 충돌 기록에서 제거
            collidedObstacles.remove(i);
            obstacles.removeAt(i);
        }
    }
    
    // 충돌 검사 (한 장애물에 한 번만)
    bool collisionOccurred = false;
    for (int i = 0; i < obstacles.size(); ++i) {
        if (!collidedObstacles.contains(i) && player.intersects(obstacles[i].rect)) {
            collidedObstacles.insert(i);
            collisionOccurred = true;
            break; // 한 번에 하나의 충돌만 처리
        }
    }
    
    if (collisionOccurred) {
        score -= PENALTY_PER_HIT;
        if (score < 0) {
            score = 0;
        }
        
        // 사운드 재생 제한 (0.5초마다 한 번씩만)
        if (gameTime - lastSoundTime > 0.5) {
            playSound("/mnt/nfs/wav/scratch.wav");
            lastSoundTime = gameTime;
        }
    }
    
    // 노래가 끝났는지 확인 (모든 노트가 처리되고 장애물이 화면에서 사라졌을 때)
    if (currentNoteIndex >= songNotes.size() && obstacles.isEmpty()) {
        gameOver();
        return;
    }
    
    // 화면 갱신
    update();
}

void SongGame::readPitchData()
{
    if (!gameRunning) return;
    
    // 파일에서 피치 데이터 읽기
    static QFile pitchFile("/tmp/pitch_score");
    
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
                        // 피치를 Y 좌표로 변환
                        static const int pitchRange = 37 - 1; // 1~37 범위
                        const float normalizedPitch = (currentPitch - 1.0f) / pitchRange;
                        targetY = (1.0f - normalizedPitch) * (height() - PLAYER_SIZE);
                        targetY = qBound(0.0, targetY, (double)(height() - PLAYER_SIZE));
                    }
                }
            }
        }
    }
}

void SongGame::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    
    // 배경 그리기
    static QPixmap bgPixmap;
    static QSize lastBgSize;
    if (bgPixmap.isNull() || lastBgSize != size()) {
        QPixmap rawBg;
        if (rawBg.load("/mnt/nfs/background.png")) {
            bgPixmap = rawBg.scaled(size(), Qt::IgnoreAspectRatio, Qt::FastTransformation);
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
    
    // 장애물 그리기
    static QPixmap pillarPixmap;
    if (pillarPixmap.isNull()) {
        pillarPixmap.load("/mnt/nfs/brick_pillar.png");
    }
    
    const int REAL_PILLAR_WIDTH = 100; // 실제 두께 증가
    for (const ObstacleData &obstacle : obstacles) {
        int h = obstacle.rect.height();
        int x = obstacle.rect.x() + (obstacle.rect.width() - REAL_PILLAR_WIDTH) / 2;
        int y = obstacle.rect.y();
        
        if (!pillarPixmap.isNull()) {
            QPixmap scaled = pillarPixmap.scaled(REAL_PILLAR_WIDTH, h, Qt::IgnoreAspectRatio, Qt::FastTransformation);
            painter.drawPixmap(x, y, REAL_PILLAR_WIDTH, h, scaled);
        } else {
            painter.setBrush(Qt::red);
            painter.setPen(Qt::NoPen);
            painter.drawRect(x, y, REAL_PILLAR_WIDTH, h);
        }
        
        // 장애물에 가사 표시 (충분히 큰 장애물에만 표시)
        if (h > 30) { // 더 작은 장애물에도 가사 표시
            painter.setPen(Qt::white);
            painter.setFont(QFont("Arial", 30, QFont::Bold));
            
            // 가사 텍스트
            QString lyricText = obstacle.lyric;
            if (!lyricText.isEmpty()) {
                QRect textRect(x, y + h/2 - 15, REAL_PILLAR_WIDTH, 30);
                painter.drawText(textRect, Qt::AlignCenter, lyricText);
            }
            
            // 음정 정보 (작은 글씨로)
            QString noteText = QString("%1%2").arg(obstacle.note).arg(obstacle.octave);
            painter.setFont(QFont("Arial", 20));
            QRect noteRect(x, y + h/2 + 15, REAL_PILLAR_WIDTH, 20);
            painter.drawText(noteRect, Qt::AlignCenter, noteText);
        }
    }
    
    // 플레이어 그리기
    painter.setBrush(Qt::white);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(player);
    
    // 텍스트 정보 표시
    static QFont infoFont("Arial", 12);
    painter.setFont(infoFont);
    painter.setPen(Qt::white);
    
    // 점수 표시
    QString scoreText = QString("Score: %1").arg(score);
    painter.drawText(10, 25, scoreText);
    
    // 현재 노트 정보 표시
    if (currentNoteIndex < songNotes.size()) {
        const NoteData &currentNote = songNotes[currentNoteIndex];
        QString noteText = QString("Current Note: %1%2").arg(currentNote.note).arg(currentNote.octave);
        painter.drawText(10, 45, noteText);
        QString lyricText = QString("Lyric: %1").arg(currentNote.lyric);
        painter.drawText(10, 65, lyricText);
    }
    
    // 게임 시간 표시
    QString timeText = QString("Time: %1").arg(QString::number(gameTime, 'f', 1));
    painter.drawText(10, 85, timeText);
    
    // 진행률 표시
    if (!songNotes.isEmpty()) {
        double progress = (double)currentNoteIndex / songNotes.size() * 100.0;
        QString progressText = QString("Progress: %1%").arg(QString::number(progress, 'f', 1));
        painter.drawText(10, 105, progressText);
    }
}

// bool SongGame::checkCollision()
// {
//     for (const ObstacleData &obstacle : obstacles) {
//         if (player.intersects(obstacle.rect)) {
//             return true;
//         }
//     }
//     return false;
// }

void SongGame::gameOver()
{
    gameRunning = false;
    
    stopMicProcess();
    
    if (gameTimer) {
        gameTimer->stop();
    }
    if (pitchTimer) {
        pitchTimer->stop();
    }
    
    // 게임 오버 메시지
    QString message;
    if (score > 0) {
        message = QString("🎵 노래 게임 완주! 🎵\n\n"
                         "최종 점수: %1점\n"
                         "애국가를 성공적으로 완주했습니다!\n\n"
                         "축하합니다! 🎉").arg(score);
    } else {
        message = QString("게임 종료\n\n"
                         "최종 점수: %1점\n"
                         "애국가를 완주했지만 점수가 부족합니다.\n\n"
                         "다시 도전해보세요! 💪").arg(score);
    }
    
    QMessageBox::information(this, "노래 게임 종료", message);
    
    // 메인 윈도우로 돌아가기
    close();
}

void SongGame::startMicProcess()
{
    if (micProcess) {
        stopMicProcess();
    }
    
    micProcess = new QProcess(this);
    QString workingDir = QApplication::applicationDirPath();
    micProcess->setWorkingDirectory(workingDir);
    qDebug() << "Starting mic process in directory:" << workingDir;
    micProcess->start("./mic", QStringList(), QIODevice::ReadWrite);
    
    if (!micProcess->waitForStarted(1000)) {
        qDebug() << "Creating default pitch_score file...";
        QFile defaultFile("/tmp/pitch_score");
        if (defaultFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&defaultFile);
            stream << "15 500.0\n";
            defaultFile.close();
        }
    }
    
    if (micProcess->waitForStarted()) {
        qDebug() << "Mic process started successfully";
    } else {
        qDebug() << "Mic not available, game will run with default values";
    }
}

void SongGame::stopMicProcess()
{
    if (micProcess) {
        micProcess->terminate();
        if (!micProcess->waitForFinished(3000)) {
            micProcess->kill();
        }
        delete micProcess;
        micProcess = nullptr;
    }
}

void SongGame::playSound(const QString &soundFile)
{
    // 기존 사운드 프로세스가 실행 중이면 강제 종료
    if (soundProcess) {
        if (soundProcess->state() == QProcess::Running) {
            soundProcess->terminate();
            if (!soundProcess->waitForFinished(500)) {
                soundProcess->kill();
                soundProcess->waitForFinished(100);
            }
        }
        delete soundProcess;
        soundProcess = nullptr;
    }
    
    // 새로운 사운드 프로세스 생성
    soundProcess = new QProcess(this);
    soundProcess->setProcessChannelMode(QProcess::MergedChannels);
    
    // 프로세스가 종료될 때 자동으로 정리되도록 설정
    connect(soundProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
        Q_UNUSED(exitCode)
        Q_UNUSED(exitStatus)
        if (soundProcess) {
            soundProcess->deleteLater();
            soundProcess = nullptr;
        }
    });
    
    // aplay 실행
    soundProcess->start("./aplay", QStringList() << "-Dhw:0,0" << soundFile);
    
    if (!soundProcess->waitForStarted(300)) {
        qDebug() << "Failed to play sound. Trying absolute path...";
        
        // 절대 경로로 시도
        soundProcess->start("/usr/bin/aplay", QStringList() << "-Dhw:0,0" << soundFile);
        
        if (!soundProcess->waitForStarted(300)) {
            qDebug() << "Failed to play sound with absolute path too.";
            delete soundProcess;
            soundProcess = nullptr;
        }
    }
}

void SongGame::keyPressEvent(QKeyEvent *event)
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

void SongGame::keyReleaseEvent(QKeyEvent *event)
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

void SongGame::setupBackButton()
{
    backButton = new QPushButton(this);
    backButton->setFixedSize(50, 50);
    backButton->move(10, 10);
    
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
    
    QStyle *style = QApplication::style();
    QIcon backIcon = style->standardIcon(QStyle::SP_ArrowBack);
    backButton->setIcon(backIcon);
    backButton->setIconSize(QSize(40, 40));
    connect(backButton, &QPushButton::clicked, this, &SongGame::goBackToMainWindow);
    backButton->show();
    backButton->raise();
}

void SongGame::goBackToMainWindow()
{
    qDebug() << "Going back to main window from song game";
    
    gameRunning = false;
    
    if (gameTimer) {
        gameTimer->stop();
    }
    if (pitchTimer) {
        pitchTimer->stop();
    }
    if (countdownTimer) {
        countdownTimer->stop();
    }
    
    close();
}

void SongGame::setCurrentPlayer(const QString &playerName)
{
    currentPlayerName = playerName;
} 