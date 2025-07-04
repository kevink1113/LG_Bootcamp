#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QDebug>
#include <QSlider>
#include <QProcess>
#include <QCheckBox>
#include <QScreen>
#include <QApplication>
#include <QTimer>
#include <QStyle>
#include <QThread>
#include <QPainter>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    gameWindow(nullptr),
    settingsDialog(nullptr),
    volumeSlider(nullptr),
    rankingButton(nullptr),
    playerButton(nullptr),  // 플레이어 버튼 초기화
    rankingDialog(nullptr),
    playerDialog(nullptr),
    currentPlayerLabel(nullptr),  // 현재 플레이어 라벨 초기화
    backgroundMusicEnabled(true),  // 배경 음악 기본값은 켜기
    backgroundMusicProcess(nullptr), // 배경음악 프로세스 초기화
    volumeLevel(50),  // 볼륨 기본값 50%
    isCreatingGameWindow(false),
    gameWindowCreationTimer(nullptr),
    backgroundPixmap("/mnt/nfs/backgroundinit.png")
{
    ui->setupUi(this);
    showFullScreen();  // 전체 화면으로 설정
    
    // 오디오 초기화
    initAudio();
    
    // Ranking 버튼 생성
    rankingButton = new QPushButton(this);
    rankingButton->setFixedSize(70, 70);  // playerButton과 같은 크기로 변경
    // trophy.png 아이콘을 rankingButton에 적용 (가운데 정렬)
    QPixmap trophyPixmap("/mnt/nfs/trophy.png");
    if (!trophyPixmap.isNull()) {
        QIcon trophyIcon(trophyPixmap.scaled(70, 70, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        rankingButton->setIcon(trophyIcon);
        rankingButton->setIconSize(QSize(70, 70));
        rankingButton->setText("");
    } else {
        rankingButton->setIcon(QIcon());
        rankingButton->setText("");
    }
    
    // Player 버튼 생성
    playerButton = new QPushButton(this);
    playerButton->setFixedSize(70, 70);
    // playerset.png 아이콘을 playerButton에 적용 (가운데 정렬)
    QPixmap playerPixmap("/mnt/nfs/playerset.png");
    if (!playerPixmap.isNull()) {
        QIcon playerIcon(playerPixmap.scaled(60, 60, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        playerButton->setIcon(playerIcon);
        playerButton->setIconSize(QSize(60, 60));
        playerButton->setText("");
    } else {
        playerButton->setIcon(QIcon());
        playerButton->setText("");
    }
    
    // 기본 버튼 스타일 설정
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
    
    // 랭킹 버튼 스타일 (노란색 텍스트)
    QString rankingStyle = buttonStyle + 
        "QPushButton { "
        "   padding: 3px; "  // 패딩 조정하여 텍스트 위치 최적화
        "   font-weight: bold; "
        "   color: #FFB700; "  // 진한 황금색
        "   border: 1px solid rgba(128, 64, 0, 0.3); "  // 테두리로 그림자 효과 대체
        "}";
    
    // 플레이어 버튼 스타일 (파란색 계열)
    QString playerStyle = buttonStyle + 
        "QPushButton { "
        "   padding: 3px; "
        "   font-weight: bold; "
        "   color: #2196F3; "  // 파란색
        "   border: 1px solid rgba(33, 150, 243, 0.3); "
        "}";
    
    // 스타일 적용
    rankingButton->setStyleSheet(rankingStyle);      // 랭킹 버튼은 커스텀 스타일
    playerButton->setStyleSheet(playerStyle);        // 플레이어 버튼은 파란색 스타일
    
    // 랭킹 버튼 설정 - 아이콘만 표시, 텍스트 제거
    // rankingButton->setText("R");  // 텍스트 제거
    // rankingButton->setFont(QFont("Arial", 22, QFont::Bold));  // 폰트 설정 제거
    
    // 현재 플레이어 표시 라벨 생성
    currentPlayerLabel = new QLabel(this);
    currentPlayerLabel->setAlignment(Qt::AlignCenter);
    currentPlayerLabel->setStyleSheet(R"(
        QLabel {
            background-color: rgba(255, 255, 255, 200);
            border: 2px solid #2196F3;
            border-radius: 12px;
            padding: 8px 16px;
            font-size: 14pt;
            font-weight: bold;
            color: #2c3e50;
            min-width: 200px;
        }
    )");
    currentPlayerLabel->setText("No Player Selected");
    currentPlayerLabel->show();
    
    // 버튼 클릭 시그널 연결
    connect(rankingButton, &QPushButton::clicked, this, &MainWindow::showRankingDialog);
    connect(playerButton, &QPushButton::clicked, this, &MainWindow::showPlayerDialog);

    // 버튼 표시 및 정렬
    rankingButton->show();
    playerButton->show();
    updateButtonPositions();
    rankingButton->raise();       // 랭킹 버튼을 그 다음으로
    playerButton->raise();        // 플레이어 버튼을 그 다음으로
    
    // 게임 윈도우 생성 타이머 초기화
    gameWindowCreationTimer = new QTimer(this);
    gameWindowCreationTimer->setSingleShot(true);
    connect(gameWindowCreationTimer, &QTimer::timeout, this, &MainWindow::createNewGameWindow);
    
    createSettingsDialog();
    
    // 초기 플레이어 표시 업데이트 (지연 실행으로 모든 UI가 준비된 후 실행)
    QTimer::singleShot(100, this, &MainWindow::updateCurrentPlayerDisplay);
    
    // 배경 음악이 활성화되어 있으면 시작
    if (backgroundMusicEnabled) {
        QTimer::singleShot(500, this, [this]() {
            controlBackgroundMusicProcess(true);
        });
    }
}

MainWindow::~MainWindow()
{
    // 게임 윈도우 생성 타이머 정리
    if (gameWindowCreationTimer) {
        gameWindowCreationTimer->stop();
        gameWindowCreationTimer->deleteLater();
        gameWindowCreationTimer = nullptr;
    }
    
    // 배경 음악 프로세스 정리
    if (backgroundMusicProcess) {
        backgroundMusicProcess->terminate();
        backgroundMusicProcess->waitForFinished(1000);
        delete backgroundMusicProcess;
        backgroundMusicProcess = nullptr;
        
        // 실행 중인 aplay 프로세스 종료
        QProcess::execute("killall", QStringList() << "-9" << "aplay");
    }
    
    // 게임 윈도우 정리
    cleanupGameWindow();
    
    if (rankingDialog) {
        rankingDialog->close();
        rankingDialog->deleteLater();
        rankingDialog = nullptr;
    }
    
    if (playerDialog) {
        playerDialog->close();
        playerDialog->deleteLater();
        playerDialog = nullptr;
    }
    
    if (settingsDialog) {
        settingsDialog->close();
        settingsDialog->deleteLater();
        settingsDialog = nullptr;
    }
    
    // delete ui는 가장 마지막에 호출 (paintEvent 등에서 멤버 접근 방지)
    delete ui;
}

void MainWindow::initAudio()
{
    // 볼륨 기본값 설정
    volumeLevel = 50;
    
    // 초기 배경 음악 시작
    if (backgroundMusicEnabled) {
        QTimer::singleShot(500, this, [this]() {
            controlBackgroundMusicProcess(true);
        });
    }
}

void MainWindow::createSettingsDialog()
{
    settingsDialog = new QDialog(this);
    settingsDialog->setWindowTitle("Settings");
    settingsDialog->setFixedSize(500, 400);  // 크기를 더 크게 조정
    settingsDialog->setModal(true);  // 모달 다이얼로그로 설정
    settingsDialog->setWindowFlags(settingsDialog->windowFlags() | Qt::WindowStaysOnTopHint);  // 항상 위에 표시

    // 다이얼로그 스타일 설정 (테두리 추가)
    settingsDialog->setStyleSheet(R"(
        QDialog {
            background-color: white;
            border: 2px solid #3498db;
            border-radius: 10px;
        }
    )");

    QVBoxLayout *layout = new QVBoxLayout(settingsDialog);
    
    // 타이틀을 담을 컨테이너 위젯 생성
    QWidget *titleContainer = new QWidget(settingsDialog);
    QHBoxLayout *titleLayout = new QHBoxLayout(titleContainer);
    titleLayout->setContentsMargins(0, 0, 0, 20);  // 아래쪽 여백 추가
    
    QLabel *titleLabel = new QLabel("Settings", titleContainer);
    titleLabel->setStyleSheet("QLabel { font-size: 28pt; font-weight: bold; color: #2c3e50; }");  // 폰트 크기 키움
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();  // 오른쪽 여백을 위한 stretch 추가
    
    layout->addWidget(titleContainer);

    // 배경 음악 설정을 위한 레이아웃
    QWidget *bgmContainer = new QWidget(settingsDialog);
    QHBoxLayout *bgmLayout = new QHBoxLayout(bgmContainer);
    bgmLayout->setContentsMargins(0, 0, 0, 0);

    // 배경 음악 라벨
    QLabel *bgmLabel = new QLabel("Background Music:", settingsDialog);
    bgmLabel->setStyleSheet("font-size: 16pt; color: #2c3e50;");
    bgmLayout->addWidget(bgmLabel);

    // ON/OFF 버튼
    QPushButton *bgmToggleButton = new QPushButton(backgroundMusicEnabled ? "ON" : "OFF", settingsDialog);
    bgmToggleButton->setFixedWidth(100);
    bgmToggleButton->setStyleSheet(QString(R"(
        QPushButton {
            font-size: 16pt;
            font-weight: bold;
            padding: 5px;
            border-radius: 8px;
            background-color: %1;
            color: white;
        }
        QPushButton:hover {
            background-color: %2;
        }
        QPushButton:pressed {
            background-color: %3;
        }
    )").arg(backgroundMusicEnabled ? "#27ae60" : "#e74c3c",  // 켜져 있으면 초록색, 꺼져 있으면 빨간색
           backgroundMusicEnabled ? "#219653" : "#c0392b",  // 호버 색상
           backgroundMusicEnabled ? "#1e8449" : "#a93226")); // 눌렸을 때 색상

    bgmLayout->addWidget(bgmToggleButton);
    bgmLayout->addStretch();  // 오른쪽 공간 채우기
    
    layout->addWidget(bgmContainer);
    
    // 버튼 클릭 시 배경 음악 상태 토글
    connect(bgmToggleButton, &QPushButton::clicked, this, [this, bgmToggleButton]() {
        backgroundMusicEnabled = !backgroundMusicEnabled;
        
        // 버튼 텍스트 및 스타일 업데이트
        bgmToggleButton->setText(backgroundMusicEnabled ? "ON" : "OFF");
        bgmToggleButton->setStyleSheet(QString(R"(
            QPushButton {
                font-size: 16pt;
                font-weight: bold;
                padding: 5px;
                border-radius: 8px;
                background-color: %1;
                color: white;
            }
            QPushButton:hover {
                background-color: %2;
            }
            QPushButton:pressed {
                background-color: %3;
            }
        )").arg(backgroundMusicEnabled ? "#27ae60" : "#e74c3c",  // 켜져 있으면 초록색, 꺼져 있으면 빨간색
               backgroundMusicEnabled ? "#219653" : "#c0392b",  // 호버 색상
               backgroundMusicEnabled ? "#1e8449" : "#a93226")); // 눌렸을 때 색상
        
        // 실제 배경 음악 제어 코드 실행
        controlBackgroundMusicProcess(backgroundMusicEnabled);
        qDebug() << "Background music:" << (backgroundMusicEnabled ? "ON" : "OFF");
    });

    // 볼륨 라벨 및 슬라이더
    QLabel *volumeLabel = new QLabel("Volume", settingsDialog);
    volumeLabel->setStyleSheet("QLabel { font-size: 16pt; color: #2c3e50; margin-top: 15px; }");  // 폰트 크기 증가
    layout->addWidget(volumeLabel);

    volumeSlider = new QSlider(Qt::Horizontal, settingsDialog);
    volumeSlider->setMinimum(0);
    volumeSlider->setMaximum(100);
    volumeSlider->setValue(50);  // 기본값
    volumeSlider->setStyleSheet(R"(
        QSlider::groove:horizontal {
            border: 1px solid #999999;
            height: 12px;  /* 슬라이더 높이 증가 */
            background: #ffffff;
            margin: 2px 0;
            border-radius: 6px;
        }
        QSlider::handle:horizontal {
            background: #3498db;
            border: 2px solid #2980b9;
            width: 24px;  /* 핸들 크기 증가 */
            height: 24px;
            margin: -6px 0;
            border-radius: 12px;
        }
        QSlider::handle:horizontal:hover {
            background: #2980b9;
        }
    )");
    layout->addWidget(volumeSlider);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        Qt::Horizontal,
        settingsDialog
    );
    buttonBox->button(QDialogButtonBox::Ok)->setText("OK");
    buttonBox->button(QDialogButtonBox::Cancel)->setText("Cancel");
    
    // 버튼 스타일 설정 - 크기 증가
    buttonBox->setStyleSheet(R"(
        QPushButton {
            min-width: 100px;  /* 버튼 너비 증가 */
            min-height: 40px;  /* 버튼 높이 증가 */
            padding: 8px;
            background-color: #3498db;
            border: none;
            border-radius: 6px;
            color: white;
            font-weight: bold;
            font-size: 14pt;  /* 폰트 크기 증가 */
        }
        QPushButton:hover {
            background-color: #2980b9;
        }
        QPushButton:pressed {
            background-color: #21618c;
        }
        QPushButton[text="Cancel"] {
            background-color: #95a5a6;
        }
        QPushButton[text="Cancel"]:hover {
            background-color: #7f8c8d;
        }
        QPushButton[text="Cancel"]:pressed {
            background-color: #666e6f;
        }
    )");
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, settingsDialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, settingsDialog, &QDialog::reject);
    connect(volumeSlider, &QSlider::valueChanged, this, &MainWindow::onVolumeChanged);

    // 버튼박스 위에 추가 공간 확보
    layout->addStretch();
    
    // 여백 설정
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(20);
}

void MainWindow::onVolumeChanged(int value)
{
    // 볼륨 레벨 저장
    volumeLevel = value;
    
    // 시스템 볼륨도 함께 조절 (선택적)
#ifdef Q_OS_WIN
    // Windows 시스템에서의 볼륨 제어
    QString command = QString("powershell -c \"$volume = New-Object -ComObject WScript.Shell; $volume.SendKeys([char]0xAF); [System.Math]::Round(%1 * 65535 / 100)\"").arg(value);
    QProcess::startDetached("powershell", QStringList() << "-c" << command);
#elif defined(Q_OS_LINUX)
    // Linux 시스템에서의 볼륨 제어 (ALSA 사용)
    QString command = QString("amixer -D pulse sset Master %1%").arg(value);
    QProcess::startDetached("bash", QStringList() << "-c" << command);
#endif
    qDebug() << "Volume Set :" << value << "%";
}

void MainWindow::on_menuButton1_clicked()
{
    qDebug() << "Menu 1 clicked - Single Player";
    
    // 기존 게임 윈도우가 있다면 안전하게 정리
    if (gameWindow) {
        qDebug() << "Cleaning up existing game window...";
        gameWindow->disconnect();
        gameWindow->close();
        gameWindow->deleteLater();
        gameWindow = nullptr;
        QApplication::processEvents();
    }
    
    // 새 게임 윈도우 생성
    try {
        qDebug() << "Creating new single player game window...";
        gameWindow = new GameWindow(nullptr, false); // 싱글플레이어 모드
        
        if (gameWindow) {
            // 현재 플레이어 이름 설정
            if (playerDialog) {
                QString currentPlayer = playerDialog->getCurrentPlayer();
                gameWindow->setCurrentPlayer(currentPlayer);
            }
            
            // 메인 윈도우로 돌아가는 시그널 연결
            connect(gameWindow, &GameWindow::requestMainWindow, this, [this]() {
                qDebug() << "Returning to main window from single player";
                if (gameWindow) {
                    gameWindow->disconnect();
                    gameWindow->close();
                    gameWindow->deleteLater();
                    gameWindow = nullptr;
                }
                showFullScreen();
                raise();
                activateWindow();
            });
            
            // 게임 윈도우가 파괴될 때 정리 (gameWindow 접근 금지, nullptr만 대입)
            connect(gameWindow, &GameWindow::destroyed, this, [this]() {
                qDebug() << "Game window destroyed";
                gameWindow = nullptr;
            });
            
            qDebug() << "Single player game window created successfully";
        } else {
            qDebug() << "Failed to create game window!";
        }
    } catch (const std::exception& e) {
        qDebug() << "Exception creating game window:" << e.what();
        if (gameWindow) {
            gameWindow->deleteLater();
            gameWindow = nullptr;
        }
    } catch (...) {
        qDebug() << "Unknown exception creating game window";
        if (gameWindow) {
            gameWindow->deleteLater();
            gameWindow = nullptr;
        }
    }
}

void MainWindow::cleanupGameWindow()
{
    if (gameWindow) {
        qDebug() << "Cleaning up game window...";
        
        // 게임 윈도우의 모든 연결 해제
        gameWindow->disconnect();
        
        // 게임 윈도우 숨기기 및 정리
        gameWindow->hide();
        gameWindow->close();
        
        // deleteLater와 delete 혼용 방지, 즉시 삭제만 사용
        gameWindow->deleteLater();
        gameWindow = nullptr;
        
        qDebug() << "Game window cleanup completed";
    }
}

void MainWindow::createNewGameWindow()
{
    // 이중 생성 방지
    if (gameWindow != nullptr) {
        qDebug() << "Game window already exists, skipping creation";
        isCreatingGameWindow = false;
        return;
    }
    
    qDebug() << "Creating new game window...";
    
    // 메인 윈도우를 먼저 숨겨서 매끄러운 전환 효과
    hide();
    
    try {
        // 게임 윈도우를 독립적인 윈도우로 생성
        gameWindow = new GameWindow(nullptr);
        
        if (!gameWindow) {
            qDebug() << "Failed to create game window!";
            showFullScreen(); // 실패 시 메인 윈도우 다시 표시
            isCreatingGameWindow = false;
            return;
        }
        
        // 현재 플레이어 이름을 게임 윈도우에 설정
        QString currentPlayer = "";
        if (playerDialog) {
            currentPlayer = playerDialog->getCurrentPlayer();
        }
        gameWindow->setCurrentPlayer(currentPlayer);
        
        // 게임 윈도우가 닫힐 때의 정리 연결
        connect(gameWindow, &GameWindow::destroyed, this, [this]() {
            qDebug() << "Game window destroyed signal received";
            gameWindow = nullptr;
            isCreatingGameWindow = false;
        });
        
        // 게임 윈도우에서 메인 윈도우로 돌아가라는 시그널 연결
        connect(gameWindow, &GameWindow::requestMainWindow, this, [this]() {
            qDebug() << "Request main window signal received";
            // 즉시 메인 윈도우 표시
            showFullScreen();
            raise();
            activateWindow();
        });
        
        qDebug() << "Game window created and shown successfully";
        
    } catch (const std::exception& e) {
        qDebug() << "Exception while creating game window:" << e.what();
        if (gameWindow) {
            gameWindow->deleteLater();
            gameWindow = nullptr;
        }
        show(); // 실패 시 메인 윈도우 다시 표시
    } catch (...) {
        qDebug() << "Unknown exception while creating game window";
        if (gameWindow) {
            gameWindow->deleteLater();
            gameWindow = nullptr;
        }
        showFullScreen(); // 실패 시 메인 윈도우 다시 표시
    }
    
    isCreatingGameWindow = false;
}

void MainWindow::on_menuButton2_clicked()
{
    qDebug() << "Menu 2 clicked - Multiplayer";
    
    // 기존 게임 윈도우가 있다면 안전하게 정리
    if (gameWindow) {
        qDebug() << "Cleaning up existing game window...";
        gameWindow->disconnect();
        gameWindow->close();
        gameWindow->deleteLater();
        gameWindow = nullptr;
        QApplication::processEvents();
    }
    
    // 새 게임 윈도우 생성
    try {
        qDebug() << "Creating new multiplayer game window...";
        gameWindow = new GameWindow(nullptr, true); // 멀티플레이어 모드
        
        if (gameWindow) {
            // 현재 플레이어 이름 설정
            if (playerDialog) {
                QString currentPlayer = playerDialog->getCurrentPlayer();
                gameWindow->setCurrentPlayer(currentPlayer);
            }
            
            // 메인 윈도우로 돌아가는 시그널 연결
            connect(gameWindow, &GameWindow::requestMainWindow, this, [this]() {
                qDebug() << "Returning to main window from multiplayer";
                if (gameWindow) {
                    gameWindow->disconnect();
                    gameWindow->close();
                    gameWindow->deleteLater();
                    gameWindow = nullptr;
                }
                showFullScreen();
                raise();
                activateWindow();
            });
            
            // 게임 윈도우가 파괴될 때 정리
            connect(gameWindow, &GameWindow::destroyed, this, [this]() {
                qDebug() << "Game window destroyed";
                gameWindow = nullptr;
            });
            
            qDebug() << "Multiplayer game window created successfully";
        } else {
            qDebug() << "Failed to create game window!";
        }
    } catch (const std::exception& e) {
        qDebug() << "Exception creating game window:" << e.what();
        if (gameWindow) {
            gameWindow->deleteLater();
            gameWindow = nullptr;
        }
    } catch (...) {
        qDebug() << "Unknown exception creating game window";
        if (gameWindow) {
            gameWindow->deleteLater();
            gameWindow = nullptr;
        }
    }
}

void MainWindow::on_menuButton3_clicked()
{
    QMessageBox::information(this, "Menu 3", "Menu 3 was selected!");
}

void MainWindow::showRankingDialog()
{
    // 기존 다이얼로그가 있으면 삭제
    if (rankingDialog) {
        rankingDialog->deleteLater();
        rankingDialog = nullptr;
    }
    
    // 새 다이얼로그 생성
    rankingDialog = new RankingDialog(this);
    if (!rankingDialog) {
        qDebug() << "Failed to create ranking dialog!";
        return;
    }
    
    // 다이얼로그 표시
    rankingDialog->exec();
    
    // 실행 후 정리
    if (rankingDialog) {
        rankingDialog->deleteLater();
        rankingDialog = nullptr;
    }
}

void MainWindow::paintEvent(QPaintEvent *event)
{
    QMainWindow::paintEvent(event);
    if (!backgroundPixmap.isNull()) {
        QPainter painter(this);
        painter.drawPixmap(rect(), backgroundPixmap.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updateButtonPositions();
}

void MainWindow::updateButtonPositions()
{
    if (!rankingButton || !playerButton || !currentPlayerLabel) return;

    // 버튼 크기와 여백 설정
    const int margin = 0;       // 여백 제거하여 최상단에 배치
    const int buttonSize = playerButton->width();  // 두 버튼 모두 같은 크기(70)
    const int spacing = 10;     // 버튼 사이 간격

    // playerButton을 우측 상단에 배치
    playerButton->setGeometry(
        width() - buttonSize - margin, // x (우측)
        margin,                        // y (상단)
        buttonSize,                    // width
        buttonSize                     // height
    );

    // rankingButton을 playerButton 왼쪽에 같은 크기로 배치
    rankingButton->setGeometry(
        width() - (2 * buttonSize) - margin - spacing, // x
        margin,                                        // y
        buttonSize,                                    // width
        buttonSize                                     // height
    );
    
    // 현재 플레이어 라벨을 상단 중앙에 배치
    currentPlayerLabel->adjustSize();  // 내용에 맞춰 크기 조정
    QSize labelSize = currentPlayerLabel->size();
    currentPlayerLabel->setGeometry(
        (width() - labelSize.width()) / 2,  // x (중앙)
        margin + 40,                        // y (상단에서 약간 아래)
        labelSize.width(),                  // width
        labelSize.height()                  // height
    );
    
    // 버튼들을 부모 위젯의 스택 순서 최상위로 이동
    currentPlayerLabel->raise();             // 플레이어 라벨을 최상위로
    rankingButton->raise();                  // 랭킹 버튼을 그 다음으로
    playerButton->raise();                   // 플레이어 버튼을 그 다음으로
}

void MainWindow::showPlayerDialog()
{
    // PlayerDialog 인스턴스 생성 (한 번만 생성하여 재사용)
    if (!playerDialog) {
        playerDialog = new PlayerDialog(this);
        
        // 플레이어 변경 신호 연결
        connect(playerDialog, &PlayerDialog::currentPlayerChanged, 
                this, &MainWindow::updateCurrentPlayerDisplay);
    }
    
    // 다이얼로그 실행
    int result = playerDialog->exec();
    
    // 다이얼로그가 닫힌 후 플레이어 표시 업데이트
    if (result == QDialog::Accepted) {
        updateCurrentPlayerDisplay();
    }
}

void MainWindow::updateCurrentPlayerDisplay()
{
    if (!currentPlayerLabel || !playerDialog) return;
    
    QString currentPlayer = playerDialog->getCurrentPlayer();
    
    if (currentPlayer.isEmpty()) {
        currentPlayerLabel->setText("No Player Selected");
        currentPlayerLabel->setStyleSheet(R"(
            QLabel {
                background-color: rgba(255, 255, 255, 200);
                border: 2px solid #95a5a6;
                border-radius: 12px;
                padding: 8px 16px;
                font-size: 14pt;
                font-weight: bold;
                color: #7f8c8d;
                min-width: 200px;
            }
        )");
    } else {
        currentPlayerLabel->setText("Player : " + currentPlayer);
        currentPlayerLabel->setStyleSheet(R"(
            QLabel {
                background-color: rgba(46, 204, 113, 200);
                border: 2px solid #27ae60;
                border-radius: 12px;
                padding: 8px 16px;
                font-size: 14pt;
                font-weight: bold;
                color: white;
                min-width: 200px;
                text-shadow: 1px 1px 2px rgba(0, 0, 0, 0.3);
            }
        )");
    }
    
    // 라벨 크기가 변경되었을 수 있으므로 위치 재조정
    updateButtonPositions();
}

// 리눅스 명령어로 배경 음악 프로세스 제어
void MainWindow::controlBackgroundMusicProcess(bool start)
{
    qDebug() << "Background music control:" << (start ? "START" : "STOP");

    // 기존 프로세스 종료
    if (backgroundMusicProcess != nullptr) {
        qDebug() << "Terminating existing background music process...";
        backgroundMusicProcess->terminate();
        backgroundMusicProcess->waitForFinished(1000);
        delete backgroundMusicProcess;
        backgroundMusicProcess = nullptr;
        QProcess::execute("killall", QStringList() << "-9" << "aplay");
        qDebug() << "Background music process terminated.";
    }

    if (start) {
        qDebug() << "Starting background music...";
        backgroundMusicProcess = new QProcess(this);
        backgroundMusicProcess->start("./aplay", QStringList() << "-Dhw:0,0" << "/mnt/nfs/wav/background.wav");
        
        if (backgroundMusicProcess->waitForStarted(1000)) {
            qDebug() << "Background music started with aplay.";
        } else {
            qDebug() << "Failed to start background music. Trying absolute path...";
            delete backgroundMusicProcess;
            
            // 절대 경로로 시도
            backgroundMusicProcess = new QProcess(this);
            backgroundMusicProcess->start("/usr/bin/aplay", QStringList() << "-Dhw:0,0" << "/mnt/nfs/wav/background.wav");
            
            if (backgroundMusicProcess->waitForStarted(1000)) {
                qDebug() << "Background music started with absolute path aplay.";
            } else {
                qDebug() << "Failed to start background music:" << backgroundMusicProcess->errorString();
                delete backgroundMusicProcess;
                backgroundMusicProcess = nullptr;
            }
        }
    } else {
        qDebug() << "Background music disabled.";
    }
}


