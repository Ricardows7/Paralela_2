#!/bin/bash
# Arquivo: run_1node_var_omp.sbatch
# Teste: 1 Nó, 1 Task MPI (Threads OMP: 1, 2, 4, 6)

# Diretivas SLURM
#SBATCH --job-name=Bench_N1_VAR
#SBATCH -p cpu
#SBATCH --output=Bench_N1_VAR_%j.out
#SBATCH --error=Bench_N1_VAR_%j.err

# Alocação
#SBATCH --nodes=1
# Apenas 1 processo MPI
#SBATCH --ntasks-per-node=1
#SBATCH --ntasks=1
# 🌟 CORREÇÃO: Aloca os 6 CPUS (núcleos) necessários para testar OMP_NUM_THREADS=6
#SBATCH --cpus-per-task=6 
# 🌟 CORREÇÃO: Aloca 15GB, um valor seguro dado que o nó tem 16GB [cite: 20]
#SBATCH --mem=15G

echo "Iniciando Job SLURM $SLURM_JOBID em 1 NÓ (1 Task, OMP variável 1-6, 20 RUNS)..."
./bench.sh
echo "Job SLURM $SLURM_JOBID concluído."