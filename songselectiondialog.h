#ifndef SONGSELECTIONDIALOG_H
#define SONGSELECTIONDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QButtonGroup>
#include <QRadioButton>

class SongSelectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SongSelectionDialog(QWidget *parent = nullptr);
    QString getSelectedSong() const;

private slots:
    void onSongSelected();

private:
    QButtonGroup *songGroup;
    QRadioButton *anthemButton;
    QRadioButton *bearButton;
    QRadioButton *rabbitButton;
    QPushButton *startButton;
    QPushButton *cancelButton;
    QString selectedSong;
};

#endif // SONGSELECTIONDIALOG_H 