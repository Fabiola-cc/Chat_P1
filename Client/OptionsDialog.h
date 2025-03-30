#ifndef OPTIONSDIALOG_H
#define OPTIONSDIALOG_H

#include <QDialog>
#include <QPushButton>
#include <QComboBox>
#include <QMap>
#include <QTextEdit> 
#include <functional> 

class OptionsDialog : public QDialog {
    Q_OBJECT

public:
    explicit OptionsDialog(QWidget *parent = nullptr);
    ~OptionsDialog();
    
    // Método para establecer la lista de usuarios
    void setUserList(QComboBox* userList);
    // Método para añadir información de usuario (estado e IP)
    void addUserInfo(const QString &username, int status);
    void setRequestInfoFunction(std::function<void(const QString&)> func);


private slots:
    // Slot para el botón aceptar
    void onAcceptClicked();

private:
    // UI Elements
    QPushButton *exitButton;
    QPushButton *acceptButton;
    QComboBox *userListView;
    QTextEdit *displayBox1;  // Primera caja para mostrar texto
    
    // Mapas para almacenar el estado y la IP de cada usuario
    QMap<QString, int> userStatusMap;
    std::function<void(const QString&)> m_requestInfoFunc;

    // Layout setup method
    void setupUI();
    
    // Método para obtener string de estado
    QString getStatusString(int status);
};

#endif // OPTIONSDIALOG_H