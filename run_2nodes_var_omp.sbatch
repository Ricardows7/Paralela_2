#!/bin/bash
# Arquivo: run_2nodes_var_omp.sbatch
# Teste: 2 Nós, 2 Tasks MPI (Threads OMP: 1, 2, 4, 6)

# Diretivas SLURM
#SBATCH --job-name=Bench_N2_VAR
#SBATCH -p cpu
#SBATCH --output=Bench_N2_VAR_%j.out
#SBATCH --error=Bench_N2_VAR_%j.err

# Alocação
#SBATCH --nodes=2
# 1 processo MPI por nó
#SBATCH --ntasks-per-node=1
# Total de 2 tasks MPI (2 nós * 1 task/nó)
#SBATCH --ntasks=2
# 🌟 CORREÇÃO: Aloca 6 CPUS por task para testar OMP_NUM_THREADS=6
#SBATCH --cpus-per-task=6
# 🌟 CORREÇÃO: Aloca 30GB (2 nós * 15GB/nó)
#SBATCH --mem=30G

echo "Iniciando Job SLURM $SLURM_JOBID em 2 NÓS (2 Tasks, OMP variável 1-6, 20 RUNS)..."
./bench.sh
echo "Job SLURM $SLURM_JOBID concluído."