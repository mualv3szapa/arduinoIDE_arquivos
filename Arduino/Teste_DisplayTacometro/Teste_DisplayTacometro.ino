  #include <Wire.h>
  #include <LCD03.h>

  // Definir endereço do LCD
  #define LCD_ADDRESS 0x63 // Endereço I2C do display LCD03. Verifique este endereço se necessário.

  LCD03 lcd(LCD_ADDRESS); // Inicializa o LCD com o endereço I2C correto

  const int sensorPin = 2; // Sensor de rotação
  const int tempSensorPin = A0; // Sensor de temperatura LM35
  const int buzzerPin = 3; // Pino do buzzer

  unsigned int rpm;
  unsigned long timeold;
  volatile unsigned long pulse;

  void setup() {
    // Inicializa a comunicação com o display LCD
    Wire.begin();
    lcd.begin(20, 4); // Inicia o LCD com 20 colunas e 4 linhas
    lcd.backlight(); // Liga a luz de fundo do LCD

    // Exibe mensagem de inicialização
    lcd.clear();
    lcd.print("Monitor de Motores");
    lcd.setCursor(0, 1);
    lcd.print("de Inducao");
    delay(5000); // Espera 5 segundos

    lcd.clear();

    // Inicializa o sensor de rotação
    Serial.begin(9600); // Inicia a comunicação serial
    pinMode(sensorPin, INPUT_PULLUP); // Configura o pino do sensor como entrada
    attachInterrupt(digitalPinToInterrupt(sensorPin), count_pulse, RISING); // Configura a interrupção para contar os pulsos
    pulse = 0;
    rpm = 0;
    timeold = 0;

    // Inicializa o pino do buzzer
    pinMode(buzzerPin, OUTPUT); // Configura o pino do buzzer como saída
    digitalWrite(buzzerPin, LOW); // Garante que o buzzer esteja desligado no início
  }

  void loop() {
    delay(1000); // Aguarda 1 segundo

    // Calcular RPM
    detachInterrupt(digitalPinToInterrupt(sensorPin)); // Desativa temporariamente a interrupção
    rpm = (60000 / (millis() - timeold)) * pulse; // Calcula RPM
    pulse = 0; // Reseta o contador de pulsos
    timeold = millis(); // Atualiza o tempo antigo
    attachInterrupt(digitalPinToInterrupt(sensorPin), count_pulse, RISING); // Reativa a interrupção

    // Ler a temperatura do LM35
    int sensorValue = analogRead(tempSensorPin); // Lê o valor analógico do sensor
    float voltage = sensorValue * (5000.0 / 1023.0); // Converte para tensão
    float temperatureC = voltage / 10.0; // Converte para temperatura em Celsius

    // Exibir RPM e temperatura no display LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("RPM: ");
    lcd.print(rpm);

    lcd.setCursor(0, 1);
    lcd.print("Temp: ");
    lcd.print(temperatureC);
    lcd.print(" C");

    // Exibe no monitor serial
    Serial.print("RPM: ");
    Serial.println(rpm);

    Serial.print("Temp: ");
    Serial.println(temperatureC);

    // Acionar o buzzer se a temperatura ultrapassar 90 °C
    if (temperatureC > 90.0) {
      digitalWrite(buzzerPin, HIGH); // Liga o buzzer
    } else {
      digitalWrite(buzzerPin, LOW); // Desliga o buzzer
    }
  }

  void count_pulse() {
    pulse++; // Incrementa o contador de pulsos quando a interrupção é acionada
  }
