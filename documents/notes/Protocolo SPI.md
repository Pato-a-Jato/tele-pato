O **protocolo SPI** (_Serial Peripheral Interface_) é um padrão de comunicação serial síncrona e _full-duplex_ (bidirecional simultânea) criado pela Motorola em 1979 para a troca rápida de dados em curtas distâncias entre um microcontrolador e periféricos.

É um protocolo síncrono de alta velocidade (até 3Mbps), existindo atualmente periféricos de velocidade ainda mais alta compatíveis com SPI.

A comunicação SPI possui algumas características básicas. Primeiramente os sinais de comunicação possuem uma direção fixa e definida. Isso significa que sempre existem dois transistores definindo o estado de um pino (Push-Pull). Essa característica é uma das grandes diferenças entre outras comunicações seriais como I2C e OneWire, que possuem um mesmo barramento de dados para os sinais de entrada e saída através do esquema de dreno-aberto (Pull-Up).
## Modos de Operação
Possui 4 modos de operação. Por padrão a comunicação SPI permite a configuração das bordas de comunicação do clock através de sua polaridade e fase. A configuração da polaridade se dá através de CPOL (Clock Polarity), e a configuração da fase se dá através de CPHA (Clock Phase). Seus modos possíveis são:

![[Protocolo SPI-1788220371931.webp|582]]
## Pinagem
![[Protocolo SPI-1788220304386.webp|583]]

- **SCLK / SCK (Serial Clock)**:
	- Sinal de clock gerado pelo Mestre para sincronizar os dados.
- **MOSI (Master Out Slave In)**:
	- Canal de envio de dados do Mestre para o Escravo.
- **MISO (Master In Slave Out)**:
	- Canal de retorno de dados do Escravo para o Mestre.
- **SS / CS (Slave Select / Chip Select)**:
	- Pino usado pelo Mestre para ativar e escolher qual Escravo vai responder (geralmente ativo em nível baixo, `0`).