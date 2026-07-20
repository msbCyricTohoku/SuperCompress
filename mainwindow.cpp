#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QByteArray>
#include <QDataStream>
#include <QCoreApplication>

#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

MainWindow::MainWindow(QWidget *parent)
  : QMainWindow(parent)
  , ui(new Ui::MainWindow)
{
  ui->setupUi(this);
  
  ui->progressBar->setValue(0);
}

MainWindow::~MainWindow()
{
  delete ui;
}

void MainWindow::on_btnAddFiles_clicked()
{
  QStringList files = QFileDialog::getOpenFileNames(this, "Select File(s)");

  int N = files.size();

  for (int i = 0; i < N; i++) {

    if (!file_list.contains(files[i])) {

      file_list.append(files[i]);
      
      ui->listWidget->addItem(files[i]);
      
    }
  }
}

void MainWindow::add_files_from_dir(const QString &dir_path)
{
  QDir dir(dir_path);
  
  QFileInfoList list = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);

  int N = list.size();

  for (int i = 0; i < N; i++) {

    QFileInfo fi = list[i];

    if (fi.isDir()) {

      add_files_from_dir(fi.absoluteFilePath());
      
    } else {

      QString path = fi.absoluteFilePath();

      if (!file_list.contains(path)) {

        file_list.append(path);
        
        ui->listWidget->addItem(path);
        
      }
    }
  }
}

void MainWindow::on_btnAddFolder_clicked()
{
  QString dir = QFileDialog::getExistingDirectory(this, "Select Folder");

  if (!dir.isEmpty()) {

    add_files_from_dir(dir);
    
  }
}

void MainWindow::on_btnClear_clicked()
{
  file_list.clear();
  
  ui->listWidget->clear();
  
  ui->progressBar->setValue(0);
}

void MainWindow::on_btnCompress_clicked()
{
  int N = file_list.size();

  if (N == 0) {
      
    return;
  }

  QString save_path = QFileDialog::getSaveFileName(this, "Save Archive", "", "Archive (*.zarc)");
  
  if (save_path.isEmpty()) {
      
    return;
  }

  if (!save_path.endsWith(".zarc", Qt::CaseInsensitive)) {

    save_path += ".zarc";
    
  }

  /*-------------calculate total size for progress bar--------------------- */
  qint64 total_bytes = 0;
  qint64 processed_bytes = 0;

  for (int i = 0; i < N; i++) {

    QFileInfo fi(file_list[i]);
    
    total_bytes += fi.size();
    
  }

  ui->progressBar->setMaximum(100);
  
  ui->progressBar->setValue(0);

  /*-------------precompute memory batches for chunking--------------------- */
  int CHUNK_SIZE = 16 * 1024 * 1024;

#ifdef _OPENMP
  int batch_count = omp_get_max_threads() * 2; 
#else
  int batch_count = 4;
#endif

  if (batch_count < 2) batch_count = 2;

  char **raw_buffers = (char **)malloc(batch_count * sizeof(char *));
  char **cmp_buffers = (char **)malloc(batch_count * sizeof(char *));
  int *raw_sizes = (int *)malloc(batch_count * sizeof(int));
  int *cmp_sizes = (int *)malloc(batch_count * sizeof(int));

  for (int b = 0; b < batch_count; b++) {

    raw_buffers[b] = (char *)malloc(CHUNK_SIZE);
    
    cmp_buffers[b] = NULL;
    
  }

  /*-------write master archive file via streaming ------*/
  QFile out_file(save_path);
  
  if (out_file.open(QIODevice::WriteOnly)) {

    QDataStream out(&out_file);
    
    out.setVersion(QDataStream::Qt_5_0);

    /* The magic number indicates this is a valid ZARC file */
    out << (quint32)0xABCDEF02; 
    
    out << (qint32)N;

    for (int i = 0; i < N; i++) {

      QFileInfo fi(file_list[i]);
      
      QString fname = fi.fileName();

      QFile in_f(file_list[i]);
      
      if (in_f.open(QIODevice::ReadOnly)) {

        qint64 f_size = in_f.size();
        
        qint32 num_chunks = 0;
        
        if (f_size > 0) {
            
          num_chunks = (qint32)((f_size + CHUNK_SIZE - 1) / CHUNK_SIZE);
          
        }

        out << fname;
        
        out << num_chunks;

        int chunks_processed = 0;
        
        while (chunks_processed < num_chunks) {

          int chunks_read = 0;

          /*-- sequentially read a batch of chunks into RAM --*/
          for (int b = 0; b < batch_count; b++) {

            if (chunks_processed + chunks_read >= num_chunks) {
                
              break;
              
            }

            qint64 bytes_read = in_f.read(raw_buffers[b], CHUNK_SIZE);
            
            if (bytes_read > 0) {

              raw_sizes[b] = (int)bytes_read;
              
              chunks_read++;

              processed_bytes += bytes_read;
              
            }
          }

/*----------omp parallel here for openmp-------------*/
#pragma omp parallel for
          for (int b = 0; b < chunks_read; b++) {

            QByteArray raw = QByteArray::fromRawData(raw_buffers[b], raw_sizes[b]);
            
            /**************calc maximum compression (level 9)****************/
            QByteArray cmp = qCompress(raw, 9); 

            cmp_sizes[b] = cmp.size();
            
            cmp_buffers[b] = (char *)malloc(cmp_sizes[b]);
            
            memcpy(cmp_buffers[b], cmp.constData(), cmp_sizes[b]);
            
          }

          /*-- sequentially write the compressed chunks to disk --*/
          for (int b = 0; b < chunks_read; b++) {

            out << (qint32)raw_sizes[b];
            
            out << (qint32)cmp_sizes[b];
            
            if (cmp_sizes[b] > 0) {

              out.writeRawData(cmp_buffers[b], cmp_sizes[b]);
              
            }

            free(cmp_buffers[b]);
            
            cmp_buffers[b] = NULL;
            
          }

          chunks_processed += chunks_read;

          /*-- update UI safely without freezing --*/
          if (total_bytes > 0) {
              
            int pct = (int)((processed_bytes * 100) / total_bytes);
            
            ui->progressBar->setValue(pct);
            
            QCoreApplication::processEvents();
            
          }
        }

        in_f.close();
        
      }
    }

    out_file.close();

    ui->progressBar->setValue(100);
    
    QMessageBox::information(this, "Success", "Compression complete!");

    file_list.clear();
    
    ui->listWidget->clear();
    
    ui->progressBar->setValue(0);
    
  } else {
      
    QMessageBox::critical(this, "Error", "Failed to create the output archive file.");
    
  }

  /*-------backward sweep (m--) ------*/
  for (int m = batch_count - 1; m >= 0; m--) {

    free(raw_buffers[m]);

    if (cmp_buffers[m] != NULL) {
        
      free(cmp_buffers[m]);
      
    }
  }

  free(raw_buffers);
  free(cmp_buffers);
  free(raw_sizes);
  free(cmp_sizes);
}

void MainWindow::on_btnDecompress_clicked()
{
  QString arc_path = QFileDialog::getOpenFileName(this, "Open Archive", "", "Archive (*.zarc)");
  
  if (arc_path.isEmpty()) {
      
    return;
  }

  QString out_dir = QFileDialog::getExistingDirectory(this, "Select Extraction Folder");
  
  if (out_dir.isEmpty()) {
      
    return;
  }

  QFile in_file(arc_path);

  if (!in_file.open(QIODevice::ReadOnly)) {
      
    return;
  }

  qint64 total_arc_bytes = in_file.size();
  
  ui->progressBar->setMaximum(100);
  
  ui->progressBar->setValue(0);

  QDataStream in(&in_file);
  
  in.setVersion(QDataStream::Qt_5_0);

  quint32 magic;
  
  in >> magic;

  if (magic != 0xABCDEF02) {

    in_file.close();
    
    QMessageBox::critical(this, "Error", "Invalid or corrupted archive format!");
    
    return;
  }

  qint32 n_files;
  
  in >> n_files;
   int N = (int)n_files;

  /*-------------precompute memory batches for chunking--------------------- */
  int CHUNK_SIZE = 16 * 1024 * 1024;

#ifdef _OPENMP
  int batch_count = omp_get_max_threads() * 2; 
#else
  int batch_count = 4;
#endif

  if (batch_count < 2) batch_count = 2;

  char **raw_buffers = (char **)malloc(batch_count * sizeof(char *));

char **cmp_buffers = (char **)malloc(batch_count * sizeof(char *));

  int *raw_sizes = (int *)malloc(batch_count * sizeof(int));
   int *cmp_sizes = (int *)malloc(batch_count * sizeof(int));

  for (int b = 0; b < batch_count; b++) {

    raw_buffers[b] = (char *)malloc(CHUNK_SIZE);
    
    cmp_buffers[b] = NULL;
    
  }
  
  QByteArray ba_out_dir = out_dir.toUtf8();
  
  const char *out_dir_str = ba_out_dir.constData();

  for (int i = 0; i < N; i++) {

    QString fname;
    
    in >> fname;

    qint32 num_chunks;
    
    in >> num_chunks;

    char out_path[4096];
    
    snprintf(out_path, sizeof(out_path), "%s/%s", out_dir_str, fname.toUtf8().constData());

    QFile out_f(QString::fromUtf8(out_path));

    if (out_f.open(QIODevice::WriteOnly)) {

      int chunks_processed = 0;

      while (chunks_processed < num_chunks) {

        int chunks_to_read = batch_count;
        
        if (num_chunks - chunks_processed < batch_count) {
            
          chunks_to_read = num_chunks - chunks_processed;
          
        }

        /*-- sequentially read compressed chunks --*/
        for (int b = 0; b < chunks_to_read; b++) {

          qint32 r_size, c_size;
          
          in >> r_size;
          in >> c_size;
          raw_sizes[b] = (int)r_size;  
          cmp_sizes[b] = (int)c_size;
          cmp_buffers[b] = (char *)malloc(cmp_sizes[b]);

          
    in.readRawData(cmp_buffers[b], cmp_sizes[b]);
          
        }

/*----------omp parallel here for openmp-------------*/
#pragma omp parallel for
        for (int b = 0; b < chunks_to_read; b++) {

          QByteArray cmp = QByteArray::fromRawData(cmp_buffers[b], cmp_sizes[b]);
          
          QByteArray raw = qUncompress(cmp);

          if (raw.size() == raw_sizes[b]) {

            memcpy(raw_buffers[b], raw.constData(), raw_sizes[b]);
            
          } else {

            memset(raw_buffers[b], 0, raw_sizes[b]); 
            
          }
        }

        /*-- sequentially write raw output --*/
        for (int b = 0; b < chunks_to_read; b++) {

          out_f.write(raw_buffers[b], raw_sizes[b]);

          free(cmp_buffers[b]);
          
          cmp_buffers[b] = NULL;
          
        }

        chunks_processed += chunks_to_read;

        /*-- update UI based on reading position --*/
        if (total_arc_bytes > 0) {

          int pct = (int)((in_file.pos() * 100) / total_arc_bytes);
          
          ui->progressBar->setValue(pct);
          
          QCoreApplication::processEvents();
          
        }
      }

      out_f.close();
      
    }
  }

  in_file.close();

  /*-------backward sweep (m--) ------*/
  for (int m = batch_count - 1; m >= 0; m--) {

    free(raw_buffers[m]);

    if (cmp_buffers[m] != NULL) {
        
      free(cmp_buffers[m]);
      
    }
  }

  free(raw_buffers);
  free(cmp_buffers);
  free(raw_sizes);
  free(cmp_sizes);

  ui->progressBar->setValue(100);
  
  QMessageBox::information(this, "Success", "Decompression complete!");
  
  ui->progressBar->setValue(0);
}

void MainWindow::on_btnAbout_clicked()
{
    QMessageBox::about(this, "About", "SuperCompress by msb -- 2026, HKMU.");
}

