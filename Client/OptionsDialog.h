#ifndef OPTIONSDIALOG_H
#define OPTIONSDIALOG_H

#include <QDialog>
#include <QPushButton>

class OptionsDialog : public QDialog {
    Q_OBJECT

public:
    explicit OptionsDialog(QWidget *parent = nullptr);
    ~OptionsDialog();

private:
    // UI Elements
    QPushButton *exitButton;
    
    // Layout setup method
    void setupUI();
};

#endif // OPTIONSDIALOG_H