Os modos **Dash Pro** e **AIM** representam perfis de mensagens pré-definidos que determinam **como os dados da [[Injepro T4000]] são empacotados na rede CAN** (quais IDs são usados, a taxa de envio e a estrutura dos bytes dentro de cada pacote):

- **Dash Pro:** Protocolo proprietário da Injepro, desenhado especificamente para comunicação com seus próprios painéis digitais (Dash Pro) e módulos de expansão (como WB Mini CAN+).
- **AIM:** Padrão industrial aberto amplamente utilizado no automobilismo para integração com painéis (dashes) e dataloggers de terceiros da marca AIM (como o _AIM MXL/MyChron_).

==A principal diferença prática para o seu firmware está no dicionário de dados: um canal de RPM ou TPS terá um **CAN ID**, multiplicador (fator de conversão) e posição de byte diferentes dependendo de qual dos dois modos estiver selecionado no software da ECU.==