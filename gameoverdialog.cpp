#include "gameoverdialog.h"
#include <QFont>

GameOverDialog::GameOverDialog(int score, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Game Over");
    setFixedSize(600, 400);  // 다이얼로그 크기 증가
    setupUI(score);
    
    // 다이얼로그를 화면 중앙에 위치
    setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
}

void GameOverDialog::setupUI(int score)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(30, 30, 30, 30);

    // Game Over 텍스트
    QLabel *gameOverLabel = new QLabel("Game Over", this);
    QFont titleFont("Arial", 24, QFont::Bold);
    gameOverLabel->setFont(titleFont);
    gameOverLabel->setAlignment(Qt::AlignCenter);

    // 점수 표시
    scoreLabel = new QLabel(QString("Score: %1").arg(score), this);
    QFont scoreFont("Arial", 18);
    scoreLabel->setFont(scoreFont);
    scoreLabel->setAlignment(Qt::AlignCenter);

    // 버튼 컨테이너
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);  // 버튼 간격 줄임

    // 버튼 스타일
    QString buttonStyle = 
        "QPushButton {"
        "   background-color: #2196F3;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 8px;"       // 모서리를 더 둥글게
        "   padding: 15px 30px;"       // 패딩 증가
        "   font-size: 18px;"          // 폰트 크기 증가
        "   font-weight: bold;"        // 굵은 글씨
        "}"
        "QPushButton:hover {"
        "   background-color: #1976D2;"
        "   transform: scale(1.05);"   // 호버 시 살짝 커지는 효과
        "}"
        "QPushButton:pressed {"
        "   background-color: #0D47A1;"
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
    connect(rankingButton, &QPushButton::clicked, this, &GameOverDialog::rankingRequested);

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

    // 레이아웃에 위젯 추가
    mainLayout->addWidget(gameOverLabel);
    mainLayout->addWidget(scoreLabel);
    mainLayout->addStretch(1);  // 상단 여백
    mainLayout->addLayout(buttonLayout);
    mainLayout->addStretch(1);  // 하단 여백 (버튼을 아래쪽으로)
}
