#include <SPI.h>            // Biblioteca para comunicação SPI
#include <MFRC522.h>        // Biblioteca para controle do módulo RFID MFRC522
#include <WiFi.h>           // Biblioteca para conexão Wi-Fi
#include <HTTPClient.h>     // Biblioteca para requisições HTTP
#include <ArduinoJson.h>    // Biblioteca para manipulação de JSON
#include <time.h>           // Biblioteca para manipulação de tempo (NTP)

#define SS_PIN         21   // Pino de seleção do módulo RFID
#define RST_PIN        22   // Pino de reset do módulo RFID
#define ATUADOR_01     27   // Pino de controle do atuador 1
#define ATUADOR_02     26   // Pino de controle do atuador 2
#define ATUADOR_03     25   // Pino de controle do atuador 3
#define BUZZER_PIN     13   // Pino de controle do buzzer

const int buttonPin = 2;    // Pino de controle do botão
const int ledPin = 13;      // Pino de controle do LED (opcional)

// Inicialização do módulo MFRC522 para leitura de RFID
MFRC522 mfrc522(SS_PIN, RST_PIN);

// Credenciais Wi-Fi e URLs do servidor para obter dados dos compartimentos
const char* ssid = "Biel";          // Nome da rede Wi-Fi
const char* password = "g26470907"; // Senha da rede Wi-Fi
const char* serverAddressCompartimento1 = "http://192.168.81.168:3000/Compartimento1";
const char* serverAddressCompartimento2 = "http://192.168.81.168:3000/Compartimento2";
const char* serverAddressCompartimento3 = "http://192.168.81.168:3000/Compartimento3";

bool tagDetectada = false; // Variável global para monitorar se uma tag foi detectada

void setup() {
  Serial.begin(115200);           // Inicia comunicação serial com taxa de 115200 bps
  Serial.println("Inicializando...");

  // Conectar ao WiFi
  WiFi.begin(ssid, password);     // Inicia a conexão Wi-Fi
  Serial.print("Conectando a ");
  Serial.println(ssid);

  int retries = 0;                // Contador de tentativas de conexão
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);                   // Espera 500 ms entre tentativas
    Serial.print(".");
    retries++;
  }

  // Se não conseguir conectar ao Wi-Fi após 20 tentativas, reinicia o ESP32
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Falha ao conectar ao WiFi. Reiniciando...");
    ESP.restart();
  }

  Serial.println("WiFi conectado");
  Serial.print("Endereço IP: ");
  Serial.println(WiFi.localIP()); // Imprime o endereço IP obtido

  // Configurar hora com o servidor NTP
  configTime(-3 * 3600, 0, "pool.ntp.org"); // Configura fuso horário (GMT-3)

  // Configurar pinos como saída ou entrada
  pinMode(ATUADOR_01, OUTPUT);
  pinMode(ATUADOR_02, OUTPUT);
  pinMode(ATUADOR_03, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT);

  // Desativa os atuadores (nível alto)
  digitalWrite(ATUADOR_01, HIGH);
  digitalWrite(ATUADOR_02, HIGH);
  digitalWrite(ATUADOR_03, HIGH);

  // Inicia a comunicação SPI e configura o módulo RFID
  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println("Inicializando MFRC522...");

  // Verifica se o módulo RFID está funcionando corretamente
  if (mfrc522.PCD_PerformSelfTest()) {
    Serial.println("MFRC522 inicializado com sucesso!");
  } else {
    Serial.println("Falha ao inicializar o MFRC522. Verifique as conexões.");
  }
}

void loop() {
  // Verifica se há uma nova tag RFID próxima ao leitor
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    Serial.println("Tag detectada! Aguarde a liberação dos remédios...");
    dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size); // Imprime o UID da tag

    // Variáveis para armazenar dados dos compartimentos
    int compartimento1 = 0;
    int numAcionamento1 = 0;
    int compartimento2 = 0;
    int numAcionamento2 = 0;
    int compartimento3 = 0;
    int numAcionamento3 = 0;

    // Obtém dados dos compartimentos através das requisições HTTP
    if (obterDadosDoCompartimento1(compartimento1, numAcionamento1) && 
        obterDadosDoCompartimento2(compartimento2, numAcionamento2) && 
        obterDadosDoCompartimento3(compartimento3, numAcionamento3)) {
      // Acionar atuadores simultaneamente
      xTaskCreatePinnedToCore(acionarAtuadorTask, "Atuador1Task", 10000, (void*)1, 1, NULL, 1);
      xTaskCreatePinnedToCore(acionarAtuadorTask, "Atuador2Task", 10000, (void*)2, 1, NULL, 1);
      xTaskCreatePinnedToCore(acionarAtuadorTask, "Atuador3Task", 10000, (void*)3, 1, NULL, 1);

      // Aguardar até que todas as tarefas terminem
      while (uxTaskGetNumberOfTasks() > 1) {
        delay(100); // Verificar a cada 100 ms
      }

      // Ativar o buzzer após todos os atuadores terem completado
      beepBuzzer();
      Serial.println("Remédios liberados!");
    } else {
      Serial.println("Falha ao obter dados do compartimento.");
    }
    
    tagDetectada = true; // Marca a tag como detectada
    
    // Aguarde um pouco antes de permitir a próxima leitura da tag
    delay(5000);
    tagDetectada = false; // Reseta a marcação após o delay
  }

  // Estrutura para armazenar informações de tempo
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 5000)) {
    Serial.println("Falha ao obter a hora");
    delay(1000);
    return;
  }

  // Arrays com os nomes dos dias da semana
  const char* dias_semana[] = {"Domingo", "Segunda-feira", "Terça-feira", "Quarta-feira", "Quinta-feira", "Sexta-feira", "Sábado"};
  // Formata a data e hora atuais
  String date = String(timeinfo.tm_mday) + "/" + String(timeinfo.tm_mon + 1) + "/" + String(timeinfo.tm_year + 1900);
  String hour = String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min) + ":" + String(timeinfo.tm_sec);
  String dia_semana = dias_semana[timeinfo.tm_wday];

  // Imprime a data, dia da semana e hora atuais
  Serial.printf("Data: %s - Dia da semana: %s - Hora: %s\n", date.c_str(), dia_semana.c_str(), hour.c_str());

  // Verifica se é a hora de ativar o buzzer (por exemplo, às 11:17:00)
  if (timeinfo.tm_hour == 11 && timeinfo.tm_min == 17 && timeinfo.tm_sec == 0 && !tagDetectada) {
    // Aciona o buzzer como uma sirene ligada por 2 segundos, desligada por 1 segundo em loop
    while (!tagDetectada) {
      digitalWrite(BUZZER_PIN, HIGH); // Liga o buzzer
      Serial.println("Buzzer ligado.");
      delay(2000);                    // Mantém o buzzer ligado por 2 segundos
      digitalWrite(BUZZER_PIN, LOW);  // Desliga o buzzer
      Serial.println("Buzzer desligado.");
      delay(1000);                    // Mantém o buzzer desligado por 1 segundo

      // Verifica se uma tag RFID foi detectada para encerrar a sirene
      if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
        Serial.println("Tag detectada! Encerrando a sirene.");
        tagDetectada = true;
      }
    }
  }

  // Verifica se o botão foi pressionado
  if (digitalRead(buttonPin) == HIGH) {
    digitalWrite(ledPin, HIGH);  // Liga o LED (opcional)
    delay(1000);                 // Espera um segundo
    ESP.restart();               // Reinicia o ESP32
  }

  // Aguarda 1 segundo antes da próxima leitura de tempo
  delay(1000);
}

// Função para obter dados do compartimento 1
bool obterDadosDoCompartimento1(int &compartimento1, int &numAtivamento1) {
  return obterDadosDoCompartimento(serverAddressCompartimento1, compartimento1, numAtivamento1);
}

// Função para obter dados do compartimento 2
bool obterDadosDoCompartimento2(int &compartimento2, int &numAtivamento2) {
  return obterDadosDoCompartimento(serverAddressCompartimento2, compartimento2, numAtivamento2);
}

// Função para obter dados do compartimento 3
bool obterDadosDoCompartimento3(int &compartimento3, int &numAtivamento3) {
  return obterDadosDoCompartimento(serverAddressCompartimento3, compartimento3, numAtivamento3);
}

// Função genérica para obter dados de um compartimento via HTTP
bool obterDadosDoCompartimento(const char* serverAddress, int &compartimento, int &numAtivamento) {
  // Verifica se o ESP32 está conectado ao Wi-Fi
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;                // Cria um objeto HTTPClient
    http.begin(serverAddress);      // Inicia a conexão HTTP com o servidor especificado
    int httpResponseCode = http.GET(); // Envia uma requisição GET

    // Verifica se a requisição foi bem-sucedida
    if (httpResponseCode > 0) {
      String payload = http.getString(); // Obtém a resposta do servidor como string
      Serial.println(httpResponseCode);  // Imprime o código de resposta HTTP
      Serial.println(payload);           // Imprime a resposta recebida

      // Configura o tamanho do buffer para deserialização do JSON
      const size_t capacity = JSON_ARRAY_SIZE(1) + 3*JSON_OBJECT_SIZE(7) + 60;
      DynamicJsonDocument doc(capacity); // Cria um documento JSON com a capacidade especificada

      // Deserializa a string JSON para o objeto 'doc'
      DeserializationError error = deserializeJson(doc, payload);
      if (!error) {
        // Itera sobre os objetos JSON para obter os dados necessários
        for (JsonObject obj : doc.as<JsonArray>()) {
          compartimento = obj["numeroCompartimento"]; // Obtém o número do compartimento
          numAtivamento = obj["medicamentoId"]["qtdMedicamentoDiario"]; // Obtém a quantidade de medicamentos
          Serial.print("Compartimento: ");
          Serial.println(compartimento);
          Serial.print("Qtd. Medicamento Diário: ");
          Serial.println(numAtivamento);
        }
        return true;
      } else {
        Serial.print("Erro ao parsear JSON: "); // Imprime mensagem de erro se a deserialização falhar
        Serial.println(error.c_str());
        return false;
      }
    } else {
      Serial.print("Erro na requisição HTTP: "); // Imprime mensagem de erro se a requisição falhar
      Serial.println(httpResponseCode);
      return false;
    }
    http.end(); // Encerra a conexão HTTP
  }
  return false;
}

// Função para acionar o atuador baseado no número do compartimento e quantidade de ativações
void acionarAtuador(int compartimento, int numAtivamento) {
  int atuadorPin = getAtuadorPin(compartimento); // Obtém o pino correspondente ao compartimento
  if (atuadorPin != -1) {
    for (int i = 0; i < numAtivamento; i++) {
      digitalWrite(atuadorPin, LOW); // Liga o atuador
      delay(1000);
      digitalWrite(atuadorPin, HIGH); // Desliga o atuador
      delay(1000);
    }
  } else {
    Serial.println("Compartimento inválido"); // Mensagem de erro se o compartimento for inválido
  }
}

// Função para mapear o número do compartimento ao pino do atuador correspondente
int getAtuadorPin(int compartimento) {
  switch (compartimento) {
    case 1:
      return ATUADOR_01;
    case 2:
      return ATUADOR_02;
    case 3:
      return ATUADOR_03;
    default:
      return -1; // Retorna -1 se o compartimento não for válido
  }
}

// Função para imprimir o UID da tag RFID em formato hexadecimal
void dump_byte_array(byte *buffer, byte bufferSize) {
  Serial.print("UID da tag: ");
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " "); // Adiciona espaço antes de números menores que 0x10
    Serial.print(buffer[i], HEX); // Imprime cada byte do UID em hexadecimal
  }
  Serial.println();
}

// Função de tarefa para acionar os atuadores simultaneamente
void acionarAtuadorTask(void *param) {
  int atuador = (int)param;
  int numAcionamentos = 0;
  switch (atuador) {
    case 1:
      numAcionamentos = 3; // Defina o número de acionamentos para cada atuador conforme necessário
      break;
    case 2:
      numAcionamentos = 2;
      break;
    case 3:
      numAcionamentos = 1;
      break;
  }
  acionarAtuador(atuador, numAcionamentos);
  vTaskDelete(NULL); // Deleta a tarefa quando terminar
}

// Função para emitir o "beep beep" com o buzzer
void beepBuzzer() {
  for (int i = 0; i < 2; i++) {
    digitalWrite(BUZZER_PIN, HIGH); // Liga o buzzer
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);  // Desliga o buzzer
    delay(200);
  }
}
