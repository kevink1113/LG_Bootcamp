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
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QFontDatabase> // QFontDatabase 추가

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
    , selectedSongIndex(0) // 기본값: 애국가
    , consecutivePerfect(0) // 연속 Perfect 횟수 초기화
    , lastFeedbackTime(0.0) // 마지막 피드백 시간 초기화
{
    qDebug() << "SongGame constructor called";
    
    // 노래 목록 초기화
    initializeSongs();
    
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
        
        // 타이머 생성 및 연결 (아직 시작하지 않음)
        if (!gameTimer) {
            gameTimer = new QTimer(this);
            if (gameTimer) {
                connect(gameTimer, &QTimer::timeout, this, &SongGame::updateGame);
            }
        }
        
        if (!pitchTimer) {
            pitchTimer = new QTimer(this);
            if (pitchTimer) {
                connect(pitchTimer, &QTimer::timeout, this, &SongGame::readPitchData);
            }
        }
        
        // 뒤로가기 버튼 설정
        if (!backButton) {
            setupBackButton();
        }
        
        // 노래 선택 다이얼로그 표시
        showSongSelectionDialog();
        
    } catch (const std::exception& e) {
        qDebug() << "Exception in setupGame:" << e.what();
    } catch (...) {
        qDebug() << "Unknown exception in setupGame";
    }
}

void SongGame::loadSongData()
{
    if (selectedSongIndex >= 0 && selectedSongIndex < availableSongs.size()) {
        const SongInfo &selectedSong = availableSongs[selectedSongIndex];
        songNotes = parseCSV(selectedSong.filename);
        qDebug() << "Loaded" << songNotes.size() << "notes from" << selectedSong.name;
    
    // 노트의 시작/끝 시간 계산
    double currentTime = 0.0;
    for (int i = 0; i < songNotes.size(); ++i) {
        songNotes[i].startTime = currentTime;
        currentTime += songNotes[i].beat * 0.2; // 박자를 0.15초 단위로 변환 (더 빠른 간격)
        songNotes[i].endTime = currentTime;
    }
    } else {
        qDebug() << "Invalid song index:" << selectedSongIndex;
        songNotes.clear();
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
    // 음정과 옥타브를 점수로 변환 (더 정확한 방식)
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
    
    // 옥타브별 오프셋 (더 넓은 범위)
    int octaveOffset = (octave - 2) * 12; // 2옥타브를 기준으로
    int noteValue = noteValues.value(note, 0);
    int totalValue = octaveOffset + noteValue;
    
    // 전체 범위를 화면 높이로 매핑 (더 넓은 범위 사용)
    int maxValue = 48; // C6까지 (더 높은 음)
    int minValue = -12; // C1까지 (더 낮은 음)
    
    if (totalValue > maxValue) totalValue = maxValue;
    if (totalValue < minValue) totalValue = minValue;
    
    // 화면 높이를 더 세밀하게 분할
    double normalizedValue = (double)(totalValue - minValue) / (maxValue - minValue);
    int y = (int)((1.0 - normalizedValue) * (height() - PLAYER_SIZE * 2));
    
    // 플레이어 크기를 고려한 여유 공간 확보
    y = qBound(PLAYER_SIZE, y, height() - PLAYER_SIZE * 2);
    
    return y;
}

void SongGame::addFeedback(const QString &message, const QPointF &position, const QColor &color, int fontSize)
{
    FeedbackData feedback(message, position, color, fontSize);
    feedback.startTime = gameTime;
    feedbacks.append(feedback);
    
    // 최대 5개의 피드백만 유지
    if (feedbacks.size() > 5) {
        feedbacks.removeFirst();
    }
}

void SongGame::updateFeedbacks()
{
    // 시간이 지난 피드백 제거
    for (int i = feedbacks.size() - 1; i >= 0; --i) {
        if (gameTime - feedbacks[i].startTime > feedbacks[i].duration) {
            feedbacks.removeAt(i);
        }
    }
}

void SongGame::checkPitchAccuracy()
{
    if (!gameRunning || currentNoteIndex >= songNotes.size()) return;
    
    const NoteData &currentNote = songNotes[currentNoteIndex];
    int targetY = noteToYPosition(currentNote.note, currentNote.octave);
    int playerY = player.y() + player.height() / 2;
    
    // 플레이어가 장애물을 통과하는 시점인지 확인
    bool passingObstacle = false;
    for (const ObstacleData &obstacle : obstacles) {
        // 플레이어가 장애물을 통과하는 순간 (플레이어가 장애물의 오른쪽 끝에 도달했을 때)
        if (player.x() >= obstacle.rect.x() + obstacle.rect.width() && 
            player.x() <= obstacle.rect.x() + obstacle.rect.width() + 10) {
            passingObstacle = true;
            break;
        }
    }
    
    if (!passingObstacle) return;
    
    // 장애물 통과 성공! (충돌하지 않고 통과했다는 것은 성공)
    consecutivePerfect++;
    
    // Perfect 보너스 점수 추가
    score += PERFECT_BONUS;
    
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
    lastFeedbackTime = gameTime;
}

void SongGame::clearFeedbacks()
{
    feedbacks.clear();
}

// 폰트 로딩을 위한 도우미 함수
QFont SongGame::loadSystemFont(const QString &fontName, int size, QFont::Weight weight)
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

void SongGame::initializeSongs()
{
    availableSongs.clear();
    
    // 애국가
    SongInfo anthem;
    anthem.name = "애국가";
    anthem.filename = "/mnt/nfs/애국가.csv";
    anthem.description = "대한민국의 국가";
    availableSongs.append(anthem);
    
    // 곰 세마리
    SongInfo bears;
    bears.name = "곰 세마리";
    bears.filename = "/mnt/nfs/song_bears.csv";
    bears.description = "전래동요";
    availableSongs.append(bears);
    
    // 나비야
    SongInfo butterfly;
    butterfly.name = "나비야";
    butterfly.filename = "/mnt/nfs/song_butterfly.csv";
    butterfly.description = "전래동요";
    availableSongs.append(butterfly);
    
    qDebug() << "Initialized" << availableSongs.size() << "songs";
}

void SongGame::showSongSelectionDialog()
{
    QDialog *songDialog = new QDialog(this);
    songDialog->setWindowTitle("노래 선택");
    songDialog->setFixedSize(400, 300);
    songDialog->setModal(true);
    
    QVBoxLayout *layout = new QVBoxLayout(songDialog);
    
    // 제목
    QLabel *titleLabel = new QLabel("🎵 노래를 선택하세요 🎵", songDialog);
    titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont("Arial", 16, QFont::Bold);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);
    
    // 노래 목록
    for (int i = 0; i < availableSongs.size(); ++i) {
        const SongInfo &song = availableSongs[i];
        
        QPushButton *songButton = new QPushButton(songDialog);
        songButton->setFixedHeight(60);
        songButton->setText(QString("%1\n%2").arg(song.name).arg(song.description));
        songButton->setFont(QFont("Arial", 12, QFont::Bold));
        
        // 현재 선택된 노래 강조
        if (i == selectedSongIndex) {
            songButton->setStyleSheet(
                "QPushButton {"
                "   background-color: #4CAF50;"
                "   color: white;"
                "   border: 2px solid #45a049;"
                "   border-radius: 10px;"
                "   padding: 10px;"
                "}"
                "QPushButton:hover {"
                "   background-color: #45a049;"
                "}"
            );
        } else {
            songButton->setStyleSheet(
                "QPushButton {"
                "   background-color: #f0f0f0;"
                "   color: black;"
                "   border: 2px solid #ddd;"
                "   border-radius: 10px;"
                "   padding: 10px;"
                "}"
                "QPushButton:hover {"
                "   background-color: #e0e0e0;"
                "}"
            );
        }
        
        // 버튼 클릭 이벤트 연결 - 노래 선택만 하고 다이얼로그는 닫지 않음
        connect(songButton, &QPushButton::clicked, this, [this, i, songDialog, songButton]() {
            selectSong(i);
            
            // 모든 버튼 스타일 초기화
            for (int j = 0; j < availableSongs.size(); ++j) {
                QPushButton *btn = songDialog->findChild<QPushButton*>(QString("songButton_%1").arg(j));
                if (btn) {
                    btn->setStyleSheet(
                        "QPushButton {"
                        "   background-color: #f0f0f0;"
                        "   color: black;"
                        "   border: 2px solid #ddd;"
                        "   border-radius: 10px;"
                        "   padding: 10px;"
                        "}"
                        "QPushButton:hover {"
                        "   background-color: #e0e0e0;"
                        "}"
                    );
                }
            }
            
            // 선택된 버튼만 강조
            songButton->setStyleSheet(
                "QPushButton {"
                "   background-color: #4CAF50;"
                "   color: white;"
                "   border: 2px solid #45a049;"
                "   border-radius: 10px;"
                "   padding: 10px;"
                "}"
                "QPushButton:hover {"
                "   background-color: #45a049;"
                "}"
            );
        });
        
        // 버튼에 고유한 이름 설정
        songButton->setObjectName(QString("songButton_%1").arg(i));
        
        layout->addWidget(songButton);
    }
    
    // 구분선
    QFrame *line = new QFrame(songDialog);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    layout->addWidget(line);
    
    // 버튼들
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    
    QPushButton *cancelButton = new QPushButton("취소", songDialog);
    QPushButton *startButton = new QPushButton("게임 시작", songDialog);
    
    cancelButton->setFont(QFont("Arial", 12));
    startButton->setFont(QFont("Arial", 12, QFont::Bold));
    startButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #2196F3;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 5px;"
        "   padding: 10px 20px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #1976D2;"
        "}"
    );
    
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(startButton);
    layout->addLayout(buttonLayout);
    
    // 버튼 연결
    connect(cancelButton, &QPushButton::clicked, songDialog, &QDialog::reject);
    connect(startButton, &QPushButton::clicked, songDialog, &QDialog::accept);
    
    // 다이얼로그 실행
    if (songDialog->exec() == QDialog::Accepted) {
        // 게임 시작
        loadSongData();
        // 게임 상태 초기화
        gameRunning = true;
        score = INITIAL_SCORE; // 100점에서 시작
        currentNoteIndex = 0;
        gameTime = 0.0;
        lastSoundTime = 0.0;
        obstacles.clear();
        collidedObstacles.clear();
        
        // 피드백 시스템 초기화
        consecutivePerfect = 0;
        lastFeedbackTime = 0.0;
        clearFeedbacks();
        
        // 타이머 시작
        if (gameTimer) gameTimer->start(8); // 120 FPS
        if (pitchTimer) pitchTimer->start(50); // 20Hz
        
        // 마이크 프로세스 시작
        startMicProcess();
        
        update();
    } else {
        // 취소 시 메인 윈도우로 돌아가기
        close();
    }
    
    songDialog->deleteLater();
}

void SongGame::selectSong(int index)
{
    if (index >= 0 && index < availableSongs.size()) {
        selectedSongIndex = index;
        qDebug() << "Selected song:" << availableSongs[index].name;
    }
}

void SongGame::createObstacleFromNote(const NoteData &note)
{
    int noteY = noteToYPosition(note.note, note.octave);
    
    // 장애물을 노트의 Y 위치에 맞춰 생성 (더 정확한 위치)
    int gapY = noteY;
    
    // 장애물 간격을 더 크게 조정하여 통과하기 쉽게
    int adjustedGap = OBSTACLE_GAP + 40; // 간격을 40px 늘림 (통과하기 쉽게)
    
    int minGapY = adjustedGap/2 + PLAYER_SIZE + 30;
    int maxGapY = height() - adjustedGap/2 - PLAYER_SIZE - 30;
    
    // 범위 조정
    if (gapY < minGapY) gapY = minGapY;
    if (gapY > maxGapY) gapY = maxGapY;
    
    // 위쪽 장애물
    ObstacleData topObstacle;
    topObstacle.rect = QRect(width(), 0, OBSTACLE_WIDTH, gapY - adjustedGap/2);
    topObstacle.lyric = note.lyric;
    topObstacle.note = note.note;
    topObstacle.octave = note.octave;
    obstacles.append(topObstacle);
    
    // 아래쪽 장애물
    ObstacleData bottomObstacle;
    bottomObstacle.rect = QRect(width(), gapY + adjustedGap/2, OBSTACLE_WIDTH, height() - (gapY + adjustedGap/2));
    bottomObstacle.lyric = note.lyric;
    bottomObstacle.note = note.note;
    bottomObstacle.octave = note.octave;
    obstacles.append(bottomObstacle);
}

void SongGame::updateGame()
{
    if (!gameRunning) return;
    
    // 게임 시간 업데이트
    gameTime += 0.008; // 8ms = 0.008초
    
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
    
    // 장애물 이동 및 충돌 검사
    for (int i = obstacles.size() - 1; i >= 0; --i) {
        ObstacleData &obstacle = obstacles[i];
        obstacle.rect.translate(-OBSTACLE_SPEED, 0);
        
        // 화면 밖으로 나간 장애물 제거
        if (obstacle.rect.x() + obstacle.rect.width() < 0) {
            obstacles.removeAt(i);
        }
    }
    
    // 피드백 시스템 업데이트
    updateFeedbacks();
    checkPitchAccuracy();
    
    // 충돌 검사
    for (const ObstacleData &obstacle : obstacles) {
        if (player.intersects(obstacle.rect)) {
        score -= PENALTY_PER_HIT;
        if (score < 0) {
            score = 0;
        }
            
            // 연속 Perfect 카운터 리셋
            consecutivePerfect = 0;
            
            // 기존 피드백 메시지 모두 제거
            clearFeedbacks();
            
            // 충돌 피드백 표시
            addFeedback("MISS!", QPointF(player.x() + 50, player.y() - 30), QColor(255, 0, 0), 20);
        
        // 사운드 재생 제한 (0.5초마다 한 번씩만)
        if (gameTime - lastSoundTime > 0.5) {
            playSound("/mnt/nfs/wav/scratch.wav");
            lastSoundTime = gameTime;
            }
            break;
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
    
    // 장애물 그리기 - 이미지 캐싱 최적화
    static QPixmap pillarPixmap;
    static QMap<int, QPixmap> scaledPillarCache; // 높이별 스케일된 이미지 캐싱
    
    if (pillarPixmap.isNull()) {
        pillarPixmap.load("/mnt/nfs/brick_pillar.png");
    }
    
    const int REAL_PILLAR_WIDTH = 100; // 실제 두께 증가
    
    // 화면에 보이는 장애물만 그리기 (성능 최적화)
    const int visibleMargin = 50; // 화면 밖 여유 공간
    for (const ObstacleData &obstacle : obstacles) {
        // 화면 밖의 장애물은 그리지 않음
        if (obstacle.rect.x() + obstacle.rect.width() < -visibleMargin || 
            obstacle.rect.x() > width() + visibleMargin) {
            continue;
        }
        
        int h = obstacle.rect.height();
        int x = obstacle.rect.x() + (obstacle.rect.width() - REAL_PILLAR_WIDTH) / 2;
        int y = obstacle.rect.y();
        
        if (!pillarPixmap.isNull()) {
            // 높이별 스케일된 이미지 캐싱
            QPixmap scaled;
            if (scaledPillarCache.contains(h)) {
                scaled = scaledPillarCache[h];
            } else {
                scaled = pillarPixmap.scaled(REAL_PILLAR_WIDTH, h, Qt::IgnoreAspectRatio, Qt::FastTransformation);
                scaledPillarCache[h] = scaled;
                
                // 캐시 크기 제한 (메모리 누수 방지)
                if (scaledPillarCache.size() > 50) {
                    scaledPillarCache.clear();
                }
            }
            painter.drawPixmap(x, y, REAL_PILLAR_WIDTH, h, scaled);
        } else {
            painter.setBrush(Qt::red);
            painter.setPen(Qt::NoPen);
            painter.drawRect(x, y, REAL_PILLAR_WIDTH, h);
        }
        
        // 장애물에 가사 표시 (충분히 큰 장애물에만 표시)
        if (h > 30) { // 더 작은 장애물에도 가사 표시
            // 음정 정보 (먼저 그리기 - 배경)
            QString noteText = QString("%1%2").arg(obstacle.note).arg(obstacle.octave);
            
            // 음정 배경 그리기 (먼저 그리기)
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0, 0, 0, 150)); // 반투명 검은색
            int bgWidth = REAL_PILLAR_WIDTH / 2; // 가로 길이를 절반으로 줄임
            int bgX = x + (REAL_PILLAR_WIDTH - bgWidth) / 2; // 중앙 정렬
            QRect noteBgRect(bgX, y + h/2 + 25, bgWidth, 25);
            painter.drawRoundedRect(noteBgRect, 5, 5);
            
            // 음정 텍스트 그리기
            painter.setPen(Qt::yellow); // 노란색으로 음정 표시
            painter.setFont(QFont("Arial", 18, QFont::Bold));
            QRect noteRect(bgX, y + h/2 + 30, bgWidth, 20);
            painter.drawText(noteRect, Qt::AlignCenter, noteText);
            
            // 가사 텍스트 (나중에 그리기 - 위에 표시)
            painter.setPen(Qt::white);
            painter.setFont(QFont("Arial", 30, QFont::Bold));
            
            QString lyricText = obstacle.lyric;
            if (!lyricText.isEmpty()) {
                QRect textRect(x, y + h/2 - 40, REAL_PILLAR_WIDTH, 40); // 높이를 40으로 증가
                painter.drawText(textRect, Qt::AlignCenter, lyricText);
            }
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
    painter.setPen(Qt::white);
    painter.setFont(loadSystemFont("CookieRun Bold", 16, QFont::Bold));
    painter.drawText(10, 30, QString("Score: %1").arg(score));
    
    // 피드백 메시지들 그리기
    for (const FeedbackData &feedback : feedbacks) {
        if (!feedback.active) continue;
        
        // 시간에 따른 투명도 계산
        double elapsed = gameTime - feedback.startTime;
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
    
    // 현재 노트 정보 표시
    if (currentNoteIndex < songNotes.size()) {
        const NoteData &currentNote = songNotes[currentNoteIndex];
        
        // 음정 정보 (더 큰 글씨로)
        QString noteText = QString("Note: %1%2").arg(currentNote.note).arg(currentNote.octave);
        painter.setFont(QFont("Arial", 14, QFont::Bold));
        painter.setPen(Qt::yellow);
        painter.drawText(10, 45, noteText);
        
        // 가사 정보
        QString lyricText = QString("Lyric: %1").arg(currentNote.lyric);
        painter.setFont(QFont("Arial", 12));
        painter.setPen(Qt::white);
        painter.drawText(10, 65, lyricText);
        
        // 다음 노트 정보 (미리보기)
        if (currentNoteIndex + 1 < songNotes.size()) {
            const NoteData &nextNote = songNotes[currentNoteIndex + 1];
            QString nextNoteText = QString("Next: %1%2").arg(nextNote.note).arg(nextNote.octave);
            painter.setFont(QFont("Arial", 10));
            painter.setPen(Qt::gray);
            painter.drawText(10, 85, nextNoteText);
        }
    }
    
    // 현재 노래 정보 표시
    if (selectedSongIndex >= 0 && selectedSongIndex < availableSongs.size()) {
        const SongInfo &currentSong = availableSongs[selectedSongIndex];
        QString songText = QString("Song: %1").arg(currentSong.name);
        painter.setFont(QFont("Arial", 12, QFont::Bold));
        painter.setPen(Qt::cyan);
        painter.drawText(10, 105, songText);
    }
    
    // 게임 시간 표시
    QString timeText = QString("Time: %1").arg(QString::number(gameTime, 'f', 1));
    painter.setFont(QFont("Arial", 12));
    painter.setPen(Qt::white);
    painter.drawText(10, 125, timeText);
    
    // 진행률 표시
    if (!songNotes.isEmpty()) {
        double progress = (double)currentNoteIndex / songNotes.size() * 100.0;
        QString progressText = QString("Progress: %1%").arg(QString::number(progress, 'f', 1));
        painter.drawText(10, 145, progressText);
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
    
    // 타이머 정지
    if (gameTimer) {
        gameTimer->stop();
    }
    if (pitchTimer) {
        pitchTimer->stop();
    }
    
    // 피드백 시스템 정리
    clearFeedbacks();
    
    // 게임 오버 다이얼로그 표시
    QString message;
    QString songName = availableSongs[selectedSongIndex].name;
    
    if (score >= 100) {
        message = QString("🎵 노래 게임 완주! 🎵\n\n"
                         "최종 점수: %1점\n"
                         "%2을(를) 성공적으로 완주했습니다!\n\n"
                         "축하합니다! 🎉").arg(score).arg(songName);
    } else if (score >= 80) {
        message = QString("🎵 노래 게임 완주! 🎵\n\n"
                         "최종 점수: %1점\n"
                         "%2을(를) 완주했습니다!\n\n"
                         "잘했어요! 👍").arg(score).arg(songName);
    } else if (score >= 60) {
        message = QString("🎵 노래 게임 완주! 🎵\n\n"
                         "최종 점수: %1점\n"
                         "%2을(를) 완주했습니다!\n\n"
                         "다음에는 더 잘할 수 있을 거예요! 💪").arg(score).arg(songName);
    } else {
        message = QString("게임 종료\n\n"
                         "최종 점수: %1점\n"
                         "%2을(를) 완주했지만 점수가 부족합니다.\n\n"
                         "다시 도전해보세요! 💪").arg(score).arg(songName);
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