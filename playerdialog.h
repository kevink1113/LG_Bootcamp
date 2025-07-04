#ifndef PLAYERDIALOG_H
#define PLAYERDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QFrame>
#include <QScrollArea>
#include <QWidget>
#include <QStringList>
#include <QMessageBox>
#include <QApplication>
#include <QScreen>
#include <QEvent>
#include <QMouseEvent>
#include <QTimer>

class PlayerDialog : public QDialog
{
    Q_OBJECT

signals:
    void currentPlayerChanged(const QString &playerName);

public:
    explicit PlayerDialog(QWidget *parent = nullptr);
    ~PlayerDialog();

    // 플레이어 관리 메서드
    QString getCurrentPlayer() const;
    void setCurrentPlayer(const QString &playerName);
    QStringList getPlayerList() const;
    bool addPlayer(const QString &playerName);
    void clearPlayerList();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void closeDialog();
    void setPlayer();
    void showVirtualKeyboard();
    void saveAndClose();

private:
    // UI 컴포넌트들
    QLabel *titleLabel;
    QPushButton *closeButton;
    QLineEdit *nameEdit;
    QPushButton *virtualKeyboardBtn;
    QPushButton *setButton;
    QLabel *playersLabel;
    QScrollArea *playersScrollArea;
    QWidget *playersListWidget;
    QVBoxLayout *playersListLayout;
    QPushButton *saveButton;
    QPushButton *cancelButton;
    
    // 플레이어 데이터
    QStringList playerList;
    QString currentPlayer;
    
    // 가상 키보드 관련
    QWidget *virtualKeyboard;
    QTimer *keyboardTimer;  // 키보드 지연 방지용 타이머
    bool isKeyboardShowing; // 키보드 표시 상태 추적
    
    // UI 및 기능 메서드
    void setupUI();
    void createVirtualKeyboard();
    void updatePlayerList();
    void selectPlayer(const QString &playerName);
    void deletePlayerWithConfirmation(const QString &playerName);
    void deletePlayer(const QString &playerName);
    void clearPlayerListWidgets();
};

#endif // PLAYERDIALOG_H
