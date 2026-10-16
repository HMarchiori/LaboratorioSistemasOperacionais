#include <stdio.h>
#include <stdlib.h>

#include "general.h"
#include "process.h"
#include "utils.h"

/* Dispatcher CGI: le QUERY_STRING e decide qual pagina renderizar.
 * Se houver um parametro pid valido (so digitos), renderiza a pagina de
 * detalhamento; caso contrario (parametro ausente, desconhecido ou com
 * valor invalido) cai na pagina geral, como exige o enunciado.
 *
 * O header HTTP e impresso aqui uma unica vez, antes de delegar, para que
 * general.c e process.c cuidem apenas do corpo HTML. */
int main(void)
{
    printf("Content-Type: text/html\r\n\r\n");

    const char *query = getenv("QUERY_STRING");
    char pid[32];

    if (query != NULL && extract_pid(query, pid, sizeof(pid))) {
        render_process_page(pid);
    } else {
        render_general_page();
    }

    return 0;
}
