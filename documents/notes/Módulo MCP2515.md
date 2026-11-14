## O que é?
O **Módulo CAN BUS MCP2515 TJA1050** é uma solução prática para adicionar comunicação CAN a microcontroladores e sistemas embarcados. Composto pelo controlador CAN MCP2515 e pelo transceptor TJA1050, o módulo permite a comunicação com redes CAN amplamente utilizadas em aplicações automotivas, industriais e de automação.

## Comunicação
A comunicação com o microcontrolador é realizada através da interface SPI, facilitando a integração com plataformas como Arduino, ESP32, ESP8266, Raspberry Pi Pico, STM32 e PIC. Compatível com a especificação CAN 2.0B e velocidades de até 1Mb/s, o módulo é ideal para monitoramento de redes veiculares, aquisição de dados, automação industrial, sistemas embarcados e projetos de Internet das Coisas (IoT).

- **Controlador CAN**: Implementa a especificação [[Protocolo CAN]] V2.0B com suporte a taxas de até 1 Mb/s.
- **Transceptor TJA1050**: Atua como a interface física entre o controlador MCP2515 e o barramento de dois fios (CAN High e CAN Low).
- **Interface SPI**: Comunica-se de maneira simples com placas de desenvolvimento usando pinos padrão [[Protocolo SPI]]. 

## Resistores de terminação
O módulo já possui resistores de terminação de 120Ω, garantindo maior praticidade na implementação da rede CAN. Seu formato compacto facilita a instalação em protótipos e projetos finais, sendo uma excelente opção para estudantes, makers, técnicos e profissionais da eletrônica.

==Em uma rede CAN física (CAN High e CAN Low), os resistores de terminação são instalados em paralelo nas duas extremidades físicas do barramento:==

- **Casamento de Impedância:** A linha de transmissão diferencial do CAN possui uma impedância característica de aproximadamente $120\,\Omega$. Sem a terminação correta em cada ponta, o sinal elétrico de alta velocidade sofre **reflexões de onda** (o sinal "bate" no final do fio e volta), causando distorções na forma de onda e gerando erros de bit (CRC Error / Stuff Error).
- **Retorno ao Estado Recessivo:** Em uma rede CAN, o estado "Dominante" (bit `0`) força ativamente uma diferença de tensão entre CANH e CANL, enquanto o estado "Recessivo" (bit `1`) depende dos resistores para drenar a carga diferencial da linha e equalizar as tensões de CANH e CANL de volta ao nível de repouso (~2,5V).
- **Valor Medido:** Com a rede desligada, ao medir a resistência com um multímetro entre CANH e CANL, você deve encontrar cerca de **$60\,\Omega$** (dois resistores de $120\,\Omega$ em paralelo).