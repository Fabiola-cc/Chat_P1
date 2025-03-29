#include "OptionsDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QDebug>
#include <iostream>

OptionsDialog::OptionsDialog(QWidget *parent)
    : QDialog(parent)
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
}

OptionsDialog::~OptionsDialog()
{
    // Cleanup if necessary
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
    
    // Añadir etiqueta para la segunda caja de visualización
    QLabel *displayBox2Label = new QLabel("IP del Usuario:", this);
    mainLayout->addWidget(displayBox2Label);
    
    // Añadir segunda caja de visualización (solo lectura)
    displayBox2 = new QTextEdit(this);
    displayBox2->setPlainText("...");
    displayBox2->setReadOnly(true);  // Establecer como solo lectura
    displayBox2->setMaximumHeight(30);
    displayBox2->setStyleSheet("background-color: #f0f0f0;");  // Fondo gris claro para indicar solo lectura
    mainLayout->addWidget(displayBox2);
   
    // Espacio en blanco, reducir la altura ya que añadimos cajas de texto
    QLabel *emptyLabel = new QLabel(this);
    emptyLabel->setMinimumHeight(20);
    mainLayout->addWidget(emptyLabel);
   
    // Botones en la parte inferior
    QHBoxLayout *buttonLayout = new QHBoxLayout();
   
    acceptButton = new QPushButton("Aceptar", this);
    exitButton = new QPushButton("Salir", this);
    buttonLayout->addWidget(acceptButton);
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
    
    //Simulación de datos 
    for (int i = 0; i < userListView->count(); i++) {
        QString username = userListView->itemText(i);
        if (username != "General") {
            // Simular datos de usuario
            addUserInfo(username, 3 , "11.11");
        }
    }
}

void OptionsDialog::addUserInfo(const QString& username, int status, const QString& ipAddress)
{
    userStatusMap[username] = status;
    userIPMap[username] = ipAddress;
}

QString OptionsDialog::getStatusString(int status)
{
    switch (status) {
        case 0: return "Desconectado";
        case 1: return "Ocupado";
        case 2: return "Activo";
        case 3: return "Inactivo";
        default: return "Desconocido";
    }
}
void OptionsDialog::onAcceptClicked()
{
    // Obtener el texto del elemento seleccionado en el ComboBox
    QString selectedUser = userListView->currentText();
    
    // Verificar si es un usuario válido (no es "General")
    if (selectedUser != "General" && userStatusMap.contains(selectedUser)) {
        int status = userStatusMap[selectedUser];
        QString statusStr = getStatusString(status);
        QString ipAddress = userIPMap.contains(selectedUser) ? userIPMap[selectedUser] : "Desconocida";
        
        // Actualizar las cajas de visualización
        displayBox1->setPlainText(statusStr);
        displayBox2->setPlainText(ipAddress);

    } else {
        // Si es "General" o no hay información
        displayBox1->setPlainText("...");
        displayBox2->setPlainText("...");
    }
}