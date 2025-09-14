#include "sham.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/select.h>
#include <sys/time.h>
#include <stdarg.h>
#include <errno.h>

// --- Shared Constants and Structures ---
#define MAX_PAYLOAD_SIZE 1024
#define HEADER_SIZE sizeof(struct sham_header)
#define MAX_PACKET_SIZE (HEADER_SIZE + MAX_PAYLOAD_SIZE)
#define RTO_MS 500 // Retransmission Timeout in milliseconds
#define SENDER_WINDOW_PACKETS 10 // Fixed sender window size in packets for file transfer
#define RECEIVER_BUFFER_SIZE (10 * MAX_PACKET_SIZE) // Receiver's total buffer capacity

// --- Logging ---
static FILE *log_file = NULL;

void init_rudp_log(const char *role) {
    char *env = getenv("RUDP_LOG");
    if (!env || strcmp(env, "1") != 0) return;

    if (strcmp(role, "server") == 0)
        log_file = fopen("server_log.txt", "w");
    else
        log_file = fopen("client_log.txt", "w");

    if (!log_file) {
        perror("fopen log file");
        exit(1);
    }
}

void close_rudp_log(void) {
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
}

void rudp_log(const char *fmt, ...) {
    if (!log_file) return;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t curtime = tv.tv_sec;
    char time_buffer[30];
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", localtime(&curtime));

    fprintf(log_file, "[%s.%06ld] [LOG] ", time_buffer, (long)tv.tv_usec);

    va_list args;
    va_start(args, fmt);
    vfprintf(log_file, fmt, args);
    va_end(args);

    fprintf(log_file, "\n");
    fflush(log_file);
}

// --- Helper Functions ---
static double parse_loss_rate_arg(const char *s) {
    if (!s) return 0.0;
    char *end;
    double v = strtod(s, &end);
    if (end == s || *end != '\0') return 0.0;
    if (v >= 0.0 && v <= 1.0) return v;
    if (v > 1.0 && v <= 100.0) return v / 100.0;
    return 0.0;
}

// --- Client-Specific Structures and Main Logic ---
struct sent_packet {
    char buffer[MAX_PACKET_SIZE];
    ssize_t len;
    uint32_t seq_num;
    struct timeval sent_time;
};

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  ./client <server_ip> <port> --chat [loss_rate]\n");
        fprintf(stderr, "  ./client <server_ip> <port> <input_file> <output_file> [loss_rate]\n");
        exit(1);
    }

    srand(time(NULL));
    init_rudp_log("client");

    const char *server_ip = argv[1];
    int port = atoi(argv[2]);
    int chat_mode = 0;
    const char *input_file = NULL;
    const char *output_file = NULL;
    double loss_rate = 0.0;

    if (strcmp(argv[3], "--chat") == 0) {
        chat_mode = 1;
        if (argc > 4) loss_rate = parse_loss_rate_arg(argv[4]);
    } else {
        if (argc < 5) {
            fprintf(stderr, "File transfer mode requires <input_file> and <output_file>.\n");
            exit(1);
        }
        input_file = argv[3];
        output_file = argv[4];
        if (argc > 5) loss_rate = parse_loss_rate_arg(argv[5]);
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); exit(1); }

    struct sockaddr_in server_addr;
    socklen_t addrlen = sizeof(server_addr);
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    // --- Connection Establishment (3-Way Handshake) ---
    uint32_t client_seq = (uint32_t)rand();
    struct sham_header syn = { .seq_num = client_seq, .flags = SYN };
    sendto(sock, &syn, sizeof(syn), 0, (struct sockaddr*)&server_addr, addrlen);
    rudp_log("SND SYN SEQ=%u", syn.seq_num);

    struct sham_header syn_ack;
    recvfrom(sock, &syn_ack, sizeof(syn_ack), 0, NULL, NULL);
    rudp_log("RCV SYN-ACK SEQ=%u ACK=%u", syn_ack.seq_num, syn_ack.ack_num);

    client_seq = syn_ack.ack_num;
    uint32_t server_seq = syn_ack.seq_num;
    struct sham_header final_ack = { .seq_num = client_seq, .ack_num = server_seq + 1, .flags = ACK };
    sendto(sock, &final_ack, sizeof(final_ack), 0, (struct sockaddr*)&server_addr, addrlen);
    rudp_log("SND ACK=%u (FOR SYN-ACK)", final_ack.ack_num);


    if (chat_mode) {
        printf("Connection established. Entering chat mode.\n");
        fd_set readfds;
        int connected = 1;
        uint32_t my_seq = client_seq;

        while (connected) {
            FD_ZERO(&readfds);
            FD_SET(0, &readfds);
            FD_SET(sock, &readfds);

            int activity = select(sock + 1, &readfds, NULL, NULL, NULL);
            if (activity < 0) { if (errno == EINTR) continue; perror("select"); break; }

            // --- Handle Network Input ---
            if (FD_ISSET(sock, &readfds)) {
                char packet_buffer[MAX_PACKET_SIZE];
                int len = recvfrom(sock, packet_buffer, sizeof(packet_buffer), 0, NULL, NULL);
                if (len <= 0) continue;

                struct sham_header* hdr = (struct sham_header*)packet_buffer;

                if (hdr->flags & DATA) {
                    if ((double)rand() / RAND_MAX < loss_rate) {
                        rudp_log("DROP DATA SEQ=%u", hdr->seq_num);
                        continue;
                    }
                    int data_len = len - HEADER_SIZE;
                    packet_buffer[len] = '\0';
                    printf("Server: %s", packet_buffer + HEADER_SIZE);
                    rudp_log("RCV DATA SEQ=%u LEN=%d", hdr->seq_num, data_len);

                    struct sham_header data_ack = { .ack_num = hdr->seq_num + data_len, .flags = ACK, .window_size = RECEIVER_BUFFER_SIZE };
                    sendto(sock, &data_ack, sizeof(data_ack), 0, (struct sockaddr*)&server_addr, addrlen);
                    rudp_log("SND ACK=%u WIN=%u", data_ack.ack_num, data_ack.window_size);
                } else if (hdr->flags & FIN) {
                    rudp_log("RCV FIN SEQ=%u", hdr->seq_num);
                    struct sham_header fin_ack = { .ack_num = hdr->seq_num + 1, .flags = ACK };
                    sendto(sock, &fin_ack, sizeof(fin_ack), 0, (struct sockaddr*)&server_addr, addrlen);
                    rudp_log("SND ACK FOR FIN");

                    struct sham_header my_fin = { .seq_num = my_seq, .flags = FIN };
                    sendto(sock, &my_fin, sizeof(my_fin), 0, (struct sockaddr*)&server_addr, addrlen);
                    rudp_log("SND FIN SEQ=%u", my_fin.seq_num);

                    struct sham_header last_ack;
                    recvfrom(sock, &last_ack, sizeof(last_ack), 0, NULL, NULL);
                    rudp_log("RCV ACK=%u (FOR OUR FIN)", last_ack.ack_num);
                    connected = 0;
                    printf("Server has disconnected.\n");
                }
            }

            // --- Handle Keyboard Input ---
            if (FD_ISSET(0, &readfds)) {
                char line_buffer[MAX_PAYLOAD_SIZE];
                if (fgets(line_buffer, sizeof(line_buffer), stdin) == NULL) break;

                if (strncmp(line_buffer, "/quit", 5) == 0) {
                    struct sham_header my_fin = { .seq_num = my_seq, .flags = FIN };
                    sendto(sock, &my_fin, sizeof(my_fin), 0, (struct sockaddr*)&server_addr, addrlen);
                    rudp_log("SND FIN SEQ=%u", my_fin.seq_num);

                    struct sham_header fin_ack;
                    recvfrom(sock, &fin_ack, sizeof(fin_ack), 0, NULL, NULL);
                    rudp_log("RCV ACK FOR FIN");

                    struct sham_header their_fin;
                    recvfrom(sock, &their_fin, sizeof(their_fin), 0, NULL, NULL);
                    rudp_log("RCV FIN SEQ=%u", their_fin.seq_num);

                    struct sham_header last_ack = { .ack_num = their_fin.seq_num + 1, .flags = ACK };
                    sendto(sock, &last_ack, sizeof(last_ack), 0, (struct sockaddr*)&server_addr, addrlen);
                    rudp_log("SND ACK=%u (FOR SERVER FIN)", last_ack.ack_num);

                    connected = 0;
                    printf("Disconnecting.\n");
                } else {
                    size_t msg_len = strlen(line_buffer);
                    char packet_buffer[MAX_PACKET_SIZE];
                    struct sham_header data_hdr = { .seq_num = my_seq, .flags = DATA };
                    memcpy(packet_buffer, &data_hdr, HEADER_SIZE);
                    memcpy(packet_buffer + HEADER_SIZE, line_buffer, msg_len);

                    int acked = 0;
                    while (!acked) {
                        if ((double)rand() / RAND_MAX < loss_rate) {
                            rudp_log("DROP DATA SEQ=%u", my_seq);
                        } else {
                            sendto(sock, packet_buffer, HEADER_SIZE + msg_len, 0, (struct sockaddr*)&server_addr, addrlen);
                            rudp_log("SND DATA SEQ=%u LEN=%zu", my_seq, msg_len);
                        }

                        fd_set ack_fds; FD_ZERO(&ack_fds); FD_SET(sock, &ack_fds);
                        struct timeval timeout = { .tv_sec = 0, .tv_usec = RTO_MS * 1000 };
                        int ready = select(sock + 1, &ack_fds, NULL, NULL, &timeout);

                        if (ready > 0) {
                            struct sham_header rcv_ack;
                            recvfrom(sock, &rcv_ack, sizeof(rcv_ack), 0, NULL, NULL);
                            if ((rcv_ack.flags & ACK) && rcv_ack.ack_num == my_seq + msg_len) {
                                rudp_log("RCV ACK=%u", rcv_ack.ack_num);
                                acked = 1;
                            }
                        } else {
                            rudp_log("TIMEOUT SEQ=%u", my_seq);
                            rudp_log("RETX DATA SEQ=%u LEN=%zu", my_seq, msg_len);
                        }
                    }
                    my_seq += msg_len;
                }
            }
        }
    } else { // File Transfer Mode
        FILE *fp = fopen(input_file, "rb");
        if (!fp) { perror("fopen input file"); exit(1); }

        // 1. Send the output filename first
        char name_packet[MAX_PACKET_SIZE];
        size_t name_len = strlen(output_file);
        struct sham_header name_hdr = { .seq_num = client_seq, .flags = DATA };
        memcpy(name_packet, &name_hdr, HEADER_SIZE);
        memcpy(name_packet + HEADER_SIZE, output_file, name_len);

        int name_acked = 0;
        while (!name_acked) {
            // *** CORRECTED: Loss on SENDING filename ***
            if ((double)rand() / RAND_MAX < loss_rate) {
                rudp_log("DROP DATA SEQ=%u", name_hdr.seq_num);
            } else {
                sendto(sock, name_packet, HEADER_SIZE + name_len, 0, (struct sockaddr*)&server_addr, addrlen);
                rudp_log("SND DATA SEQ=%u LEN=%zu (Filename: %s)", name_hdr.seq_num, name_len, output_file);
            }

            fd_set name_fds; FD_ZERO(&name_fds); FD_SET(sock, &name_fds);
            struct timeval timeout = { .tv_sec = 0, .tv_usec = RTO_MS * 1000 };
            int ready = select(sock + 1, &name_fds, NULL, NULL, &timeout);

            if (ready > 0) {
                struct sham_header name_ack;
                recvfrom(sock, &name_ack, sizeof(name_ack), 0, NULL, NULL);
                if ((name_ack.flags & ACK) && name_ack.ack_num == client_seq + name_len) {
                     rudp_log("RCV ACK=%u", name_ack.ack_num);
                     client_seq = name_ack.ack_num;
                     name_acked = 1;
                }
            } else {
                 rudp_log("TIMEOUT SEQ=%u", name_hdr.seq_num);
                 rudp_log("RETX DATA SEQ=%u LEN=%zu", name_hdr.seq_num, name_len);
            }
        }

        // 2. Send file data using a sliding window
        struct sent_packet window_buffer[SENDER_WINDOW_PACKETS];
        uint32_t base = client_seq;
        uint32_t next_seq_num = client_seq;
        int file_done = 0;
        uint32_t receiver_win = RECEIVER_BUFFER_SIZE;

        while (!file_done || base < next_seq_num) {
            while (!file_done && (next_seq_num < base + (SENDER_WINDOW_PACKETS * MAX_PAYLOAD_SIZE)) && (next_seq_num - base < receiver_win)) {
                int win_index = ((next_seq_num - client_seq) / MAX_PAYLOAD_SIZE) % SENDER_WINDOW_PACKETS;

                size_t bytes_read = fread(window_buffer[win_index].buffer + HEADER_SIZE, 1, MAX_PAYLOAD_SIZE, fp);
                if (bytes_read == 0) {
                    file_done = 1;
                    break;
                }

                struct sham_header data_hdr = { .seq_num = next_seq_num, .flags = DATA };
                memcpy(window_buffer[win_index].buffer, &data_hdr, HEADER_SIZE);
                window_buffer[win_index].len = bytes_read + HEADER_SIZE;

                // *** CORRECTED: Loss on SENDING file data ***
                if ((double)rand() / RAND_MAX < loss_rate) {
                    rudp_log("DROP DATA SEQ=%u", next_seq_num);
                } else {
                    sendto(sock, window_buffer[win_index].buffer, window_buffer[win_index].len, 0, (struct sockaddr*)&server_addr, addrlen);
                    rudp_log("SND DATA SEQ=%u LEN=%zu", next_seq_num, bytes_read);
                }

                next_seq_num += bytes_read;
            }

            fd_set readfds; FD_ZERO(&readfds); FD_SET(sock, &readfds);
            struct timeval timeout = { .tv_sec = 0, .tv_usec = RTO_MS * 1000 };
            int ready = select(sock + 1, &readfds, NULL, NULL, &timeout);

            if (ready > 0) {
                struct sham_header ack_hdr;
                recvfrom(sock, &ack_hdr, sizeof(ack_hdr), 0, NULL, NULL);
                if (ack_hdr.flags & ACK) {
                    rudp_log("RCV ACK=%u", ack_hdr.ack_num);
                    if (ack_hdr.ack_num > base) {
                        base = ack_hdr.ack_num;
                    }
                    if (receiver_win != ack_hdr.window_size) {
                       rudp_log("FLOW WIN UPDATE=%u", ack_hdr.window_size);
                       receiver_win = ack_hdr.window_size;
                    }
                }
            } else if (ready == 0) {
                if (base < next_seq_num) {
                    rudp_log("TIMEOUT SEQ=%u", base);
                    int win_index = ((base - client_seq) / MAX_PAYLOAD_SIZE) % SENDER_WINDOW_PACKETS;
                    // Retransmit without loss simulation
                    sendto(sock, window_buffer[win_index].buffer, window_buffer[win_index].len, 0, (struct sockaddr*)&server_addr, addrlen);
                    rudp_log("RETX DATA SEQ=%u LEN=%zu", base, window_buffer[win_index].len - HEADER_SIZE);
                }
            }
        }
        fclose(fp);

        // --- Connection Termination (4-Way Handshake) ---
        struct sham_header fin = { .seq_num = next_seq_num, .flags = FIN };
        sendto(sock, &fin, sizeof(fin), 0, (struct sockaddr*)&server_addr, addrlen);
        rudp_log("SND FIN SEQ=%u", fin.seq_num);

        struct sham_header their_ack, their_fin;
        recvfrom(sock, &their_ack, sizeof(their_ack), 0, NULL, NULL);
        rudp_log("RCV ACK FOR FIN");

        recvfrom(sock, &their_fin, sizeof(their_fin), 0, NULL, NULL);
        rudp_log("RCV FIN SEQ=%u", their_fin.seq_num);

        struct sham_header last_ack = { .ack_num = their_fin.seq_num + 1, .flags = ACK };
        sendto(sock, &last_ack, sizeof(last_ack), 0, (struct sockaddr*)&server_addr, addrlen);
        rudp_log("SND ACK=%u (FOR SERVER FIN)", last_ack.ack_num);
    }

    close(sock);
    close_rudp_log();
    return 0;
}