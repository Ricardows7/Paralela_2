#!/usr/bin/env bash
# Arquivo: generate_inputs.sh
# Uso: Roda no nó de login (nomade) ANTES de submeter o job SLURM.

INPUT_LIST="entradas_nao_processadas"
OUTPUT_DIR="./entradas"
GEN_BIN="./input_generator"

# Valida o gerador e o arquivo de lista
if [ ! -x "$GEN_BIN" ]; then 
    echo "ERRO: O binário $GEN_BIN não é executável. Compile-o primeiro:"
    echo "g++ -std=c++17 -O3 input-generator.cc -o input_generator"
    exit 1
fi
if [ ! -f "$INPUT_LIST" ]; then 
    echo "ERRO: Arquivo de lista de parâmetros '$INPUT_LIST' não encontrado."
    exit 1
fi

echo "Iniciando geração de entradas..."
mkdir -p "$OUTPUT_DIR"

# Loop para gerar um arquivo de entrada para cada parâmetro na lista
while IFS= read -r param; do
    param_clean=$(echo "$param" | tr -d '\r\n' | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
    if [ -z "$param_clean" ]; then continue; fi

    OUTPUT_FILE="${OUTPUT_DIR}/${param_clean}.txt"
    
    echo "  Gerando entrada para parâmetro: '$param_clean' -> $OUTPUT_FILE"
    
    # Envia o parâmetro para o gerador e salva a saída (N seguido pelas Strings)
    if echo "$param_clean" | "$GEN_BIN" > "$OUTPUT_FILE"; then
        echo "  ✅ Sucesso."
    else
        echo "  ❌ ERRO na geração para '$param_clean'."
    fi
done < "$INPUT_LIST"

echo "Geração concluída. O diretório '$OUTPUT_DIR' está pronto para o benchmark."