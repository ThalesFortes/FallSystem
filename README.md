# FallSystem — Sistema Vestível de Detecção de Quedas

> Firmware embarcado, testado e **funcionando em hardware real**, para um dispositivo vestível de detecção de quedas baseado em Raspberry Pi Pico W e FreeRTOS.

![Linguagem](https://img.shields.io/badge/C-89.5%25-blue)
![Plataforma](https://img.shields.io/badge/Raspberry%20Pi-Pico%20W-c51a4a)
![RTOS](https://img.shields.io/badge/RTOS-FreeRTOS-green)
![Licença](https://img.shields.io/badge/license-MIT-black)

---

## 📌 Sobre o Projeto

O **FallSystem** é um sistema embarcado completo, implementado e validado em bancada e uso real, para **detecção de quedas em um dispositivo vestível**. Ele combina sensor inercial, sensor de distância, feedback local (visual e sonoro), exibição em display e notificação remota via rede.

A detecção não depende de um único sinal: uma queda só é confirmada quando uma **sequência validada de gatilhos** ocorre, o que reduz falsos positivos típicos de detectores baseados apenas em pico de aceleração. Além disso, o sistema oferece um **modo de simulação por chave física**, que permite demonstrar e testar a lógica de queda sem precisar derrubar o dispositivo de verdade.

O firmware roda sobre **FreeRTOS**, com cada responsabilidade isolada em sua própria *task*.

### O que o sistema faz

- **Detecção real de queda (MPU6050)** — baseada na magnitude da aceleração, na variação angular e em uma sequência de gatilhos `T1 → T2 → T3`.
- **Confirmação por proximidade (VL53L0X)** — identifica a aproximação extrema do sensor com o chão (corpo caído), com leitura filtrada, *warm-up* e anti-ruído. Pode confirmar a queda de forma independente.
- **Notificação remota via MQTT** — envio periódico de status e envio imediato no instante da queda, com o motivo e as leituras dos sensores.
- **Feedback local** — LED RGB e buzzer indicam o estado atual (normal, simulação, queda).
- **Display OLED** — mostra em tempo real as leituras de aceleração, giroscópio, distância e o alerta de queda.
- **Modo simulação** — aciona a sequência de queda por chave física, para testes e demonstração.

---

## 📡 Funcionalidades Principais

### ✔ Detecção de queda por IMU (MPU6050)

A queda real é reconhecida por uma máquina de estados de três gatilhos encadeados:

| Gatilho | Evento | Critério (implementado) |
| ------- | ------ | ----------------------- |
| **T1** | Queda livre | Magnitude da aceleração cai a um valor muito baixo (`Amp ≤ 2`) |
| **T2** | Impacto | Pico de aceleração logo em seguida (`Amp ≥ 12`) |
| **T3** | Imobilidade | Grande variação angular no impacto (`30 ≤ Δang ≤ 400`) seguida de repouso (`Δang ≤ 10`) após estabilização |

Se a sequência não se completar dentro da janela esperada, os gatilhos expiram e o sistema volta ao estado normal, evitando falsos positivos.

### ✔ Confirmação por proximidade (VL53L0X)

- Mede continuamente a distância até o chão/objeto.
- Só passa a valer após um **warm-up de ~1 s**.
- Ignora leituras espúrias muito curtas (`< 50 mm`) como ruído.
- Exige **3 leituras consecutivas** abaixo do limiar (`≤ 200 mm`) para gerar alerta.
- Distâncias muito pequenas (`≤ 120 mm`) confirmam a queda de forma independente do IMU.

### ✔ Notificação via MQTT

- Relatório periódico de status a cada **5 segundos**.
- Envio **imediato** ao detectar uma queda.
- Tópico: `pico/fall`.
- Payload em JSON, por exemplo:

```json
{ "queda": true, "motivo": "mpu", "led": "vermelho", "buzzer": true, "dist_mm": 118 }
```

O campo `motivo` assume os valores `nenhum`, `mpu`, `proximidade` ou `simulacao`.

### ✔ Multitarefa com FreeRTOS

Tarefas que compõem o sistema:

| Task | Função |
| ---- | ------ |
| `mpu_task` | Lê o IMU, roda a lógica de queda **e atualiza o display OLED** |
| `vl53_task` | Lê o sensor de distância e gera o alerta de proximidade |
| `sim_task` | Executa a sequência de queda simulada pela chave física |
| `led_task` | Controla o LED RGB e o buzzer conforme o estado |
| `mqtt_task` | Conecta ao Wi-Fi/broker e publica os eventos |

> Observação: não há uma `oled_task` separada — a atualização do display acontece dentro da `mpu_task`.

### ✔ Três modos de operação

- **Normal / monitoramento** — LED azul piscando, sistema vigiando os sensores.
- **Detecção real** — queda confirmada pelo IMU e/ou proximidade: LED vermelho + buzzer.
- **Simulação** — acionada pela chave física: LED verde e sequência de queda pré-programada.

Também é possível **marcar uma queda manualmente** pelo botão físico (interrupção de borda), útil para testes.

---

## 🛠 Hardware Utilizado

| Componente | Função |
| ---------- | ------ |
| **Raspberry Pi Pico W** | Microcontrolador com Wi-Fi; executa o firmware |
| **MPU6050** | Acelerômetro + giroscópio (detecção de queda) |
| **VL53L0X** | Sensor de distância ToF (confirmação por proximidade) |
| **SSD1306 (OLED I2C)** | Interface visual em tempo real |
| **LED RGB** | Indicação do estado do sistema |
| **Buzzer ativo** | Alerta sonoro na queda |
| **Chave / botão** | Modo simulação e marcação manual de queda |

### Pinagem (conforme o firmware)

| Sinal | GPIO | Observação |
| ----- | ---- | ---------- |
| I2C SDA | **GP0** | Barramento `i2c0`, 400 kHz — compartilhado por MPU6050, SSD1306 e VL53L0X |
| I2C SCL | **GP1** | |
| LED Vermelho | **GP13** | Queda detectada |
| LED Verde | **GP11** | Modo simulação |
| LED Azul | **GP12** | Monitoramento normal |
| Buzzer | **GP21** | Alerta sonoro |
| Chave de simulação | **GP5** | Pull-up, ativa em nível baixo |
| Botão de queda manual | **GP6** | Pull-up, interrupção na borda de descida |

> Os três dispositivos I2C compartilham o mesmo barramento, com acesso protegido por um *mutex* (`i2c_mutex`) para evitar colisões entre as tarefas.

---

## ⚙️ Fluxo de Funcionamento

1. **Inicialização** — configura I2C, GPIOs, mutex, interrupção do botão, e inicializa MPU6050, OLED e VL53L0X.
2. **Leitura de sensores** — `mpu_task` e `vl53_task` leem continuamente aceleração, giroscópio e distância.
3. **Lógica de detecção** — a máquina de estados de gatilhos avalia queda real; a proximidade confirma de forma independente.
4. **Notificação** — LED + buzzer localmente; publicação MQTT do estado e do evento de queda.
5. **Loop contínuo** — o sistema volta ao monitoramento e reinicia o ciclo.

---

## 🔧 Como Compilar e Gravar

Este projeto usa o **Raspberry Pi Pico SDK 2.1.0** e o **FreeRTOS-Kernel** (incluído como submódulo), sendo mais simples de construir pela extensão **Raspberry Pi Pico** do VS Code.

1. **Clone o repositório com os submódulos:**

   ```bash
   git clone --recurse-submodules https://github.com/ThalesFortes/FallSystem.git
   ```

   Se já tiver clonado sem os submódulos:

   ```bash
   git submodule update --init --recursive
   ```

2. **Configure as credenciais** em `src/CMakeLists.txt`, no bloco `add_compile_definitions`:

   ```cmake
   add_compile_definitions(
       WIFI_SSID="SEU_WIFI"
       WIFI_PASSWORD="SENHA_WIFI"
       MQTT_SERVER="IP_DO_BROKER"
       MQTT_PORT=1883
       MQTT_USERNAME="USUARIO_MQTT"
       MQTT_PASSWORD="SENHA_MQTT"
   )
   ```

3. **Compile** pelo botão *Build* da extensão do Pico no VS Code (ou via CMake/Ninja no terminal). O binário `.uf2` é gerado na pasta de build.

4. **Grave na Pico W:**
   - Segure o botão **BOOTSEL** e conecte a Pico W ao PC via USB.
   - Ela será montada como um disco removível.
   - Arraste o arquivo **`.uf2`** para dentro desse disco (ou use *Run* na extensão do VS Code).
   - A Pico reinicia sozinha e começa a executar o firmware.

> A saída de log (`printf`) é enviada pela USB serial, útil para acompanhar os gatilhos e as leituras em tempo real.

---

## 📦 Estrutura e Dependências

```
src/
├── main.c                      # Tarefas, inicialização e loop principal
├── mqtt_task.c / .h            # Conexão Wi-Fi + MQTT e publicação de eventos
├── FreeRTOSConfig.h            # Configuração do FreeRTOS
└── drivers/
    ├── mpu6050/                # Driver do IMU (aceleração + giroscópio)
    ├── vl53l0x/                # Driver do sensor de distância ToF
    ├── fallDetector/           # Lógica de detecção de queda (triggers T1→T2→T3)
    ├── display/                # Driver do OLED SSD1306 + fontes/bitmaps
    ├── leds/                   # Controle do LED RGB
    └── simulation/             # Rotina de queda simulada
```

Cabeçalhos principais:

- `mpu6050.h` — leitura do IMU (aceleração e giroscópio).
- `vl53l0x.h` — detecção por aproximação.
- `fall_detector.h` — análise e validação da queda.
- `ssd1306.h` / `ssd1306_fonts.h` — display OLED.
- `leds.h` — controle dos LEDs.
- `mqtt_task.h` — comunicação MQTT.

Bibliotecas de plataforma: `pico_stdlib`, `hardware_i2c`, `hardware_gpio`, `pico_cyw43_arch_lwip_threadsafe_background`, `pico_lwip_mqtt`, `pico_unique_id` e `FreeRTOS-Kernel`.

---

## 🎯 Conclusão

O FallSystem é um sistema de detecção de quedas **completo, modular e comprovadamente funcional em hardware real**, pronto para uso como dispositivo vestível e adaptável para cenários de saúde, monitoramento remoto de idosos ou protótipos industriais. A combinação de detecção inercial, confirmação por proximidade, feedback local e notificação MQTT torna a solução robusta e fácil de estender.

---

## 📄 Licença

Distribuído sob a licença **MIT**. Consulte o arquivo [`LICENSE`](LICENSE) para mais detalhes.
