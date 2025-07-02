#ifndef GAMEOVERDIALOG_H
#define GAMEOVERDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

class GameOverDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GameOverDialog(int score, QWidget *parent = nullptr);

signals:
    void mainMenuRequested();
    void rankingRequested();
    void restartRequested();

private:
    void setupUI(int score);
    
    QLabel *scoreLabel;
    QPushButton *mainButton;
    QPushButton *rankingButton;
    QPushButton *restartButton;
};

#endif // GAMEOVERDIALOG_H
