/*
  Controle de semáforos para cruzamento de 2 vias + passadeira de peões.

  Regras implementadas:
  - Arranque em AMARELO intermitente (5 s), depois tudo-vermelho antes da via principal.
  - Via principal abre com VERDE e mantém um tempo mínimo obrigatório.
  - Via secundária e peões ficam VERMELHO enquanto a principal está VERDE.
  - Se houver veículo na via secundária (sensor ativo) ou pedido de peão (botão),
    após o mínimo da via principal, o ciclo de mudança é executado em segurança.
  - Sequência dos veículos (principal e secundária): VERMELHO -> VERDE -> AMARELO -> VERMELHO.
  - Sequência dos peões: VERMELHO -> VERDE -> VERDE INTERMITENTE -> VERMELHO.
  - Secundária e peões podem ficar VERDE em simultâneo.
  - Nunca existe VERDE simultâneo entre via principal e secundária/peões.
  - Existe chave de polícia para AMARELO intermitente nas duas vias.
    A entrada neste modo só acontece em estados de tudo-vermelho:
    antes de abrir a via secundária ou antes de abrir a via principal.

  Lógica temporal sem delay(): usa millis() para manter o sistema responsivo.
*/

// ==================== Mapeamento de pinos ====================
const uint8_t VP_VERMELHO = 2;
const uint8_t VP_AMARELO  = 3;
const uint8_t VP_VERDE    = 4;

const uint8_t VS_VERMELHO = 5;
const uint8_t VS_AMARELO  = 6;
const uint8_t VS_VERDE    = 7;

const uint8_t PED_VERMELHO = 8;
const uint8_t PED_VERDE    = 9;

const uint8_t SENSOR_VS    = 10; // Sensor veículo via secundária (INPUT_PULLUP: LOW=detetado)
const uint8_t BOTAO_PED    = 11; // Botão chamada peões (INPUT_PULLUP: LOW=pressionado)
const uint8_t CHAVE_POLICIA = 12; // Chave polícia (INPUT_PULLUP: LOW=ativo)

// ==================== Tempos (ms) ====================
const unsigned long T_MIN_VIA_PRINCIPAL_VERDE = 15000; // mínimo obrigatório via principal aberta
const unsigned long T_AMARELO                 = 3000;  // amarelo para qualquer via
const unsigned long T_TUDO_VERMELHO_SEG       = 1000;  // intervalo de segurança

const unsigned long T_VS_VERDE_MIN            = 7000;  // verde mínimo da via secundária
const unsigned long T_PED_VERDE               = 5000;  // peões verde fixo
const unsigned long T_PED_INTERMITENTE        = 4000;  // duração do verde intermitente
const unsigned long T_PED_BLINK_PERIODO       = 500;   // período do pisca
const unsigned long T_PED_EXTENSAO            = 3000;  // extensão por pedido durante verde peões
const unsigned long T_PED_VERDE_MAX           = 12000; // limite de verde contínuo para peões

const unsigned long T_POLICIA_BLINK_PERIODO   = 500;   // período amarelo intermitente (ambas vias)
const unsigned long T_INICIAL_AMARELO_INTERMITENTE = 5000; // arranque em amarelo intermitente

// ==================== Máquina de estados ====================
enum Estado {
  E_INICIAL_AMARELO_INTERMITENTE, // arranque seguro
  E_PRINCIPAL_VERDE,          // VP verde | VS vermelho | Ped vermelho
  E_PRINCIPAL_AMARELO,        // VP amarelo | VS vermelho | Ped vermelho
  E_TUDO_VERMELHO_ANTES_VS,   // todos veículos vermelho (segurança)
  E_SECUNDARIA_VERDE_PED_VERDE,
  E_SECUNDARIA_VERDE_PED_BLINK,
  E_SECUNDARIA_AMARELO,
  E_TUDO_VERMELHO_ANTES_VP,
  E_MODO_POLICIA_INTERMITENTE // VP amarelo intermitente | VS amarelo intermitente | Ped vermelho
};

Estado estadoAtual = E_INICIAL_AMARELO_INTERMITENTE;
unsigned long tEstado = 0;

bool pedidoPeao = false;
bool pedidoPolicia = false;
bool ultimoBotaoPeao = false;
unsigned long duracaoPedVerdeAtual = T_PED_VERDE;

// ==================== Funções auxiliares ====================
void setViaPrincipal(bool vermelho, bool amarelo, bool verde) {
  digitalWrite(VP_VERMELHO, vermelho ? HIGH : LOW);
  digitalWrite(VP_AMARELO, amarelo ? HIGH : LOW);
  digitalWrite(VP_VERDE, verde ? HIGH : LOW);
}

void setViaSecundaria(bool vermelho, bool amarelo, bool verde) {
  digitalWrite(VS_VERMELHO, vermelho ? HIGH : LOW);
  digitalWrite(VS_AMARELO, amarelo ? HIGH : LOW);
  digitalWrite(VS_VERDE, verde ? HIGH : LOW);
}

void setPeoes(bool vermelho, bool verde) {
  digitalWrite(PED_VERMELHO, vermelho ? HIGH : LOW);
  digitalWrite(PED_VERDE, verde ? HIGH : LOW);
}

bool haVeiculoNaSecundaria() {
  return digitalRead(SENSOR_VS) == LOW;
}

bool botaoPeaoPremido() {
  return digitalRead(BOTAO_PED) == LOW;
}

bool chavePoliciaAtiva() {
  return digitalRead(CHAVE_POLICIA) == LOW;
}

void entrarEstado(Estado novo) {
  estadoAtual = novo;
  tEstado = millis();

  switch (estadoAtual) {
    case E_PRINCIPAL_VERDE:
      setViaPrincipal(false, false, true);
      setViaSecundaria(true, false, false);
      setPeoes(true, false);
      break;

    case E_INICIAL_AMARELO_INTERMITENTE:
      // Arranque: duas vias em amarelo intermitente e peões a vermelho.
      setViaPrincipal(false, true, false);
      setViaSecundaria(false, true, false);
      setPeoes(true, false);
      break;

    case E_PRINCIPAL_AMARELO:
      setViaPrincipal(false, true, false);
      setViaSecundaria(true, false, false);
      setPeoes(true, false);
      break;

    case E_TUDO_VERMELHO_ANTES_VS:
      setViaPrincipal(true, false, false);
      setViaSecundaria(true, false, false);
      setPeoes(true, false);
      break;

    case E_SECUNDARIA_VERDE_PED_VERDE:
      setViaPrincipal(true, false, false);
      setViaSecundaria(false, false, true);
      setPeoes(false, true);
      duracaoPedVerdeAtual = T_PED_VERDE;
      break;

    case E_SECUNDARIA_VERDE_PED_BLINK:
      setViaPrincipal(true, false, false);
      setViaSecundaria(false, false, true);
      // LED pedestre verde intermitente tratado no loop
      break;

    case E_SECUNDARIA_AMARELO:
      setViaPrincipal(true, false, false);
      setViaSecundaria(false, true, false);
      setPeoes(true, false);
      break;

    case E_TUDO_VERMELHO_ANTES_VP:
      setViaPrincipal(true, false, false);
      setViaSecundaria(true, false, false);
      setPeoes(true, false);
      break;

    case E_MODO_POLICIA_INTERMITENTE:
      // Inicia com amarelo ligado; o pisca é atualizado no loop.
      setViaPrincipal(false, true, false);
      setViaSecundaria(false, true, false);
      setPeoes(true, false);
      break;
  }
}

void setup() {
  pinMode(VP_VERMELHO, OUTPUT);
  pinMode(VP_AMARELO, OUTPUT);
  pinMode(VP_VERDE, OUTPUT);

  pinMode(VS_VERMELHO, OUTPUT);
  pinMode(VS_AMARELO, OUTPUT);
  pinMode(VS_VERDE, OUTPUT);

  pinMode(PED_VERMELHO, OUTPUT);
  pinMode(PED_VERDE, OUTPUT);

  pinMode(SENSOR_VS, INPUT_PULLUP);
  pinMode(BOTAO_PED, INPUT_PULLUP);
  pinMode(CHAVE_POLICIA, INPUT_PULLUP);

  entrarEstado(E_INICIAL_AMARELO_INTERMITENTE);
}

void loop() {
  const unsigned long agora = millis();
  const unsigned long dt = agora - tEstado;
  const bool botaoPeaoAgora = botaoPeaoPremido();
  const bool eventoBotaoPeao = botaoPeaoAgora && !ultimoBotaoPeao;
  ultimoBotaoPeao = botaoPeaoAgora;

  // Memória do pedido de peão: basta pressionar uma vez para guardar pedido.
  if (eventoBotaoPeao && estadoAtual != E_SECUNDARIA_VERDE_PED_VERDE) {
    pedidoPeao = true;
  }

  // Pedido de polícia pode surgir em qualquer fase, mas só é executado em estados tudo-vermelho.
  if (chavePoliciaAtiva()) {
    pedidoPolicia = true;
  }

  switch (estadoAtual) {
    case E_INICIAL_AMARELO_INTERMITENTE: {
      const bool amareloOn = ((dt / T_POLICIA_BLINK_PERIODO) % 2) == 0;
      setViaPrincipal(false, amareloOn, false);
      setViaSecundaria(false, amareloOn, false);
      setPeoes(true, false);

      if (dt >= T_INICIAL_AMARELO_INTERMITENTE) {
        entrarEstado(E_TUDO_VERMELHO_ANTES_VP);
      }
      break;
    }

    case E_PRINCIPAL_VERDE: {
      // Via principal tem de cumprir o mínimo aberto.
      const bool minimoCumprido = dt >= T_MIN_VIA_PRINCIPAL_VERDE;
      const bool haPedidoMudanca = haVeiculoNaSecundaria() || pedidoPeao;

      if (minimoCumprido && haPedidoMudanca) {
        entrarEstado(E_PRINCIPAL_AMARELO);
      }
      break;
    }

    case E_PRINCIPAL_AMARELO:
      if (dt >= T_AMARELO) {
        entrarEstado(E_TUDO_VERMELHO_ANTES_VS);
      }
      break;

    case E_TUDO_VERMELHO_ANTES_VS:
      if (dt >= T_TUDO_VERMELHO_SEG) {
        if (pedidoPolicia && chavePoliciaAtiva()) {
          entrarEstado(E_MODO_POLICIA_INTERMITENTE);
        } else {
          entrarEstado(E_SECUNDARIA_VERDE_PED_VERDE);
        }
      }
      break;

    case E_SECUNDARIA_VERDE_PED_VERDE:
      // Secundária e peões verdes em simultâneo.
      if (eventoBotaoPeao) {
        duracaoPedVerdeAtual += T_PED_EXTENSAO;
        if (duracaoPedVerdeAtual > T_PED_VERDE_MAX) {
          duracaoPedVerdeAtual = T_PED_VERDE_MAX;
        }
      }

      if (dt >= duracaoPedVerdeAtual) {
        entrarEstado(E_SECUNDARIA_VERDE_PED_BLINK);
      }
      break;

    case E_SECUNDARIA_VERDE_PED_BLINK: {
      // Verde dos peões intermitente.
      const bool pedOn = ((dt / T_PED_BLINK_PERIODO) % 2) == 0;
      setPeoes(false, pedOn);

      const bool tempoPeaoTerminado = dt >= T_PED_INTERMITENTE;
      const unsigned long tempoTotalVSVerde = duracaoPedVerdeAtual + dt;
      const bool verdeVsMinimoCumprido = tempoTotalVSVerde >= T_VS_VERDE_MIN;

      if (tempoPeaoTerminado && verdeVsMinimoCumprido) {
        entrarEstado(E_SECUNDARIA_AMARELO);
      }
      break;
    }

    case E_SECUNDARIA_AMARELO:
      if (dt >= T_AMARELO) {
        entrarEstado(E_TUDO_VERMELHO_ANTES_VP);
      }
      break;

    case E_TUDO_VERMELHO_ANTES_VP:
      if (dt >= T_TUDO_VERMELHO_SEG) {
        if (pedidoPolicia && chavePoliciaAtiva()) {
          entrarEstado(E_MODO_POLICIA_INTERMITENTE);
        } else {
          // Pedido de peão atendido neste ciclo.
          pedidoPeao = false;
          entrarEstado(E_PRINCIPAL_VERDE);
        }
      }
      break;

    case E_MODO_POLICIA_INTERMITENTE: {
      // Mantém peões a vermelho e amarelos das duas vias em pisca.
      setPeoes(true, false);

      const bool amareloOn = ((dt / T_POLICIA_BLINK_PERIODO) % 2) == 0;
      setViaPrincipal(false, amareloOn, false);
      setViaSecundaria(false, amareloOn, false);

      // Saída segura do modo polícia: quando a chave desativa,
      // força estado tudo-vermelho antes de retomar ciclo normal.
      if (!chavePoliciaAtiva()) {
        pedidoPolicia = false;
        entrarEstado(E_TUDO_VERMELHO_ANTES_VP);
      }
      break;
    }
  }
}
