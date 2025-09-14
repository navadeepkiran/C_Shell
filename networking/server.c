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
#include <openssl/evp.h>

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

void calculate_md5(const char *filename, char *md5_str) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("fopen for MD5");
        strcpy(md5_str, "error calculating checksum");
        return;
    }
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    const EVP_MD *md = EVP_md5();
    unsigned char md_value[EVP_MAX_MD_SIZE];
    unsigned int md_len;
    char buf[1024];
    size_t bytes;

    EVP_DigestInit_ex(mdctx, md, NULL);
    while ((bytes = fread(buf, 1, sizeof(buf), file)) != 0) {
        EVP_DigestUpdate(mdctx, buf, bytes);
    }
    EVP_DigestFinal_ex(mdctx, md_value, &md_len);
    EVP_MD_CTX_free(mdctx);
    fclose(file);

    for (unsigned int i = 0; i < md_len; i++) {
        sprintf(&md5_str[i * 2], "%02x", md_value[i]);
    }
    md5_str[md_len * 2] = '\0';
}

// --- Main Server Logic ---
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  ./server <port> [--chat] [loss_rate]\n");
        fprintf(stderr, "  ./server <port> [loss_rate]\n");
        exit(1);
    }

    srand(time(NULL));
    init_rudp_log("server");

    int port = atoi(argv[1]);
    int chat_mode = 0;
    double loss_rate = 0.0;

    if (argc > 2) {
        if (strcmp(argv[2], "--chat") == 0) {
            chat_mode = 1;
            if (argc > 3) loss_rate = parse_loss_rate_arg(argv[3]);
        } else {
            loss_rate = parse_loss_rate_arg(argv[2]);
        }
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); exit(1); }

    struct sockaddr_in server_addr, client_addr;
    socklen_t addrlen = sizeof(client_addr);
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind"); exit(1);
    }

    printf("Server listening on port %d\n", port);
    if (loss_rate > 0.0) {
        printf("Packet loss rate set to %.2f%%\n", loss_rate * 100);
    }

    // --- Connection Establishment (3-Way Handshake) ---
    struct sham_header syn;
    recvfrom(sock, &syn, sizeof(syn), 0, (struct sockaddr*)&client_addr, &addrlen);
    rudp_log("RCV SYN SEQ=%u", syn.seq_num);

    struct sham_header syn_ack = {
        .seq_num = (uint32_t)rand(),
        .ack_num = syn.seq_num + 1,
        .flags = SYN | ACK,
        .window_size = RECEIVER_BUFFER_SIZE
    };
    sendto(sock, &syn_ack, sizeof(syn_ack), 0, (struct sockaddr*)&client_addr, addrlen);
    rudp_log("SND SYN-ACK SEQ=%u ACK=%u", syn_ack.seq_num, syn_ack.ack_num);

    struct sham_header ack;
    recvfrom(sock, &ack, sizeof(ack), 0, (struct sockaddr*)&client_addr, &addrlen);
    rudp_log("RCV ACK FOR SYN");

    // --- Mode-Specific Logic ---
    if (chat_mode) {
        printf("Connection established. Entering chat mode.\n");
        fd_set readfds;
        int connected = 1;
        uint32_t my_seq = 1;

        while (connected) {
            FD_ZERO(&readfds);
            FD_SET(0, &readfds); // Standard input
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
                    // Loss on RECEIVING data
                    if ((double)rand() / RAND_MAX < loss_rate) {
                        rudp_log("DROP DATA SEQ=%u", hdr->seq_num);
                        continue;
                    }
                    int data_len = len - HEADER_SIZE;
                    packet_buffer[len] = '\0';
                    printf("Client: %s", packet_buffer + HEADER_SIZE);
                    rudp_log("RCV DATA SEQ=%u LEN=%d", hdr->seq_num, data_len);

                    struct sham_header data_ack = { .ack_num = hdr->seq_num + data_len, .flags = ACK, .window_size = RECEIVER_BUFFER_SIZE };
                    sendto(sock, &data_ack, sizeof(data_ack), 0, (struct sockaddr*)&client_addr, addrlen);
                    rudp_log("SND ACK=%u WIN=%u", data_ack.ack_num, data_ack.window_size);
                } else if (hdr->flags & FIN) {
                    rudp_log("RCV FIN SEQ=%u", hdr->seq_num);
                    struct sham_header fin_ack = { .ack_num = hdr->seq_num + 1, .flags = ACK };
                    sendto(sock, &fin_ack, sizeof(fin_ack), 0, (struct sockaddr*)&client_addr, addrlen);
                    rudp_log("SND ACK FOR FIN");

                    struct sham_header my_fin = { .seq_num = my_seq, .flags = FIN };
                    sendto(sock, &my_fin, sizeof(my_fin), 0, (struct sockaddr*)&client_addr, addrlen);
                    rudp_log("SND FIN SEQ=%u", my_fin.seq_num);

                    struct sham_header last_ack;
                    recvfrom(sock, &last_ack, sizeof(last_ack), 0, NULL, NULL);
                    rudp_log("RCV ACK=%u (FOR OUR FIN)", last_ack.ack_num);
                    connected = 0;
                    printf("Client has disconnected.\n");
                }
            }

            // --- Handle Keyboard Input ---
            if (FD_ISSET(0, &readfds)) {
                char line_buffer[MAX_PAYLOAD_SIZE];
                if (fgets(line_buffer, sizeof(line_buffer), stdin) == NULL) break;

                if (strncmp(line_buffer, "/quit", 5) == 0) {
                    struct sham_header my_fin = { .seq_num = my_seq, .flags = FIN };
                    sendto(sock, &my_fin, sizeof(my_fin), 0, (struct sockaddr*)&client_addr, addrlen);
                    rudp_log("SND FIN SEQ=%u", my_fin.seq_num);

                    struct sham_header fin_ack;
                    recvfrom(sock, &fin_ack, sizeof(fin_ack), 0, NULL, NULL);
                    rudp_log("RCV ACK FOR FIN");

                    struct sham_header their_fin;
                    recvfrom(sock, &their_fin, sizeof(their_fin), 0, NULL, NULL);
                    rudp_log("RCV FIN SEQ=%u", their_fin.seq_num);

                    struct sham_header last_ack = { .ack_num = their_fin.seq_num + 1, .flags = ACK };
                    sendto(sock, &last_ack, sizeof(last_ack), 0, (struct sockaddr*)&client_addr, addrlen);
                    rudp_log("SND ACK=%u (FOR CLIENT FIN)", last_ack.ack_num);

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
                        // *** CORRECTED LOGIC IS HERE ***
                        // Decide whether to drop the packet right before sending.
                        if ((double)rand() / RAND_MAX < loss_rate) {
                            rudp_log("DROP DATA SEQ=%u", my_seq);
                        } else {
                            sendto(sock, packet_buffer, HEADER_SIZE + msg_len, 0, (struct sockaddr*)&client_addr, addrlen);
                            rudp_log("SND DATA SEQ=%u LEN=%zu", my_seq, msg_len);
                        }

                        // Now, wait for an ACK regardless of whether it was dropped or sent.
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
                            // This will now trigger correctly after a dropped packet
                            rudp_log("TIMEOUT SEQ=%u", my_seq);
                            rudp_log("RETX DATA SEQ=%u LEN=%zu", my_seq, msg_len);
                        }
                    }
                    my_seq += msg_len;
                }
            }
        }
    } else { // File Transfer Mode
        char packet_buffer[MAX_PACKET_SIZE];
        char output_filename[256] = {0};
        uint32_t expected_seq = ack.seq_num;

        // 1. Receive filename
        int received_filename = 0;
        while (!received_filename) {
            int len = recvfrom(sock, packet_buffer, sizeof(packet_buffer), 0, (struct sockaddr*)&client_addr, &addrlen);
            if (len <= (int)HEADER_SIZE) continue;

            struct sham_header *hdr = (struct sham_header*)packet_buffer;
            if ((hdr->flags & DATA) && hdr->seq_num == expected_seq) {
                int data_len = len - HEADER_SIZE;
                strncpy(output_filename, packet_buffer + HEADER_SIZE, data_len);
                output_filename[data_len] = '\0';

                expected_seq += data_len;
                received_filename = 1;

                rudp_log("RCV DATA SEQ=%u LEN=%d (Filename: %s)", hdr->seq_num, data_len, output_filename);

                struct sham_header file_ack = { .ack_num = expected_seq, .flags = ACK, .window_size = RECEIVER_BUFFER_SIZE };
                sendto(sock, &file_ack, sizeof(file_ack), 0, (struct sockaddr*)&client_addr, addrlen);
                rudp_log("SND ACK=%u WIN=%u", file_ack.ack_num, file_ack.window_size);
            }
        }

        FILE *fp = fopen(output_filename, "wb");
        if (!fp) { perror("fopen output file"); exit(1); }

        // 2. Receive file data
        int connection_active = 1;
        while (connection_active) {
            int len = recvfrom(sock, packet_buffer, sizeof(packet_buffer), 0, (struct sockaddr*)&client_addr, &addrlen);
            if (len < 0) continue;

            struct sham_header *hdr = (struct sham_header*)packet_buffer;

            if ((hdr->flags & DATA) && ((double)rand() / RAND_MAX < loss_rate)) {
                rudp_log("DROP DATA SEQ=%u", hdr->seq_num);
                continue;
            }

            if (hdr->flags & DATA) {
                if (hdr->seq_num == expected_seq) {
                    int data_len = len - HEADER_SIZE;
                    fwrite(packet_buffer + HEADER_SIZE, 1, data_len, fp);
                    expected_seq += data_len;
                    rudp_log("RCV DATA SEQ=%u LEN=%d", hdr->seq_num, data_len);
                } else {
                    rudp_log("RCV DUP/OUT-OF-ORDER DATA SEQ=%u, EXPECTED=%u", hdr->seq_num, expected_seq);
                }
                struct sham_header data_ack = { .ack_num = expected_seq, .flags = ACK, .window_size = RECEIVER_BUFFER_SIZE };
                sendto(sock, &data_ack, sizeof(data_ack), 0, (struct sockaddr*)&client_addr, addrlen);
                rudp_log("SND ACK=%u WIN=%u", data_ack.ack_num, data_ack.window_size);

            } else if (hdr->flags & FIN) {
                rudp_log("RCV FIN SEQ=%u", hdr->seq_num);
                connection_active = 0;

                struct sham_header fin_ack = { .ack_num = hdr->seq_num + 1, .flags = ACK };
                sendto(sock, &fin_ack, sizeof(fin_ack), 0, (struct sockaddr*)&client_addr, addrlen);
                rudp_log("SND ACK FOR FIN");

                struct sham_header my_fin = { .seq_num = (uint32_t)rand(), .flags = FIN };
                sendto(sock, &my_fin, sizeof(my_fin), 0, (struct sockaddr*)&client_addr, addrlen);
                rudp_log("SND FIN SEQ=%u", my_fin.seq_num);

                recvfrom(sock, &ack, sizeof(ack), 0, NULL, NULL);
                if ((ack.flags & ACK) && ack.ack_num == my_fin.seq_num + 1) {
                    rudp_log("RCV ACK=%u (FOR OUR FIN)", ack.ack_num);
                }
            }
        }
        fclose(fp);

        char md5_str[33];
        calculate_md5(output_filename, md5_str);
        printf("MD5: %s\n", md5_str);
    }

    close(sock);
    close_rudp_log();
    return 0;
}