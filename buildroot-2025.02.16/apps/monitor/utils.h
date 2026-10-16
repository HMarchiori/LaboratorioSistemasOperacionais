#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

/* Ticks de clock por segundo (HZ), usado para converter utime/stime de
 * /proc/<pid>/stat em segundos. Sempre retorna um valor valido (usa 100
 * como fallback se sysconf falhar). */
long read_clk_tck(void);

/* 1 se s for uma string nao vazia composta so por digitos, 0 caso contrario. */
int is_all_digits(const char *s);

/* Formata total_seconds como "XXd HH:MM:SS" em out. */
void format_dhms(long total_seconds, char *out, size_t outlen);

/* Imprime s em stdout escapando os caracteres com significado em HTML.
 * Obrigatorio para qualquer texto vindo do /proc (nome de processo,
 * argumentos de linha de comando): sem isso um processo chamado
 * "<script>" injetaria HTML na pagina. */
void print_escaped(const char *s);

/* Cabecalho e rodape comuns as duas paginas, para que a pagina geral e a
 * de detalhamento tenham a mesma aparencia. O <meta refresh> nao leva URL
 * de proposito: assim o navegador recarrega o endereco corrente e a pagina
 * de detalhamento preserva o ?pid= atual. */
void print_page_head(const char *title);
void print_page_foot(void);

#endif
