#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "common.h"


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class EditorList;
class QLabel;
class QComboBox;
class CompilerManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void updateStatusBarForEncoding();
    void updateStatusBarForEditingInfo(int line,int col,int lines,int charCount);

protected:
    void openFiles(const QStringList& files);
    void openFile(const QString& filename);
private slots:
    void on_actionNew_triggered();

    void on_EditorTabsLeft_tabCloseRequested(int index);

    void on_actionOpen_triggered();

    void on_actionSave_triggered();

    void on_actionSaveAs_triggered();

    void on_actionOptions_triggered();

    // qt will auto bind slots with the prefix "on_"
    void onCompilerSetChanged(int index);

    void on_actionCompile_triggered();

    void on_actionRun_triggered();

    void on_actionUndo_triggered();

    void on_actionRedo_triggered();

    void on_actionCut_triggered();

    void on_actionSelectAll_triggered();

    void on_actionCopy_triggered();

    void on_actionPaste_triggered();

    void on_actionIndent_triggered();

    void on_actionUnIndent_triggered();

    void on_actionToggleComment_triggered();

    void on_actionUnfoldAll_triggered();

    void on_actionFoldAll_triggered();

public slots:
    void onCompileLog(const QString& msg);
    void onCompileIssue(PCompileIssue issue);

private:
    void setupActions();

    void updateCompilerSet();

private:
    Ui::MainWindow *ui;
    EditorList* mEditorList;
    QLabel* mFileInfoStatus;
    QLabel* mFileEncodingStatus;
    QComboBox* mCompilerSet;
    CompilerManager* mCompilerManager;

   // QWidget interface
protected:
    void closeEvent(QCloseEvent *event) override;
};

extern MainWindow* pMainWindow;
#endif // MAINWINDOW_H
