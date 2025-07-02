#ifndef RANKINGDIALOG_H
#define RANKINGDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>

struct RankingRecord {
    int score;
    
    RankingRecord(int s = 0) : score(s) {}
};

class RankingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RankingDialog(QWidget *parent = nullptr);
    explicit RankingDialog(int newScore, QWidget *parent = nullptr);
    ~RankingDialog();

private slots:
    void closeDialog();

private:
    QLabel *titleLabel;
    QPushButton *closeButton;
    QWidget *rankingListWidget;
    QList<RankingRecord> rankings;
    
    void setupUI();
    void loadRankings();
    void saveRankings();
    void addScore(int score);
    void updateRankingDisplay();
};

#endif // RANKINGDIALOG_H
