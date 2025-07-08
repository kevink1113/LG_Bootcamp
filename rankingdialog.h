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
    QString playerName;
    
    RankingRecord(int s = 0, const QString& name = "Unknown") : score(s), playerName(name) {}
};

class RankingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RankingDialog(QWidget *parent = nullptr);
    explicit RankingDialog(int newScore, QWidget *parent = nullptr);
    explicit RankingDialog(int newScore, const QString& playerName, QWidget *parent = nullptr);
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
    void addScore(int score, const QString& playerName = "Unknown");
    void updateRankingDisplay();
};

#endif // RANKINGDIALOG_H
