#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/queue.h>

#define COREX_VERSION   "2.1.4"

// client message
#define INPUT '0'
#define RESIZE_TERMINAL '1'
#define JSON_DATA '{'

// server message
#define OUTPUT '0'
#define SET_WINDOW_TITLE '1'
#define SET_PREFERENCES '2'
#define SET_RECONNECT '3'

// websocket url path
#define WS_PATH "/ws"

#define BUF_SIZE 32768 // 32K

#define LOGS_SIZE 16384 // 16K

extern volatile bool force_exit;
extern struct lws_context *context;
extern struct tty_server *server;

enum pty_state {
    STATE_INIT, STATE_READY, STATE_DONE
};

struct tty_server;

typedef struct buffer_t {
    uint8_t *buffer;
    size_t length;

} buffer_t;

typedef struct circbuf_t {
    size_t length;
    char *buffer;
    char *reader;
    char *writer;

} circbuf_t;

typedef enum tty_process_state {
    CREATED,
    STARTING,
    RUNNING,
    STOPPING,
    STOPPED,
    CRASHED,

} tty_process_state;

struct tty_process {
    size_t id;                     // internal id representation
    int pid;                       // child process id
    int pty;                       // pty file descriptor
    int running;                   // process is running
    char **argv;                   // command with arguments
    char *command;                 // full command line
    char **error;                  // error message if any
    int wstatus;                   // process end-of-life status
    struct tty_server *server;     // main server link
    circbuf_t *logs;               // circular buffer for logs
    tty_process_state state;       // process state

    LIST_ENTRY(tty_process) list;
};

struct tty_client {
    bool running;
    bool initialized;
    int initial_cmd_index;
    bool authenticated;
    char hostname[100];
    char address[50];

    struct lws *wsi;
    struct winsize size;
    char *buffer;
    size_t len;

    int pid;
    int pty;
    struct tty_process *process;
    enum pty_state state;
    char pty_buffer[LWS_PRE + 1 + BUF_SIZE];
    ssize_t pty_len;

    LIST_ENTRY(tty_client) list;

    lws_sorted_usec_list_t sul_stagger;
};

struct pss_http {
    char path[128];
    char *buffer;
    char *ptr;
    size_t len;
};

struct tty_server {
    LIST_HEAD(client, tty_client) clients;     // client list
    LIST_HEAD(process, tty_process) processes; // process list

    int client_count;                          // client count
    char *prefs_json;                          // client preferences
    char *credential;                          // encoded basic auth credential
    int reconnect;                             // reconnect timeout
    char *index;                               // custom index.html
    int sig_code;                              // close signal
    char sig_name[20];                         // human readable signal string
    bool readonly;                             // whether not allow clients to write to the TTY
    bool check_origin;                         // whether allow websocket connection from different origin
    int max_clients;                           // maximum clients to support
    bool once;                                 // whether accept only one client and exit on disconnection
    char socket_path[255];                     // UNIX domain socket path
    char terminal_type[30];                    // terminal type to report
};

extern int callback_http(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);
extern int callback_tty(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

char *tty_server_process_state(struct tty_process *process);
struct tty_process *tty_server_process_kill(struct tty_process *process, int signal);
struct tty_process *tty_server_process_stop(struct tty_process *process);
struct tty_process *tty_server_process_start(struct tty_server *ts, int argc, char **argv);
void process_remove(struct tty_process *process);

struct tty_process *process_getby_pid(int pid, int only_running);
struct tty_process *process_getby_id(size_t id);

// circular buffer
circbuf_t *circular_new(size_t length);
void circular_free(circbuf_t *circular);
size_t circular_append(circbuf_t *circular, uint8_t *data, size_t length);
buffer_t *circular_get(circbuf_t *circular, size_t length);

buffer_t *buffer_new(size_t length);
void buffer_free(buffer_t *buffer);

extern int __verbose;
void *warnp(char *str);

#ifndef RELEASE
    // #define verbose(...) { if(__verbose) { printf(__VA_ARGS__); } }
    #define verbose(...) { printf(__VA_ARGS__); }
    #define debug(...) { printf(__VA_ARGS__); }
    #define debughex(...) { hexdump(__VA_ARGS__); }
#else
    #define verbose(...) { if(__verbose) { printf(__VA_ARGS__); } }
    #define debug(...) ((void)0)
    #define debughex(...) ((void)0)
#endif

// extend missing LIST_FOREACH_SAFE
#define	LIST_FOREACH_SAFE(var, head, field, tvar)			\
	for ((var) = LIST_FIRST((head));				\
	    (var) && ((tvar) = LIST_NEXT((var), field), 1);		\
	    (var) = (tvar))
