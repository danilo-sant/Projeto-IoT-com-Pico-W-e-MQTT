# Projeto de IoT com MQTT e FreeRTOS na BitDogLab (RP2040)

Este repositório contém o firmware para um dispositivo IoT desenvolvido na placa BitDogLab (baseada no microcontrolador RP2040) durante a residência em Sistemas Embarcados. O projeto implementa um cliente MQTT robusto que publica dados de sensores e recebe comandos para controlar periféricos, utilizando o FreeRTOS para gerir as tarefas de forma eficiente e concorrente.

## Visão Geral

O objetivo principal deste projeto é criar um nó IoT funcional, capaz de:
1.  **Ler** a temperatura do sensor interno do RP2040.
2.  **Publicar** essa leitura de temperatura num tópico MQTT a intervalos regulares.
3.  **Subscrever** um tópico MQTT para receber comandos.
4.  **Controlar** um LED RGB com base nos comandos recebidos.
5.  **Manter** uma conexão estável e reconectar-se automaticamente em caso de falha.
6.  **Gerir** as tarefas de rede e de aplicação de forma independente usando FreeRTOS.

---

## Evolução para FreeRTOS

Inicialmente, o projeto foi desenvolvido com um único loop infinito (`while(true)`) na função `main()`. Embora funcional para tarefas simples, esta abordagem apresentava limitações de estabilidade e escalabilidade, especialmente em redes instáveis.

Para criar um sistema mais robusto e profissional, o projeto foi migrado para utilizar o **FreeRTOS**, um Sistema Operacional de Tempo Real. Esta mudança trouxe os seguintes benefícios:

* **Paralelismo Real**: Com os dois núcleos do RP2040, podemos dedicar um núcleo para as tarefas de rede e outro para as tarefas da aplicação.
* **Organização e Modularidade**: O código foi dividido em tarefas independentes, cada uma com uma única responsabilidade.
* **Estabilidade**: A gestão da rede numa tarefa dedicada com maior prioridade garante que a conexão MQTT permaneça estável, enquanto outras tarefas (como a leitura de sensores) rodam em paralelo sem interferir.
* **Escalabilidade**: Adicionar novas funcionalidades (como ler outro sensor) torna-se tão simples quanto criar uma nova tarefa, sem comprometer as existentes.

### Nova Arquitetura de Software

A lógica agora está dividida em duas tarefas principais:

1.  **`mqtt_task`**: A tarefa de infraestrutura. A sua única responsabilidade é gerir a conexão Wi-Fi e MQTT. Ela executa a função `cyw43_arch_poll()` constantemente e implementa a lógica de reconexão automática.
2.  **`temp_publish_task`**: A tarefa de aplicação. "Acorda" a cada 5 segundos para ler o sensor de temperatura e solicitar a publicação dos dados.

Para garantir que ambas as tarefas possam aceder ao cliente MQTT de forma segura, foi implementado um **Mutex (`SemaphoreHandle_t`)**, prevenindo condições de corrida e corrupção de dados.

---

## Funcionalidades

* **Publisher**: Publica a temperatura (em graus Celsius) no tópico `ha/bitdog/temp` a cada 5 segundos.
* **Subscriber**: Subscreve o tópico `ha/bitdog/led/set` e aguarda por comandos para controlar o LED RGB.
    * Payload `11`: Acende o LED **Verde**.
    * Payload `12`: Acende o LED **Azul**.
    * Payload `13`: Acende o LED **Vermelho**.
    * Qualquer outro payload: Apaga o LED.
* **Status de Conexão**: Publica a mensagem `online` no tópico `ha/bitdog/status` quando se conecta. Se a conexão cair, o broker publicará `offline` no mesmo tópico (graças ao *Last Will and Testament*).
* **ID de Cliente Único**: Gera um ID de cliente único para o MQTT usando o endereço MAC do chip Wi-Fi para evitar conflitos de conexão.
* **Modo de Performance Wi-Fi**: O modo de economia de energia do Wi-Fi é desativado para garantir a máxima estabilidade da conexão TCP.

## Hardware

* Placa de desenvolvimento **BitDogLab** (ou qualquer outra baseada no RP2040 com Wi-Fi, como a Raspberry Pi Pico W).
* LED RGB integrado na placa.
* Sensor de temperatura interno do RP2040.

## Software e Ferramentas

* **Linguagem**: C/C++
* **SDK**: Raspberry Pi Pico C/C++ SDK
* **Sistema Operacional**: FreeRTOS
* **Ambiente de Desenvolvimento**: Visual Studio Code
* **Broker MQTT**: Qualquer broker padrão (testado com Mosquitto e brokers públicos).
* **Cliente de Teste**: MQTT Explorer ou uma aplicação móvel como o MQTT Client.

## Como Compilar e Usar

1.  **Configurar o Ambiente**: Certifique-se de que tem o [Pico C/C++ SDK](https://github.com/raspberrypi/pico-sdk) e o [FreeRTOS](https://github.com/FreeRTOS/FreeRTOS-Kernel) configurados no seu ambiente de desenvolvimento.

2.  **Clonar o Projeto**: Clone este repositório para a sua máquina.

3.  **Configurar o `CMakeLists.txt`**: Verifique se as bibliotecas necessárias estão incluídas no `target_link_libraries`:
    ```cmake
    target_link_libraries(nome_do_projeto PRIVATE
        pico_stdlib
        pico_cyw43_arch_lwip_threadsafe_background
        pico_lwip_mqtt
        hardware_adc
        # Bibliotecas do FreeRTOS
        FreeRTOS-Kernel
        FreeRTOS-Kernel-Heap4 
    )
    ```

4.  **Configurar o `main.c`**: Abra o ficheiro `main.c` e edite as seguintes macros com os seus dados:
    * `WIFI_SSID`: Nome da sua rede Wi-Fi.
    * `WIFI_PASSWORD`: Senha da sua rede Wi-Fi.
    * `MQTT_SERVER_IP`: Endereço IP do seu broker MQTT.
    * `MQTT_USER`: Utilizador do seu broker (se aplicável).
    * `MQTT_PASSWORD`: Senha do seu broker (se aplicável).

5.  **Compilar**: Crie uma pasta `build`, navegue até ela e execute os comandos:
    ```bash
    cmake ..
    make
    ```

6.  **Gravar na Placa**: Coloque a BitDogLab em modo bootloader (segurando o botão `BOOTSEL` ao ligar o USB). Arraste o ficheiro `.uf2` gerado na pasta `build` para o dispositivo de armazenamento que aparece.

7.  **Monitorizar e Testar**: Abra um monitor serial para ver os logs da placa e use um cliente MQTT (como o MQTT Explorer) para ver as publicações de temperatura e enviar comandos para o LED.
