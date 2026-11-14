## Como funciona a comunicação em um protocolo MQTT
Em um Protocolo MQTT, a publicação e o recebimento de dados são realizados através de um tipo de servidor chamado de Broker.  

Toda a comunicação se dá através de tópicos, que é o local ou endereço para qual a mensagem será encaminhada. Os dispositivos que enviam mensagens são os **publishers**, que publicam nesses tópicos. Já os dispositivos que desejam receber essas mensagens se inscrevem neste endereço e são chamados de **subscribers.**  

Assim, sempre que um dispositivo publica uma mensagem em um tópico, todos os que estão seguindo esse tópico recebem a mensagem. O **Broker** apenas faz essa conexão entre quem pública e quem está interessado, ele não altera nem armazena a informação, só encaminha. 

Essa forma de comunicação é simples, rápida e muito eficiente, ideal para aplicações com sensores, automação industrial, ou dispositivos conectados na internet (IoT). 

Os tipos de brokers mais utilizados são os Mosquitto, EMQX, HiveMQ e VerneMQ.
## Instalação Passo a Passo
Com os pré-requisitos em mãos, é hora de partir para a configuração do broker MQTT. Neste guia, utilizaremos o Mosquitto, um dos brokers MQTT mais populares e amplamente adotados no mercado como exemplo prático.  

O processo inclui etapas essenciais como instalação do broker, ajustes de configuração básica (porta de comunicação, autenticação de usuários e permissões de acesso), além de configurações de segurança utilizando criptografia TLS/SSL. 

Também vamos mostrar como realizar testes de publicação e assinatura de mensagens para garantir que o broker esteja funcionando corretamente.  

Por fim, abordaremos como monitorar logs e acompanhar o status das conexões para manter a operação segura e eficiente. 

No Linux (Ubuntu/Debian), abra o terminal e execute os seguintes comandos: 

sudo apt update 

sudo apt install mosquitto mosquitto-clients 

No Windows, acesse o site oficial do Mosquitto ([https://mosquitto.org/download/](https://mosquitto.org/download/)) e baixe o instalador compatível com sua versão do sistema operacional. Siga as instruções do assistente de instalação. 

Após a instalação, é possível ajustar o arquivo de configuração principal, geralmente localizado em /etc/mosquitto/mosquitto.conf (Linux) ou na pasta de instalação (Windows). 

Os principais ajustes incluem: 

- **Porta de comunicação:** A padrão é 1883 para conexões sem TLS; 

- **Criação de usuários e senhas:** Para habilitar autenticação, crie um arquivo de senhas com o comando: 

sudo mosquitto_passwd -c /etc/mosquitto/passwd usuario 

Depois, adicione a configuração no arquivo .conf: 

allow_anonymous false 

password_file /etc/mosquitto/passwd 

- **Controle de acesso (ACL):** Define quem pode publicar ou assinar tópicos específicos. 

Para ambientes de produção, recomenda-se habilitar a comunicação criptografada. Isso garante que os dados transmitidos não sejam interceptados por terceiros. Para isso, é necessário: 

- Gerar um certificado digital (ou adquirir de uma autoridade certificadora); 

- Adicionar as linhas abaixo no arquivo mosquitto.conf: 

listener 8883 

cafile /etc/mosquitto/certs/ca.crt 

certfile /etc/mosquitto/certs/server.crt 

keyfile /etc/mosquitto/certs/server.key 

A porta 8883 é padrão para conexões MQTT seguras. 

Com o broker configurado e em execução, você pode testar a comunicação utilizando os clientes MQTT: 

**Para publicar uma mensagem:** 

mosquitto_pub -h localhost -t “teste/topico” -m “Olá, MQTT!” 

Importante 

Esse teste deve ser feito no mesmo computador com duas instâncias do terminal aberta, justamente por estar utilizando o -h (host) como localhost. Ou seja, apenas a máquina vai conseguir se comunicar com ela mesma.  

Para que outros dispositivos conectados na mesma rede se comuniquem com o broker, deve-se adicionar no arquivo “mosquitto.conf” o seguinte código:  

listener 1883 

Ao incluir o código irá abrir a porta de comunicação para que qualquer dispositivo com IP válido na rede consiga se conectar no broker. Feito isso, deve rodar no terminal o seguinte código:  

mosquitto -c “Local\de\caminho\do\mosquitto.conf” -v  

Deve especificar o local de caminho do arquivo mosquitto.conf para rodar com as configurações feitas. 

Feito isso, o broker vai rodar e todo e qualquer dispositivo que estiver na mesma rede vai conseguir se conectar usando o IP do computador (caso a rede não esteja com a porta bloqueada por firewall).

## Referências
- https://blog.novus.com.br/como-configurar-um-broker-mqtt-passo-a-passo/
- https://test.mosquitto.org/
- https://mosquitto.org/