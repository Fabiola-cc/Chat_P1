#ifndef AYUDA_H
#define AYUDA_H

#include <QDialog>
#include <QTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QLabel>
#include <QComboBox>
#include <functional>
#include <unordered_map>
#include <string>

// Removing Q_OBJECT since we don't need signals/slots in this class
class Ayuda : public QDialog {
    // Remove Q_OBJECT macro

public:
    explicit Ayuda(QWidget *parent = nullptr);
    ~Ayuda();
    
    void setUserList(QComboBox* userList) { Q_UNUSED(userList); }
    void addUserInfo(const QString &username, int status) { Q_UNUSED(username); Q_UNUSED(status); }
    void setRequestInfoFunction(std::function<void(const QString&)> func) { Q_UNUSED(func); }
    void setUserStatesFunction(QComboBox* userList, const std::unordered_map<std::string, std::string>& userStates) 
    { Q_UNUSED(userList); Q_UNUSED(userStates); }

    // Changed these from private slots to regular methods
    void onAcceptClicked() {}
    void onShowAllUsersClicked() {}

private:
    // UI Elements
    QTabWidget *tabWidget;
    QTextEdit *generalHelpText;
    QTextEdit *connectionHelpText;
    QTextEdit *chatHelpText;
    QTextEdit *statusHelpText;
    QTextEdit *optionsHelpText;
    QPushButton *closeButton;

    // Layout setup method
    void setupUI();
};

#endif // AYUDA_H