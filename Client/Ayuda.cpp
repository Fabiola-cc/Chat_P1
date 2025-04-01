#include "Ayuda.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>

Ayuda::Ayuda(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Ayuda del Chat");
    setMinimumSize(600, 400);
    
    if (parent) {
        // Obtener la geometría de la ventana principal
        QRect parentGeometry = parent->geometry();
        
        // Calcular la posición para nuestra ventana (a la derecha)
        int newX = parentGeometry.x() + parentGeometry.width() + 10;
        int newY = parentGeometry.y();
        
        // Establecer la posición
        move(newX, newY);
    }
    
    setupUI();
    
    // Use regular Qt connection syntax instead of the newer Qt5 syntax
    connect(closeButton, SIGNAL(clicked()), this, SLOT(close()));
}

Ayuda::~Ayuda()
{
    // Limpieza si es necesario
}

void Ayuda::setupUI()
{
    // Layout principal
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Crear widget de pestañas
    tabWidget = new QTabWidget(this);
    
    // Pestaña de ayuda general
    generalHelpText = new QTextEdit(this);
    generalHelpText->setReadOnly(true);
    generalHelpText->setHtml(
        "<h2>Bienvenido a la Aplicación de Chat</h2>"
        "<p>Esta aplicación le permite comunicarse con otros usuarios a través de mensajes de texto en tiempo real.</p>"
        "<p>Funcionalidades principales:</p>"
        "<ul>"
        "<li>Chat general para todos los usuarios conectados</li>"
        "<li>Mensajes privados entre usuarios específicos</li>"
        "<li>Estados de usuario configurables (Activo, Ocupado, Inactivo)</li>"
        "<li>Lista de usuarios conectados en tiempo real</li>"
        "<li>Historial de conversaciones</li>"
        "</ul>"
        "<p>Explore las pestañas de esta ventana de ayuda para obtener más información sobre cada característica.</p>"
    );
    tabWidget->addTab(generalHelpText, "General");
    
    // Pestaña de ayuda sobre conexión
    connectionHelpText = new QTextEdit(this);
    connectionHelpText->setReadOnly(true);
    connectionHelpText->setHtml(
        "<h2>Conexión al Servidor</h2>"
        "<p>Para utilizar la aplicación de chat, primero debe conectarse a un servidor:</p>"
        "<ol>"
        "<li>Ingrese la dirección del servidor en el campo 'Host' (por defecto: localhost)</li>"
        "<li>Ingrese el puerto del servidor en el campo 'Puerto' (por defecto: 8080)</li>"
        "<li>Escriba el nombre de usuario que desea utilizar</li>"
        "<li>Haga clic en el botón 'Conectar'</li>"
        "</ol>"
        "<p>Una vez conectado, podrá ver la interfaz de chat completa con la lista de usuarios y las áreas de chat.</p>"
        "<p><strong>Nota:</strong> Si el nombre de usuario ya está en uso, recibirá un mensaje de error.</p>"
        "<p><strong>Importante:</strong> Si se desconecta del servidor, ya sea voluntariamente o por problemas de conexión, "
        "tendrá que volver a conectarse siguiendo los pasos anteriores.</p>"
    );
    tabWidget->addTab(connectionHelpText, "Conexión");
    
    // Pestaña de ayuda sobre el chat
    chatHelpText = new QTextEdit(this);
    chatHelpText->setReadOnly(true);
    chatHelpText->setHtml(
        "<h2>Uso del Chat</h2>"
        "<p>La aplicación tiene dos áreas de chat:</p>"
        "<h3>Chat General</h3>"
        "<p>El chat general aparece en el lado izquierdo de la pantalla. Los mensajes enviados aquí son visibles para todos los usuarios conectados.</p>"
        "<ol>"
        "<li>Escriba su mensaje en el campo de texto debajo del área de chat general</li>"
        "<li>Presione el botón 'Enviar' o la tecla Enter para enviar el mensaje</li>"
        "</ol>"
        "<h3>Chat Privado</h3>"
        "<p>El chat privado aparece en el lado derecho de la pantalla. Permite enviar mensajes a un usuario específico.</p>"
        "<ol>"
        "<li>Seleccione el usuario al que desea enviar un mensaje de la lista desplegable</li>"
        "<li>Escriba su mensaje en el campo de texto</li>"
        "<li>Presione el botón 'Enviar' o la tecla Enter para enviar el mensaje</li>"
        "</ol>"
        "<p><strong>Notificaciones:</strong> Cuando reciba un mensaje privado de un usuario que no es su interlocutor actual, "
        "verá una notificación en la parte superior de la ventana.</p>"
        "<p><strong>Limitaciones:</strong></p>"
        "<ul>"
        "<li>No puede enviar mensajes a usuarios desconectados</li>"
        "<li>Los usuarios en estado 'Ocupado' no recibirán sus mensajes inmediatamente, pero los verán cuando cambien su estado</li>"
        "<li>Los mensajes están limitados a 255 caracteres</li>"
        "</ul>"
    );
    tabWidget->addTab(chatHelpText, "Chat");
    
    // Pestaña de ayuda sobre estados
    statusHelpText = new QTextEdit(this);
    statusHelpText->setReadOnly(true);
    statusHelpText->setHtml(
        "<h2>Estados de Usuario</h2>"
        "<p>Puede cambiar su estado para indicar a otros usuarios su disponibilidad:</p>"
        "<ul>"
        "<li><strong>Activo:</strong> Indica que está disponible para chatear. Recibirá todos los mensajes inmediatamente.</li>"
        "<li><strong>Ocupado:</strong> Indica que está conectado pero prefiere no ser interrumpido. No recibirá nuevos mensajes mientras "
        "esté en este estado, pero se guardarán para cuando cambie a Activo.</li>"
        "<li><strong>Inactivo:</strong> Indica que está ausente temporalmente. La aplicación cambiará automáticamente su estado a "
        "Inactivo después de 40 segundos sin actividad.</li>"
        "<li><strong>Desconectado:</strong> Estado asignado a usuarios que se han desconectado del servidor. No puede seleccionar "
        "este estado manualmente.</li>"
        "</ul>"
        "<p>Para cambiar su estado:</p>"
        "<ol>"
        "<li>Seleccione el estado deseado en el menú desplegable de estados en la parte superior de la ventana</li>"
        "</ol>"
        "<p>Cuando un usuario cambia su estado, todos los usuarios conectados reciben una notificación.</p>"
        "<p>Puede ver el estado de todos los usuarios a través de la ventana de 'Opciones'.</p>"
    );
    tabWidget->addTab(statusHelpText, "Estados");
    
    // Pestaña de ayuda sobre opciones
    optionsHelpText = new QTextEdit(this);
    optionsHelpText->setReadOnly(true);
    optionsHelpText->setHtml(
        "<h2>Ventana de Opciones</h2>"
        "<p>La ventana de Opciones proporciona información adicional y funcionalidades:</p>"
        "<ol>"
        "<li>Haga clic en el botón 'Opciones' en la ventana principal para abrir esta ventana</li>"
        "</ol>"
        "<h3>Funcionalidades:</h3>"
        "<ul>"
        "<li><strong>Ver Estados de Usuario:</strong> Seleccione un usuario de la lista desplegable y haga clic en 'Aceptar' para "
        "ver su estado actual (Activo, Ocupado, Inactivo o Desconectado)</li>"
        "<li><strong>Ver Todos los Usuarios:</strong> Haga clic en el botón 'Ver Todos los Usuarios' para mostrar una lista "
        "completa de todos los usuarios y sus estados actuales</li>"
        "</ul>"
        "<p>Esta ventana se mantiene abierta de forma independiente, lo que le permite consultar información mientras "
        "continúa usando la ventana principal de chat.</p>"
        "<p>Para cerrar la ventana de Opciones, simplemente haga clic en el botón 'Salir'.</p>"
    );
    tabWidget->addTab(optionsHelpText, "Opciones");
    
    // Añadir widget de pestañas al layout principal
    mainLayout->addWidget(tabWidget);
    
    // Botón de cierre
    closeButton = new QPushButton("Cerrar", this);
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);
    mainLayout->addLayout(buttonLayout);
    
    // Establecer layout
    setLayout(mainLayout);
}