#include <SPI.h>
#include <MFRC522.h> //Biblioteca do RFID
#include <WiFi.h> // Biblioteca do WiFi
#include <HTTPClient.h>
#include <time.h>

#define SS_PIN 21
#define RST_PIN 22
#define ATUADOR_01 27
#define ATUADOR_02 26
#define ATUADOR_03 25
#define BUZZER_PIN 13  // Pino ao qual o buzzer está conectado
#define NUM_ATUADORES 3
#define BOTAO_RESET 34

MFRC522 mfrc522(SS_PIN, RST_PIN);

// WiFi Credentials
const char* ssid = "iPhone de Filipe";
const char* password = "filipe18";

const char* serverAddress = "http://172.20.10.5:3000/Medicamento";
bool tagDetectada = false;

void setup() {
  Serial.begin(115200);

  // Conecta-se à rede WiFi
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

  // Configura o servidor de tempo NTP
  configTime(-3 * 3600, 0, "pool.ntp.org");

  // Inicializa os pinos dos atuadores RFID e do buzzer
  pinMode(ATUADOR_01, OUTPUT);
  pinMode(ATUADOR_02, OUTPUT);
  pinMode(ATUADOR_03, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BOTAO_RESET, INPUT);

  digitalWrite(ATUADOR_01, HIGH);  // Desliga o atuador 01
  digitalWrite(ATUADOR_02, HIGH);  // Desliga o atuador 02
  digitalWrite(ATUADOR_03, HIGH);  // Desliga o atuador 03

  // Inicializa o leitor RFID
  SPI.begin();
  mfrc522.PCD_Init();
}

void loop() {
  // Verifica se uma tag RFID está presente e realiza a leitura
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    Serial.println("Tag detectada! Aguarde a liberação dos remédios...");

    // Identifica o compartimento da tag RFID
    int compartimento = 1; // Altere para o compartimento correto
    int num_acionamentos = (1, 2, 3); // Obtém a quantidade de medicamento diário

    // Realiza os acionamentos do atuador de acordo com a quantidade de medicamento diário
    for (int i = 0; i < NUM_ATUADORES; i++) {
      if (i < num_acionamentos) {
        digitalWrite(getAtuadorPin(i), LOW);  // Liga o atuador

        // Aguarda um tempo entre os acionamentos
        delay(1000);

        digitalWrite(getAtuadorPin(i), HIGH);  // Desliga o atuador
      }
    }

    // Atualiza a quantidade de medicamento restante no compartimento
    updateMedicationQuantity(compartimento, num_acionamentos);

    Serial.println("Remédios liberados!");
    delay(5000);  // Aguarda um momento antes de permitir a leitura de outra tag
  }

  // Verifica se o botão de reset foi pressionado
  if (digitalRead(BOTAO_RESET) == HIGH) {
    Serial.println("Reinicializando o sistema...");
    delay(1000);    // Espera um segundo
    ESP.restart();  // Reinicia o ESP32
  }
}

int getMedicationQuantity(int compartimento) {
  // Realiza uma solicitação GET para obter os dados do compartimento correspondente
  HTTPClient http;
  String url = String(serverAddress) + "/" + String(compartimento);
  http.begin(url);

  int quantidade = 0;

  // Verifica se a solicitação foi bem-sucedida
  int httpResponseCode = http.GET();
  if (httpResponseCode == HTTP_CODE_OK) {
    String payload = http.getString(); // Obtém o corpo da resposta
    Serial.println("Resposta da API:");
    Serial.println(payload); // Log da resposta da API

    // Extrai a quantidade de medicamento diário do JSON
    quantidade = extractMedicationQuantity(payload);
  } else {
    Serial.print("Falha ao obter a quantidade de medicamento. Código de erro: ");
    Serial.println(httpResponseCode);
  }

  // Fecha a conexão HTTP
  http.end();

  return quantidade;
}

int extractMedicationQuantity(String payload) {
  // Fazer o parsing do JSON e extrair a quantidade de medicamento diário
  // Por exemplo, se a estrutura for {"qtdMedicamentoDiario": 3}, você pode fazer assim:

  int quantidade = 0;

  // Encontra a posição da chave "qtdMedicamentoDiario"
  int startIndex = payload.indexOf("\"qtdMedicamentoDiario\":");
  if (startIndex != -1) {
    // Move o índice para o início do valor
    startIndex += strlen("\"qtdMedicamentoDiario\":");

    // Encontra a posição do próximo caractere não numérico
    int endIndex = startIndex;
    while (endIndex < payload.length() && isDigit(payload[endIndex])) {
      endIndex++;
    }

    // Extrai a substring que contém o valor
    String quantidadeStr = payload.substring(startIndex, endIndex);

    // Converte a string para um inteiro
    quantidade = quantidadeStr.toInt();
  }

  return quantidade;
}

void updateMedicationQuantity(int compartimento, int quantidade) {
  // Cria um objeto JSON para enviar à API
  String jsonData = "{\"compartimento\": " + String(compartimento) + ", \"qtdMedicamentoAdd\": " + String(quantidade) + "}";

  HTTPClient http;

  // Configura a URL e os cabeçalhos HTTP
  http.begin(serverAddress);

  // Define o tipo de conteúdo como aplicativo JSON
  http.addHeader("Content-Type", "application/json");

  // Envia a solicitação POST com os dados JSON
  int httpResponseCode = http.POST(jsonData);

  // Verifica a resposta do servidor
  if (httpResponseCode > 0) {
    String response = http.getString(); // Obtém a resposta do servidor
    Serial.println("Resposta do servidor:");
    Serial.println(response); // Log da resposta do servidor
  } else {
    Serial.print("Erro ao enviar a quantidade de medicamento. Código de erro: ");
    Serial.println(httpResponseCode);
  }

  // Fecha a conexão HTTP
  http.end();
}

int getAtuadorPin(int index) {
  // Retorna o pino do atuador com base no índice
  switch (index) {
    case 0:
      return ATUADOR_01;
    case 1:
      return ATUADOR_02;
    case 2:
      return ATUADOR_03;
    default:
      return -1; // Se o índice estiver fora do intervalo
  }
}