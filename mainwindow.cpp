#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    gameWindow(nullptr)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    if (gameWindow) {
        gameWindow->close();
        gameWindow->deleteLater();
        gameWindow = nullptr;
    }
    delete ui;
}

void MainWindow::on_menuButton1_clicked()
{
    if (gameWindow) {
        gameWindow->close();
        gameWindow->deleteLater();
        gameWindow = nullptr;
    }
    
    gameWindow = new GameWindow();
    gameWindow->setAttribute(Qt::WA_DeleteOnClose, false);
    gameWindow->show();
}

void MainWindow::on_menuButton2_clicked()
{
    QMessageBox::information(this, "Menu 2", "Menu 2 was selected!");
}

void MainWindow::on_menuButton3_clicked()
{
    QMessageBox::information(this, "Menu 3", "Menu 3 was selected!");
}
