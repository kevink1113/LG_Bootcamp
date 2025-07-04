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

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    gameWindow(nullptr),
    rankingButton(nullptr),
    playerButton(nullptr),  // 플레이어 버튼 초기화
    rankingDialog(nullptr),
    playerDialog(nullptr),
    currentPlayerLabel(nullptr),  // 현재 플레이어 라벨 초기화
    backgroundMusicEnabled(true),  // 배경 음악 기본값은 켜기
    backgroundMusicProcess(nullptr), // 배경음악 프로세스 초기화
    volumeLevel(50),  // 볼륨 기본값 50%
    isCreatingGameWindow(false),
    gameWindowCreationTimer(nullptr)
{
    ui->setupUi(this);
    showFullScreen();  // 전체 화면으로 설정
    
    // 오디오 초기화
    initAudio();
    
    // 랭킹 버튼 생성
    rankingButton = new QPushButton(this);
    rankingButton->setFixedSize(70, 70);
    // trophy.png 아이콘을 랭킹 버튼에 적용 (가운데 정렬)
    QPixmap rankingPixmap("/mnt/nfs/trophy.png");
    QString rankingStyle =
        "QPushButton {"
        "   background-color: rgba(255, 255, 255, 180);"
        "   border: none;"
        "   border-radius: 10px;"
        "   padding: 5px;"
        "   padding: 3px; "
        "   font-weight: bold; "
        "   color: #FFB700; "
        "   border: 1px solid rgba(128, 64, 0, 0.3); "
        "}"
        "QPushButton:hover {"
        "   background-color: rgba(255, 255, 255, 220);"
        "}"
        "QPushButton:pressed {"
        "   background-color: rgba(200, 200, 200, 220);"
        "}";
    if (!rankingPixmap.isNull()) {
        QIcon rankingIcon(rankingPixmap.scaled(60, 60, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        rankingButton->setIcon(rankingIcon);
        rankingButton->setIconSize(QSize(60, 60));
        rankingButton->setText("");
        rankingButton->setStyleSheet(rankingStyle);
    } else {
        rankingButton->setText("");
        rankingButton->setStyleSheet(rankingStyle);
    }
    // 플레이어 버튼 생성
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
    // 버튼 스타일
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
    QString playerStyle = buttonStyle +
        "QPushButton { "
        "   padding: 3px; "
        "   font-weight: bold; "
        "   color: #2196F3; "
        "   border: 1px solid rgba(33, 150, 243, 0.3); "
        "}";
    playerButton->setStyleSheet(playerStyle);
    // rankingButton->setText("R"); // 중복 텍스트 제거
    // rankingButton->setFont(QFont("Arial", 22, QFont::Bold)); // 중복 폰트 제거
    QStyle *style = QApplication::style();
    QIcon personIcon = style->standardIcon(QStyle::SP_FileDialogDetailedView);
    playerButton->setIcon(personIcon);
    playerButton->setIconSize(QSize(30, 30));
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
    connect(rankingButton, &QPushButton::clicked, this, &MainWindow::showRankingDialog);
    connect(playerButton, &QPushButton::clicked, this, &MainWindow::showPlayerDialog);
    rankingButton->show();
    playerButton->show();
    updateButtonPositions();
    rankingButton->raise();
    playerButton->raise();
    // 설정 버튼 관련 코드 및 생성/연결/스타일 모두 삭제
    // gameWindowCreationTimer, 오디오, 플레이어 라벨 등 기존대로 유지
    QTimer::singleShot(100, this, &MainWindow::updateCurrentPlayerDisplay);
    if (backgroundMusicEnabled) {
        QTimer::singleShot(500, this, [this]() {
            controlBackgroundMusicProcess(true);
        });
    }
}

MainWindow::~MainWindow()
{
    if (gameWindowCreationTimer) {
        gameWindowCreationTimer->stop();
        gameWindowCreationTimer->deleteLater();
        gameWindowCreationTimer = nullptr;
    }
    if (backgroundMusicProcess) {
        backgroundMusicProcess->terminate();
        backgroundMusicProcess->waitForFinished(1000);
        delete backgroundMusicProcess;
        backgroundMusicProcess = nullptr;
        QProcess::execute("killall", QStringList() << "-9" << "aplay");
    }
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

void MainWindow::on_menuButton1_clicked()
{
    // 기존 게임 윈도우 정리
    if (gameWindow) {
        gameWindow->disconnect(); // 모든 시그널 연결 해제
        gameWindow->close();
        gameWindow->deleteLater();
        gameWindow = nullptr;
    }
    
    // 새 게임 윈도우 생성
    gameWindow = new GameWindow(nullptr, false); // 싱글플레이어 모드
    if (gameWindow) {
        gameWindow->setAttribute(Qt::WA_DeleteOnClose, false);
        connect(gameWindow, &GameWindow::requestMainWindow, this, [this]() {
            showFullScreen();
            raise();
            activateWindow();
        });
        gameWindow->show();
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
        
        // 메모리 정리
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
    // 기존 게임 윈도우 정리
    if (gameWindow) {
        gameWindow->disconnect(); // 모든 시그널 연결 해제
        gameWindow->close();
        gameWindow->deleteLater();
        gameWindow = nullptr;
    }

    // 새 게임 윈도우 생성
    gameWindow = new GameWindow(nullptr, true); // 멀티플레이어 모드
    if (gameWindow) {
        gameWindow->setAttribute(Qt::WA_DeleteOnClose, false);
        connect(gameWindow, &GameWindow::requestMainWindow, this, [this]() {
            showFullScreen();
            raise();
            activateWindow();
        });
        gameWindow->show();
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

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updateButtonPositions();
}

void MainWindow::updateButtonPositions()
{
    if (!rankingButton || !playerButton || !currentPlayerLabel) return;
    const int margin = 0;
    const int buttonSize = 70;
    const int spacing = 10;
    // 플레이어 버튼을 우측 상단(원래 설정 버튼 자리)에 배치
    playerButton->setGeometry(
        width() - buttonSize - margin,  // x (오른쪽)
        margin,                        // y (상단)
        buttonSize,                    // width
        buttonSize                     // height
    );
    // 랭킹 버튼을 플레이어 버튼 왼쪽에 배치
    rankingButton->setGeometry(
        width() - (2 * buttonSize) - margin - spacing,  // x
        margin,                                         // y
        buttonSize,                                     // width
        buttonSize                                      // height
    );
    // 현재 플레이어 라벨을 상단 중앙에 배치
    currentPlayerLabel->adjustSize();
    QSize labelSize = currentPlayerLabel->size();
    currentPlayerLabel->setGeometry(
        (width() - labelSize.width()) / 2,  // x (중앙)
        margin + buttonSize - 20,           // y (상단에서 약간 아래)
        labelSize.width(),                  // width
        labelSize.height()                  // height
    );
    currentPlayerLabel->raise();
    playerButton->raise();
    rankingButton->raise();
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


