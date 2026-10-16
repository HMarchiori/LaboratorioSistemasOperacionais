#ifndef PROCESS_H
#define PROCESS_H

#include <stddef.h>

/* Extrai o valor do parametro "pid" da QUERY_STRING (formato
 * "a=1&pid=2&b=3"). Retorna 1 e copia o valor para out somente se ele for
 * composto exclusivamente por digitos e couber em out; 0 em qualquer outro
 * caso (ausente, vazio, nao numerico ou longo demais).
 *
 * O criterio de validade do pid e responsabilidade desta unidade; main.c so
 * usa o resultado para decidir qual pagina renderizar. */
int extract_pid(const char *query, char *out, size_t outlen);

/* Imprime a pagina HTML completa com detalhes do processo pid_str
 * (ja validado como sequencia de digitos por extract_pid). */
void render_process_page(const char *pid_str);

#endif
