#include "gameoverdialog.h"
#include <QFont>

GameOverDialog::GameOverDialog(int score, const QString &playerName, QWidget *parent)
    : QDialog(parent), currentScore(score), scoreAdded(false), rankingDialog(nullptr)
{
    setWindowTitle("Game Over");
    setFixedSize(600, 400);  // 다이얼로그 크기 증가
    setupUI(score, playerName);
    
    // 다이얼로그를 화면 중앙에 위치하되, 비모달로 설정
    setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
    setModal(false);  // 비모달로 설정하여 부모 윈도우 이벤트 허용
    
    // 다이얼로그 전체 스타일 설정
    setStyleSheet(R"(
        QDialog {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                      stop:0 #f8f9fa, stop:1 #e9ecef);
            border: 3px solid #2196F3;
            border-radius: 20px;
        }
        QLabel {
            color: #2c3e50;
            background: transparent;
        }
    )");
}

void GameOverDialog::setupUI(int score, const QString &playerName)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(30, 30, 30, 30);

    // Game Over 제목과 느낌표 아이콘을 담을 수평 레이아웃
    QHBoxLayout *titleLayout = new QHBoxLayout();
    titleLayout->setAlignment(Qt::AlignCenter);
    titleLayout->setSpacing(15);  // 제목과 아이콘 사이 간격

    // 느낌표 아이콘 추가 (왼쪽에 배치)
    QLabel *exclamationIcon = new QLabel(this);
    exclamationIcon->setFixedSize(40, 40);  // 아이콘 크기 설정
    exclamationIcon->setStyleSheet(R"(
        QLabel {
            background-color: #E74C3C;
            border-radius: 20px;
            color: white;
            font-weight: bold;
            font-size: 26px;
            font-family: 'Comic Sans MS', 'Segoe UI', sans-serif;
        }
    )");
    exclamationIcon->setText("!");
    exclamationIcon->setAlignment(Qt::AlignCenter);

    // Game Over 텍스트
    QLabel *gameOverLabel = new QLabel("Game Over", this);
    QFont titleFont("Comic Sans MS", 28, QFont::Bold);  // 둥근 폰트로 변경하고 크기도 증가
    gameOverLabel->setFont(titleFont);
    gameOverLabel->setAlignment(Qt::AlignCenter);
    gameOverLabel->setStyleSheet(R"(
        QLabel {
            color: #2c3e50;
            background: transparent;
        }
    )");

    // 제목 레이아웃에 아이콘과 라벨 추가 (순서 변경: 아이콘이 먼저, 텍스트가 나중에)
    titleLayout->addWidget(exclamationIcon);
    titleLayout->addWidget(gameOverLabel);

    // 메인 레이아웃에 제목 레이아웃 추가
    mainLayout->addLayout(titleLayout);

    // 플레이어 표시 (제목 아래)
    playerLabel = new QLabel(this);
    if (playerName.isEmpty()) {
        playerLabel->setText("Player: No Player Selected");
    } else {
        playerLabel->setText(QString("Player: %1").arg(playerName));
    }
    QFont playerFont("Comic Sans MS", 18);
    playerLabel->setFont(playerFont);
    playerLabel->setAlignment(Qt::AlignCenter);
    playerLabel->setStyleSheet(R"(
        QLabel {
            color: #6c757d;
            background: transparent;
        }
    )");

    // 버튼 컨테이너
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);  // 버튼 간격 줄임

    // 점수 표시 (버튼 위쪽)
    scoreLabel = new QLabel(QString("Score: %1").arg(score), this);
    QFont scoreFont("Comic Sans MS", 22);  // 점수 폰트 크기 증가
    scoreLabel->setFont(scoreFont);
    scoreLabel->setAlignment(Qt::AlignCenter);
    scoreLabel->setStyleSheet(R"(
        QLabel {
            color: #e74c3c;
            background: transparent;
            font-weight: bold;
        }
    )");

    // 버튼 스타일
    QString buttonStyle = 
        "QPushButton {"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "                               stop:0 #4FC3F7, stop:1 #2196F3);"
        "   color: white;"
        "   border: 2px solid #1976D2;"
        "   border-radius: 18px;"       // 더욱 둥글게
        "   padding: 15px 30px;"        // 패딩 증가
        "   font-size: 24px;"           // 폰트 크기 증가
        "   font-weight: bold;"         // 굵은 글씨
        "   font-family: 'Comic Sans MS', 'Segoe UI', sans-serif;"  // 둥근 폰트
        "}"
        "QPushButton:hover {"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "                               stop:0 #42A5F5, stop:1 #1976D2);"
        "   border: 2px solid #0D47A1;"
        "}"
        "QPushButton:pressed {"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "                               stop:0 #1976D2, stop:1 #0D47A1);"
        "}";

    // Main 버튼
    mainButton = new QPushButton("Main", this);
    mainButton->setStyleSheet(buttonStyle);
    mainButton->setFixedSize(180, 70);  // 버튼 크기 증가
    connect(mainButton, &QPushButton::clicked, this, &GameOverDialog::mainMenuRequested);
    connect(mainButton, &QPushButton::clicked, this, &GameOverDialog::accept);

    // Ranking 버튼
    rankingButton = new QPushButton("Ranking", this);
    rankingButton->setStyleSheet(buttonStyle);
    rankingButton->setFixedSize(180, 70);  // 버튼 크기 증가
    connect(rankingButton, &QPushButton::clicked, this, &GameOverDialog::onRankingButtonClicked);

    // Restart 버튼
    restartButton = new QPushButton("Restart", this);
    restartButton->setStyleSheet(buttonStyle);
    restartButton->setFixedSize(180, 70);  // 버튼 크기 증가
    connect(restartButton, &QPushButton::clicked, this, &GameOverDialog::restartRequested);
    connect(restartButton, &QPushButton::clicked, this, &GameOverDialog::accept);

    // 버튼 추가
    buttonLayout->addWidget(mainButton);
    buttonLayout->addWidget(rankingButton);
    buttonLayout->addWidget(restartButton);

    // 레이아웃에 위젯 추가 (동일한 간격으로)
    // titleLayout은 이미 추가되었음 (Game Over 제목과 느낌표 아이콘 포함)
    mainLayout->addSpacing(30);              // 2. 제목과 플레이어 사이 간격
    mainLayout->addWidget(playerLabel);      // 3. "Player: 플레이어명"
    mainLayout->addSpacing(30);              // 4. 플레이어와 점수 사이 간격
    mainLayout->addWidget(scoreLabel);       // 5. "Score: XXX"
    mainLayout->addSpacing(30);              // 6. 점수와 버튼 사이 간격
    mainLayout->addLayout(buttonLayout);     // 7. 버튼들 (Main, Ranking, Restart)
    mainLayout->addStretch(1);               // 8. 하단 여백 (남은 공간)
}

void GameOverDialog::onRankingButtonClicked()
{
    // 기존 다이얼로그가 있으면 삭제
    if (rankingDialog) {
        rankingDialog->deleteLater();
        rankingDialog = nullptr;
    }
    
    // 점수가 아직 추가되지 않았다면 점수와 함께 랭킹 다이얼로그 생성
    // 이미 추가되었다면 점수 없이 랭킹만 표시
    if (!scoreAdded) {
        rankingDialog = new RankingDialog(currentScore, this);
        scoreAdded = true;  // 점수가 추가되었음을 표시
    } else {
        rankingDialog = new RankingDialog(this);  // 점수 추가 없이 랭킹만 표시
    }
    
    if (rankingDialog) {
        rankingDialog->exec();
        
        // 실행 후 정리
        rankingDialog->deleteLater();
        rankingDialog = nullptr;
    }
}
