#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDialog>
#include <QSlider>
#include <QPushButton>
#include "gamewindow.h"
#include "rankingdialog.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

protected:
    virtual void resizeEvent(QResizeEvent *event) override;

private slots:
    void on_menuButton1_clicked();
    void on_menuButton2_clicked();
    void on_menuButton3_clicked();
    void on_settingsButton_clicked();
    void onVolumeChanged(int value);
    void on_rankingButton_clicked();

private:
    Ui::MainWindow *ui;
    GameWindow *gameWindow;
    QDialog *settingsDialog;
    QSlider *volumeSlider;
    QPushButton *rankingButton;
    RankingDialog *rankingDialog;
    void createSettingsDialog();
    void updateButtonPositions();
};

#endif // MAINWINDOW_H
