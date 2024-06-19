#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// Definições de pinos para os atuadores e outros componentes
#define SS_PIN         21
#define RST_PIN        22
#define ATUADOR_01     27
#define ATUADOR_02     26
#define ATUADOR_03     25
#define BUZZER_PIN     13

const int buttonPin = 2;  // Define o pino ao qual o botão está conectado
const int ledPin = 13;    // Define o pino ao qual um LED está conectado (opcional)

// Inicialização do módulo MFRC522 para leitura de RFID
MFRC522 mfrc522(SS_PIN, RST_PIN);

// Credenciais e endereço do servidor
const char* ssid = "Biel";
const char* password = "g26470907";
const char* serverAddressCompartimento1 = "http://192.168.81.168:3000/Compartimento1";
const char* serverAddressCompartimento2 = "http://192.168.81.168:3000/Compartimento2";
const char* serverAddressCompartimento3 = "http://10.0.0.106:3000/Compartimento3";

bool tagDetectada = false; // Variável global para monitorar se uma tag foi detectada

void setup() {
  Serial.begin(115200);
  Serial.println("Inicializando...");

  // Conectar ao WiFi
  WiFi.begin(ssid, password);
  Serial.print("Conectando a ");
  Serial.println(ssid);

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Falha ao conectar ao WiFi. Reiniciando...");
    ESP.restart();
  }

  Serial.println("WiFi conectado");
  Serial.print("Endereço IP: ");
  Serial.println(WiFi.localIP());

  // Configurar hora
  configTime(-3 * 3600, 0, "pool.ntp.org");

  // Configurar pinos
  pinMode(ATUADOR_01, OUTPUT);
  pinMode(ATUADOR_02, OUTPUT);
  pinMode(ATUADOR_03, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT);

  digitalWrite(ATUADOR_01, HIGH);
  digitalWrite(ATUADOR_02, HIGH);
  digitalWrite(ATUADOR_03, HIGH);

  // Iniciar SPI e RFID
  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println("Inicializando MFRC522...");

  if (mfrc522.PCD_PerformSelfTest()) {
    Serial.println("MFRC522 inicializado com sucesso!");
  } else {
    Serial.println("Falha ao inicializar o MFRC522. Verifique as conexões.");
  }
}

void loop() {
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    Serial.println("Tag detectada! Aguarde a liberação dos remédios...");
    dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);

    int compartimento1 = 0;
    int numAcionamento1 = 0;
    int compartimento2 = 0;
    int numAcionamento2 = 0;
    if (obterDadosDoCompartimento1(compartimento1, numAcionamento1) && obterDadosDoCompartimento2(compartimento2, numAcionamento2)) {
      acionarAtuador(compartimento1, numAcionamento1);
      delay(1000);
      acionarAtuador(compartimento2, numAcionamento2);
      Serial.println("Remédios liberados!");
    } else {
      Serial.println("Falha ao obter dados do compartimento.");
    }
    
    tagDetectada = true; // Marca a tag como detectada
    
    // Aguarde um pouco antes de permitir a próxima leitura da tag
    delay(5000);
    tagDetectada = false; // Reseta a marcação após o delay
  }

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 5000)) {
    Serial.println("Falha ao obter a hora");
    delay(1000);
    return;
  }

  const char* dias_semana[] = {"Domingo", "Segunda-feira", "Terça-feira", "Quarta-feira", "Quinta-feira", "Sexta-feira", "Sábado"};
  String date = String(timeinfo.tm_mday) + "/" + String(timeinfo.tm_mon + 1) + "/" + String(timeinfo.tm_year + 1900);
  String hour = String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min) + ":" + String(timeinfo.tm_sec);
  String dia_semana = dias_semana[timeinfo.tm_wday];

  Serial.printf("Data: %s - Dia da semana: %s - Hora: %s\n", date.c_str(), dia_semana.c_str(), hour.c_str());

  // Verifica se é hora de ativar o buzzer (por exemplo, às 11:17:00)
  if (timeinfo.tm_hour == 11 && timeinfo.tm_min == 17 && timeinfo.tm_sec == 0 && !tagDetectada) {
    // Aciona o buzzer como uma sirene ligado por 2 segundos, desligado por 1 segundo em loop
    while (!tagDetectada) {
      digitalWrite(BUZZER_PIN, HIGH); // Liga o buzzer
      Serial.println("Buzzer ligado.");
      delay(2000); // Mantém o buzzer ligado por 2 segundos
      digitalWrite(BUZZER_PIN, LOW); // Desliga o buzzer
      Serial.println("Buzzer desligado.");
      delay(1000); // Mantém o buzzer desligado por 1 segundo

      // Verifica se uma tag RFID foi detectada para encerrar a sirene
      if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
        Serial.println("Tag detectada! Encerrando a sirene.");
        tagDetectada = true;
      }
    }
  }

  if (digitalRead(buttonPin) == HIGH) {
    // Se o botão for pressionado
    digitalWrite(ledPin, HIGH);  // Ligar o LED (opcional)
    delay(1000);                 // Esperar um segundo
    ESP.restart();               // Reiniciar o ESP32
  }

  // Aguarda 1 segundo antes da próxima leitura de tempo
  delay(1000);
}

bool obterDadosDoCompartimento1(int &compartimento1, int &numAtivamento1) {
  return obterDadosDoCompartimento(serverAddressCompartimento1, compartimento1, numAtivamento1);
}

bool obterDadosDoCompartimento2(int &compartimento2, int &numAtivamento2) {
  return obterDadosDoCompartimento(serverAddressCompartimento2, compartimento2, numAtivamento2);
}

bool obterDadosDoCompartimento(const char* serverAddress, int &compartimento, int &numAtivamento) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverAddress);
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      String payload = http.getString();
      Serial.println(httpResponseCode);
      Serial.println(payload);

      const size_t capacity = JSON_ARRAY_SIZE(1) + 3*JSON_OBJECT_SIZE(7) + 60;
      DynamicJsonDocument doc(capacity);

      DeserializationError error = deserializeJson(doc, payload);
      if (!error) {
        for (JsonObject obj : doc.as<JsonArray>()) {
          compartimento = obj["numeroCompartimento"];
          numAtivamento = obj["medicamentoId"]["qtdMedicamentoDiario"];
          Serial.print("Compartimento: ");
          Serial.println(compartimento);
          Serial.print("Qtd. Medicamento Diário: ");
          Serial.println(numAtivamento);
        }
        return true;
      } else {
        Serial.print("Erro ao parsear JSON: ");
        Serial.println(error.c_str());
        return false;
      }
    } else {
      Serial.print("Erro na requisição HTTP: ");
      Serial.println(httpResponseCode);
      return false;
    }
    http.end();
  }
  return false;
}

void acionarAtuador(int compartimento, int numAtivamento) {
  int atuadorPin = getAtuadorPin(compartimento);
  if (atuadorPin != -1) {
    for (int i = 0; i < numAtivamento; i++) {
      digitalWrite(atuadorPin, LOW);
      delay(500);
      digitalWrite(atuadorPin, HIGH);
      delay(500);
    }
  } else {
    Serial.println("Compartimento inválido");
  }
}

int getAtuadorPin(int compartimento) {
  switch (compartimento) {
    case 1:
      return ATUADOR_01;
    case 2:
      return ATUADOR_02;
    case 3:
      return ATUADOR_03;
    default:
      return -1;
  }
}

void dump_byte_array(byte *buffer, byte bufferSize) {
  Serial.print("UID da tag: ");
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
  Serial.println();
}
