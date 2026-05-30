/*
 * ============================================================
 *  STM32传感器数据服务端 (第二期)
 *  ============================================================
 *
 *  功能:
 *    1. 接收 STM32 通过 ESP8266 发来的传感器数据 (POST /x)
 *    2. 响应手机APP的数据查询请求 (GET /1)
 *    3. 响应手机APP的清空数据请求 (GET /2)
 *    4. select 多路复用，支持同时处理多个客户端
 *    5. 从配置文件读取 IP 和端口
 *    6. 带时间戳和级别的日志系统
 *
 *  编译:
 *    gcc -o sensor_server sensor_server.c -Wall
 *
 *  运行:
 *    ./sensor_server
 *    # 或者指定配置文件:
 *    ./sensor_server server.conf
 *
 *  协议约定 (必须严格遵循，手机端格式已固定):
 *
 *    STM32 上传:
 *      请求: POST /x HTTP/1.1\r\nHost: x.x.x.x\r\n\r\nlight=xxx,temp=xxx
 *      响应: HTTP/1.1 200 OK\r\n\r\nOK
 *
 *    手机查询数据:
 *      请求: GET /1 HTTP/1.1\r\n...\r\n\r\n
 *      响应: HTTP/1.1 200 OK\r\n\r\n["light=xx,temp=xx","light=yy,temp=yy"]
 *
 *    手机清空数据:
 *      请求: GET /2 HTTP/1.1\r\n...\r\n\r\n
 *      响应: HTTP/1.1 200 OK\r\n\r\n[]
 *
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

/* 网络相关头文件 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* ============================================================
 *  默认配置 (如果没有配置文件或某配置项缺失, 使用默认值)
 * ============================================================ */
#define DEFAULT_SERVER_IP    "0.0.0.0"       /* 监听所有网卡 */
#define DEFAULT_SERVER_PORT  8866            /* 监听端口 */
#define DEFAULT_BUF_SIZE     4096            /* 接收缓冲区大小 */
#define DEFAULT_RESP_SIZE    8192            /* 响应缓冲区大小 */
#define DEFAULT_DATA_FILE    "data.txt"      /* 数据存储文件 */
#define DEFAULT_MAX_CLIENTS  64              /* 最大客户端数 */
#define DEFAULT_LISTEN_BACKLOG 10            /* listen backlog */
#define DEFAULT_MAX_LAST_LINES 10            /* 手机端每次返回最多几条数据 */
#define DEFAULT_MAX_LINE_LEN  256            /* 一条传感器数据最大长度 */
#define DEFAULT_LOG_FILE     "server.log"    /* 日志文件 */
#define DEFAULT_MAX_CONF_LINE 128            /* 配置文件每行最大长度 */

/* ============================================================
 *  日志级别
 * ============================================================ */
typedef enum {
    LOG_DEBUG   = 0,
    LOG_INFO    = 1,
    LOG_WARNING = 2,
    LOG_ERROR   = 3
} LogLevel_t;

/* ============================================================
 *  全局配置 (运行时从配置文件加载, 未配置的用默认值)
 * ============================================================ */
typedef struct {
    char server_ip[64];
    int  server_port;
    int  buf_size;
    int  resp_size;
    char data_file[256];
    int  max_clients;
    int  listen_backlog;
    int  max_last_lines;
    int  max_line_len;
    char log_file[256];
} ServerConfig_t;

static ServerConfig_t g_cfg;
static FILE *g_log_fp = NULL;   /* 日志文件指针 */

/* ============================================================
 *  日志函数
 * ============================================================ */
static void LogWrite(LogLevel_t level, const char *fmt, ...) {
    static const char *level_str[] = {
        "[DEBUG]  ", "[INFO]   ", "[WARNING]", "[ERROR]  "
    };

    /* 获取时间 */
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_buf[32];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", t);

    /* 打印到控制台 */
    fprintf(stderr, "%s %s ", time_buf, level_str[level]);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    fflush(stderr);

    /* 写入日志文件 */
    if (g_log_fp != NULL) {
        fprintf(g_log_fp, "%s %s ", time_buf, level_str[level]);
        va_start(args, fmt);
        vfprintf(g_log_fp, fmt, args);
        va_end(args);
        fprintf(g_log_fp, "\n");
        fflush(g_log_fp);
    }
}

/* ============================================================
 *  错误检查宏
 * ============================================================ */
#define ERROR_CHECK(ret, err_val, name)  \
    do {                                 \
        if ((ret) == (err_val)) {        \
            LogWrite(LOG_ERROR,           \
                "%s() failed: %s (line %d)", \
                (name), strerror(errno), __LINE__); \
            exit(EXIT_FAILURE);          \
        }                                \
    } while (0)

/* ============================================================
 *  读取配置文件
 *
 *  配置文件格式: 每行 key=value
 *  支持的 key:
 *    server_ip      - 监听IP, 默认 0.0.0.0
 *    server_port    - 监听端口, 默认 8866
 *    data_file      - 数据文件路径, 默认 data.txt
 *    max_clients    - 最大客户端数, 默认 64
 *    max_last_lines - 手机查询时返回几行, 默认 10
 *    log_file       - 日志文件路径, 默认 server.log
 *
 *  没有被配置的项保持默认值。
 * ============================================================ */
static void LoadConfig(const char *conf_path) {
    /* 先用默认值初始化 */
    strcpy(g_cfg.server_ip,  DEFAULT_SERVER_IP);
    g_cfg.server_port        = DEFAULT_SERVER_PORT;
    g_cfg.buf_size           = DEFAULT_BUF_SIZE;
    g_cfg.resp_size          = DEFAULT_RESP_SIZE;
    strcpy(g_cfg.data_file,  DEFAULT_DATA_FILE);
    g_cfg.max_clients        = DEFAULT_MAX_CLIENTS;
    g_cfg.listen_backlog     = DEFAULT_LISTEN_BACKLOG;
    g_cfg.max_last_lines     = DEFAULT_MAX_LAST_LINES;
    g_cfg.max_line_len       = DEFAULT_MAX_LINE_LEN;
    strcpy(g_cfg.log_file,   DEFAULT_LOG_FILE);

    FILE *fp = fopen(conf_path, "r");
    if (fp == NULL) {
        LogWrite(LOG_WARNING, "Config file '%s' not found, using defaults", conf_path);
        return;
    }

    char line[DEFAULT_MAX_CONF_LINE];
    while (fgets(line, sizeof(line), fp) != NULL) {
        /* 去掉行尾的 \r \n */
        line[strcspn(line, "\r\n")] = '\0';

        /* 跳过空行和注释行 */
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        /* 按 = 分割 */
        char *eq = strchr(line, '=');
        if (eq == NULL) continue;

        *eq = '\0';
        char *key = line;
        char *value = eq + 1;

        /* 去掉 key 的前后空格 */
        while (*key == ' ' || *key == '\t') key++;
        char *kend = key + strlen(key) - 1;
        while (kend > key && (*kend == ' ' || *kend == '\t')) {
            *kend = '\0'; kend--;
        }

        /* 去掉 value 的前后空格 */
        while (*value == ' ' || *value == '\t') value++;
        char *vend = value + strlen(value) - 1;
        while (vend > value && (*vend == ' ' || *vend == '\t')) {
            *vend = '\0'; vend--;
        }

        /* 匹配并赋值 */
        if (strcmp(key, "server_ip") == 0) {
            strncpy(g_cfg.server_ip, value, sizeof(g_cfg.server_ip) - 1);
        } else if (strcmp(key, "server_port") == 0) {
            g_cfg.server_port = atoi(value);
        } else if (strcmp(key, "data_file") == 0) {
            strncpy(g_cfg.data_file, value, sizeof(g_cfg.data_file) - 1);
        } else if (strcmp(key, "max_clients") == 0) {
            g_cfg.max_clients = atoi(value);
        } else if (strcmp(key, "max_last_lines") == 0) {
            g_cfg.max_last_lines = atoi(value);
        } else if (strcmp(key, "log_file") == 0) {
            strncpy(g_cfg.log_file, value, sizeof(g_cfg.log_file) - 1);
        }
    }

    fclose(fp);
    LogWrite(LOG_INFO, "Config loaded from '%s': ip=%s, port=%d, data=%s, max_clients=%d",
             conf_path, g_cfg.server_ip, g_cfg.server_port,
             g_cfg.data_file, g_cfg.max_clients);
}

/* ============================================================
 *  删除一个客户端连接
 *
 *  关闭 client socket，并将该位置标记为空闲。
 *  主循环中每次都会重新构造 readfds，
 *  因此这里不需要额外调用 FD_CLR。
 * ============================================================ */
static void remove_client(int client_fds[], int index) {
    if (client_fds[index] != -1) {
        close(client_fds[index]);
        LogWrite(LOG_INFO, "Client fd=%d removed (slot %d)", client_fds[index], index);
        client_fds[index] = -1;
    }
}

/* ============================================================
 *  提取 HTTP 请求行
 *
 *  HTTP 报文的第一行就是请求行，例如:
 *    GET /1 HTTP/1.1
 *    POST /x HTTP/1.1
 *
 *  从接收缓冲区中提取第一行内容，保存到 line 中。
 * ============================================================ */
static void get_request_line(const char *buf, char *line, int size) {
    int i = 0;
    while (buf[i] != '\0' && buf[i] != '\r' && buf[i] != '\n' && i < size - 1) {
        line[i] = buf[i];
        i++;
    }
    line[i] = '\0';
}

/* ============================================================
 *  发送 HTTP 响应
 *
 *  直接调用一次 send 发送整个响应字符串。
 *  返回 0=成功, -1=失败
 * ============================================================ */
static int send_response(int client_fd, const char *resp) {
    if (send(client_fd, resp, strlen(resp), 0) <= 0) {
        LogWrite(LOG_ERROR, "send_response() failed: %s", strerror(errno));
        return -1;
    }
    return 0;
}

/* ============================================================
 *  处理一个 HTTP 请求 (核心函数)
 *
 *  支持三种请求:
 *    1. POST /x  → 上传传感器数据 (STM32发来的)
 *    2. GET  /1  → 获取最后N条数据   (手机发来的)
 *    3. GET  /2  → 清空全部数据      (手机发来的)
 *
 *  返回值:
 *    0  → 处理成功, 保持连接 (用于STM32的长连接)
 *    1  → 处理成功, 关闭连接 (用于手机端的短连接)
 *   -1  → 请求出错, 关闭连接
 * ============================================================ */
static int handle_request(int client_fd, char *buf) {
    char req_line[128] = {0};

    /* 提取请求行 */
    get_request_line(buf, req_line, sizeof(req_line));

    LogWrite(LOG_DEBUG, "Request: %s", req_line);

    /* ========== POST /x: STM32上传传感器数据 ========== */
    if (strcmp(req_line, "POST /x HTTP/1.1") == 0 ||
        strcmp(req_line, "POST /x HTTP/1.0") == 0) {

        char *body = NULL;
        FILE *fp = NULL;

        /*
         * HTTP 报文结构:
         *   POST /x HTTP/1.1\r\n
         *   Host: x.x.x.x\r\n
         *   \r\n
         *   light=2000,temp=1700
         *
         * 找到空行 \r\n\r\n，后面就是 body。
         */
        body = strstr(buf, "\r\n\r\n");
        if (body == NULL) {
            LogWrite(LOG_WARNING, "POST /x: no body found");
            return -1;
        }
        body += 4;  /* 跳过 \r\n\r\n */

        /*
         * 追加写入数据文件，每条数据单独一行。
         */
        fp = fopen(g_cfg.data_file, "a");
        if (fp == NULL) {
            LogWrite(LOG_ERROR, "fopen(%s) failed: %s", g_cfg.data_file, strerror(errno));
            return -1;
        }
        fprintf(fp, "%s\n", body);
        fclose(fp);

        LogWrite(LOG_INFO, "POST /x: data saved: %s", body);

        /* 返回固定响应 */
        if (send_response(client_fd, "HTTP/1.1 200 OK\r\n\r\nOK") != 0) {
            return -1;
        }

        /* STM32端保持连接 */
        return 0;
    }

    /* ========== GET /1: 手机查询最后N条数据 ========== */
    if (strcmp(req_line, "GET /1 HTTP/1.1") == 0) {

        /* 分配响应缓冲区 */
        char *resp = (char *)malloc(g_cfg.resp_size);
        if (resp == NULL) return -1;

        strcpy(resp, "HTTP/1.1 200 OK\r\n\r\n[");

        char line[DEFAULT_MAX_LINE_LEN];
        char last10[DEFAULT_MAX_LAST_LINES][DEFAULT_MAX_LINE_LEN];
        memset(last10, 0, sizeof(last10));

        FILE *fp = NULL;
        int count = 0;  /* 文件中总共读到的有效数据条数 */
        int start = 0;  /* 最终输出时的起始下标 */
        int num   = 0;  /* 最终输出多少条 */

        fp = fopen(g_cfg.data_file, "r");
        if (fp != NULL) {
            /*
             * 使用长度为 N 的循环数组，
             * 始终只保留最新的 N 条数据：
             * last10[count % N] 会被反复覆盖。
             */
            while (fgets(line, sizeof(line), fp) != NULL) {
                line[strcspn(line, "\r\n")] = '\0';  /* 去换行 */
                if (line[0] == '\0') continue;        /* 跳过空行 */
                strncpy(last10[count % g_cfg.max_last_lines], line,
                        sizeof(last10[0]) - 1);
                count++;
            }
            fclose(fp);
        }

        if (count == 0) {
            /* 文件中没有数据 → 返回空数组 */
            strcat(resp, "]");
        } else {
            /* 确定起止位置 */
            if (count < g_cfg.max_last_lines) {
                num   = count;
                start = 0;
            } else {
                num   = g_cfg.max_last_lines;
                start = count % g_cfg.max_last_lines;
            }

            /* 按顺序拼成 JSON 数组: ["data1","data2",...] */
            for (int i = 0; i < num; i++) {
                int idx = (start + i) % g_cfg.max_last_lines;

                if (i != 0) strcat(resp, ",");  /* 非第一项前加逗号 */
                strcat(resp, "\"");
                strcat(resp, last10[idx]);
                strcat(resp, "\"");
            }
            strcat(resp, "]");
        }

        LogWrite(LOG_INFO, "GET /1: returned %d records (total %d in file)", num, count);

        if (send_response(client_fd, resp) != 0) {
            free(resp);
            return -1;
        }
        free(resp);

        /* 手机端查询完就关 (短连接) */
        return 1;
    }

    /* ========== GET /2: 手机清空全部数据 ========== */
    if (strcmp(req_line, "GET /2 HTTP/1.1") == 0) {

        FILE *fp = fopen(g_cfg.data_file, "w");
        if (fp == NULL) {
            LogWrite(LOG_ERROR, "fopen(%s, w) failed: %s",
                     g_cfg.data_file, strerror(errno));
            return -1;
        }
        fclose(fp);  /* w模式打开即清空 */

        LogWrite(LOG_INFO, "GET /2: data file cleared");

        /* 返回空数组表示清空成功 */
        if (send_response(client_fd, "HTTP/1.1 200 OK\r\n\r\n[]") != 0) {
            return -1;
        }

        /* 手机端短连接 */
        return 1;
    }

    /*
     * 不是约定的三种请求:
     *  1. POST /x HTTP/1.x
     *  2. GET  /1 HTTP/1.1
     *  3. GET  /2 HTTP/1.1
     */
    LogWrite(LOG_WARNING, "Unknown request: %s", req_line);
    return -1;
}

/* ============================================================
 *  main - 服务端主函数
 * ============================================================ */
int main(int argc, char *argv[]) {

    /* 1. 加载配置 */
    const char *conf_path = (argc > 1) ? argv[1] : "server.conf";
    LoadConfig(conf_path);

    /* 2. 打开日志文件 */
    g_log_fp = fopen(g_cfg.log_file, "a");
    if (g_log_fp == NULL) {
        fprintf(stderr, "Warning: cannot open log file '%s'\n", g_cfg.log_file);
    }

    LogWrite(LOG_INFO, "========================================");
    LogWrite(LOG_INFO, "Sensor Server Starting...");
    LogWrite(LOG_INFO, "========================================");

    /* 3. 创建监听 socket */
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    ERROR_CHECK(listen_fd, -1, "socket");

    /* 4. 设置端口复用 (防止重启时端口被占用) */
    int reuse = 1;
    int ret = setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR,
                         &reuse, sizeof(reuse));
    ERROR_CHECK(ret, -1, "setsockopt");

    /* 5. 绑定 IP + 端口 */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(g_cfg.server_port);
    server_addr.sin_addr.s_addr = inet_addr(g_cfg.server_ip);

    ret = bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    ERROR_CHECK(ret, -1, "bind");

    /* 6. 开始监听 */
    ret = listen(listen_fd, g_cfg.listen_backlog);
    ERROR_CHECK(ret, -1, "listen");

    /* 7. 初始化客户端数组 */
    int *client_fds = (int *)malloc(sizeof(int) * g_cfg.max_clients);
    if (client_fds == NULL) {
        LogWrite(LOG_ERROR, "malloc failed");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < g_cfg.max_clients; i++) {
        client_fds[i] = -1;  /* -1 表示空闲 */
    }

    LogWrite(LOG_INFO, "Server listening on %s:%d",
             g_cfg.server_ip, g_cfg.server_port);

    /* 8. 主事件循环 */
    while (1) {
        int max_fd = listen_fd;
        fd_set readfds;

        /* 每轮循环重新构造读集合 */
        FD_ZERO(&readfds);
        FD_SET(listen_fd, &readfds);

        for (int i = 0; i < g_cfg.max_clients; i++) {
            if (client_fds[i] != -1) {
                FD_SET(client_fds[i], &readfds);
                if (client_fds[i] > max_fd) {
                    max_fd = client_fds[i];
                }
            }
        }

        /*
         * select 阻塞等待事件:
         *   1. 新客户端连接 (listen_fd 可读)
         *   2. 已有客户端发来数据 (client_fds[i] 可读)
         */
        ret = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (ret < 0) {
            if (errno == EINTR) continue;  /* 信号中断, 下一轮 */
            LogWrite(LOG_ERROR, "select() failed: %s", strerror(errno));
            continue;
        }

        /* ---- 处理新连接 ---- */
        if (FD_ISSET(listen_fd, &readfds)) {
            int client_fd = accept(listen_fd, NULL, NULL);
            if (client_fd < 0) {
                LogWrite(LOG_ERROR, "accept() failed: %s", strerror(errno));
            } else {
                /* 找空闲位置存新连接 */
                int inserted = 0;
                for (int i = 0; i < g_cfg.max_clients; i++) {
                    if (client_fds[i] == -1) {
                        client_fds[i] = client_fd;
                        inserted = 1;
                        LogWrite(LOG_INFO, "New client fd=%d (slot %d)", client_fd, i);
                        break;
                    }
                }
                /* 满了就不要了 */
                if (!inserted) {
                    LogWrite(LOG_WARNING, "Too many clients, rejecting fd=%d", client_fd);
                    close(client_fd);
                }
            }
        }

        /* ---- 处理已有客户端的数据 ---- */
        for (int i = 0; i < g_cfg.max_clients; i++) {
            if (client_fds[i] == -1) continue;

            if (FD_ISSET(client_fds[i], &readfds)) {
                char *buf = (char *)malloc(g_cfg.buf_size);
                if (buf == NULL) {
                    remove_client(client_fds, i);
                    continue;
                }
                memset(buf, 0, g_cfg.buf_size);

                ssize_t recv_len = recv(client_fds[i], buf,
                                        g_cfg.buf_size - 1, 0);

                if (recv_len <= 0) {
                    /* 客户端断开或出错 */
                    if (recv_len == 0) {
                        LogWrite(LOG_INFO, "Client fd=%d disconnected", client_fds[i]);
                    } else {
                        LogWrite(LOG_ERROR, "recv() fd=%d failed: %s",
                                 client_fds[i], strerror(errno));
                    }
                    remove_client(client_fds, i);
                    free(buf);
                    continue;
                }

                buf[recv_len] = '\0';
                LogWrite(LOG_DEBUG, "Received %zd bytes from fd=%d", recv_len, client_fds[i]);

                /* 处理这个HTTP请求 */
                int deal_ret = handle_request(client_fds[i], buf);
                free(buf);

                /*
                 * deal_ret:
                 *   0  = 保持连接 (STM32)
                 *   1  = 主动关闭 (手机短连接)
                 *  -1  = 出错关闭
                 */
                if (deal_ret == 1 || deal_ret == -1) {
                    remove_client(client_fds, i);
                }
            }
        }
    }

    /* 清理 (永远不会执行到,但保持完整性) */
    free(client_fds);
    close(listen_fd);
    if (g_log_fp != NULL) fclose(g_log_fp);

    return 0;
}
