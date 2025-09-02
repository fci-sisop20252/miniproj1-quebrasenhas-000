#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include "hash_utils.h"

/**
 * PROCESSO COORDENADOR - Mini-Projeto 1: Quebra de Senhas Paralelo
 * 
 * Este programa coordena múltiplos workers para quebrar senhas MD5 em paralelo.
 * O MD5 JÁ ESTÁ IMPLEMENTADO - você deve focar na paralelização (fork/exec/wait).
 * 
 * Uso: ./coordinator <hash_md5> <tamanho> <charset> <num_workers>
 * 
 * Exemplo: ./coordinator "900150983cd24fb0d6963f7d28e17f72" 3 "abc" 4
 * 
 * SEU TRABALHO: Implementar os TODOs marcados abaixo
 */

#define MAX_WORKERS 16
#define RESULT_FILE "password_found.txt"

/**
 * Calcula o tamanho total do espaço de busca
 * 
 * @param charset_len Tamanho do conjunto de caracteres
 * @param password_len Comprimento da senha
 * @return Número total de combinações possíveis
 */
long long calculate_search_space(int charset_len, int password_len) {
    long long total = 1;
    for (int i = 0; i < password_len; i++) {
        total *= charset_len;
    }
    return total;
}

/**
 * Converte um índice numérico para uma senha
 * Usado para definir os limites de cada worker
 * 
 * @param index Índice numérico da senha
 * @param charset Conjunto de caracteres
 * @param charset_len Tamanho do conjunto
 * @param password_len Comprimento da senha
 * @param output Buffer para armazenar a senha gerada
 */
void index_to_password(long long index, const char *charset, int charset_len, 
                       int password_len, char *output) {
    for (int i = password_len - 1; i >= 0; i--) {
        output[i] = charset[index % charset_len];
        index /= charset_len;
    }
    output[password_len] = '\0';
}

/**
 * Função principal do coordenador
 */
int main(int argc, char *argv[]) {
    // TODO 1: Validar argumentos de entrada
    // Verificar se argc == 5 (programa + 4 argumentos)
    // Se não, imprimir mensagem de uso e sair com código 1
    
    // IMPLEMENTE AQUI: verificação de argc e mensagem de erro
    if(argc != 5){
        printf("Uso correto: %s <hash_md5> <tamanho> <charset> <num_workers>\n", argv[0]);
        return 1;
    }
    
    // Parsing dos argumentos (após validação)
    const char *target_hash = argv[1];
    int password_len = atoi(argv[2]);
    const char *charset = argv[3];
    int num_workers = atoi(argv[4]);
    int charset_len = strlen(charset);
    
    // TODO: Adicionar validações dos parâmetros
    // - password_len deve estar entre 1 e 10
    // - num_workers deve estar entre 1 e MAX_WORKERS
    // - charset não pode ser vazio
    if (password_len < 1 || password_len > 10) {
        fprintf(stderr, "Erro: <tamanho> deve estar entre 1 e 10.\n");
        return 1;
    }
    if (num_workers < 1 || num_workers > MAX_WORKERS) {
        fprintf(stderr, "Erro: <num_workers> deve estar entre 1 e %d.\n", MAX_WORKERS);
        return 1;
    }
    if (charset_len <= 0) {
        fprintf(stderr, "Erro: <charset> não pode ser vazio.\n");
        return 1;
    }
    
    printf("=== Mini-Projeto 1: Quebra de Senhas Paralelo ===\n");
    printf("Hash MD5 alvo: %s\n", target_hash);
    printf("Tamanho da senha: %d\n", password_len);
    printf("Charset: %s (tamanho: %d)\n", charset, charset_len);
    printf("Número de workers: %d\n", num_workers);
    
    // Calcular espaço de busca total
    long long total_space = calculate_search_space(charset_len, password_len);
    printf("Espaço de busca total: %lld combinações\n\n", total_space);
    
    // Remover arquivo de resultado anterior se existir
    unlink(RESULT_FILE);
    
    // Registrar tempo de início
    time_t start_time = time(NULL);
    
    // TODO 2: Dividir o espaço de busca entre os workers
    // Calcular quantas senhas cada worker deve verificar
    // DICA: Use divisão inteira e distribua o resto entre os primeiros workers
    
    // IMPLEMENTE AQUI:
    long long passwords_per_worker = total_space/num_workers;
    long long remaining = total_space % num_workers;
    
    // Arrays para armazenar PIDs dos workers
    pid_t workers[MAX_WORKERS];
    
    // TODO 3: Criar os processos workers usando fork()
    printf("Iniciando workers...\n");
    
    // IMPLEMENTE AQUI: Loop para criar workers
    long long start_index = 0;
    for (int i = 0; i < num_workers; i++) {
        long long count = passwords_per_worker;
        if (i < remaining) {
            count++; 
        }
        long long end_index = start_index + count - 1;

        // TODO: Calcular intervalo de senhas para este worker
        char start_pw[32], end_pw[32];
        index_to_password(start_index, charset, charset_len, password_len, start_pw);
        index_to_password(end_index, charset, charset_len, password_len, end_pw);

        // TODO: Converter indices para senhas de inicio e fim
        // TODO 4: Usar fork() para criar processo filho
        pid_t pid = fork();
    
        if (pid < 0) {
            perror("Erro ao criar worker");
            exit(1);
        } else if (pid == 0) {
            // TODO 6: No processo filho: usar execl() para executar worker
            char len_str[16], id_str[16];
            snprintf(len_str, sizeof(len_str), "%d", password_len);
            snprintf(id_str, sizeof(id_str), "%d", i);
            execl("./worker", "worker", target_hash, start_pw, end_pw, charset, len_str, id_str, NULL);
            perror("Erro no execl");
            exit(1);
        } else {
            // TODO 5: No processo pai: armazenar PID
            workers[i] = pid;
            printf("Worker %d (PID %d): intervalo [%s .. %s]\n", i, pid, start_pw, end_pw);
        }

        start_index = end_index + 1;
    }
    
    // TODO 7: Tratar erros de fork() e execl()
    
    printf("\nTodos os workers foram iniciados. Aguardando conclusão...\n");
    
    // TODO 8: Aguardar todos os workers terminarem usando wait()
    // IMPORTANTE: O pai deve aguardar TODOS os filhos para evitar zumbis
    int workers_terminated = 0;
    while (workers_terminated < num_workers) {
        int status;
        pid_t pid = wait(&status);
        if (pid == -1) {
            perror("Erro no wait");
            break;
        }
        workers_terminated++;

        int worker_index = -1;
        for (int j = 0; j < num_workers; j++) {
            if (workers[j] == pid) {
                worker_index = j;
                break;
            }
        }

        if (worker_index != -1) {
            printf("Worker %d (PID %d) terminou. ", worker_index, pid);
        } else {
            printf("Processo desconhecido (PID %d) terminou. ", pid);
        }

        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            printf("Código de saída: %d\n", exit_code);
        } else if (WIFSIGNALED(status)) {
            printf("Encerrado por sinal: %d\n", WTERMSIG(status));
        } else {
            printf("Término desconhecido.\n");
        }
    }
    printf("Quantidade total do workers que terminaram: %d\n", workers_terminated);

    // IMPLEMENTE AQUI:
    // - Loop para aguardar cada worker terminar
    // - Usar wait() para capturar status de saída
    // - Identificar qual worker terminou
    // - Verificar se terminou normalmente ou com erro
    // - Contar quantos workers terminaram
    
    // Registrar tempo de fim
    time_t end_time = time(NULL);
    double elapsed_time = difftime(end_time, start_time);
    
    printf("\n=== Resultado ===\n");
    
    // TODO 9: Verificar se algum worker encontrou a senha
    // Ler o arquivo password_found.txt se existir
    
    // IMPLEMENTE AQUI:
    // - Abrir arquivo RESULT_FILE para leitura
    // - Ler conteúdo do arquivo
    // - Fazer parse do formato "worker_id:password"
    // - Verificar o hash usando md5_string()
    // - Exibir resultado encontrado
    int found = 0;
    int fd = open(RESULT_FILE, O_RDONLY);
    if (fd >= 0) {
        char buf[256];
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0) {
            buf[n] = '\0';
            char *colon = strchr(buf, ':');
            if (colon) {
                *colon = '\0';
                const char *id_str = buf;
                char *password = colon + 1;
                size_t plen = strlen(password);
                if (plen && (password[plen - 1] == '\n' || password[plen - 1] == '\r')) {
                    password[plen - 1] = '\0';
                }
                char check_hash[33];
                md5_string(password, check_hash);
                if (strcmp(check_hash, target_hash) == 0) {
                    printf("✓ Senha encontrada pelo worker %s: %s\n", id_str, password);
                    found = 1;
                } else {
                    printf("⚠ Resultado inconsistente no arquivo: '%s'\n", password);
                }
            }
        }
    }
    
    // Estatísticas finais (opcional)
    // TODO: Calcular e exibir estatísticas de performance
    if (!found) {
        printf("✗ Nenhum worker encontrou a senha.\n");
    }
    printf("Tempo total: %.2f s\n", elapsed_time);
    
    return found ? 0 : 2;
}
