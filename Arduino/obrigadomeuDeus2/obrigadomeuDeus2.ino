#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

#define SS_PIN         21   // Pino de seleção do módulo RFID
#define RST_PIN        22   // Pino de reset do módulo RFID
#define ATUADOR_01     27   // Pino de controle do atuador 1
#define ATUADOR_02     26   // Pino de controle do atuador 2
#define ATUADOR_03     25   // Pino de controle do atuador 3
#define BUZZER_PIN     13   // Pino de controle do buzzer

const int buttonPin = 2;  // Define o pino ao qual o botão está conectado

// Inicialização do módulo MFRC522 para leitura de RFID
MFRC522 mfrc522(SS_PIN, RST_PIN);

// Credenciais Wi-Fi e URLs do servidor para obter dados dos compartimentos
const char* ssid = "Biel";
const char* password = "g26470907";
const char* serverAddressCompartimento1 = "http://192.168.168.168:3000/Compartimento1";
const char* serverAddressCompartimento2 = "http://192.168.168.168:3000/Compartimento2";
const char* serverAddressCompartimento3 = "http://192.168.168.168:3000/Compartimento3";

bool tagDetectada = false;

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

  // Configurar hora com o servidor NTP
  configTime(-3 * 3600, 0, "pool.ntp.org");

  // Configurar pinos como saída ou entrada
  pinMode(ATUADOR_01, OUTPUT);
  pinMode(ATUADOR_02, OUTPUT);
  pinMode(ATUADOR_03, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(buttonPin, INPUT);

  // Desativa os atuadores (nível alto)
  digitalWrite(ATUADOR_01, HIGH);
  digitalWrite(ATUADOR_02, HIGH);
  digitalWrite(ATUADOR_03, HIGH);

  // Inicia a comunicação SPI e configura o módulo RFID
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
  // Verifica se o botão foi pressionado
  if (digitalRead(buttonPin) == HIGH) {
    delay(1000);                 // Espera um segundo
    ESP.restart();               // Reinicia o ESP32
  }

  // Verifica se há uma nova tag RFID próxima ao leitor
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    Serial.println("Tag detectada! Aguarde a liberação dos remédios...");
    dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);

    // Obtém dados dos compartimentos
    int compartimento1, numAcionamento1, compartimento2, numAcionamento2, compartimento3, numAcionamento3;
    if (obterDadosDoCompartimento(serverAddressCompartimento1, compartimento1, numAcionamento1) &&
        obterDadosDoCompartimento(serverAddressCompartimento2, compartimento2, numAcionamento2) &&
        obterDadosDoCompartimento(serverAddressCompartimento3, compartimento3, numAcionamento3)) {
      
      // Aciona os atuadores simultaneamente
      acionarAtuadoresSimultaneamente(compartimento1, numAcionamento1, compartimento2, numAcionamento2, compartimento3, numAcionamento3);

      // Toca o buzzer após liberar os remédios
      buzzerBeep();
    } else {
      Serial.println("Falha ao obter dados do compartimento.");
    }
    
    tagDetectada = true;
    delay(5000);
    tagDetectada = false;
  }

  // Verifica se o buzzer precisa ser acionado
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Falha ao obter a hora");
    return;
  }

  const char* dias_semana[] = {"Domingo", "Segunda-feira", "Terça-feira", "Quarta-feira", "Quinta-feira", "Sexta-feira", "Sábado"};
  String date = String(timeinfo.tm_mday) + "/" + String(timeinfo.tm_mon + 1) + "/" + String(timeinfo.tm_year + 1900);
  String hour = String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min) + ":" + String(timeinfo.tm_sec);
  String dia_semana = dias_semana[timeinfo.tm_wday];

  Serial.printf("Data: %s - Dia da semana: %s - Hora: %s\n", date.c_str(), dia_semana.c_str(), hour.c_str());

  // Verifica se é hora de ativar o buzzer (por exemplo, às 15:34:00)
  if (timeinfo.tm_hour == 20 && timeinfo.tm_min == 28 && timeinfo.tm_sec == 0 && !tagDetectada) {
    // Aciona o buzzer como uma sirene ligado por 2 segundos, desligado por 1 segundo em loop
    while (!tagDetectada) {
      digitalWrite(BUZZER_PIN, HIGH); // Liga o buzzer
      delay(2000); // Mantém o buzzer ligado por 2 segundos
      digitalWrite(BUZZER_PIN, LOW); // Desliga o buzzer
      delay(1000); // Mantém o buzzer desligado por 1 segundo

      // Verifica se uma tag RFID foi detectada para encerrar a sirene
      if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
        Serial.println("Tag detectada! Encerrando a sirene.");
        tagDetectada = true;
      }
    }
  }

  // Aguarda 1 segundo antes da próxima leitura de tempo
  delay(1000);
}

// Função para obter dados de um compartimento via HTTP
bool obterDadosDoCompartimento(const char* serverAddress, int &compartimento, int &numAtivamento) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverAddress);
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      String payload = http.getString();
      Serial.println(httpResponseCode);
      Serial.println(payload);

      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, payload);
      if (!error) {
        compartimento = doc[0]["numeroCompartimento"];
        numAtivamento = doc[0]["medicamentoId"]["qtdMedicamentoDiario"];
        Serial.print("Compartimento: ");
        Serial.println(compartimento);
        Serial.print("Qtd. Medicamento Diário: ");
        Serial.println(numAtivamento);
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

// Função para acionar os atuadores simultaneamente
void acionarAtuadoresSimultaneamente(int compartimento1, int numAtivamento1, int compartimento2, int numAtivamento2, int compartimento3, int numAtivamento3) {
  int atuadorPin1 = getAtuadorPin(compartimento1);
  int atuadorPin2 = getAtuadorPin(compartimento2);
  int atuadorPin3 = getAtuadorPin(compartimento3);

  for (int i = 0; i < max(numAtivamento1, max(numAtivamento2, numAtivamento3)); i++) {
    if (i < numAtivamento1) digitalWrite(atuadorPin1, LOW);
    if (i < numAtivamento2) digitalWrite(atuadorPin2, LOW);
    if (i < numAtivamento3) digitalWrite(atuadorPin3, LOW);
    delay(1000);
    if (i < numAtivamento1) digitalWrite(atuadorPin1, HIGH);
    if (i < numAtivamento2) digitalWrite(atuadorPin2, HIGH);
    if (i < numAtivamento3) digitalWrite(atuadorPin3, HIGH);
    delay(1000);
  }
  Serial.println("Remédios liberados!");
}

// Função para mapear o número do compartimento ao pino do atuador correspondente
int getAtuadorPin(int compartimento) {
  switch (compartimento) {
    case 1: return ATUADOR_01;
    case 2: return ATUADOR_02;
    case 3: return ATUADOR_03;
    default: return -1;
  }
}

// Função para tocar o buzzer após liberar os remédios
void buzzerBeep() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);
  }
  Serial.println("Buzzer tocou (beep beep)!");
}

// Função para imprimir o UID da tag RFID em formato hexadecimal
void dump_byte_array(byte *buffer, byte bufferSize) {
  Serial.print("UID da tag: ");
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
  Serial.println();
}
