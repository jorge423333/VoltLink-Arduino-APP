// tiene la biblioteca para poder tener wifi autonomo y retornar y cambiarlo
// tiene conexion a firebase y crea nodos y subnodos en /dispositivos haciendo la lista de dispositivos
// crea los nodos y los escribe si esta escrito y si no esta lo crea en /dispositivos
// tiene mensajes de errores y muestra direccion ip mac puerto host iniciacion con firebase
// crea los nodos con Irms power Voltage y borrando crea con todo lo que esta 
// envia los datos a los nodos datos Irms power voltage
// en este caso se quito los subnodos de info y status pero tiene una barrera de cifrado para guardar la informacion de Irms power Voltage
// cifrado de la informacion y creacion de nodos datos info status
// tiene la funcion relestatus y rele pin donde funciona si si o si no
#include <WiFiManager.h>
#include <FirebaseESP32.h>
#include <EmonLib.h>
#include <ZMPT101B.h>

#define FIREBASE_HOST "graphite-scout-266200-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "AIzaSyBhb7GEY6Zfy9N0zh6Lu3tN2BOrrTKzXfk"
FirebaseData firebaseData;
WiFiManager wifiManager;

const int llavePin = 32;
const int relePin = 27;
const int voltagePin = 33;
#define CURRENT_SENSITIVITY 1.0f
#define VOLTAGE_SENSITIVITY 500.0f

EnergyMonitor energyMonitor;
ZMPT101B voltageSensor(voltagePin, 50.0);

String claveSecreta = "LXOP12459342";
String idDispositivo = "VOLT01";
String rutaFirebase = "/dispositivos/" + idDispositivo + "/" + claveSecreta;

unsigned long prevMillis = 0;
const long intervalo = 1000;

void verificaYCreaNodos() {
    String pathDatos = rutaFirebase + "/datos";
    
    if (!Firebase.getBool(firebaseData, pathDatos)) {
        FirebaseJson json; // Creamos una instancia vacía
        if (!Firebase.set(firebaseData, pathDatos, json)) {
            Serial.println("Error al crear el nodo 'datos' en Firebase.");
        }
    }
    
    // Ahora `pathDatos` es accesible aquí
    String pathEstadoRele = pathDatos + "/estadoRele";
    
    if (!Firebase.getBool(firebaseData, pathEstadoRele)) {
        if (!Firebase.setString(firebaseData, pathEstadoRele, "OFF")) {
            Serial.println("Error al crear el nodo 'estadoRele' en Firebase.");
        }
    }
}


void updateStatusFirebase() {
    String rutaStatusFirebase = rutaFirebase + "/status";
    FirebaseJson statusJson;
    statusJson.add("status", "en linea");
    statusJson.add("timestamp", String(millis()));
    if (!Firebase.updateNode(firebaseData, rutaStatusFirebase, statusJson)) {
        Serial.println("Error al actualizar el estado en Firebase.");
    }
}

void controlRele() {
    String path = rutaFirebase + "/datos/estadoRele";
    if (Firebase.getString(firebaseData, path)) {
        String estado = firebaseData.stringData(); // Obtenemos el valor directamente del objeto firebaseData
        if (estado == "ON") {
            digitalWrite(relePin, LOW);
            Serial.println("react: ON");
        } else if (estado == "OFF") {
            digitalWrite(relePin, HIGH);
            Serial.println("react: OFF");
        } else {
            Serial.println("Estado desconocido del relé.");
        }
    } else {
        Serial.println("Error al obtener el estado del relé desde Firebase: " + firebaseData.errorReason());
    }
}

void crearActualizarInfo() {
    String infoPath = rutaFirebase + "/info";
    FirebaseJson infoJson;

    // Agregar los campos que desee al subnodo de información
    infoJson.add("correo_asignado", "");  // Debes especificar el correo aquí o configurarlo de alguna manera
    infoJson.add("usuario_id", "");       // Configura el ID de usuario
    infoJson.add("mac", WiFi.macAddress());// Dirección MAC del dispositivo
    infoJson.add("nombre_usuario", "");   // Establece el nombre del usuario
    infoJson.add("nombre", idDispositivo);// Establece el nombre del dispositivo
    infoJson.add("password", claveSecreta);// La clave secreta como contraseña
    infoJson.add("wifi", WiFi.SSID());    // Nombre de la red WiFi a la que está conectado

    // Actualizar o crear el subnodo en Firebase
    if (!Firebase.updateNode(firebaseData, infoPath, infoJson)) {
        Serial.println("Error al configurar la información inicial en Firebase.");
    } else {
        Serial.println("Información del dispositivo actualizada en Firebase.");
    }
}

void setup() {
    pinMode(relePin, OUTPUT);
    digitalWrite(relePin, LOW);
    Serial.begin(115200);
    
    WiFiManagerParameter custom_firebase_host("host", "Firebase Host", FIREBASE_HOST, 40);
    WiFiManagerParameter custom_firebase_auth("auth", "Firebase Auth", FIREBASE_AUTH, 40);
    wifiManager.addParameter(&custom_firebase_host);
    wifiManager.addParameter(&custom_firebase_auth);

    Serial.print("Conectando a la red WiFi");
    if (!wifiManager.autoConnect("MiESP32")) {
        Serial.println("\nFallo al conectar. Reiniciando...");
        delay(3000);
        ESP.restart();
    }

    Serial.println("\nConectado a la red WiFi: " + WiFi.SSID());
    Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
    if (!Firebase.ready()) {
        Serial.println("Error al iniciar Firebase");
        while (true) {
            delay(1000);
            Serial.println("Esperando conexión a Firebase...");
        }
    } else {
        Serial.println("Conectado a Firebase");
    }

    Serial.println("The Actual Voltage: 220.0 V");
    energyMonitor.current(llavePin, CURRENT_SENSITIVITY);
    voltageSensor.setSensitivity(VOLTAGE_SENSITIVITY);

    verificaYCreaNodos();  // Verifica y crea los nodos necesarios en Firebase
    crearActualizarInfo();
}

void loop() {
  
    // Leer del Monitor Serie
    if (Serial.available()) {
        String entrada = Serial.readStringUntil('\n');
        entrada.trim();
        
        if (entrada == "ON" || entrada == "OFF") {
            String path = rutaFirebase + "/datos/estadoRele";
            if (Firebase.setString(firebaseData, path, entrada)) {
                Serial.println("Cambio de estado guardado en Firebase: " + entrada);
            } else {
                Serial.println("Error al guardar el cambio de estado en Firebase.");
            }
        } else {
            Serial.println("Entrada no reconocida. Utilice 'ON' o 'OFF'.");
        }
    }

    // Actualizar Medidas
    double Irms = energyMonitor.calcIrms(1500);
    float voltage = 1.1 * voltageSensor.getRmsVoltage();
    double power = Irms * voltage;

    Serial.print("Potencia = ");
    Serial.print(power);
    Serial.print(" W    Irms = ");
    Serial.print(Irms);
    Serial.print(" A    Voltage = ");
    Serial.println(voltage);

    if (voltage >= 240.0) {
     //   digitalWrite(llavePin, HIGH);
        Serial.println("Tensión alta");
    } else if (voltage <= 110.0) {
      //  digitalWrite(llavePin, HIGH);
        Serial.println("Tensión baja");
    } else if (Irms >= 2.0) {
     //   digitalWrite(llavePin, HIGH);
        Serial.println("Sobre Corriente");
    } else {
     //   digitalWrite(llavePin, LOW);
        Serial.println("Todo bien");
    }

    String devicePath = rutaFirebase + "/datos";
    FirebaseJson json;
    json.add("Irms", String(Irms, 2));
    json.add("Power", String(power, 2));
    json.add("Voltage", String(voltage,2));
    if (!Firebase.updateNode(firebaseData, devicePath, json)) {
        Serial.println("Error al actualizar los datos en Firebase.");
    }

    if (millis() - prevMillis >= intervalo) {
        prevMillis = millis();
        updateStatusFirebase();
    }

    controlRele();

    delay(500);
}




