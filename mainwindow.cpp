#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QDebug>
#include <QSlider>
#include <QProcess>
#include <QCheckBox>
#include <QScreen>
#include <QApplication>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    gameWindow(nullptr),
    settingsDialog(nullptr),
    volumeSlider(nullptr),
    rankingButton(nullptr)
{
    ui->setupUi(this);
    showFullScreen();  // 전체 화면으로 설정
    
    // 설정 버튼 크기 설정 (크기 일관성을 위해)
    ui->settingsButton->setFixedSize(50, 50);  // 크기 증가
    
    // Ranking 버튼 생성
    rankingButton = new QPushButton(this);
    rankingButton->setObjectName("rankingButton");
    rankingButton->setFixedSize(50, 50);  // 설정 버튼과 동일한 크기로 증가
    
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
        "   text-shadow: 1px 1px 2px #804000; "  // 그림자 효과
        "}";
    
    // 스타일 적용
    ui->settingsButton->setStyleSheet(buttonStyle);  // 설정 버튼은 기본 스타일
    rankingButton->setStyleSheet(rankingStyle);      // 랭킹 버튼은 커스텀 스타일
    
    // 랭킹 버튼 설정 - 설정 버튼과 동일한 크기로 텍스트 설정
    rankingButton->setText("R");  // R 텍스트 사용
    rankingButton->setFont(QFont("Arial", 22, QFont::Bold));  // 폰트 크기를 더 크게 조정
    
    // 버튼 클릭 시그널 연결
    connect(rankingButton, SIGNAL(clicked()), this, SLOT(on_rankingButton_clicked()));
    
    // 버튼 표시 및 정렬
    rankingButton->show();
    updateButtonPositions();
    ui->settingsButton->raise();  // 설정 버튼을 최상위로
    rankingButton->raise();       // 랭킹 버튼을 그 다음으로
    
    createSettingsDialog();
}

MainWindow::~MainWindow()
{
    if (gameWindow) {
        gameWindow->close();
        gameWindow->deleteLater();
        gameWindow = nullptr;
    }
    delete ui;
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
    titleLabel->setStyleSheet("QLabel { font-size: 24pt; font-weight: bold; color: #2c3e50; }");
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();  // 오른쪽 여백을 위한 stretch 추가
    
    layout->addWidget(titleContainer);

    QCheckBox *bgmCheckBox = new QCheckBox("Background Music", settingsDialog);
    bgmCheckBox->setStyleSheet(R"(
        QCheckBox {
            font-size: 12pt;
            color: #2c3e50;
            margin-top: 15px;
        }
        QCheckBox::indicator {
            width: 20px;
            height: 20px;
        }
        QCheckBox::indicator:unchecked {
            border: 2px solid #2c3e50;
            background-color: #ffffff;
            border-radius: 4px;
        }
        QCheckBox::indicator:checked {
            border: 2px solid #2c3e50;
            background-color: #3498db;
            border-radius: 4px;
        }
        QCheckBox::indicator:checked:hover {
            background-color: #2980b9;
        }
    )");
    bgmCheckBox->setChecked(true);  // 기본값은 ON으로 설정
    layout->addWidget(bgmCheckBox);

    QLabel *volumeLabel = new QLabel("Volume", settingsDialog);
    volumeLabel->setStyleSheet("QLabel { font-size: 12pt; color: #2c3e50; margin-top: 15px; }");
    layout->addWidget(volumeLabel);

    volumeSlider = new QSlider(Qt::Horizontal, settingsDialog);
    volumeSlider->setMinimum(0);
    volumeSlider->setMaximum(100);
    volumeSlider->setValue(50);  // 기본값
    volumeSlider->setStyleSheet(R"(
        QSlider::groove:horizontal {
            border: 1px solid #999999;
            height: 10px;
            background: #ffffff;
            margin: 2px 0;
            border-radius: 5px;
        }
        QSlider::handle:horizontal {
            background: #3498db;
            border: 2px solid #2980b9;
            width: 18px;
            height: 18px;
            margin: -5px 0;
            border-radius: 10px;
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
    
    // 버튼 스타일 설정
    buttonBox->setStyleSheet(R"(
        QPushButton {
            min-width: 80px;
            min-height: 30px;
            padding: 5px;
            background-color: #3498db;
            border: none;
            border-radius: 5px;
            color: white;
            font-weight: bold;
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

void MainWindow::on_settingsButton_clicked()
{
    // 현재 화면의 중앙 위치 계산
    QScreen *screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    QPoint center = screenGeometry.center();
    
    // 다이얼로그를 화면 중앙에 위치시킴
    QSize dialogSize = settingsDialog->size();
    settingsDialog->move(center.x() - dialogSize.width()/2, 
                        center.y() - dialogSize.height()/2);
    
    // 다이얼로그를 최상위로 표시하고 활성화
    settingsDialog->raise();
    settingsDialog->activateWindow();
    settingsDialog->exec();
}

void MainWindow::onVolumeChanged(int value)
{
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
    if (gameWindow) {
        gameWindow->close();
        gameWindow->deleteLater();
        gameWindow = nullptr;
    }
    
    qDebug() << "Creating new game window...";
    // 게임 윈도우를 독립적인 윈도우로 생성
    gameWindow = new GameWindow(nullptr);
    
    if (!gameWindow) {
        qDebug() << "Failed to create game window!";
        return;
    }
    
    gameWindow->setAttribute(Qt::WA_DeleteOnClose, false);
    qDebug() << "Initializing game window...";
    // setupGame 함수에서 show()를 처리하므로 여기서는 호출하지 않음
}

void MainWindow::on_menuButton2_clicked()
{
    QMessageBox::information(this, "Menu 2", "Menu 2 was selected!");
}

void MainWindow::on_menuButton3_clicked()
{
    QMessageBox::information(this, "Menu 3", "Menu 3 was selected!");
}

void MainWindow::on_rankingButton_clicked()
{
    // TODO: 랭킹 시스템 구현
    QMessageBox::information(this, "Ranking", "Ranking system will be implemented soon!");
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updateButtonPositions();
}

void MainWindow::updateButtonPositions()
{
    if (!rankingButton || !ui->settingsButton) return;

    // 버튼 크기와 여백 설정
    const int margin = 0;       // 여백 제거하여 최상단에 배치
    const int buttonSize = 50;  // 버튼 크기 유지
    const int spacing = 10;     // 버튼 사이 간격

    // 설정 버튼의 위치를 우측 상단으로 설정
    ui->settingsButton->setGeometry(
        width() - buttonSize - margin,  // x
        margin,                         // y
        buttonSize,                     // width
        buttonSize                      // height
    );
    
    // 랭킹 버튼을 설정 버튼 왼쪽에 배치 (간격 추가)
    rankingButton->setGeometry(
        width() - (2 * buttonSize) - margin - spacing,  // x
        margin,                                         // y
        buttonSize,                                     // width
        buttonSize                                      // height
    );
    
    // 버튼들을 부모 위젯의 스택 순서 최상위로 이동
    rankingButton->stackUnder(ui->settingsButton);  // 설정 버튼 바로 아래에 위치
}
