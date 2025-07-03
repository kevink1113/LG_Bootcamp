#ifndef GAMEOVERDIALOG_H
#define GAMEOVERDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "rankingdialog.h"

class GameOverDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GameOverDialog(int score, QWidget *parent = nullptr);

signals:
    void mainMenuRequested();
    void rankingRequested();
    void restartRequested();

private slots:
    void onRankingButtonClicked();

private:
    void setupUI(int score);
    
    int currentScore;  // 현재 점수 저장
    bool scoreAdded;   // 점수가 이미 추가되었는지 확인하는 플래그
    QLabel *scoreLabel;
    QPushButton *mainButton;
    QPushButton *rankingButton;
    QPushButton *restartButton;
    RankingDialog *rankingDialog;
};

#endif // GAMEOVERDIALOG_H
