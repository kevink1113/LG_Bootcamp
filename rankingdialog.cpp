#include "rankingdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QScreen>
#include <QApplication>
#include <algorithm>

// 전역 정적 변수로 랭킹 데이터 유지
static QList<RankingRecord> globalRankings;

RankingDialog::RankingDialog(QWidget *parent)
    : QDialog(parent), titleLabel(nullptr), closeButton(nullptr), rankingListWidget(nullptr)
{
    loadRankings();
    setupUI();
}

RankingDialog::RankingDialog(int newScore, QWidget *parent)
    : QDialog(parent), titleLabel(nullptr), closeButton(nullptr), rankingListWidget(nullptr)
{
    loadRankings();
    addScore(newScore);
    setupUI();
}

RankingDialog::~RankingDialog()
{
    saveRankings();
}

void RankingDialog::setupUI()
{
    // 다이얼로그 크기 설정 (화면에 맞게 축소)
    setFixedSize(500, 600);
    
    // 타이틀바 숨기기 및 모달 설정
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setModal(true);
    
    // 전체 배경 스타일 설정
    setStyleSheet(R"(
        RankingDialog {
            background-color: white;
            border: 2px solid #3498db;
            border-radius: 15px;
        }
    )");
    
    // 메인 레이아웃
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);  // 간격 줄임
    mainLayout->setContentsMargins(15, 15, 15, 15);  // 여백 줄임

    // 상단 헤더 레이아웃
    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    
    // 제목 레이블 (크기 축소)
    titleLabel = new QLabel("RANKING", this);
    QFont titleFont("Arial", 28, QFont::ExtraBold);  // 폰트 크기 축소
    titleLabel->setFont(titleFont);
    titleLabel->setStyleSheet("color: #2c3e50; padding: 5px;");  // 패딩 축소
    titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    
    // 닫기 버튼 (크기 축소)
    closeButton = new QPushButton("×", this);
    closeButton->setFixedSize(35, 35);  // 크기 축소
    closeButton->setCursor(Qt::PointingHandCursor);
    closeButton->setStyleSheet(R"(
        QPushButton {
            background-color: transparent;
            border: none;
            color: #2c3e50;
            font-family: Arial;
            font-size: 28px;  
            font-weight: bold;
            padding: 0;
            margin: 0;
        }
        QPushButton:hover {
            color: #e74c3c;
        }
    )");
    
    // 헤더 레이아웃에 위젯 추가
    headerLayout->addWidget(titleLabel, 1);
    headerLayout->addWidget(closeButton, 0, Qt::AlignRight | Qt::AlignTop);
    
    // 메인 레이아웃에 헤더 추가
    mainLayout->addLayout(headerLayout);
    
    // 구분선 추가 (높이 축소)
    QFrame *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("background-color: #bdc3c7; margin: 0 -10px;");
    line->setFixedHeight(1);  // 높이 축소
    mainLayout->addWidget(line);
    
    // 랭킹 리스트 영역
    rankingListWidget = new QWidget(this);
    updateRankingDisplay();
    
    mainLayout->addWidget(rankingListWidget);
    
    // 창을 화면 중앙에 위치
    QScreen *screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    move((screenGeometry.width() - width()) / 2,
         (screenGeometry.height() - height()) / 2);
    
    // 닫기 버튼 연결을 슬롯으로 변경
    connect(closeButton, &QPushButton::clicked, this, &RankingDialog::closeDialog);
    
    // 배경색 설정
    setStyleSheet("background-color: white;");
}

void RankingDialog::closeDialog()
{
    accept();
}

void RankingDialog::loadRankings()
{
    // 전역 정적 변수에서 랭킹 데이터 로드
    rankings = globalRankings;
}

void RankingDialog::saveRankings()
{
    // 전역 정적 변수에 랭킹 데이터 저장
    globalRankings = rankings;
}

void RankingDialog::addScore(int score)
{
    // 새 점수 추가
    RankingRecord newRecord(score);
    rankings.append(newRecord);
    
    // 점수 순으로 정렬 (높은 점수부터)
    std::sort(rankings.begin(), rankings.end(), [](const RankingRecord& a, const RankingRecord& b) {
        return a.score > b.score;
    });
    
    // 최대 7개까지만 유지
    if (rankings.size() > 7) {
        rankings = rankings.mid(0, 7);
    }
    
    // 즉시 저장
    saveRankings();
}

void RankingDialog::updateRankingDisplay()
{
    // 기존 레이아웃 정리
    if (rankingListWidget->layout()) {
        QLayoutItem* item;
        while ((item = rankingListWidget->layout()->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        delete rankingListWidget->layout();
    }
    
    QVBoxLayout* rankingLayout = new QVBoxLayout(rankingListWidget);
    rankingLayout->setSpacing(8);
    
    if (rankings.isEmpty()) {
        // "No records." 메시지 표시
        QLabel* noRecordsLabel = new QLabel("No records.", rankingListWidget);
        noRecordsLabel->setStyleSheet(R"(
            QLabel {
                font-size: 24px;
                color: #7f8c8d;
                font-style: italic;
            }
        )");
        noRecordsLabel->setAlignment(Qt::AlignCenter);
        
        rankingLayout->addStretch();
        rankingLayout->addWidget(noRecordsLabel);
        rankingLayout->addStretch();
    } else {
        // 랭킹 리스트 표시
        for (int i = 0; i < rankings.size(); ++i) {
            const RankingRecord& record = rankings[i];
            
            QWidget* rankItem = new QWidget(rankingListWidget);
            rankItem->setFixedHeight(50);
            rankItem->setStyleSheet(QString(R"(
                QWidget {
                    background-color: %1;
                    border-radius: 10px;
                    margin: 2px;
                }
            )").arg(i < 3 ? "#f0f9ff" : "#ffffff"));
            
            QHBoxLayout* itemLayout = new QHBoxLayout(rankItem);
            itemLayout->setContentsMargins(15, 0, 15, 0);
            
            // 순위 번호
            QLabel* rankNum = new QLabel(QString::number(i + 1), rankItem);
            rankNum->setStyleSheet(QString(R"(
                QLabel {
                    font-size: 18px;
                    font-weight: bold;
                    color: %1;
                }
            )").arg(i < 3 ? "#2980b9" : "#7f8c8d"));
            rankNum->setFixedWidth(30);
            
            // 점수 (중앙에 배치)
            QLabel* scoreLabel = new QLabel(QString("%1 pt").arg(record.score), rankItem);
            scoreLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #e67e22;");
            scoreLabel->setAlignment(Qt::AlignCenter);
            
            itemLayout->addWidget(rankNum);
            itemLayout->addWidget(scoreLabel, 1);  // stretch factor 1로 중앙 확장
            
            rankingLayout->addWidget(rankItem);
        }
        
        // 남은 공간을 위한 스트레치
        rankingLayout->addStretch();
    }
}
