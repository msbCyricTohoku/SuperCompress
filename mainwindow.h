#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStringList>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

private slots:
  void on_btnAddFiles_clicked();

  void on_btnAddFolder_clicked();

  void on_btnClear_clicked();

  void on_btnCompress_clicked();

  void on_btnDecompress_clicked();

  void on_btnAbout_clicked();

  private:
  Ui::MainWindow *ui;

  QStringList file_list;

  void add_files_from_dir(const QString &dir_path);
};

#endif // MAINWINDOW_H
