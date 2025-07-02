#include "gameoverdialog.h"
#include <QFont>

GameOverDialog::GameOverDialog(int score, QWidget *parent)
    : QDialog(parent), currentScore(score), scoreAdded(false), rankingDialog(nullptr)
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

    // 레이아웃에 위젯 추가
    mainLayout->addWidget(gameOverLabel);
    mainLayout->addWidget(scoreLabel);
    mainLayout->addStretch(1);  // 상단 여백
    mainLayout->addLayout(buttonLayout);
    mainLayout->addStretch(1);  // 하단 여백 (버튼을 아래쪽으로)
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
