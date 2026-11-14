## O que é?
==Barramento CAN (Rede de Área Controladora) é um sistema de comunicação usado em veículos/máquinas para permitir que ECUs (Unidades de Controle Eletrônico) se comuniquem entre si==, sem um computador hospedeiro. Por exemplo, o barramento CAN permite o compartilhamento rápido e confiável de informações entre os freios e o motor do seu carro.

Por sua vez, ECUs ([[Injepro T4000]]) (também chamadas de 'nós CAN') são como partes do corpo, interconectadas pelo CAN bus. Informações percebidas por uma parte podem ser compartilhadas com outra. Em termos físicos, todas as ECUs são conectadas em um barramento de dois fios composto por um par trançado: CAN high e CAN low. ==A informação é interpretada pela **diferença de potencial** entre eles, e não pela tensão absoluta em relação ao terra, cancelando ruídos eletromagnéticos induzidos no chicote elétrico.==

![[Protocolo CAN-1788280749453.webp|640]]

> [!note]
> ==Os nós não têm um "endereço de destino" como no IP ou I2C. Cada mensagem contém um **Identificador (CAN ID)** que indica o conteúdo da mensagem== (ex: dados de RPM/temperatura) e <u>sua prioridade de transmissão</u>.

![[Protocolo CAN-1788280857767.webp|638]]
## Quadro CAN
De acordo com a camada de enlace de dados, a comunicação pelo barramento CAN é feita via quadros CAN.

Abaixo está um quadro de dados CAN padrão com identificador de 11 bits (CAN 2.0A), que é o tipo usado na maioria dos carros. O Estendido O quadro identificador de 29 bits (CAN 2.0B) é idêntico, exceto pelo ID mais longo. É usado, por exemplo, no [protocolo J1939](https://www.csselectronics.com/pages/j1939-explained-simple-intro-tutorial) para veículos pesados. Note que o ID CAN e os dados estão destacados – isso é importante ao registrar dados do barramento CAN, como veremos abaixo.

![[Protocolo CAN-1788280989411.webp]]

![[Protocolo CAN-1788286585845.webp|528]]
>[!success] Estrutura de um Frame (Quadro) CAN:
>- **SOF (Start of Frame):** 1 bit que sincroniza todos os nós no início da mensagem.
>- **Identificador (CAN ID):** 11 bits (Padrão 2.0A) ou 29 bits (Estendido 2.0B).
>- **RTR (Remote Transmission Request)**: 1 bit que define o tipo do frame, distingue um quadro de dados (dominante) de um quadro remoto.
>- **IDE (Identifier Extension):** 1 bit que indica se o quadro é padrão (dominante) ou estendido.
>- **DLC (Data Length Code):** 4 bits indicando o número de bytes do payload (0 a 8 bytes).
>- **Dados (Payload):** O conteúdo útil, contendo de 0 até 8 bytes.
>- **CRC e ACK:** Campos para verificação de integridade e confirmação de recepção por outros nós da rede.

### Quadro dominante x Quadro remoto
A diferença fundamental entre o **Quadro de Dados (Data Frame)** e o **Quadro Remoto (Remote Frame)** está na **direção do fluxo de informação** e no **bit RTR (Remote Transmission Request)**.

| Característica         | Quadro de Dados              | Quadro Remoto                  |
| ---------------------- | ---------------------------- | ------------------------------ |
| O que ele faz?         | Entrega uma informação       | Pede uma informação            |
| Tem dados dentro?      | Sim (Leva o valor do sensor) | Não (Vai vazio, é só o pedido) |
| O bit RTR vale quanto? | 0 (DOMINANTE)                | 1 (RECESSIVO)                  |
![[Protocolo CAN-1788444039922.webp]]

[CAN Bus Explicado - Uma Introdução Simples [2026] – CSS Electronics](https://www.csselectronics.com/pages/can-bus-simple-intro-tutorial)