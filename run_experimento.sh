#!/usr/bin/env bash
#
# Script de execução de experimentos em tmux
# Corrigido: Governor e Frequência desativados; Taskset isolado para o teste.
#

### CONFIGURAÇÕES ==============================
SESSION_NAME="experimento"
LOG_FILE="$HOME/experimento_$(date +%Y%m%d_%H%M%S).log"
SCRIPT_PATH="$HOME/paralela/Paralela_1/run_experiments.sh"
CPU_GOVERNOR="performance"
CPU_FREQ_BASE_PATH="/sys/devices/system/cpu/cpu"
NUM_CORES=16
### ============================================

echo "==> Preparando o ambiente..."

# --- 1️⃣ Criar sessão tmux com tudo rodando dentro dela ---
tmux kill-session -t "$SESSION_NAME" 2>/dev/null

tmux new-session -d -s "$SESSION_NAME" bash -c "
  echo '=== Configurando o ambiente de teste ===';

  # Define a lista de CPUs que usaremos (0-15)
  ISOLATED=\$(seq 0 $((NUM_CORES-1)) | paste -sd, -)

  # --- 2️⃣ MODO PERFORMANCE (TENTATIVA, SEM SAÍDA DE ERRO) ---
  # Mantemos a tentativa, mas agora silenciosa para não quebrar.
  if [[ \$(id -u) -eq 0 ]]; then
      echo 'Tentando configurar o governor para performance (root).';
      for c in $CPU_FREQ_BASE_PATH[0-9]*; do
        if [[ -f \$c/cpufreq/scaling_governor ]]; then
          echo $CPU_GOVERNOR > \$c/cpufreq/scaling_governor 2>/dev/null
        fi
      done
  else
      echo '⚠️ Ignorando configuração de frequência (não-root). O kernel irá gerenciar.';
  fi

  # --- 3️⃣ TRAVAMENTO DE FREQUÊNCIA (REMOVIDO, POIS FALHOU) ---
  # Removido o travamento de frequência e o código MAX_FREQ.

  # --- 4️⃣ RESERVAR NÚCLEOS (taskset no processo principal REMOVIDO) ---
  echo 'Afinando o SCRIPT DE TESTE para os '\$NUM_CORES' núcleos mais rápidos (IDs: '\$ISOLATED').';
  echo 'O gerenciamento do log e tmux rodará em todos os núcleos.'
  
  # --- 5️⃣ Rodar o script de experimento (taskset APENAS AQUI) ---
  echo '=== Iniciando experimento ===';
  echo 'Log: $LOG_FILE';
  echo '-------------------------------------------';
  
  # Aplica o taskset apenas ao seu script de benchmark!
  taskset -c \$ISOLATED bash \"$SCRIPT_PATH\" 2>&1 | tee \"$LOG_FILE\";
  
  echo '-------------------------------------------';
  echo 'Experimento concluído. Pressione Enter para encerrar.';
  read;
"

# --- 6️⃣ Mensagem final ---
echo "✅ Sessão '$SESSION_NAME' criada com sucesso."
echo "📄 Log: $LOG_FILE"
echo "💻 Para conectar à sessão:"
echo "    tmux attach -t $SESSION_NAME"
