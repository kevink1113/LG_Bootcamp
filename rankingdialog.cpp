#include "rankingdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QScreen>
#include <QApplication>

RankingDialog::RankingDialog(QWidget *parent)
    : QDialog(parent), titleLabel(nullptr), closeButton(nullptr)
{
    setupUI();
}

RankingDialog::~RankingDialog()
{
    // Qt의 자동 메모리 관리로 인해 명시적 삭제는 필요하지 않음
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
    QWidget* rankingListWidget = new QWidget(this);
    QVBoxLayout* rankingLayout = new QVBoxLayout(rankingListWidget);
    rankingLayout->setSpacing(8);
    
    // "No records." 메시지를 중앙에 표시
    QLabel* noRecordsLabel = new QLabel("No records.", rankingListWidget);
    noRecordsLabel->setStyleSheet(R"(
        QLabel {
            font-size: 24px;
            color: #7f8c8d;
            font-style: italic;
        }
    )");
    noRecordsLabel->setAlignment(Qt::AlignCenter);
    
    // 상하 여백을 위한 스트레치 추가
    rankingLayout->addStretch();
    rankingLayout->addWidget(noRecordsLabel);
    rankingLayout->addStretch();
    
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
