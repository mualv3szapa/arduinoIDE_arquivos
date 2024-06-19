#include <SPI.h>
#include <MFRC522.h> // Biblioteca RFID
#include <WiFi.h> // Biblioteca do WiFi
#include <HTTPClient.h> // Biblioteca para realizar operações HTTP
#include <time.h> // Biblioteca data e hora. Obs: pode ser retirada ao usar data e hora da API

int estado; // RFID

#define SS_PIN         21 // RFID
#define RST_PIN        22 // RFID
#define ATUADOR_01     27 // Atuador 01
#define ATUADOR_02     26 // Atuador 02
#define ATUADOR_03     25 // Atuador 03
#define BUZZER_PIN     13 // Pino ao qual o buzzer está conectado
#define NUM_ATUADORES  3 // Quantidade de atuadores

MFRC522 mfrc522(SS_PIN, RST_PIN);

// Credenciais do WiFi
const char* ssid = "Biel";
const char* password = "g26470907";

const char* serverAddress = "http://192.168.163.168:3000/Medicamento"; // Servidor

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

  // Configura o servidor de tempo NTP.
  configTime(-3 * 3600, 0, "pool.ntp.org");

  // Realiza uma requisição HTTP GET
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    http.begin(serverAddress); // URL para requisição
    int httpCode = http.GET(); // Realiza a requisição GET

    // Verifica o código de resposta HTTP
    if (httpCode > 0) {
      // A resposta foi recebida
      String payload = http.getString();
      Serial.println(httpCode);
      Serial.println(payload);
    } else {
      Serial.println("Erro na requisição");
    }

    http.end(); // Fecha a conexão
  }

  // Inicializa os pinos dos atuadores RFID e do buzzer
  pinMode(ATUADOR_01, OUTPUT);
  pinMode(ATUADOR_02, OUTPUT);
  pinMode(ATUADOR_03, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(ATUADOR_01, HIGH); // Desliga o atuador 01
  digitalWrite(ATUADOR_02, HIGH); // Desliga o atuador 02
  digitalWrite(ATUADOR_03, HIGH); // Desliga o atuador 03

  // Inicializa o leitor RFID
  SPI.begin();
  mfrc522.PCD_Init();
}

void loop() {
  // Verifica se uma tag RFID está presente e realiza a leitura
  estado=digitalRead(21);
  Serial.println(estado);
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    Serial.println("Tag detectada! Aguarde a liberação dos remédios...");

    // Identifica o compartimento da tag RFID
    int compartimento = 1; // Altere para o compartimento correto
    int num_acionamentos = getMedicationQuantity(compartimento); // Obtém a quantidade de medicamento diário

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

    // Reseta a flag de detecção da tag RFID
    tagDetectada = false;
  }

  // Verifica se o buzzer precisa ser acionado
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
  if (timeinfo.tm_hour == 19 && timeinfo.tm_min == 57 && timeinfo.tm_sec == 0 && !tagDetectada) {
    // Aciona o buzzer como uma sirene ligada por 2 segundos, desligada por 1 segundo em loop
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

int getMedicationQuantity(int compartimento) {
  // Realiza uma solicitação GET para obter todos os dados dos medicamentos
  HTTPClient http;
  http.begin(serverAddress);

  int quantidade = 0;

  // Verifica se a solicitação foi bem-sucedida
  int httpResponseCode = http.GET();
  if (httpResponseCode == HTTP_CODE_OK) {
    String payload = http.getString(); // Obtém o corpo da resposta
    Serial.println("Resposta da API:");
    Serial.println(payload); // Log da resposta da API

    // Extrai a quantidade de medicamento diário do JSON do compartimento específico
    quantidade = extractMedicationQuantity(payload, compartimento);
  } else {
    Serial.print("Falha ao obter os dados dos medicamentos. Código de erro: ");
    Serial.println(httpResponseCode);
  }

  // Fecha a conexão HTTP
  http.end();

  return quantidade;
}

int extractMedicationQuantity(String payload, int compartimento) {
  // Fazer o parsing do JSON e extrair a quantidade de medicamento diário do compartimento específico

  int quantidade = 0;

  // Construa sua lógica para filtrar os dados do compartimento específico
  // Você pode usar bibliotecas como ArduinoJson para analisar o JSON

  // Por exemplo, se o JSON tiver a estrutura de um array de objetos de medicamento,
  // você pode percorrer o array e verificar se o ID do compartimento corresponde ao desejado,
  // e então extrair a quantidade de medicamento desse objeto específico.

  // Aqui está um exemplo simplificado:
  // Suponha que você tenha um array de objetos de medicamento no JSON
  // [{"compartimento":1,"qtdMedicamentoDiario":3},{"compartimento":2,"qtdMedicamentoDiario":5},...]
  // Você pode percorrer o array para encontrar o objeto com o compartimento correspondente ao desejado
  // e extrair a quantidade de medicamento desse objeto.

  // Encontra o início do array no JSON
  int arrayStartIndex = payload.indexOf("[");
  if (arrayStartIndex != -1) {
    // Percorre o array para encontrar o objeto do compartimento específico
    String compartimentoStr = String("\"compartimento\":") + String(compartimento);
    int compartimentoIndex = payload.indexOf(compartimentoStr, arrayStartIndex);
    if (compartimentoIndex != -1) {
      // Move o índice para o início do valor da quantidade de medicamento
      int startIndex = payload.indexOf("\"qtdMedicamentoDiario\":", compartimentoIndex);
      if (startIndex != -1) {
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
    }
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
