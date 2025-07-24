# Projeto IoT com Pico W e MQTT: `testwithmqtt`

Este projeto demonstra a implementação de um dispositivo IoT utilizando a placa Raspberry Pi Pico W (compatível com BitDogLab) e o protocolo MQTT. O dispositivo é capaz de publicar dados de sensores e receber comandos para atuar em componentes locais, como um LED RGB.

A comunicação é feita através de um broker MQTT rodando em uma rede local, permitindo o monitoramento e controle em tempo real através de qualquer cliente MQTT, como o MQTT Explorer.

## Funcionalidades Principais

1.  **Publisher de Temperatura**:
    * A placa lê a temperatura do seu sensor interno a cada 5 segundos.
    * Publica o valor em graus Celsius no tópico MQTT: `Temp`.

2.  **Subscriber de Controle de LED**:
    * A placa se inscreve (assina) o tópico MQTT: `Led`.
    * Aguarda por mensagens nesse tópico para controlar o LED RGB embutido.
    * **Comandos:**
        * `11`: Acende o LED na cor **Verde**.
        * `12`: Acende o LED na cor **Azul**.
        * `13`: Acende o LED na cor **Vermelho**.
        * Qualquer outra mensagem apaga o LED.

---

## Requisitos de Hardware e Software

* **Hardware**:
    * Placa Raspberry Pi Pico W ou compatível (ex: BitDogLab).
    * Cabo Micro-USB.
* **Software**:
    * **Ambiente de Desenvolvimento**: [Pico C/C++ SDK](https://github.com/raspberrypi/pico-sdk) configurado.
    * **Editor de Código**: Visual Studio Code com as extensões C/C++ e CMake Tools.
    * **Broker MQTT**: Um broker rodando na mesma rede local que a placa. Ex: [Mosquitto](https://mosquitto.org/).
    * **Cliente MQTT**: Um cliente para interagir com o broker. Ex: [MQTT Explorer](http://mqtt-explorer.com/).

---

## Como Compilar e Usar

1.  **Configuração do Projeto**:
    * Clone ou baixe os arquivos deste projeto para uma pasta no seu computador.
    * Abra o arquivo `testwithmqtt.c`.
    * **Edite as seguintes linhas** com as suas informações:
        ```c
        // Altere para os dados da sua rede Wi-Fi
        #define WIFI_SSID       "NOME_DA_SUA_REDE_WIFI"
        #define WIFI_PASSWORD   "SENHA_DA_SUA_REDE_WIFI"

        // Altere para o endereço IP do computador onde o broker está rodando
        #define MQTT_SERVER_IP  "IP_DO_SEU_NOTEBOOK" 
        ```

2.  **Compilação**:
    * Abra um terminal (como o terminal do VS Code) na pasta raiz do projeto.
    * Execute os seguintes comandos para compilar o código:
        ```bash
        mkdir build
        cd build
        cmake ..
        make
        ```
    * Se a compilação for bem-sucedida, um arquivo chamado `testwithmqtt.uf2` será criado dentro da pasta `build`.

3.  **Gravação na Placa**:
    * Pressione e segure o botão `BOOTSEL` na sua Pico W.
    * Conecte a placa ao seu computador via USB.
    * Solte o botão `BOOTSEL`. A placa aparecerá como um dispositivo de armazenamento (como um pen drive).
    * Arraste e solte o arquivo `testwithmqtt.uf2` para dentro desse dispositivo. A placa irá reiniciar automaticamente e começar a rodar o programa.

---

## Interação com o Dispositivo

1.  **Inicie o Broker MQTT** no seu notebook.
2.  **Abra o MQTT Explorer** e conecte-se ao seu broker local (Host: `localhost` ou o IP do seu notebook, Porta: `1883`, Usuário: `user02`, Senha: `147258369`).
3.  **Monitorar a Temperatura**:
    * No MQTT Explorer, você verá o tópico `Temp` aparecer automaticamente.
    * Clique nele para visualizar os valores de temperatura sendo enviados pela placa a cada 5 segundos.
4.  **Controlar o LED**:
    * No MQTT Explorer, vá para a seção de publicação.
    * No campo `topic`, digite `Led`.
    * No campo de `payload` (a mensagem), digite `11`, `12` ou `13` e clique em `Publish` para alterar a cor do LED na placa.

## Estrutura do Projeto

* `testwithmqtt.c`: O arquivo principal com toda a lógica da aplicação em C.
* `CMakeLists.txt`: Arquivo de configuração do compilador (CMake) que define como o projeto é construído e quais bibliotecas do Pico SDK são necessárias.
* `lwipopts.h`: Arquivo de configuração para a pilha de rede LwIP, ajustando parâmetros de rede.
* `pico_sdk_import.cmake`: Script padrão do SDK para ajudar o CMake a localizar os arquivos do SDK.