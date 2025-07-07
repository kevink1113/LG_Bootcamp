#include "songselectiondialog.h"
#include <QApplication>
#include <QScreen>
#include <QStyle>

SongSelectionDialog::SongSelectionDialog(QWidget *parent)
    : QDialog(parent)
    , songGroup(nullptr)
    , anthemButton(nullptr)
    , bearButton(nullptr)
    , rabbitButton(nullptr)
    , startButton(nullptr)
    , cancelButton(nullptr)
    , selectedSong("")
{
    setWindowTitle("음악 선택");
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    
    // 화면 중앙에 배치
    QScreen *screen = QApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->geometry();
        setGeometry(screenGeometry.center().x() - 300, 
                   screenGeometry.center().y() - 200, 
                   600, 400);
    }
    
    // 메인 레이아웃
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(30, 30, 30, 30);
    
    // 제목 라벨
    QLabel *titleLabel = new QLabel("🎵 노래를 선택하세요 🎵", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(R"(
        QLabel {
            font-size: 24pt;
            font-weight: bold;
            color: #2c3e50;
            background-color: rgba(255, 255, 255, 200);
            border: 3px solid #3498db;
            border-radius: 15px;
            padding: 15px;
        }
    )");
    mainLayout->addWidget(titleLabel);
    
    // 라디오 버튼 그룹
    songGroup = new QButtonGroup(this);
    
    // 애국가 버튼
    anthemButton = new QRadioButton("🇰🇷 애국가", this);
    anthemButton->setStyleSheet(R"(
        QRadioButton {
            font-size: 18pt;
            font-weight: bold;
            color: #2c3e50;
            background-color: rgba(255, 255, 255, 180);
            border: 2px solid #e74c3c;
            border-radius: 10px;
            padding: 15px;
            min-width: 200px;
        }
        QRadioButton:hover {
            background-color: rgba(231, 76, 60, 100);
            border-color: #c0392b;
        }
        QRadioButton:checked {
            background-color: rgba(231, 76, 60, 150);
            border-color: #c0392b;
            color: white;
        }
    )");
    songGroup->addButton(anthemButton, 0);
    mainLayout->addWidget(anthemButton);
    
    // 곰 세마리 버튼
    bearButton = new QRadioButton("🐻 곰 세마리", this);
    bearButton->setStyleSheet(R"(
        QRadioButton {
            font-size: 18pt;
            font-weight: bold;
            color: #2c3e50;
            background-color: rgba(255, 255, 255, 180);
            border: 2px solid #f39c12;
            border-radius: 10px;
            padding: 15px;
            min-width: 200px;
        }
        QRadioButton:hover {
            background-color: rgba(243, 156, 18, 100);
            border-color: #d68910;
        }
        QRadioButton:checked {
            background-color: rgba(243, 156, 18, 150);
            border-color: #d68910;
            color: white;
        }
    )");
    songGroup->addButton(bearButton, 1);
    mainLayout->addWidget(bearButton);
    
    // 산토끼 버튼
    rabbitButton = new QRadioButton("🐰 산토끼", this);
    rabbitButton->setStyleSheet(R"(
        QRadioButton {
            font-size: 18pt;
            font-weight: bold;
            color: #2c3e50;
            background-color: rgba(255, 255, 255, 180);
            border: 2px solid #27ae60;
            border-radius: 10px;
            padding: 15px;
            min-width: 200px;
        }
        QRadioButton:hover {
            background-color: rgba(39, 174, 96, 100);
            border-color: #229954;
        }
        QRadioButton:checked {
            background-color: rgba(39, 174, 96, 150);
            border-color: #229954;
            color: white;
        }
    )");
    songGroup->addButton(rabbitButton, 2);
    mainLayout->addWidget(rabbitButton);
    
    // 기본 선택
    anthemButton->setChecked(true);
    selectedSong = "애국가";
    
    // 버튼 연결
    connect(songGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), 
            this, &SongSelectionDialog::onSongSelected);
    
    // 버튼 레이아웃
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(20);
    
    // 시작 버튼
    startButton = new QPushButton("🎮 게임 시작", this);
    startButton->setStyleSheet(R"(
        QPushButton {
            font-size: 16pt;
            font-weight: bold;
            color: white;
            background-color: #3498db;
            border: 3px solid #2980b9;
            border-radius: 10px;
            padding: 12px 24px;
            min-width: 150px;
        }
        QPushButton:hover {
            background-color: #2980b9;
            border-color: #1f5f8b;
        }
        QPushButton:pressed {
            background-color: #1f5f8b;
        }
    )");
    connect(startButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(startButton);
    
    // 취소 버튼
    cancelButton = new QPushButton("❌ 취소", this);
    cancelButton->setStyleSheet(R"(
        QPushButton {
            font-size: 16pt;
            font-weight: bold;
            color: white;
            background-color: #e74c3c;
            border: 3px solid #c0392b;
            border-radius: 10px;
            padding: 12px 24px;
            min-width: 150px;
        }
        QPushButton:hover {
            background-color: #c0392b;
            border-color: #a93226;
        }
        QPushButton:pressed {
            background-color: #a93226;
        }
    )");
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancelButton);
    
    mainLayout->addLayout(buttonLayout);
    
    // 배경 스타일
    setStyleSheet(R"(
        SongSelectionDialog {
            background-color: rgba(52, 73, 94, 220);
            border: 3px solid #34495e;
            border-radius: 15px;
        }
    )");
}

QString SongSelectionDialog::getSelectedSong() const
{
    return selectedSong;
}

void SongSelectionDialog::onSongSelected()
{
    int buttonId = songGroup->checkedId();
    switch (buttonId) {
    case 0:
        selectedSong = "애국가";
        break;
    case 1:
        selectedSong = "곰세마리";
        break;
    case 2:
        selectedSong = "산토끼";
        break;
    default:
        selectedSong = "애국가";
        break;
    }
} 