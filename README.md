# Aplicación de Chat

## Descripción General
Este proyecto es una aplicación de chat cliente-servidor desarrollada en C++ utilizando el framework Qt. Permite a los usuarios conectarse a un servidor de chat, enviar mensajes a todos los usuarios (difusión) o directamente a usuarios específicos (mensajes privados), cambiar su estado y ver información sobre otros usuarios conectados.

## Características
- **Registro de Usuarios**: Conectarse al servidor con un nombre de usuario único
- **Chat General**: Enviar y recibir mensajes visibles para todos los usuarios conectados
- **Mensajería Privada**: Enviar y recibir mensajes directos a/de usuarios específicos
- **Estado de Usuario**: Establecer tu estado como Activo, Ocupado o Inactivo
- **Información de Usuario**: Ver estado y detalles de los usuarios conectados
- **Detección Automática de Inactividad**: El estado cambia a Inactivo después de 40 segundos sin actividad
- **Historial de Chat**: Ver historial de mensajes tanto para chats generales como privados
- **Interfaz Amigable**: Áreas separadas para chats generales y privados

## Componentes

### Cliente
- **Ventana Principal del Cliente**: Proporciona interfaz para conexión, mensajería y gestión de estado
- **Diálogo de Opciones**: Muestra información detallada del usuario y estado de conexión
- **Diálogo de Ayuda**: Proporciona instrucciones para usar la aplicación
- **Manejador de Mensajes**: Gestiona el envío/recepción de mensajes usando WebSockets

### Protocolo de Comunicación
La aplicación utiliza WebSockets para la comunicación cliente-servidor con diferentes tipos de mensajes:
- Tipo 1: Solicitar lista de usuarios
- Tipo 2: Solicitar información de usuario
- Tipo 3: Cambiar estado de usuario
- Tipo 4: Enviar mensaje de chat
- Tipo 5: Solicitar historial de chat

## Requisitos
- C++11 o superior
- Qt 5.12 o superior
- Módulo QtWebSockets

## Instrucciones de Compilación

### Prerrequisitos
- Entorno de desarrollo Qt (se recomienda Qt Creator)
- Módulo Qt WebSockets instalado

### Compilando el Cliente
1. Abrir el proyecto en Qt Creator
2. Configurar el proyecto para tu plataforma
3. Compilar el proyecto usando el menú Compilar o Ctrl+B

Alternativamente, usando la línea de comandos:
```bash
qmake
make
```


## Guía de Uso

### Chat General
- Los mensajes enviados en el panel izquierdo se difunden a todos los usuarios conectados
- Escribe tu mensaje en el campo de entrada y haz clic en "Enviar" o presiona Enter

### Chat Privado
1. Selecciona un usuario del menú desplegable en el panel derecho
2. Escribe tu mensaje en el campo de entrada
3. Haz clic en "Enviar" o presiona Enter

### Cambio de Estado
- Utiliza el menú desplegable de estado para seleccionar:
  - **Activo**: Recibirás todos los mensajes inmediatamente
  - **Ocupado**: Los mensajes se almacenarán pero no se mostrarán hasta que cambies tu estado
  - **Inactivo**: Se establece automáticamente después de 40 segundos de inactividad

### Visualización de Información de Usuario
1. Haz clic en el botón "Opciones" para abrir el diálogo de Opciones
2. Selecciona un usuario del menú desplegable
3. Haz clic en "Aceptar" para ver su estado
4. Haz clic en "Ver Todos los Usuarios" para ver todos los usuarios conectados y sus estados

### Ayuda
Haz clic en el botón "Ayuda" para acceder a instrucciones detalladas sobre cómo usar la aplicación.

### Desconexión
Haz clic en el botón "Desconectar" para cerrar sesión de forma segura del servidor.

## Detalles de Implementación

### Manejo de Mensajes
- Los mensajes se gestionan a través de la clase MessageHandler
- WebSockets proporcionan comunicación en tiempo real con el servidor
- El historial de mensajes se almacena localmente mientras la aplicación está en ejecución

### Gestión de Estado de Usuario
- El estado se sincroniza con el servidor
- El temporizador de inactividad cambia automáticamente el estado a "Inactivo" después de 40 segundos
- El estado "Ocupado" almacenará los mensajes entrantes pero no los mostrará inmediatamente

### Estructura de UI
- Interfaz dividida con chat general a la izquierda, chat privado a la derecha
- Menú desplegable para seleccionar destinatarios de mensajes
- Indicador de estado muestra el estado actual del usuario
- Notificaciones para nuevos mensajes, cambios de estado y conexiones de usuarios

## Notas
- Los usuarios no pueden enviar mensajes a usuarios desconectados
- Los mensajes están limitados a 255 caracteres
- La aplicación muestra notificaciones cuando los usuarios se conectan, desconectan o cambian de estado
- El historial de chat se conserva durante la sesión

Este cliente de chat está diseñado para trabajar con un servidor que implemente el mismo protocolo y puede comunicarse con servidores desarrollados por otros equipos siguiendo la misma especificación.