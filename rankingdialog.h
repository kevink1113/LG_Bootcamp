#ifndef RANKINGDIALOG_H
#define RANKINGDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>

class RankingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RankingDialog(QWidget *parent = nullptr);
    ~RankingDialog();

private slots:
    void closeDialog();

private:
    QLabel *titleLabel;
    QPushButton *closeButton;
    void setupUI();
};

#endif // RANKINGDIALOG_H
