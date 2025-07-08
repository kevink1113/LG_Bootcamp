#include "playerdialog.h"
#include <QDebug>

PlayerDialog::PlayerDialog(QWidget *parent)
    : QDialog(parent), virtualKeyboard(nullptr), keyboardTimer(nullptr), isKeyboardShowing(false)
{
    // 키보드 타이머 초기화
    keyboardTimer = new QTimer(this);
    keyboardTimer->setSingleShot(true);
    keyboardTimer->setInterval(50); // 50ms 지연으로 중복 클릭 방지
    
    // 화면 크기에 맞춰 다이얼로그 크기 조정
    QScreen *screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    
    // 화면 크기의 80% 사용하되, 최소/최대 크기 제한
    int dialogWidth = qMin(qMax(500, int(screenGeometry.width() * 0.4)), 600);
    int dialogHeight = qMin(qMax(600, int(screenGeometry.height() * 0.8)), int(screenGeometry.height() * 0.9));
    
    setFixedSize(dialogWidth, dialogHeight);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setModal(true);
    
    // 전체 배경 스타일 설정
    setStyleSheet(R"(
        QDialog {
            background-color: white;
            border: 2px solid #2196F3;
            border-radius: 15px;
        }
    )");
    
    setupUI();
    
    // 다이얼로그를 화면 중앙에 위치
    QPoint center = screenGeometry.center();
    move(center.x() - width()/2, center.y() - height()/2);
}

PlayerDialog::~PlayerDialog()
{
    if (keyboardTimer) {
        keyboardTimer->stop();
        keyboardTimer->deleteLater();
        keyboardTimer = nullptr;
    }
    
    if (virtualKeyboard) {
        virtualKeyboard->deleteLater();
        virtualKeyboard = nullptr;
    }
}

void PlayerDialog::setupUI()
{
    // 메인 레이아웃 (다이얼로그 전체)
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(15, 15, 15, 15);

    // 상단 헤더 레이아웃 (고정)
    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 15);
    
    // 제목 레이블
    titleLabel = new QLabel("PLAYER SETTINGS", this);
    QFont titleFont("Arial", 24, QFont::ExtraBold);
    titleLabel->setFont(titleFont);
    titleLabel->setStyleSheet("color: #2c3e50; padding: 3px;");
    titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    
    // 닫기 버튼
    closeButton = new QPushButton("×", this);
    closeButton->setFixedSize(35, 35);
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
            background-color: rgba(231, 76, 60, 0.1);
            border-radius: 17px;
            color: #e74c3c;
        }
        QPushButton:pressed {
            background-color: rgba(231, 76, 60, 0.2);
        }
    )");
    
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(closeButton);
    
    mainLayout->addLayout(headerLayout);
    
    // 스크롤 영역 생성 (메인 컨텐츠용)
    QScrollArea *mainScrollArea = new QScrollArea(this);
    mainScrollArea->setWidgetResizable(true);
    mainScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mainScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    mainScrollArea->setStyleSheet(R"(
        QScrollArea {
            border: none;
            background-color: transparent;
        }
        QScrollBar:vertical {
            border: none;
            background: #ecf0f1;
            width: 12px;
            border-radius: 6px;
        }
        QScrollBar::handle:vertical {
            background: #bdc3c7;
            border-radius: 6px;
            min-height: 20px;
        }
        QScrollBar::handle:vertical:hover {
            background: #95a5a6;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
    )");
    
    // 스크롤 가능한 컨텐츠 위젯
    QWidget *scrollContentWidget = new QWidget();
    scrollContentWidget->setStyleSheet("QWidget { background-color: transparent; }");
    
    // 스크롤 컨텐츠 레이아웃
    QVBoxLayout *contentLayout = new QVBoxLayout(scrollContentWidget);
    contentLayout->setSpacing(15);
    contentLayout->setContentsMargins(0, 0, 0, 0);

    // 구분선
    QFrame *line = new QFrame(scrollContentWidget);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    line->setStyleSheet("QFrame { color: #bdc3c7; margin: 5px 0; }");
    contentLayout->addWidget(line);

    // 플레이어 정보 입력 영역
    QWidget *inputWidget = new QWidget(scrollContentWidget);
    inputWidget->setStyleSheet("QWidget { background-color: transparent; }");
    QVBoxLayout *inputLayout = new QVBoxLayout(inputWidget);
    inputLayout->setSpacing(12);
    inputLayout->setContentsMargins(15, 15, 15, 15);

    // 플레이어 이름 입력
    QLabel *nameLabel = new QLabel("Player Name:", inputWidget);
    nameLabel->setStyleSheet(R"(
        QLabel { 
            font-size: 14pt; 
            font-weight: bold; 
            color: #2c3e50; 
            margin-bottom: 3px;
        }
    )");
    inputLayout->addWidget(nameLabel);

    nameEdit = new QLineEdit(inputWidget);
    nameEdit->setPlaceholderText("Enter your name...");
    nameEdit->setFixedHeight(45);
    nameEdit->setStyleSheet(R"(
        QLineEdit {
            padding: 12px;
            border: 2px solid #bdc3c7;
            border-radius: 8px;
            font-size: 14pt;
            background-color: #f8f9fa;
        }
        QLineEdit:focus {
            border-color: #2196F3;
            background-color: white;
        }
    )");
    
    // 이름 입력 필드에 포커스를 주어 즉시 키보드 입력 가능하게 함
    nameEdit->setFocus();
    
    inputLayout->addWidget(nameEdit);
    
    // 키보드 입력 안내 메시지와 가상 키보드 버튼
    QHBoxLayout *keyboardHintLayout = new QHBoxLayout();
    
    QLabel *keyboardHint = new QLabel("💡 Tip: Use keyboard to type", inputWidget);
    keyboardHint->setStyleSheet(R"(
        QLabel { 
            font-size: 10pt; 
            color: #7f8c8d; 
            font-style: italic;
        }
    )");
    
    virtualKeyboardBtn = new QPushButton("📱 Virtual", inputWidget);
    virtualKeyboardBtn->setFixedHeight(25);
    virtualKeyboardBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #2196F3;
            color: white;
            border: none;
            border-radius: 4px;
            font-size: 9pt;
            font-weight: bold;
            padding: 3px 8px;
        }
        QPushButton:hover {
            background-color: #1976D2;
        }
        QPushButton:pressed {
            background-color: #0D47A1;
        }
    )");
    
    keyboardHintLayout->addWidget(keyboardHint);
    keyboardHintLayout->addStretch();
    keyboardHintLayout->addWidget(virtualKeyboardBtn);
    
    inputLayout->addLayout(keyboardHintLayout);

    // Set 버튼 추가
    setButton = new QPushButton("✓ Set Player", inputWidget);
    setButton->setFixedHeight(35);
    setButton->setStyleSheet(R"(
        QPushButton {
            background-color: #27AE60;
            color: white;
            border: none;
            border-radius: 6px;
            font-size: 12pt;
            font-weight: bold;
            padding: 8px;
            margin-top: 5px;
        }
        QPushButton:hover {
            background-color: #229954;
        }
        QPushButton:pressed {
            background-color: #1E8449;
        }
    )");
    inputLayout->addWidget(setButton);

    // 플레이어 목록 레이블
    playersLabel = new QLabel("Registered Players (0/10):", inputWidget);
    playersLabel->setStyleSheet(R"(
        QLabel { 
            font-size: 14pt; 
            font-weight: bold; 
            color: #2c3e50; 
            margin-top: 15px;
            margin-bottom: 5px;
        }
    )");
    inputLayout->addWidget(playersLabel);

    // 플레이어 목록을 담을 스크롤 영역
    playersScrollArea = new QScrollArea(inputWidget);
    playersScrollArea->setFixedHeight(180); // 높이를 조금 줄임
    playersScrollArea->setWidgetResizable(true);
    playersScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    playersScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    playersScrollArea->setStyleSheet(R"(
        QScrollArea {
            border: 2px solid #bdc3c7;
            border-radius: 8px;
            background-color: #f8f9fa;
        }
        QScrollBar:vertical {
            border: none;
            background: #ecf0f1;
            width: 10px;
            border-radius: 5px;
        }
        QScrollBar::handle:vertical {
            background: #bdc3c7;
            border-radius: 5px;
            min-height: 15px;
        }
        QScrollBar::handle:vertical:hover {
            background: #95a5a6;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
    )");

    // 플레이어 목록을 담을 위젯
    playersListWidget = new QWidget();
    playersListLayout = new QVBoxLayout(playersListWidget);
    playersListLayout->setSpacing(3);
    playersListLayout->setContentsMargins(8, 8, 8, 8);

    playersScrollArea->setWidget(playersListWidget);
    inputLayout->addWidget(playersScrollArea);

    inputLayout->addStretch();
    contentLayout->addWidget(inputWidget);

    // 하단 버튼 영역
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(12);
    buttonLayout->setContentsMargins(15, 10, 15, 10); // 여백 조정

    saveButton = new QPushButton("💾 Close & Save", scrollContentWidget);
    saveButton->setFixedHeight(40);
    saveButton->setStyleSheet(R"(
        QPushButton {
            background-color: #2196F3;
            color: white;
            border: none;
            border-radius: 6px;
            font-size: 14pt;
            font-weight: bold;
            padding: 10px;
        }
        QPushButton:hover {
            background-color: #1976D2;
        }
        QPushButton:pressed {
            background-color: #0D47A1;
        }
    )");

    cancelButton = new QPushButton("❌ Cancel", scrollContentWidget);
    cancelButton->setFixedHeight(40);
    cancelButton->setStyleSheet(R"(
        QPushButton {
            background-color: #95a5a6;
            color: white;
            border: none;
            border-radius: 6px;
            font-size: 14pt;
            font-weight: bold;
            padding: 10px;
        }
        QPushButton:hover {
            background-color: #7f8c8d;
        }
        QPushButton:pressed {
            background-color: #666e6f;
        }
    )");

    buttonLayout->addWidget(saveButton);
    buttonLayout->addWidget(cancelButton);
    contentLayout->addLayout(buttonLayout);

    // 스크롤 영역에 컨텐츠 설정
    mainScrollArea->setWidget(scrollContentWidget);
    mainLayout->addWidget(mainScrollArea);

    // 시그널 연결
    connect(closeButton, &QPushButton::clicked, this, &PlayerDialog::closeDialog);
    connect(cancelButton, &QPushButton::clicked, this, &PlayerDialog::closeDialog);
    connect(saveButton, &QPushButton::clicked, this, &PlayerDialog::saveAndClose);
    connect(setButton, &QPushButton::clicked, this, &PlayerDialog::setPlayer);
    connect(virtualKeyboardBtn, &QPushButton::clicked, this, &PlayerDialog::showVirtualKeyboard);
    connect(nameEdit, &QLineEdit::returnPressed, setButton, &QPushButton::click);
    
    // 플레이어 목록 업데이트
    updatePlayerList();
}



void PlayerDialog::updatePlayerList()
{
    // 플레이어 수 업데이트
    playersLabel->setText(QString("Registered Players (%1/10):").arg(playerList.size()));
    
    // 기존 위젯들 정리 (성능 최적화)
    clearPlayerListWidgets();

    // 플레이어가 없을 때 안내 메시지 표시
    if (playerList.isEmpty()) {
        QLabel *emptyLabel = new QLabel("No players registered yet.\nAdd a player using the form above.", playersListWidget);
        emptyLabel->setStyleSheet(R"(
            QLabel {
                font-size: 12pt;
                color: #7f8c8d;
                font-style: italic;
                text-align: center;
                padding: 20px;
                background-color: #f8f9fa;
                border: 1px dashed #bdc3c7;
                border-radius: 8px;
                margin: 10px;
            }
        )");
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setWordWrap(true);
        playersListLayout->addWidget(emptyLabel);
        playersListLayout->addStretch();
        return;
    }

    // 플레이어 목록 다시 생성
    for (const QString &player : playerList) {
        // 각 플레이어를 담을 컨테이너 위젯
        QWidget *playerContainer = new QWidget(playersListWidget);
        QHBoxLayout *playerLayout = new QHBoxLayout(playerContainer);
        playerLayout->setContentsMargins(0, 0, 0, 0);
        playerLayout->setSpacing(5);
        
        // 플레이어 이름 레이블
        QLabel *playerItem = new QLabel(playerContainer);
        
        // 현재 플레이어인지 확인하여 체크 표시 추가
        if (player == currentPlayer) {
            playerItem->setText("✅ " + player + " (Current)");
            playerItem->setStyleSheet(R"(
                QLabel {
                    font-size: 11pt;
                    color: #27AE60;
                    font-weight: bold;
                    padding: 6px;
                    background-color: rgba(39, 174, 96, 0.1);
                    border-radius: 4px;
                    border-left: 3px solid #27AE60;
                }
            )");
        } else {
            playerItem->setText("👤 " + player);
            playerItem->setStyleSheet(R"(
                QLabel {
                    font-size: 11pt;
                    color: #2c3e50;
                    padding: 6px;
                    background-color: white;
                    border-radius: 4px;
                    border: 1px solid #ecf0f1;
                }
                QLabel:hover {
                    background-color: #f8f9fa;
                    border-color: #bdc3c7;
                }
            )");
        }
        
        // 플레이어 클릭 이벤트 (현재 플레이어로 설정)
        playerItem->setCursor(Qt::PointingHandCursor);
        playerItem->setProperty("playerName", player);
        playerItem->setMinimumHeight(32);
        
        // 마우스 클릭 이벤트를 위한 이벤트 필터 설치
        playerItem->installEventFilter(this);
        
        // 삭제 버튼 생성
        QPushButton *deleteBtn = new QPushButton("🗑️", playerContainer);
        deleteBtn->setFixedSize(30, 30);
        deleteBtn->setCursor(Qt::PointingHandCursor);
        deleteBtn->setToolTip("Delete this player");
        deleteBtn->setStyleSheet(R"(
            QPushButton {
                background-color: transparent;
                border: none;
                border-radius: 15px;
                font-size: 14pt;
                color: #e74c3c;
                padding: 2px;
            }
            QPushButton:hover {
                background-color: rgba(231, 76, 60, 0.1);
                color: #c0392b;
            }
            QPushButton:pressed {
                background-color: rgba(231, 76, 60, 0.2);
            }
        )");
        
        // 삭제 버튼 클릭 이벤트
        connect(deleteBtn, &QPushButton::clicked, [this, player]() {
            deletePlayerWithConfirmation(player);
        });
        
        // 레이아웃에 추가
        playerLayout->addWidget(playerItem);
        playerLayout->addWidget(deleteBtn);
        
        playersListLayout->addWidget(playerContainer);
    }

    playersListLayout->addStretch();
}

bool PlayerDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        QLabel *playerLabel = qobject_cast<QLabel*>(watched);
        if (playerLabel) {
            QString playerName = playerLabel->property("playerName").toString();
            if (!playerName.isEmpty()) {
                selectPlayer(playerName);
                return true;
            }
        }
    }
    return QDialog::eventFilter(watched, event);
}

void PlayerDialog::selectPlayer(const QString &playerName)
{
    if (playerList.contains(playerName)) {
        currentPlayer = playerName;
        updatePlayerList();
        
        // 현재 플레이어 변경 신호 발생
        emit currentPlayerChanged(currentPlayer);
        
        QMessageBox::information(this, "✅ Player Selected", 
            QString("Player '%1' has been selected as the current player!")
            .arg(playerName));
    }
}

QString PlayerDialog::getCurrentPlayer() const
{
    return currentPlayer;
}

void PlayerDialog::setCurrentPlayer(const QString &playerName)
{
    if (playerList.contains(playerName)) {
        currentPlayer = playerName;
        updatePlayerList();
        
        // 현재 플레이어 변경 신호 발생
        emit currentPlayerChanged(currentPlayer);
    }
}

QStringList PlayerDialog::getPlayerList() const
{
    return playerList;
}

bool PlayerDialog::addPlayer(const QString &playerName)
{
    QString trimmedName = playerName.trimmed();
    
    if (trimmedName.isEmpty()) {
        return false;
    }
    
    if (playerList.contains(trimmedName)) {
        return false; // 이미 존재하는 플레이어
    }
    
    if (playerList.size() >= 10) {
        return false; // 최대 10명 제한
    }
    
    playerList.append(trimmedName);
    
    // 현재 플레이어가 없거나 새로 추가된 플레이어를 현재 플레이어로 설정
    if (currentPlayer.isEmpty()) {
        currentPlayer = trimmedName;
        // 현재 플레이어 변경 신호 발생
        emit currentPlayerChanged(currentPlayer);
    }
    
    updatePlayerList();
    return true;
}

void PlayerDialog::clearPlayerList()
{
    playerList.clear();
    currentPlayer.clear();
    updatePlayerList();
}

void PlayerDialog::closeDialog()
{
    reject();
}

void PlayerDialog::setPlayer()
{
    QString playerName = nameEdit->text().trimmed();
    if (playerName.isEmpty()) {
        QMessageBox::warning(this, "⚠️ Warning", 
            "Please enter a player name!\n\nUse your keyboard to type your name in the text field above.");
        nameEdit->setFocus();
        return;
    }
    
    if (addPlayer(playerName)) {
        QMessageBox::information(this, "✅ Player Set", 
            QString("Player '%1' has been added to the list!\n\nYou can now select this player from the list below.")
            .arg(playerName));
        
        // 입력 필드 초기화
        nameEdit->clear();
        nameEdit->setFocus();
    } else {
        if (playerList.contains(playerName)) {
            // 이미 존재하는 플레이어를 현재 플레이어로 설정
            currentPlayer = playerName;
            updatePlayerList();
            
            QMessageBox::information(this, "✅ Player Selected", 
                QString("Player '%1' is now the current player!")
                .arg(playerName));
                
            nameEdit->clear();
            nameEdit->setFocus();
        } else if (playerList.size() >= 10) {
            QMessageBox::warning(this, "⚠️ Player Limit Reached", 
                "Maximum of 10 players allowed!\n\nPlease select an existing player or remove some players first.");
        }
    }
}

void PlayerDialog::showVirtualKeyboard()
{
    // 이미 키보드를 표시 중이면 무시
    if (isKeyboardShowing) {
        return;
    }
    
    // 키보드가 이미 있으면 바로 표시
    if (virtualKeyboard) {
        virtualKeyboard->show();
        virtualKeyboard->raise();
        virtualKeyboard->setFocus();
        isKeyboardShowing = true;
        return;
    }
    
    // 새로운 키보드 생성 (즉시 실행)
    isKeyboardShowing = true;
    createVirtualKeyboard();
}

void PlayerDialog::saveAndClose()
{
    QMessageBox::information(this, "✅ Settings Saved", 
        "Player settings have been saved successfully!\n\nYou can now start playing with your settings.");
    
    accept();
}

void PlayerDialog::createVirtualKeyboard()
{
    // 기존 키보드가 있으면 제거
    if (virtualKeyboard) {
        virtualKeyboard->deleteLater();
        virtualKeyboard = nullptr;
    }
    
    // 가상 키보드 위젯 생성
    virtualKeyboard = new QWidget(this);
    virtualKeyboard->setObjectName("virtualKeyboard");
    virtualKeyboard->setFixedSize(480, 200);
    virtualKeyboard->setAttribute(Qt::WA_DeleteOnClose);
    virtualKeyboard->setStyleSheet(R"(
        QWidget#virtualKeyboard {
            background-color: #f0f0f0;
            border: 2px solid #2196F3;
            border-radius: 10px;
        }
    )");

    // 키보드 레이아웃
    QVBoxLayout *keyboardLayout = new QVBoxLayout(virtualKeyboard);
    keyboardLayout->setSpacing(3);
    keyboardLayout->setContentsMargins(8, 8, 8, 8);

    // 기본 키보드 버튼 스타일
    QString baseKeyStyle = R"(
        QPushButton {
            background-color: white;
            border: 1px solid #bdc3c7;
            border-radius: 3px;
            font-size: 11pt;
            font-weight: bold;
            color: #2c3e50;
            min-height: 32px;
        }
        QPushButton:pressed {
            background-color: #2196F3;
            color: white;
        }
        QPushButton:disabled {
            background-color: #ecf0f1;
            color: #bdc3c7;
        }
    )";

    // 특수 버튼 스타일들
    QString backspaceStyle = baseKeyStyle + R"(
        QPushButton { background-color: #ff6b6b; color: white; font-size: 14pt; }
        QPushButton:pressed { background-color: #e53e3e; }
    )";
    
    QString clearStyle = baseKeyStyle + R"(
        QPushButton { background-color: #ffa726; color: white; }
        QPushButton:pressed { background-color: #f57c00; }
    )";
    
    QString hideStyle = baseKeyStyle + R"(
        QPushButton { background-color: #9e9e9e; color: white; }
        QPushButton:pressed { background-color: #616161; }
    )";

    // 키 생성을 위한 헬퍼 함수 (중복 클릭 방지 포함)
    auto createKey = [&](const QString &text, const QString &style, int width = 35, int height = 32) -> QPushButton* {
        QPushButton *btn = new QPushButton(text, virtualKeyboard);
        btn->setStyleSheet(style);
        btn->setFixedSize(width, height);
        btn->setAutoRepeat(false); // 자동 반복 비활성화
        btn->setAutoExclusive(false); // 배타적 선택 비활성화
        return btn;
    };

    // 안전한 키 입력을 위한 헬퍼 함수
    auto connectKeyWithDebounce = [this](QPushButton *btn, const QString &text) {
        connect(btn, &QPushButton::clicked, [this, text, btn]() {
            // 버튼을 잠시 비활성화하여 중복 클릭 방지
            btn->setEnabled(false);
            
            if (nameEdit && !text.isEmpty()) {
                nameEdit->insert(text);
            }
            
            // 짧은 지연 후 버튼 재활성화
            QTimer::singleShot(100, [btn]() {
                if (btn) {
                    btn->setEnabled(true);
                }
            });
        });
    };

    // 첫 번째 줄: 숫자
    QHBoxLayout *row1 = new QHBoxLayout();
    row1->setSpacing(2);
    QStringList numbers = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};
    for (const QString &num : numbers) {
        QPushButton *btn = createKey(num, baseKeyStyle);
        connectKeyWithDebounce(btn, num);
        row1->addWidget(btn);
    }
    keyboardLayout->addLayout(row1);

    // 두 번째 줄: QWERTY 첫 번째 줄
    QHBoxLayout *row2 = new QHBoxLayout();
    row2->setSpacing(2);
    QStringList letters1 = {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"};
    for (const QString &letter : letters1) {
        QPushButton *btn = createKey(letter, baseKeyStyle);
        connectKeyWithDebounce(btn, letter);
        row2->addWidget(btn);
    }
    keyboardLayout->addLayout(row2);

    // 세 번째 줄: QWERTY 두 번째 줄
    QHBoxLayout *row3 = new QHBoxLayout();
    row3->setSpacing(2);
    QStringList letters2 = {"A", "S", "D", "F", "G", "H", "J", "K", "L"};
    for (const QString &letter : letters2) {
        QPushButton *btn = createKey(letter, baseKeyStyle);
        connectKeyWithDebounce(btn, letter);
        row3->addWidget(btn);
    }
    keyboardLayout->addLayout(row3);

    // 네 번째 줄: QWERTY 세 번째 줄 + Backspace
    QHBoxLayout *row4 = new QHBoxLayout();
    row4->setSpacing(2);
    
    QStringList letters3 = {"Z", "X", "C", "V", "B", "N", "M"};
    for (const QString &letter : letters3) {
        QPushButton *btn = createKey(letter, baseKeyStyle);
        connectKeyWithDebounce(btn, letter);
        row4->addWidget(btn);
    }
    
    // Backspace 버튼 (특수 처리)
    QPushButton *backspaceBtn = createKey("⌫", backspaceStyle, 55, 32);
    connect(backspaceBtn, &QPushButton::clicked, [this, backspaceBtn]() {
        backspaceBtn->setEnabled(false);
        if (nameEdit) {
            nameEdit->backspace();
        }
        QTimer::singleShot(150, [backspaceBtn]() {
            if (backspaceBtn) {
                backspaceBtn->setEnabled(true);
            }
        });
    });
    row4->addWidget(backspaceBtn);
    
    keyboardLayout->addLayout(row4);

    // 다섯 번째 줄: 기능 키들
    QHBoxLayout *row5 = new QHBoxLayout();
    row5->setSpacing(2);
    
    // Clear 버튼
    QPushButton *clearBtn = createKey("Clear", clearStyle, 55, 32);
    connect(clearBtn, &QPushButton::clicked, [this, clearBtn]() {
        clearBtn->setEnabled(false);
        if (nameEdit) {
            nameEdit->clear();
        }
        QTimer::singleShot(200, [clearBtn]() {
            if (clearBtn) {
                clearBtn->setEnabled(true);
            }
        });
    });
    row5->addWidget(clearBtn);
    
    // 스페이스바
    QPushButton *spaceBtn = createKey("Space", baseKeyStyle, 190, 32);
    connectKeyWithDebounce(spaceBtn, " ");
    row5->addWidget(spaceBtn);
    
    // Hide 버튼
    QPushButton *hideBtn = createKey("Hide", hideStyle, 55, 32);
    connect(hideBtn, &QPushButton::clicked, [this]() {
        if (virtualKeyboard) {
            isKeyboardShowing = false;
            virtualKeyboard->hide();
            virtualKeyboard->deleteLater();
            virtualKeyboard = nullptr;
        }
    });
    row5->addWidget(hideBtn);
    
    keyboardLayout->addLayout(row5);

    // 키보드 위치 설정
    int x = (width() - virtualKeyboard->width()) / 2;
    int y = height() - virtualKeyboard->height() - 20;
    virtualKeyboard->move(x, y);
    
    // 키보드 표시 (즉시 표시)
    virtualKeyboard->show();
    virtualKeyboard->raise();
    virtualKeyboard->setFocus();
    virtualKeyboard->activateWindow();
    
    // 키보드 표시 상태 업데이트
    isKeyboardShowing = true;
}

void PlayerDialog::deletePlayerWithConfirmation(const QString &playerName)
{
    // 삭제 확인 메시지 박스 생성
    QMessageBox confirmBox(this);
    confirmBox.setWindowTitle("Delete Player");
    confirmBox.setText(QString("Delete player '%1'?").arg(playerName));
    confirmBox.setInformativeText("This action cannot be undone.");
    confirmBox.setIcon(QMessageBox::Question);
    
    // 커스텀 버튼 추가
    QPushButton *yesButton = confirmBox.addButton("Yes", QMessageBox::YesRole);
    QPushButton *cancelButton = confirmBox.addButton("Cancel", QMessageBox::NoRole);
    
    // 버튼 스타일링
    yesButton->setStyleSheet(R"(
        QPushButton {
            background-color: #e74c3c;
            color: white;
            border: none;
            border-radius: 4px;
            padding: 8px 16px;
            font-weight: bold;
            min-width: 60px;
        }
        QPushButton:hover {
            background-color: #c0392b;
        }
        QPushButton:pressed {
            background-color: #a93226;
        }
    )");
    
    cancelButton->setStyleSheet(R"(
        QPushButton {
            background-color: #95a5a6;
            color: white;
            border: none;
            border-radius: 4px;
            padding: 8px 16px;
            font-weight: bold;
            min-width: 60px;
        }
        QPushButton:hover {
            background-color: #7f8c8d;
        }
        QPushButton:pressed {
            background-color: #666e6f;
        }
    )");
    
    confirmBox.setDefaultButton(cancelButton); // Cancel을 기본 버튼으로 설정
    
    // 메시지 박스 실행
    confirmBox.exec();
    
    // 사용자가 Yes를 클릭한 경우
    if (confirmBox.clickedButton() == yesButton) {
        deletePlayer(playerName);
    }
}

void PlayerDialog::deletePlayer(const QString &playerName)
{
    if (!playerList.contains(playerName)) {
        return; // 플레이어가 존재하지 않음
    }
    
    // 플레이어 목록에서 제거
    playerList.removeAll(playerName);
    
    // 삭제된 플레이어가 현재 플레이어였다면 현재 플레이어 설정 해제
    if (currentPlayer == playerName) {
        // 다른 플레이어가 있으면 첫 번째 플레이어를 현재 플레이어로 설정
        if (!playerList.isEmpty()) {
            currentPlayer = playerList.first();
        } else {
            currentPlayer.clear();
        }
        // 현재 플레이어 변경 신호 발생
        emit currentPlayerChanged(currentPlayer);
    }
    
    // 플레이어 목록 UI 업데이트
    updatePlayerList();
    
    // 삭제 완료 메시지
    QMessageBox::information(this, "✅ Player Deleted", 
        QString("Player '%1' has been deleted successfully!").arg(playerName));
}

void PlayerDialog::clearPlayerListWidgets()
{
    // 기존 위젯들 안전하게 정리 (메모리 누수 방지)
    QLayoutItem *child;
    while ((child = playersListLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            // 이벤트 필터 제거
            child->widget()->removeEventFilter(this);
            // 위젯 삭제
            child->widget()->deleteLater();
        }
        delete child;
    }
}
