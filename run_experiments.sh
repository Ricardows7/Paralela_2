#!/usr/bin/env bash
# run_benchmarks.sh — versão corrigida para GitHub

RUNS=20
SEQ_BIN="./shsup_seq"
PAR_BIN="./shsup"
GEN="./input-generator"
INPUT_TMP="./entrada.txt"
RESULTS_DIR="./results"
RAW_DIR="${RESULTS_DIR}/raw"
OUT_CSV="${RESULTS_DIR}/metrics.csv"
LOG_FILE="${RESULTS_DIR}/log.txt"

set -euo pipefail
# 🌟 CORREÇÃO 1: Garante que o separador decimal seja o PONTO (.), crucial para o CSV
export LC_NUMERIC="C"

mkdir -p "$RAW_DIR"

# Limpa arquivos anteriores
rm -f "$OUT_CSV" "$LOG_FILE"

# valida
if [ ! -f entradas.txt ]; then echo "ERRO: entradas.txt não encontrado."; exit 1; fi
if [ ! -f threads.txt ]; then echo "ERRO: threads.txt não encontrado."; exit 1; fi
if [ ! -x "$GEN" ]; then echo "ERRO: input-generator não encontrado/executável ($GEN)."; exit 1; fi
if [ ! -f shortest_superstring.cc ]; then echo "ERRO: código-fonte shortest_superstring.cc não encontrado."; exit 1; fi

# Função para limpar CR e espaços
clean_value() {
    echo "$1" | tr -d '\r' | sed 's/^[[:space:]]*//;s/[[:space:]]*$//'
}

# Função para escapar CSV
escape_csv() {
    local field="$1"
    # Se contém vírgula, aspas ou quebra de linha, coloca entre aspas
    if [[ "$field" =~ [,\"] ]]; then
        field="\"${field//\"/\"\"}\""
    fi
    echo "$field"
}

# compila
echo "Compilando versões..."
g++ -std=c++17 -O3 -Wall shortest_superstring.cc -o "$SEQ_BIN"
g++ -std=c++17 -O3 -Wall -fopenmp shortest_superstring.cc -o "$PAR_BIN"
echo "Compilação concluída."

mapfile -t ENTRADAS < entradas.txt
mapfile -t THREADS < threads.txt

calc_mean_stddev() {
    awk '{
      sum+=$1; 
      sumsq+=$1*$1; 
      n+=1
    }
    END {
      if(n==0) {
        print "0.000000 0.000000"
      } else if(n==1) {
        printf "%.6f 0.000000\n", sum
      } else {
        mean=sum/n
        variance=(sumsq - (sum*sum/n))/(n-1)
        if(variance < 0) variance = 0
        sd=sqrt(variance)
        printf "%.6f %.6f\n", mean, sd
      }
    }'
}

# Função segura para divisão
safe_division() {
    local numerator="$1"
    local denominator="$2"
    local default="$3"
    
    awk -v num="$numerator" -v den="$denominator" -v def="$default" 'BEGIN {
        if (den != 0 && den != "" && num != "") 
            printf "%.6f", num/den
        else 
            print def
    }'
}

# Função segura para porcentagem
safe_percentage() {
    local total="$1"
    local parallel="$2"
    
    awk -v total="$total" -v parallel="$parallel" 'BEGIN {
        if (total > 0 && total != "" && parallel != "") 
            printf "%.2f", 100 * (total - parallel) / total
        else 
            print "0.00"
    }'
}

# Cria arquivo CSV com cabeçalho
{
    echo "entrada,tipo,threads,mean_total_ms,std_total_ms,mean_parallel_ms,std_parallel_ms,serial_percent_mean,speedup,efficiency_percent,runs"
} > "$OUT_CSV"

echo "==== Início dos testes: $(date) ====" > "$LOG_FILE"

for entrada in "${ENTRADAS[@]}"; do
    entrada_clean=$(clean_value "$entrada")
    [[ -z "$entrada_clean" ]] && continue

    # Escapa a entrada para CSV
    entrada_escaped=$(escape_csv "$entrada_clean")
    
    # Para nomes de arquivo, usa versão segura
    safe_entry=$(echo "$entrada_clean" | tr ' /,' '__' | sed 's/__*/_/g')

    echo ">>> Entrada: '$entrada_clean'"
    echo "$entrada_clean" | "$GEN" > "$INPUT_TMP"

    #
    # 1️⃣ Execução SEQUENCIAL
    #
    out_seq="${RAW_DIR}/out_${safe_entry}_SEQ.txt"
    echo "===== SEQUENTIAL RUN =====" > "$out_seq"

    total_times_seq=()
    parallel_times_seq=()

    for ((r=1; r<=RUNS; r++)); do
        echo "===== SEQ RUN ${r} =====" >> "$out_seq"
        
        # Linha de threads fictícia + entrada
        if ( printf "1\n"; cat "$INPUT_TMP" ) | "$SEQ_BIN" >> "$out_seq" 2>&1; then
            :
        else
            echo "Erro na execução SEQ $r" >> "$out_seq"
        fi

        # Extrai tempos garantindo valores numéricos
        # 🌟 CORREÇÃO 2: Adiciona -a (ou --text) ao grep para tratar arquivos binários/misto como texto
        total_ms=$(grep -aEo "Tempo total:[[:space:]]*[0-9]+(\.[0-9]*)?" "$out_seq" | tail -n 1 | grep -Eo "[0-9]+(\.[0-9]*)?")
        parallel_ms=$(grep -aEo "Tempo paralelo:[[:space:]]*[0-9]+(\.[0-9]*)?" "$out_seq" | tail -n 1 | grep -Eo "[0-9]+(\.[0-9]*)?")
        
        total_ms=${total_ms:-0}
        parallel_ms=${parallel_ms:-0}

        total_times_seq+=("$(echo "$total_ms" | awk '{if ($1 == "" || $1 == ".") print "0.000000"; else printf "%.6f", $1}')")
        parallel_times_seq+=("$(echo "$parallel_ms" | awk '{if ($1 == "" || $1 == ".") print "0.000000"; else printf "%.6f", $1}')")
    done

    # Estatísticas sequenciais
    mean_std_total_seq=$(printf "%s\n" "${total_times_seq[@]}" | calc_mean_stddev)
    mean_std_parallel_seq=$(printf "%s\n" "${parallel_times_seq[@]}" | calc_mean_stddev)
    
    mean_total_seq=$(echo "$mean_std_total_seq" | awk '{printf "%.6f", $1}')
    std_total_seq=$(echo "$mean_std_total_seq" | awk '{printf "%.6f", $2}')
    mean_parallel_seq=$(echo "$mean_std_parallel_seq" | awk '{printf "%.6f", $1}')
    std_parallel_seq=$(echo "$mean_std_parallel_seq" | awk '{printf "%.6f", $2}')

    serial_percent_seq=$(safe_percentage "$mean_total_seq" "$mean_parallel_seq")

    # Linha sequencial no CSV - usando printf para formato consistente
    printf "%s,seq,1,%.6f,%.6f,%.6f,%.6f,%.2f,1.00,100.00,%d\n" \
      "$entrada_escaped" "$mean_total_seq" "$std_total_seq" "$mean_parallel_seq" "$std_parallel_seq" \
      "$serial_percent_seq" "$RUNS" >> "$OUT_CSV"

    #
    # 2️⃣ Execuções paralelas (OpenMP)
    #
    for t in "${THREADS[@]}"; do
        t_clean=$(clean_value "$t")
        [[ -z "$t_clean" ]] || [[ ! "$t_clean" =~ ^[0-9]+$ ]] && continue
        
        echo "  >> Threads: $t_clean"

        total_times=()
        parallel_times=()

        out="${RAW_DIR}/out_${safe_entry}_T${t_clean}.txt"
        echo -n "" > "$out"

        for ((r=1; r<=RUNS; r++)); do
            echo "===== RUN ${r} =====" >> "$out"
            if ( printf "%s\n" "$t_clean"; cat "$INPUT_TMP" ) | timeout 600 "$PAR_BIN" >> "$out" 2>&1; then
                :
            else
                echo "Erro na execução $r com ${t_clean} threads" >> "$out"
            fi

            # Extrai tempos
            # 🌟 CORREÇÃO 2: Adiciona -a (ou --text) ao grep
            total_ms=$(grep -aEo "Tempo total:[[:space:]]*[0-9]+(\.[0-9]*)?" "$out" | tail -n 1 | grep -Eo "[0-9]+(\.[0-9]*)?")
            parallel_ms=$(grep -aEo "Tempo paralelo:[[:space:]]*[0-9]+(\.[0-9]*)?" "$out" | tail -n 1 | grep -Eo "[0-9]+(\.[0-9]*)?")
            
            total_ms=${total_ms:-0}
            parallel_ms=${parallel_ms:-0}

            total_times+=("$(echo "$total_ms" | awk '{if ($1 == "" || $1 == ".") print "0.000000"; else printf "%.6f", $1}')")
            parallel_times+=("$(echo "$parallel_ms" | awk '{if ($1 == "" || $1 == ".") print "0.000000"; else printf "%.6f", $1}')")
        done

        # Estatísticas paralelas
        mean_std_total=$(printf "%s\n" "${total_times[@]}" | calc_mean_stddev)
        mean_std_parallel=$(printf "%s\n" "${parallel_times[@]}" | calc_mean_stddev)
        
        mean_total=$(echo "$mean_std_total" | awk '{printf "%.6f", $1}')
        std_total=$(echo "$mean_std_total" | awk '{printf "%.6f", $2}')
        mean_parallel=$(echo "$mean_std_parallel" | awk '{printf "%.6f", $1}')
        std_parallel=$(echo "$mean_std_parallel" | awk '{printf "%.6f", $2}')

        serial_percent=$(safe_percentage "$mean_total" "$mean_parallel")

        speedup=$(safe_division "$mean_total_seq" "$mean_total" "0.000")
        speedup_clean=$(echo "$speedup" | awk '{printf "%.3f", $1}')
        
        efficiency=$(safe_division "$speedup_clean" "$t_clean" "0.000")
        efficiency_clean=$(echo "100 * $efficiency" | bc -l 2>/dev/null | awk '{printf "%.2f", $1}' || echo "0.00")

        # Linha paralela no CSV usando printf
        printf "%s,omp,%d,%.6f,%.6f,%.6f,%.6f,%.2f,%.3f,%.2f,%d\n" \
          "$entrada_escaped" "$t_clean" "$mean_total" "$std_total" "$mean_parallel" "$std_parallel" \
          "$serial_percent" "$speedup_clean" "$efficiency_clean" "$RUNS" >> "$OUT_CSV"
    done
done

echo "==== Fim dos testes: $(date) ====" >> "$LOG_FILE"

# Verifica e corrige o arquivo CSV final
if [ -f "$OUT_CSV" ]; then
    # Remove linhas vazias e caracteres de controle
    sed -i '/^$/d' "$OUT_CSV"
    # Garante que o arquivo termina com quebra de linha
    echo "" >> "$OUT_CSV"
    echo "✅ CSV gerado com sucesso: $OUT_CSV"
    echo "📊 Primeiras linhas do CSV:"
    head -n 5 "$OUT_CSV"
else
    echo "❌ Erro: CSV não foi gerado"
    exit 1
fi
