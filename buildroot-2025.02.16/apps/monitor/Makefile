# Configuracao do programa
TARGET       = monitor
INSTALL_PATH = var/www/data/cgi-bin

## Configuracao de diretorios
ROOT_DIR  = ../..
TOOLCHAIN = $(ROOT_DIR)/output/host/bin
OVERLAY   = $(ROOT_DIR)/overlay

## Configuracao de ferramentas
TRIPLET = $(TOOLCHAIN)/i686-buildroot-linux-gnu
CC      = $(TRIPLET)-gcc

SRCS   = main.c general.c process.c utils.c
CFLAGS = -Wall -Wextra -O2

# Build cruzado para o Buildroot (segue o mesmo padrao do apps/banner)
default: $(OVERLAY)/$(INSTALL_PATH)/$(TARGET) $(OVERLAY)/$(INSTALL_PATH)/index.cgi

$(OVERLAY)/$(INSTALL_PATH)/$(TARGET): $(OVERLAY)/$(INSTALL_PATH) $(SRCS) *.h
	@$(CC) $(CFLAGS) $(SRCS) -o $@

# O busybox httpd tem um bug ao servir "/" quando existe um index.html
# estatico (ver NOTES.md). O fallback nativo cgi-bin/index.cgi nao passa
# pelo mesmo caminho de codigo e nao e afetado, entao usamos ele em vez
# de um index.html.
$(OVERLAY)/$(INSTALL_PATH)/index.cgi: $(OVERLAY)/$(INSTALL_PATH)/$(TARGET)
	@ln -sf $(TARGET) $@

$(OVERLAY)/$(INSTALL_PATH):
	@mkdir -p $@

# Build local com o gcc do host (sem cross-compile), so para testar a
# logica rapidamente antes de integrar no Buildroot. Ver NOTES.md.
host: $(SRCS) *.h
	@gcc $(CFLAGS) $(SRCS) -o monitor_host

clean:
	@rm -f $(OVERLAY)/$(INSTALL_PATH)/$(TARGET) $(OVERLAY)/$(INSTALL_PATH)/index.cgi monitor_host

.PHONY: default host clean
