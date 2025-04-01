#include "OptionsDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QDebug>
#include <iostream>
#include <QTimer>


OptionsDialog::OptionsDialog(QWidget *parent)
    : QDialog(parent), m_userStates()
{
    setWindowTitle("Opciones de Usuario");
    setMinimumSize(400, 300);
    if (parent) {
        // Obtener la geometría de la ventana principal
        QRect parentGeometry = parent->geometry();
       
        // Calcular la posición para nuestra ventana (a la derecha)
        int newX = parentGeometry.x() + parentGeometry.width() + 10; // 10 píxeles de margen
        int newY = parentGeometry.y(); // Mantener la misma altura Y
       
        // Establecer la posición
        move(newX, newY);
    }
   
    setupUI();
   
    // Connect signal and slot para los botones
    connect(exitButton, &QPushButton::clicked, this, &OptionsDialog::close);
    connect(acceptButton, &QPushButton::clicked, this, &OptionsDialog::onAcceptClicked);
    connect(showAllUsersButton, &QPushButton::clicked, this, &OptionsDialog::onShowAllUsersClicked);

}

OptionsDialog::~OptionsDialog()
{
    // Cleanup if necessary
}

void OptionsDialog::setRequestInfoFunction(std::function<void(const QString&)> func) {
    m_requestInfoFunc = func;
}

void OptionsDialog::setupUI()
{
    // Main layout
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
   
    // Añadir etiqueta para lista de usuarios
    QLabel *usersLabel = new QLabel("Usuarios Conectados:", this);
    mainLayout->addWidget(usersLabel);
   
    // Añadir ComboBox para mostrar usuarios
    userListView = new QComboBox(this);
    mainLayout->addWidget(userListView);
    
    // Añadir etiqueta para la primera caja de visualización
    QLabel *displayBox1Label = new QLabel("Estado del Usuario:", this);
    mainLayout->addWidget(displayBox1Label);
    
    // Añadir primera caja de visualización (solo lectura)
    displayBox1 = new QTextEdit(this);
    displayBox1->setPlainText("...");
    displayBox1->setReadOnly(true);  // Establecer como solo lectura
    displayBox1->setMaximumHeight(30);
    displayBox1->setStyleSheet("background-color: #f0f0f0;");  // Fondo gris claro para indicar solo lectura
    mainLayout->addWidget(displayBox1);
    acceptButton = new QPushButton("Aceptar", this);
    mainLayout->addWidget(acceptButton);

    showAllUsersButton = new QPushButton("Ver Todos los Usuarios", this);
    mainLayout->addWidget(showAllUsersButton);
    QLabel *allUsersLabel = new QLabel("Lista de Todos los Usuarios:", this);
    mainLayout->addWidget(allUsersLabel);
    allUsersTextArea = new QTextEdit(this);
    allUsersTextArea->setReadOnly(true);
    allUsersTextArea->setMinimumHeight(150);
    mainLayout->addWidget(allUsersTextArea);
    
   
    // Espacio en blanco, reducir la altura ya que añadimos cajas de texto
    QLabel *emptyLabel = new QLabel(this);
    emptyLabel->setMinimumHeight(20);
    mainLayout->addWidget(emptyLabel);
   
    // Botones en la parte inferior
    QHBoxLayout *buttonLayout = new QHBoxLayout();
   

    exitButton = new QPushButton("Salir", this);
    buttonLayout->addStretch();
    buttonLayout->addWidget(exitButton);
   
    mainLayout->addLayout(buttonLayout);
   
    // Set layout
    setLayout(mainLayout);
}

void OptionsDialog::setUserList(QComboBox* userList)
{
    // Limpiar la lista actual
    userListView->clear();
   
    // Copiar todos los elementos de la lista original
    for (int i = 0; i < userList->count(); i++) {
        userListView->addItem(userList->itemText(i));
    }
}

void OptionsDialog::addUserInfo(const QString& username, int status)
{
    userStatusMap[username] = status;
    
    // Actualizar interfaz si corresponde al usuario seleccionado
    if (userListView->currentText() == username) {
        displayBox1->setPlainText(getStatusString(status));
    }
}

QString OptionsDialog::getStatusString(int status)
{
    switch (status) {
        case 0: return "Desconectado";
        case 1: return "Activo";    
        case 2: return "Ocupado";    
        case 3: return "Inactivo";
        default: return "Desconocido";
    }
}

void OptionsDialog::onAcceptClicked()
{
    // Obtener el texto del elemento seleccionado en el ComboBox
    QString selectedUser = userListView->currentText();
    
    if (selectedUser != "General") {
        // Usar la función de callback en lugar de emitir una señal
        if (m_requestInfoFunc) {
            m_requestInfoFunc(selectedUser);
        }
        // La interfaz se actualizará cuando llegue la respuesta
    } else {
        displayBox1->setPlainText("...");
    }
}

void OptionsDialog::setUserStatesFunction(QComboBox* userList, const std::unordered_map<std::string, std::string>& userStates) {
    // Guardar el mapa de estados
    m_userStates = userStates;
    
    // Limpiar la lista actual
    userListView->clear();
   
    // Copiar todos los elementos de la lista original
    for (int i = 0; i < userList->count(); i++) {
        userListView->addItem(userList->itemText(i));
    }
    
    // Conectar el botón para mostrar todos los usuarios
    connect(showAllUsersButton, &QPushButton::clicked, this, &OptionsDialog::onShowAllUsersClicked);
}

void OptionsDialog::updateAllUsersTextArea(const std::unordered_map<std::string, std::string>& userStates) {
    allUsersTextArea->clear();
    
    // Check if we received any users
    if (userStates.empty()) {
        allUsersTextArea->append("No hay usuarios conectados actualmente.");
        return;
    }
    
    // Display the users and their status
    for (const auto& [username, status] : userStates) {
        allUsersTextArea->append(QString::fromStdString(username) + ": " + 
                                QString::fromStdString(status));
    }
}

void OptionsDialog::setRequestAllUsersFunction(std::function<void()> func) {
    m_requestAllUsersFunc = func;
}

void OptionsDialog::onShowAllUsersClicked() {
    allUsersTextArea->clear();
    allUsersTextArea->append("Solicitando lista de usuarios del servidor...");
    
    if (m_requestAllUsersFunc) {
        m_requestAllUsersFunc();
        
        // Add timeout to show error if no response within 5 seconds
        QTimer* responseTimer = new QTimer(this);
        responseTimer->setSingleShot(true);
        connect(responseTimer, &QTimer::timeout, this, [this, responseTimer]() {
            if (allUsersTextArea->toPlainText().startsWith("Solicitando lista")) {
                allUsersTextArea->clear();
                allUsersTextArea->append("El servidor no respondió a tiempo. Puede haber un problema de conexión.");
                
                // Try to diagnose the issue
                allUsersTextArea->append("\nPosibles causas:");
                allUsersTextArea->append("- El servidor está sobrecargado");
                allUsersTextArea->append("- Hay un problema de red");
                allUsersTextArea->append("- El WebSocket está desconectado");
            }
            responseTimer->deleteLater();
        });
        responseTimer->start(5000);  // 5-second timeout
    }
}
