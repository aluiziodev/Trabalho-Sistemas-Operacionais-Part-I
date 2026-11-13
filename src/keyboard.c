#include "keyboard.h"

#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include "sched.h"

static struct termios terminal_salvo;
static int terminal_salvo_valido = 0;
static int teclado_fd = -1;

static void configurar_teclado(int fd) {
    if (fd < 0) {
        return;
    }

    struct termios terminal;
    if (tcgetattr(fd, &terminal_salvo) == 0) {
        terminal_salvo_valido = 1;
        terminal = terminal_salvo;
        terminal.c_lflag &= ~(ICANON | ECHO);
        terminal.c_cc[VMIN] = 0;
        terminal.c_cc[VTIME] = 0;
        if (tcsetattr(fd, TCSANOW, &terminal) == 0) {
            int flags = fcntl(fd, F_GETFL, 0);
            if (flags != -1) {
                fcntl(fd, F_SETFL, flags | O_NONBLOCK);
            }
            teclado_fd = fd;
        }
    }
}

void teclado_run(void *arg) {
    app_t *app = arg;
    if (app == NULL) {
        return;
    }

    configurar_teclado(STDIN_FILENO);

    while (!app->parar) {
        if (app->n > 0 && app->recepcao_concluida && app->attended >= app->n) {
            app->parar = 1;
            break;
        }

        char caractere = '\0';
        ssize_t leitura = read(STDIN_FILENO, &caractere, 1);

        if (leitura > 0) {
            if (caractere == 's') {
                app->parar = 1;
                break;
            }
            continue;
        }

        if (leitura < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                thread_yield();
                continue;
            }
        }

        thread_yield();
    }
}

void teclado_restore(void) {
    if (teclado_fd >= 0 && terminal_salvo_valido) {
        tcsetattr(teclado_fd, TCSANOW, &terminal_salvo);
        terminal_salvo_valido = 0;
    }

    if (teclado_fd >= 0) {
        int flags = fcntl(teclado_fd, F_GETFL, 0);
        if (flags != -1) {
            fcntl(teclado_fd, F_SETFL, flags & ~O_NONBLOCK);
        }
        teclado_fd = -1;
    }
}
