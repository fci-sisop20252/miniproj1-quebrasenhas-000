# Relatório: Mini-Projeto 1 - Quebra-Senhas Paralelo

**Aluno(s):** 
Nome:Camila Huang (10419606) 
Nome:Vinicius Wu (10435817)
Nome:Xuanjun Jin (10436456),,,  
---

## 1. Estratégia de Paralelização


**Como você dividiu o espaço de busca entre os workers?**

O espaço de busca é calculado com base no número total de combinações possíveis, determinado pelo tamanho do conjunto de caracteres (charset_len) e o comprimento da senha (password_len). Este valor total é dividido entre os workers de forma que cada worker tenha uma quantidade aproximadamente igual de trabalho.

Passos:

Divisão Inicial: O número total de combinações é dividido igualmente entre os workers utilizando divisão inteira.

Distribuição do Resto: O resto das combinações é distribuído entre os primeiros workers, garantindo que os primeiros workers recebam uma combinação extra se necessário.

**Código relevante:** Cole aqui a parte do coordinator.c onde você calcula a divisão:
```c
// Cole seu código de divisão aqui
    long long passwords_per_worker = total_space/num_workers;
    long long remaining = total_space % num_workers;
```

---
aa
## 2. Implementação das System Calls

**Descreva como você usou fork(), execl() e wait() no coordinator:**

No coordinator.c, utilizei a chamada fork() para criar novos processos filhos, onde cada filho se transforma em um worker. Após o fork(), o processo filho substitui sua imagem de execução chamando execl(), passando como argumentos o hash alvo, a senha inicial e final do intervalo, o charset, o tamanho da senha e o ID do worker. Já o processo pai armazena o PID do worker criado e continua criando os demais. Depois que todos os workers foram iniciados, o coordenador utiliza a chamada wait() em um laço para aguardar a finalização de todos os processos filhos, evitando processos zumbis e permitindo capturar o status de saída de cada worker.

**Código do fork/exec:**
```c
// Cole aqui seu loop de criação de workers
// Loop para criar workers
long long start_index = 0;
for (int i = 0; i < num_workers; i++) {
    long long count = passwords_per_worker;
    if (i < remaining) {
        count++; 
    }
    if(count<=0) count=1;
    if (start_index >= total_space) {
        break; // Workers extras não são necessários
    }
    long long end_index = start_index + count - 1;
    if (end_index >= total_space) {
        end_index = total_space - 1;
    }
    // Calcular intervalo de senhas para este worker
    char start_pw[32], end_pw[32];
    index_to_password(start_index, charset, charset_len, password_len, start_pw);
    index_to_password(end_index, charset, charset_len, password_len, end_pw);

    pid_t pid = fork();
    
    if (pid < 0) {
        perror("Erro ao criar worker");
        exit(1);
    } else if (pid == 0) {
        // Processo filho: executa o worker
        char len_str[16], id_str[16];
        snprintf(len_str, sizeof(len_str), "%d", password_len);
        snprintf(id_str, sizeof(id_str), "%d", i);
        execl("./worker", "worker", target_hash, start_pw, end_pw, charset, len_str, id_str, NULL);
        perror("Erro no execl");
        exit(1);
    } else {
        // Processo pai: armazena PID e imprime intervalo
        workers[i] = pid;
        printf("Worker %d (PID %d): intervalo [%s .. %s]\n", i, pid, start_pw, end_pw);
    }

    start_index = end_index + 1;
}

```

---

## 3. Comunicação Entre Processos

**Como você garantiu que apenas um worker escrevesse o resultado?**

No meu código do worker, eu implementei a escrita atômica usando:

int fd = open(RESULT_FILE, O_CREAT | O_EXCL | O_WRONLY, 0644);


Com isso, apenas o primeiro worker que encontrar a senha consegue criar e escrever no arquivo password_found.txt. Os outros workers que tentarem escrever falharão no open() e não vão sobrescrever o arquivo.

Além disso, cada worker verifica periodicamente se o arquivo já existe usando check_result_exists(). Se o arquivo existir, o worker encerra sua execução imediatamente.

Dessa forma, evito condições de corrida, garantindo que nenhum worker sobrescreva o resultado de outro, mantendo os dados consistentes e confiáveis.
**Como o coordinator consegue ler o resultado?**

No coordinator, depois que todos os workers terminam, eu abro o arquivo password_found.txt em modo leitura, leio o conteúdo para um buffer e faço o parse do formato "worker_id:password" usando strchr para separar o ID do worker da senha.

Em seguida, eu verifico o hash da senha lida com md5_string() para confirmar que ela corresponde ao hash alvo. Por fim, exibo a senha encontrada e qual worker a descobriu.

Assim, consigo ler o resultado de forma segura e confiável, sabendo que apenas um worker pôde escrever o arquivo devido à escrita atômica.

---

## 4. Análise de Performance
Complete a tabela com tempos reais de execução:
O speedup é o tempo do teste com 1 worker dividido pelo tempo com 4 workers.

| Teste | 1 Worker | 2 Workers | 4 Workers | Speedup (4w) |
|-------|----------|-----------|-----------|--------------|
| Hash: 202cb962ac59075b964b07152d234b70<br>Charset: "0123456789"<br>Tamanho: 3<br>Senha: "123" | _0.006__s | _0.008__s | __0.008_s | _0.75__ |
| Hash: 5d41402abc4b2a76b9719d911017c592<br>Charset: "abcdefghijklmnopqrstuvwxyz"<br>Tamanho: 5<br>Senha: "hello" | _4.492__s | __7.829_s | 1.730___s | __2.6_ |

**O speedup foi linear? Por quê?**
Não, o speedup **não foi linear**.

Um speedup linear significaria que, ao dobrar o número de workers, o tempo de execução seria reduzido pela metade. No seu caso:

Para o primeiro teste (senha “123”), 1 worker levou 0.006 s e 4 workers 0.008 s → o tempo **aumentou**, mostrando que não houve ganho.
Para o segundo teste (senha “hello”), 1 worker levou 4.492 s e 4 workers 1.730 s → o speedup foi \~2.6, **menor que o ideal 4**, que seria o speedup linear.

Isso acontece por causa de **overhead de paralelização**:

1. **Criação de processos** (`fork`/`execl`) consome tempo.
2. **Sincronização e comunicação indireta** (verificação periódica do arquivo de resultado) adiciona atrasos.
3. **Tarefas pequenas** (como no primeiro teste) podem ter overhead maior que o benefício da divisão, tornando a execução com múltiplos workers até mais lenta.

Portanto, o ganho de paralelização depende do tamanho da tarefa: para tarefas muito pequenas, o overhead domina; para tarefas maiores, o speedup melhora, mas raramente atinge a linearidade perfeita devido aos custos de gerenciamento de processos.


---


## 5. Desafios e Aprendizados
**Qual foi o maior desafio técnico que você enfrentou?**
Perfeito! Então podemos descrever assim:

Um problema que tive foi o **warning “set but not used”** da variável `workers` no coordinator. Inicialmente, eu declarava o array `pid_t workers[MAX_WORKERS]`, mas não o utilizava imediatamente, então o compilador gerava o warning.

Depois, passei a **usar o array dentro do loop de criação e de espera dos workers** (`fork()` e `wait()`), armazenando os PIDs e identificando qual worker terminou. Isso resolveu o warning e, ao mesmo tempo, me permitiu controlar corretamente a execução e o término de cada worker.

---

## Comandos de Teste Utilizados

```bash
# Teste básico
./coordinator "900150983cd24fb0d6963f7d28e17f72" 3 "abc" 2

# Teste de performance
time ./coordinator "202cb962ac59075b964b07152d234b70" 3 "0123456789" 1
time ./coordinator "202cb962ac59075b964b07152d234b70" 3 "0123456789" 4


# Teste com senha maior
time ./coordinator "5d41402abc4b2a76b9719d911017c592" 5 "abcdefghijklmnopqrstuvwxyz" 4
```
---

**Checklist de Entrega:**
- [ ] Código compila sem erros
- [ ] Todos os TODOs foram implementados
- [ ] Testes passam no `./tests/simple_test.sh`
- [ ] Relatório preenchido