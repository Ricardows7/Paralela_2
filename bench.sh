#!/usr/bin/env bash
# Arquivo: bench.sh - Versão Final para Cluster Nomade (20 Runs por Config)

# --- Variáveis de Configuração ---
RUNS=20 # Número de repetições
SRC_FILE="shortest_superstring.cc"
INPUTS_DIR="./entradas"
THREADS_FILE="./threads.txt"
RESULTS_DIR="./results"
RAW_DIR="${RESULTS_DIR}/raw"
OUT_CSV="${RESULTS_DIR}/metrics.csv"
LOG_FILE="${RESULTS_DIR}/log.txt"

# Binários
SEQ_PURE_BIN="./shsup_seq_pure" 
PAR_MPI_BIN="./shsup_hybrid"    

# --- Setup Inicial ---
set -euo pipefail
export LC_NUMERIC="C"

mkdir -p "$RAW_DIR"

rm -f "$OUT_CSV" "$LOG_FILE"
echo "==== Início dos testes: $(date) (RUNS=${RUNS}) ====" > "$LOG_FILE"

# Validação e Carregamento (Omitido o corpo por brevidade)
if [ ! -d "$INPUTS_DIR" ]; then echo "ERRO: Diretório de entradas ($INPUTS_DIR) não encontrado." >> "$LOG_FILE"; exit 1; fi
if [ ! -f "$THREADS_FILE" ]; then echo "ERRO: threads.txt não encontrado." >> "$LOG_FILE"; exit 1; fi
if [ ! -f "$SRC_FILE" ]; then echo "ERRO: código-fonte $SRC_FILE não encontrado." >> "$LOG_FILE"; exit 1; fi

mapfile -t ENTRADAS_ARQUIVOS < <(find "$INPUTS_DIR" -maxdepth 1 -type f -name "*.txt" | sort)
mapfile -t OMP_THREADS_LIST < "$THREADS_FILE"

# --- Funções Auxiliares (Omitido o corpo por brevidade) ---
clean_value() { echo "$1" | tr -d '\r' | sed 's/^[[:space:]]*//;s/[[:space:]]*$//'; }
escape_csv() { local field="$1"; if [[ "$field" =~ [,\"] ]]; then field="\"${field//\"/\"\"}\""; fi; echo "$field"; }
calc_mean_stddev() { awk '{sum+=$1; sumsq+=$1*$1; n+=1} END {if(n==0){print "0.000000 0.000000"}else if(n==1){printf "%.6f 0.000000\n", sum}else{mean=sum/n;variance=(sumsq-(sum*sum/n))/(n-1);if(variance<0)variance=0;sd=sqrt(variance);printf "%.6f %.6f\n", mean, sd}}'; }
safe_division() { awk -v num="$1" -v den="$2" -v def="$3" 'BEGIN {if(den!=0&&den!=""&&num!="")printf "%.6f", num/den;else print def}'; }
safe_percentage() { awk -v total="$1" -v parallel="$2" 'BEGIN {if(total>0&&total!=""&&parallel!="")printf "%.2f", 100*(total-parallel)/total;else print "0.00"}'; }

# --- Compilação ---
echo "Compilando versões..." >> "$LOG_FILE"
# 1. Sequencial Pura (g++ + DSEQUENTIAL_PURE) - BASELINE
g++ -std=c++17 -O3 -Wall -DSEQUENTIAL_PURE "$SRC_FILE" -o "$SEQ_PURE_BIN"
# 2. Híbrida (mpicxx + OpenMP)
mpicxx -std=c++17 -O3 -Wall -fopenmp "$SRC_FILE" -o "$PAR_MPI_BIN" 
echo "Compilação concluída." >> "$LOG_FILE"

# --- Cabeçalho do CSV ---
{
    echo "entrada,tipo,threads_omp,tasks_mpi_total,num_nodes,mean_total_ms,std_total_ms,mean_parallel_ms,std_parallel_ms,serial_percent_mean,speedup,efficiency_percent,runs"
} > "$OUT_CSV"

# --- Variáveis SLURM ---
NUM_NODES=${SLURM_NNODES:-1}
TOTAL_TASKS=${SLURM_NTASKS:-1}

echo "Configuração do Job SLURM: Nós=$NUM_NODES, Tasks Totais=$TOTAL_TASKS (RUNS=${RUNS})" >> "$LOG_FILE"

# ==========================================================
# 1️⃣ Loop de Entrada
# ==========================================================
for entrada_path in "${ENTRADAS_ARQUIVOS[@]}"; do
    entrada_base=$(basename "$entrada_path" .txt)
    entrada_escaped=$(escape_csv "$entrada_base")
    safe_entry=$(echo "$entrada_base" | tr ' /,' '__' | sed 's/__*/_/g')

    echo ">>> Entrada: '$entrada_base'" | tee -a "$LOG_FILE"

    # --- Execução SEQUENCIAL PURA (NOVA BASELINE) ---
    SEQ_THREADS=1 
    out_seq_pure="${RAW_DIR}/out_${safe_entry}_SEQ_PURE.txt"
    echo "===== SEQUENTIAL PURE BASE RUN (20 RUNS) =====" > "$out_seq_pure"
    
    total_times_seq_pure=()
    parallel_times_seq_pure=()
    
    for ((r=1; r<=RUNS; r++)); do
        echo "===== SEQ PURE RUN ${r} =====" >> "$out_seq_pure"
        
        if cat "$entrada_path" | srun -n 1 --exclusive --cpus-per-task=1 "$SEQ_PURE_BIN" >> "$out_seq_pure" 2>&1; then :; else echo "Erro na execução SEQ PURE $r" >> "$out_seq_pure"; fi
        
        total_ms=$(grep -aEo "Tempo total:[[:space:]]*[0-9]+(\.[0-9]*)?" "$out_seq_pure" | tail -n 1 | grep -Eo "[0-9]+(\.[0-9]*)?")
        parallel_ms=$(grep -aEo "Tempo paralelo:[[:space:]]*[0-9]+(\.[0-9]*)?" "$out_seq_pure" | tail -n 1 | grep -Eo "[0-9]+(\.[0-9]*)?")
        
        total_times_seq_pure+=("$(echo "${total_ms:-0}" | awk '{if ($1 == "" || $1 == ".") print "0.000000"; else printf "%.6f", $1}')")
        parallel_times_seq_pure+=("$(echo "${parallel_ms:-0}" | awk '{if ($1 == "" || $1 == ".") print "0.000000"; else printf "%.6f", $1}')")
    done

    # Estatísticas sequenciais puras
    mean_std_total_seq_pure=$(printf "%s\n" "${total_times_seq_pure[@]}" | calc_mean_stddev)
    mean_total_seq_pure=$(echo "$mean_std_total_seq_pure" | awk '{printf "%.6f", $1}')
    mean_std_parallel_seq_pure=$(printf "%s\n" "${parallel_times_seq_pure[@]}" | calc_mean_stddev)
    
    # Linha sequencial Pura no CSV
    printf "%s,seq_pure,%d,1,1,%.6f,%.6f,%.6f,%.6f,%.2f,1.00,%.2f,%d\n" \
      "$entrada_escaped" "$SEQ_THREADS" "$mean_total_seq_pure" "$(echo "$mean_std_total_seq_pure" | awk '{printf "%.6f", $2}')" \
      "$(echo "$mean_std_parallel_seq_pure" | awk '{printf "%.6f", $1}')" "$(echo "$mean_std_parallel_seq_pure" | awk '{printf "%.6f", $2}')" \
      "$(safe_percentage "$mean_total_seq_pure" "$(echo "$mean_std_parallel_seq_pure" | awk '{printf "%.6f", $1}')")" \
      "100.00" "$RUNS" >> "$OUT_CSV"


    # ==========================================================
    # 2️⃣ Loop de Variação de Threads (HÍBRIDO)
    # ==========================================================
    for omp_t in "${OMP_THREADS_LIST[@]}"; do
        omp_t_clean=$(clean_value "$omp_t")
        [[ -z "$omp_t_clean" ]] || [[ ! "$omp_t_clean" =~ ^[0-9]+$ ]] && continue
        
        # --- Execução HÍBRIDA (MPI + OMP) ---
        echo "  >> Híbrido: $TOTAL_TASKS tasks ($omp_t_clean OMP threads/task) em $NUM_NODES nós (20 RUNS)" | tee -a "$LOG_FILE"

        total_times=()
        parallel_times=()
        
        out="${RAW_DIR}/out_${safe_entry}_HYBRID_N${NUM_NODES}_T${TOTAL_TASKS}_OMP${omp_t_clean}.txt"
        echo -n "" > "$out"

        for ((r=1; r<=RUNS; r++)); do
            echo "===== HYBRID RUN ${r} =====" >> "$out"
            
            export OMP_NUM_THREADS="$omp_t_clean"
            
            if cat "$entrada_path" | srun --exclusive --mpi=pmi2 "$PAR_MPI_BIN" >> "$out" 2>&1; then
                :
            else
                echo "Erro na execução HÍBRIDA $r" >> "$out"
            fi

            total_ms=$(grep -aEo "Tempo total:[[:space:]]*[0-9]+(\.[0-9]*)?" "$out" | tail -n 1 | grep -Eo "[0-9]+(\.[0-9]*)?")
            parallel_ms=$(grep -aEo "Tempo paralelo:[[:space:]]*[0-9]+(\.[0-9]*)?" "$out" | tail -n 1 | grep -Eo "[0-9]+(\.[0-9]*)?")
            
            total_times+=("$(echo "${total_ms:-0}" | awk '{if ($1 == "" || $1 == ".") print "0.000000"; else printf "%.6f", $1}')")
            parallel_times+=("$(echo "${parallel_ms:-0}" | awk '{if ($1 == "" || $1 == ".") print "0.000000"; else printf "%.6f", $1}')")
        done

        # Estatísticas híbridas
        mean_std_total=$(printf "%s\n" "${total_times[@]}" | calc_mean_stddev)
        mean_total=$(echo "$mean_std_total" | awk '{printf "%.6f", $1}')
        mean_std_parallel=$(printf "%s\n" "${parallel_times[@]}" | calc_mean_stddev)

        PAR_FACTOR=$(( TOTAL_TASKS * omp_t_clean ))
        
        speedup=$(safe_division "$mean_total_seq_pure" "$mean_total" "0.000")
        speedup_clean=$(echo "$speedup" | awk '{printf "%.3f", $1}')
        
        efficiency_clean=$(echo "100 * $(safe_division "$speedup_clean" "$PAR_FACTOR" "0.000")" | bc -l 2>/dev/null | awk '{printf "%.2f", $1}' || echo "0.00")

        # Linha híbrida no CSV
        printf "%s,hibrido,%d,%d,%d,%.6f,%.6f,%.6f,%.6f,%.2f,%.3f,%.2f,%d\n" \
          "$entrada_escaped" "$omp_t_clean" "$TOTAL_TASKS" "$NUM_NODES" "$mean_total" "$(echo "$mean_std_total" | awk '{printf "%.6f", $2}')" \
          "$(echo "$mean_std_parallel" | awk '{printf "%.6f", $1}')" "$(echo "$mean_std_parallel" | awk '{printf "%.6f", $2}')" \
          "$(safe_percentage "$mean_total" "$(echo "$mean_std_parallel" | awk '{printf "%.6f", $1}')")" "$speedup_clean" "$efficiency_clean" "$RUNS" >> "$OUT_CSV"
    done
done

echo "==== Fim dos testes: $(date) ====" >> "$LOG_FILE"
echo "✅ CSV gerado com sucesso: $OUT_CSV"