#include <AccelStepper.h>

// Definindo os pinos para o driver DRV8825
#define STEP_PIN 13  // Pino D27 no ESP32 (GPIO 27)
#define DIR_PIN 26   // Pino D14 no ESP32 (GPIO 14)

// Criando uma instância da biblioteca AccelStepper
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

void setup() {
  // Configurando a velocidade máxima e a aceleração do motor
  stepper.setMaxSpeed(500);  // Velocidade máxima em passos por segundo
  stepper.setAcceleration(250); // Aceleração em passos por segundo^2

  // Inicialmente, configuramos a direção e iniciamos a contagem de passos
  stepper.setSpeed(100); // Velocidade de movimento (passos por segundo)
}

void loop() {
  // Número de passos por volta completa (ajuste conforme o seu motor)
  int stepsPerRevolution = 200; // Assumindo um motor de 1.8° por passo
  
  // Definindo o número de passos para uma rotação completa
  int stepsToMove = stepsPerRevolution * 16; // Multiplicador de microstepping (16x no DRV8825)

  // Mover o motor
  stepper.moveTo(stepsToMove);

  // Executar o movimento até que a posição alvo seja alcançada
  while (stepper.distanceToGo() != 0) {
    stepper.run();  // Necessário para que a biblioteca faça o motor rodar
  }

  // Pausa de 2 segundos antes de girar novamente
  delay(2000);

  // Inverter a direção para girar de volta
  stepper.moveTo(0);

  // Executar o movimento até retornar à posição original
  while (stepper.distanceToGo() != 0) {
    stepper.run();
  }

  // Pausa de 2 segundos antes de girar novamente
  delay(2000);
}