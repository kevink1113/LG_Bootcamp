#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDialog>
#include <QSlider>
#include "gamewindow.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_menuButton1_clicked();
    void on_menuButton2_clicked();
    void on_menuButton3_clicked();
    void on_settingsButton_clicked();
    void onVolumeChanged(int value);

private:
    Ui::MainWindow *ui;
    GameWindow *gameWindow;
    QDialog *settingsDialog;
    QSlider *volumeSlider;
    void createSettingsDialog();
};

#endif // MAINWINDOW_H
