#include <WiFi.h>
#include <Wire.h>
#include <RTClib.h> // Biblioteca RTC
#include <time.h>

const char* ssid     = "BERNARDO";
const char* password = "be06082006ele";

RTC_DS1307 rtc; // Objeto do RTC

long timezone = -3;
byte daysavetime = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  rtc.begin();

  if (!rtc.isrunning()) {
    Serial.println("RTC não está funcionando. Configurando data e hora padrão...");
    rtc.adjust(DateTime(F(_DATE), F(TIME_))); // Configura a data e hora padrão com a data e hora da compilação
  }

  // Inicia a conexão com a rede WiFi
  Serial.println();
  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (i > 20) {
      ESP.restart();
    }
    i++;
  }
  Serial.println("WiFi conectado");
  Serial.println("Endereço IP: ");
  Serial.println(WiFi.localIP());

  // Configuração do servidor de tempo
  Serial.println("Contatando o servidor de tempo");
  configTime(3600 * timezone, daysavetime * 3600, "time.nist.gov", "0.pool.ntp.org", "1.pool.ntp.org");
}

void loop() {
  struct tm tmstruct ;
  tmstruct.tm_year = 0;
  getLocalTime(&tmstruct);

  // Dias da semana em português
  const char* dias_semana[] = {"Domingo", "Segunda-feira", "Terça-feira", "Quarta-feira", "Quinta-feira", "Sexta-feira", "Sábado"};

  // Formatar data
  String date = (String(tmstruct.tm_mday) + "/" + String((tmstruct.tm_mon) + 1) + "/" + String(tmstruct.tm_year + 1900));
  
  // Formatar hora
  String hour = (String(tmstruct.tm_hour) + ":" + String(tmstruct.tm_min) + ":" + String(tmstruct.tm_sec));

  // Obter o dia da semana
  String dia_semana = dias_semana[tmstruct.tm_wday];

  Serial.println("Data: " + date + " - Dia da semana: " + dia_semana + " - Hora: " + hour);

  // Verifica se é hora de sincronizar o RTC com o servidor de tempo
  static unsigned long lastSync = 0;
  const unsigned long syncInterval = 4 * 3600 * 1000; // Sincroniza a cada 4 horas

  if (millis() - lastSync > syncInterval) {
    Serial.println("Sincronizando com o servidor de tempo...");
    if (syncTime()) {
      lastSync = millis();
      Serial.println("RTC sincronizado com sucesso.");
    } else {
      Serial.println("Falha ao sincronizar com o servidor de tempo.");
    }
  }

  delay(1000);
}

bool syncTime() {
  if (!rtc.isrunning()) {
    Serial.println("RTC não está funcionando.");
    return false;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi não está conectado.");
    return false;
  }

  tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Falha ao obter hora do servidor de tempo.");
    return false;
  }

  DateTime dt = DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  rtc.adjust(dt);

  return true;
}