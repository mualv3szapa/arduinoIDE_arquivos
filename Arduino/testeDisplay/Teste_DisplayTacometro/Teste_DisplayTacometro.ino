#include <LiquidCrystal_I2C.h>
#define LCD_ADDRESS 0x27
LiquidCrystal_I2C lcd(LCD_ADDRESS, 20, 4);

const int sensorPin = 2; // Conecte a saída digital do sensor ao pino 2 do Arduino
unsigned int rpm;
unsigned long timeold;
volatile unsigned long pulse;

void setup() {
  lcd.init(); // INICIA A COMUNICAÇÃO COM O DISPLAY
  lcd.backlight(); // LIGA A ILUMINAÇÃO DO DISPLAY
  lcd.clear(); // LIMPA O DISPLAY

  lcd.print("Monitor para motores de induçao");
  delay(5000); // DELAY DE 5 SEGUNDOS
  lcd.setCursor(0, 1); // POSICIONA O CURSOR NA PRIMEIRA COLUNA DA LINHA 2
  lcd.print("Blau");
  delay(5000); // DELAY DE 5 SEGUNDOS
  
  lcd.noBacklight(); // DESLIGA A ILUMINAÇÃO DO DISPLAY
  delay(2000); // DELAY DE 2 SEGUNDOS
  lcd.backlight(); // LIGA A ILUMINAÇÃO DO DISPLAY
  delay(2000); // DELAY DE 2 SEGUNDOS
  
  lcd.clear(); // LIMPA O DISPLAY
  //lcd.noBacklight(); // DESLIGA A ILUMINAÇÃO DO DISPLAY

  // Inicialize o sensor KY-032
  Serial.begin(9600); // Iniciar a comunicação serial
  pinMode(sensorPin, INPUT_PULLUP); // Configurar o pino do sensor como entrada
  attachInterrupt(digitalPinToInterrupt(sensorPin), count_pulse, RISING); // Configurar a interrupção para contar os pulsos
  pulse = 0;
  rpm = 0;
  timeold = 0;
}

void loop() {
delay(1000); // Aguardar 1 segundo
  detachInterrupt(digitalPinToInterrupt(sensorPin)); // Desativar temporariamente a interrupção
  rpm = (60000 / (millis() - timeold)) * pulse; // Calcular RPM
  pulse = 0; // Resetar o contador de pulsos
  timeold = millis(); // Atualizar o tempo antigo
  attachInterrupt(digitalPinToInterrupt(sensorPin), count_pulse, RISING); // Reativar a interrupção
  // Exiba a RPM no display LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("RPM: ");
    lcd.print(rpm);
  Serial.print("RPM: ");
  Serial.println(rpm); // Mostrar o valor de RPM no monitor serial
}

void count_pulse() {
  pulse++; // Incrementar o contador de pulsos quando a interrupção é acionada
}

