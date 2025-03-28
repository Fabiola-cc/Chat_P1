#include "OptionsDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>

OptionsDialog::OptionsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Nueva_pantalla_opciones");
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
    
    // Connect signal and slot para el botón de salir
    connect(exitButton, &QPushButton::clicked, this, &OptionsDialog::close);
}

OptionsDialog::~OptionsDialog()
{
    // Cleanup if necessary
}

void OptionsDialog::setupUI()
{
    // Main layout
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Área central en blanco
    QLabel *emptyLabel = new QLabel(this);
    emptyLabel->setMinimumHeight(200);
    mainLayout->addWidget(emptyLabel);
    
    // Solo un botón de salir en la parte inferior
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    
    exitButton = new QPushButton("Salir", this);
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(exitButton);
    
    mainLayout->addLayout(buttonLayout);
    
    // Set layout
    setLayout(mainLayout);
}