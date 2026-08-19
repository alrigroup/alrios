/*
 * Servidor de Demonstração e Teste Local do ALRI DB PG-Wire
 */
#include "ardb_pgwire.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("[ALRI DB] Iniciando Guardião de Banco de Dados PG-Wire na porta 5432...\n");
    if (ardb_pgwire_server_start(5432) != 0) {
        printf("[ERROR] Falha ao iniciar servidor PG-Wire na porta 5432.\n");
        return 1;
    }
    printf("[ALRI DB] Pronto e aguardando conexoes (DBeaver / Mock Client)...\n");

    /* Manter rodando para atender os testes */
    while (1) {
        ar_sleep_ms(1000);
    }
    return 0;
}
