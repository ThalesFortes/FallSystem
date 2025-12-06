# Sistema de Detecção de Quedas com Raspberry Pi Pico W, MPU6050, VL53L0X, FreeRTOS e MQTT
## 📌 Sobre o Projeto

<p>Este projeto implementa um sistema embarcado completo para detecção de quedas, utilizando a Raspberry Pi Pico W, sensores de movimento MPU6050, sensor de proximidade VL53L0X, comunicação MQTT, multitarefas com FreeRTOS, um módulo de feedback com LED RGB + buzzer, e exibição em display OLED SSD1306.

<p>O sistema foi projetado para funcionar tanto em cenários reais quanto em simulações (modo de teste), permitindo avaliar cada gatilho de queda individualmente: Trigger 1: Queda livre ,Trigger 2: Impacto ,Trigger 3: Imobilidade</p>
  <p>-- Proximidade Crítica: Detecção por sensor VL53L0X</p>
  <p>-- Simulação Guiada: Etapas pré-programadas de queda</p>
  <p>-- Envio MQTT: Motivo da queda + dados dos sensores</p>
  <p>-- LED/Buzzer: Indicadores visuais e sonoros</p>
  <p>-- OLED: Exibe status, leituras e motivos</p>

## 📡 Funcionalidades Principais
  <p>✔ Detecção real de queda usando MPU6050 baseada em: Magnitude da aceleração ,variação de ângulo ,sequência de triggers validada (T1 → T2 → T3)</p>
  <p>✔ Detecção por proximidade (VL53L0X): Identifica aproximação extrema (ex.: corpo batendo no chão), usa leitura filtrada com anti-ruído e warm-up, pode confirmar queda independentemente dos demais triggers</p>
  <p>✔ Comunicação MQTT:Envio periódico do estado do sistema (a cada 5 segundos), envio imediato ao detectar queda, mensagens incluem: Motivo ,tipo de trigger,leituras de sensores, timestamp, estado atual</p>
  <p>✔ Multitarefas com FreeRTOS</p>
  <p>✔ Tasks principais: mpu_task(), vl53_task(), mqtt_task(), oled_task(), led_task(), sim_task()</p>
  <p>✔ Três modos de operação: Normal, detecção real, simulação por chave física</p>

## 🛠 Hardware Utilizado
  <p>Componentes: Raspberry Pi Pico W	Microcontrolador Wi-Fi, execução do firmware ,MPU6050	Acelerômetro + giroscópio ,VL53L0X	ToF – Medição de proximidade
  ,Display SSD1306 I2C	Interface visual, LED RGB	Indicação do estado, Buzzer ativo	Alerta sonoro ,BUTTONS</p>

## ⚙️ Fluxo de Funcionamento
<ul>
  <li> 1️⃣Inicialização</li>
  <li> 2️⃣ Leitura de Sensores</li>
  <li> 3️⃣ Lógica de Detecção</li>
  <li> 4️⃣ Notificação</li>
  <li>5️⃣ Loop Contínuo</li>
</ul>


# 🔧 Como Compilar
  1- Clone o repositorio
  2- Deploy no Pico W
  3 - Coloque no Cmake, os dados como ip do servidor, broker, wifi e senha, usuario e senha
      add_compile_definitions(
      WIFI_SSID="SEUWIFI"
      WIFI_PASSWORD="SENHAWIFI"
      MQTT_SERVER="IP_EXTERNO_BROKER_CRIADO"
      MQTT_PORT=1883
      MQTT_USERNAME="USUARIO_MQTT"
      MQTT_PASSWORD="SENHA_MQTT")
  4 - Segure o botão BOOTSEL do Pico W e conecte-o ao PC
  Clique no botão run no Vscode ou arraste o arquivo .u2 para dentro do disco removível que aparecer
  O Pico irá reiniciar executando o firmware
  5- Grave na Pico W Segure BOOTSEL → conecte USB → arraste o .uf2.

# 📦 Dependências
  mpu6500.h para o sensor de aceleração
  ssd1306.h e ssd1306_fonts.h para o display OLED
  servo.h para atuador servo motor
  leds.h para função de ativar e desativar os LEDs
  mqtt_task.h para conexão via mqtt
  fall_detector.h para analise de queda
  vl53l0x.h para detecção por aproximação

# 🎯 Conclusão

Este projeto fornece um sistema de detecção de quedas completo, robusto e modular, pronto para uso em dispositivos vestíveis, sistemas de saúde, monitoramento remoto ou protótipos industriais.