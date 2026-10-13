/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "alrios/events/broker.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#define AR_EVENTD_DEFAULT_SOCKET_PATH    "/tmp/ar_eventd.sock"
#define AR_EVENTD_DEFAULT_MAX_TOPICS     256U
#define AR_EVENTD_DEFAULT_MAX_SUBS       512U
#define AR_EVENTD_DEFAULT_MAX_PAYLOAD    AR_EVENT_HARD_MAX_PAYLOAD
#define AR_EVENTD_DEFAULT_QUEUE_DEPTH    64U
#define AR_EVENTD_MAX_CLIENTS            64U
#define AR_EVENTD_MAX_SUBS_PER_CLIENT    16U
#define AR_EVENTD_MAX_FRAME_SIZE         (sizeof(ar_event_wire_header_t) + AR_EVENT_HARD_MAX_PAYLOAD)
#define AR_EVENT_MAX_FRAME_SIZE          AR_EVENTD_MAX_FRAME_SIZE

static volatile sig_atomic_t g_keep_running = 1;

static void handle_signal(int sig) {
    (void)sig;
    g_keep_running = 0;
}

typedef struct ar_eventd_client_sub {
    uint16_t topic;
    ar_event_subscription_t *sub;
} ar_eventd_client_sub_t;

typedef struct ar_eventd_client {
    int fd;
    ar_event_identity_t identity;
    ar_eventd_client_sub_t subs[AR_EVENTD_MAX_SUBS_PER_CLIENT];
    size_t sub_count;
} ar_eventd_client_t;

typedef struct ar_eventd_server {
    int listen_fd;
    char socket_path[sizeof(((struct sockaddr_un *)0)->sun_path)];
    ar_event_broker_t *broker;
    ar_eventd_client_t clients[AR_EVENTD_MAX_CLIENTS];
    size_t client_count;
} ar_eventd_server_t;

static int ar_eventd_authorize(
    void *context,
    ar_event_authorization_action_t action,
    const ar_event_identity_t *actor,
    const ar_event_identity_t *subscription_owner,
    uint16_t topic) {
    (void)context;
    (void)topic;

    if (actor == NULL || actor->principal[0] == '\0') {
        return 0;
    }
    if (actor->uid == 0U) {
        return 1;
    }
    if (action == AR_EVENT_AUTHORIZE_SUBSCRIBE) {
        return 1;
    }
    if (action == AR_EVENT_AUTHORIZE_UNSUBSCRIBE) {
        if (subscription_owner != NULL &&
            actor->uid == subscription_owner->uid &&
            strncmp(actor->principal, subscription_owner->principal, AR_EVENT_MAX_PRINCIPAL_LEN) == 0) {
            return 1;
        }
        return 0;
    }
    if (action == AR_EVENT_AUTHORIZE_EMIT) {
        return 1;
    }
    return 0;
}

static void print_usage(const char *prog) {
    (void)fprintf(stderr,
        "Usage: %s [options]\n"
        "Options:\n"
        "  --socket <path>         Path to UNIX domain socket (default: %s)\n"
        "  --max-topics <N>        Maximum active topics (default: %u)\n"
        "  --max-subscribers <N>   Maximum subscribers (default: %u)\n"
        "  --max-payload <N>       Maximum payload size in bytes (default: %u)\n"
        "  --queue-depth <N>       Subscriber queue depth (default: %u)\n"
        "  --once                  Run single processing cycle and exit\n"
        "  --help                  Show this usage message\n",
        prog, AR_EVENTD_DEFAULT_SOCKET_PATH,
        AR_EVENTD_DEFAULT_MAX_TOPICS,
        AR_EVENTD_DEFAULT_MAX_SUBS,
        AR_EVENTD_DEFAULT_MAX_PAYLOAD,
        AR_EVENTD_DEFAULT_QUEUE_DEPTH);
}

static int get_peer_credentials(int fd, ar_event_identity_t *out_identity) {
    struct ucred cred;
    socklen_t len = (socklen_t)sizeof(cred);
    int written;

    if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0) {
        return -1;
    }

    memset(out_identity, 0, sizeof(*out_identity));
    out_identity->pid = (uint32_t)cred.pid;
    out_identity->uid = (uint32_t)cred.uid;
    out_identity->gid = (uint32_t)cred.gid;

    if (cred.uid == 0U) {
        written = snprintf(out_identity->principal, sizeof(out_identity->principal), "system");
    } else {
        written = snprintf(out_identity->principal, sizeof(out_identity->principal),
                           "app-uid-%u", (unsigned int)cred.uid);
    }
    if (written < 0 || (size_t)written >= sizeof(out_identity->principal)) {
        return -1;
    }
    return 0;
}

static void client_remove_subscription_index(ar_eventd_client_t *client, size_t idx) {
    if (idx < client->sub_count) {
        client->sub_count--;
        client->subs[idx] = client->subs[client->sub_count];
        memset(&client->subs[client->sub_count], 0, sizeof(client->subs[client->sub_count]));
    }
}

static void client_disconnect(ar_eventd_server_t *server, size_t client_idx) {
    ar_eventd_client_t *client = &server->clients[client_idx];
    size_t i;

    for (i = 0U; i < client->sub_count; ++i) {
        if (client->subs[i].sub != NULL) {
            (void)ar_event_broker_unsubscribe(server->broker, &client->identity, client->subs[i].sub);
            client->subs[i].sub = NULL;
        }
    }
    client->sub_count = 0U;

    if (client->fd >= 0) {
        (void)close(client->fd);
        client->fd = -1;
    }

    server->client_count--;
    server->clients[client_idx] = server->clients[server->client_count];
    memset(&server->clients[server->client_count], 0, sizeof(server->clients[server->client_count]));
}

static int send_response(int fd, uint16_t opcode, uint16_t topic, uint64_t transaction_id) {
    ar_event_wire_header_t hdr;
    uint8_t wire[sizeof(ar_event_wire_header_t)];

    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = AR_EVENT_WIRE_MAGIC;
    hdr.version = AR_EVENT_WIRE_VERSION;
    hdr.opcode = opcode;
    hdr.topic = topic;
    hdr.transaction_id = transaction_id;

    if (ar_event_wire_header_encode(&hdr, wire) != AR_EVENT_OK) {
        return -1;
    }

    ssize_t sent = send(fd, wire, sizeof(wire), MSG_DONTWAIT | MSG_NOSIGNAL);
    return sent == (ssize_t)sizeof(wire) ? 0 : -1;
}

static void dispatch_pending_events(ar_eventd_server_t *server) {
    size_t c_idx;
    uint8_t payload_buf[AR_EVENT_HARD_MAX_PAYLOAD];
    uint8_t frame_buf[AR_EVENT_MAX_FRAME_SIZE];

    for (c_idx = 0U; c_idx < server->client_count; ++c_idx) {
        ar_eventd_client_t *client = &server->clients[c_idx];
        size_t s_idx;

        for (s_idx = 0U; s_idx < client->sub_count; ++s_idx) {
            ar_event_subscription_t *sub = client->subs[s_idx].sub;

            while (ar_event_subscription_pending(sub) > 0U) {
                ar_event_metadata_t meta;
                size_t payload_len = 0U;
                int r = ar_event_subscription_receive(
                    sub, &meta, payload_buf, sizeof(payload_buf), &payload_len);
                if (r != AR_EVENT_OK) {
                    break;
                }

                ar_event_wire_header_t out_hdr;
                memset(&out_hdr, 0, sizeof(out_hdr));
                out_hdr.magic = AR_EVENT_WIRE_MAGIC;
                out_hdr.version = AR_EVENT_WIRE_VERSION;
                out_hdr.opcode = IPC_EVENT_EMIT;
                out_hdr.topic = meta.topic;
                out_hdr.sequence = meta.sequence;
                out_hdr.transaction_id = meta.transaction_id;
                out_hdr.timestamp_nanos = meta.timestamp_nanos;
                out_hdr.producer_pid = meta.producer.pid;
                out_hdr.producer_uid = meta.producer.uid;
                out_hdr.producer_gid = meta.producer.gid;
                out_hdr.payload_len = (uint32_t)payload_len;
                memcpy(out_hdr.producer_principal, meta.producer.principal,
                       sizeof(out_hdr.producer_principal));

                if (ar_event_wire_header_encode(&out_hdr, frame_buf) != AR_EVENT_OK) {
                    break;
                }
                if (payload_len > 0U) {
                    memcpy(frame_buf + sizeof(ar_event_wire_header_t), payload_buf, payload_len);
                }

                size_t total_len = sizeof(ar_event_wire_header_t) + payload_len;
                ssize_t sent = send(client->fd, frame_buf, total_len, MSG_DONTWAIT | MSG_NOSIGNAL);
                if (sent < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        /* Socket buffer saturated: slow consumer isolation */
                        break;
                    }
                    /* Socket error: client will be disconnected on next poll */
                    break;
                }
            }
        }
    }
}

static int handle_client_datagram(ar_eventd_server_t *server, size_t client_idx) {
    ar_eventd_client_t *client = &server->clients[client_idx];
    uint8_t raw_msg[AR_EVENT_MAX_FRAME_SIZE];
    char cmsg_buf[CMSG_SPACE(sizeof(struct ucred)) + 64];
    struct msghdr msg;
    struct iovec iov;
    struct cmsghdr *cmsg;
    ar_event_wire_header_t header;
    ssize_t nrecv;

    memset(&msg, 0, sizeof(msg));
    iov.iov_base = raw_msg;
    iov.iov_len = sizeof(raw_msg);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);

    do {
        nrecv = recvmsg(client->fd, &msg, MSG_DONTWAIT);
    } while (nrecv < 0 && errno == EINTR);

    if (nrecv <= 0) {
        if (nrecv == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
            client_disconnect(server, client_idx);
            return -1;
        }
        return 0;
    }

    if ((size_t)nrecv < sizeof(ar_event_wire_header_t)) {
        return 0;
    }

    /* Verify kernel-authenticated credentials if passed */
    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_CREDENTIALS) {
            struct ucred *uc = (struct ucred *)(void *)CMSG_DATA(cmsg);
            if ((uint32_t)uc->uid != client->identity.uid ||
                (uint32_t)uc->pid != client->identity.pid) {
                /* Credential mismatch: reject and disconnect */
                client_disconnect(server, client_idx);
                return -1;
            }
        }
    }

    if (ar_event_wire_header_decode(raw_msg, &header) != AR_EVENT_OK) {
        (void)send_response(client->fd, IPC_EVENT_NACK, 0U, 0ULL);
        return 0;
    }

    if (header.opcode == IPC_EVENT_SUBSCRIBE) {
        if (client->sub_count >= AR_EVENTD_MAX_SUBS_PER_CLIENT) {
            (void)send_response(client->fd, IPC_EVENT_NACK, header.topic, header.transaction_id);
            return 0;
        }

        ar_event_overflow_policy_t policy =
            (header.flags == AR_EVENT_OVERFLOW_DROP_OLDEST)
                ? AR_EVENT_OVERFLOW_DROP_OLDEST
                : AR_EVENT_OVERFLOW_DROP_NEWEST;
        ar_event_subscription_t *sub = NULL;

        /* Derive actor identity from authenticated socket credentials, NOT untrusted payload */
        int status = ar_event_broker_subscribe(
            server->broker, &client->identity, header.topic, policy, &sub);
        if (status == AR_EVENT_OK) {
            client->subs[client->sub_count].topic = header.topic;
            client->subs[client->sub_count].sub = sub;
            client->sub_count++;
            (void)send_response(client->fd, IPC_EVENT_ACK, header.topic, header.transaction_id);
        } else {
            (void)send_response(client->fd, IPC_EVENT_NACK, header.topic, header.transaction_id);
        }
    } else if (header.opcode == IPC_EVENT_UNSUBSCRIBE) {
        size_t s_idx;
        int found = 0;

        for (s_idx = 0U; s_idx < client->sub_count; ++s_idx) {
            if (client->subs[s_idx].topic == header.topic) {
                int status = ar_event_broker_unsubscribe(
                    server->broker, &client->identity, client->subs[s_idx].sub);
                client_remove_subscription_index(client, s_idx);
                if (status == AR_EVENT_OK) {
                    (void)send_response(client->fd, IPC_EVENT_ACK, header.topic, header.transaction_id);
                } else {
                    (void)send_response(client->fd, IPC_EVENT_NACK, header.topic, header.transaction_id);
                }
                found = 1;
                break;
            }
        }
        if (!found) {
            (void)send_response(client->fd, IPC_EVENT_NACK, header.topic, header.transaction_id);
        }
    } else if (header.opcode == IPC_EVENT_EMIT) {
        size_t payload_len = header.payload_len;
        const void *payload_ptr = raw_msg + sizeof(ar_event_wire_header_t);

        if ((size_t)nrecv < sizeof(ar_event_wire_header_t) + payload_len) {
            (void)send_response(client->fd, IPC_EVENT_NACK, header.topic, header.transaction_id);
            return 0;
        }

        uint64_t now_ns = 0U;
        struct timespec ts;
        if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
            now_ns = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
        }

        ar_event_emit_result_t emit_res;
        /* Producer identity is derived from authenticated socket credentials */
        int status = ar_event_broker_emit(
            server->broker,
            &client->identity,
            header.topic,
            header.transaction_id,
            now_ns,
            payload_ptr,
            payload_len,
            &emit_res);

        if (status == AR_EVENT_OK || status == AR_EVENT_PARTIAL) {
            (void)send_response(client->fd, IPC_EVENT_ACK, header.topic, header.transaction_id);
            dispatch_pending_events(server);
        } else {
            (void)send_response(client->fd, IPC_EVENT_NACK, header.topic, header.transaction_id);
        }
    } else {
        (void)send_response(client->fd, IPC_EVENT_NACK, header.topic, header.transaction_id);
    }
    return 0;
}

int main(int argc, char **argv) {
    const char *socket_path = AR_EVENTD_DEFAULT_SOCKET_PATH;
    size_t max_topics = AR_EVENTD_DEFAULT_MAX_TOPICS;
    size_t max_subscribers = AR_EVENTD_DEFAULT_MAX_SUBS;
    size_t max_payload = AR_EVENTD_DEFAULT_MAX_PAYLOAD;
    size_t queue_depth = AR_EVENTD_DEFAULT_QUEUE_DEPTH;
    int run_once = 0;
    int i;
    struct sigaction sa;
    struct sockaddr_un sun;
    ar_event_broker_config_t config;
    ar_eventd_server_t server;

    memset(&server, 0, sizeof(server));
    server.listen_fd = -1;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--socket") == 0) {
            if (++i >= argc) {
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
            socket_path = argv[i];
        } else if (strcmp(argv[i], "--max-topics") == 0) {
            if (++i >= argc) {
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
            max_topics = (size_t)strtoul(argv[i], NULL, 10);
        } else if (strcmp(argv[i], "--max-subscribers") == 0) {
            if (++i >= argc) {
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
            max_subscribers = (size_t)strtoul(argv[i], NULL, 10);
        } else if (strcmp(argv[i], "--max-payload") == 0) {
            if (++i >= argc) {
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
            max_payload = (size_t)strtoul(argv[i], NULL, 10);
        } else if (strcmp(argv[i], "--queue-depth") == 0) {
            if (++i >= argc) {
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
            queue_depth = (size_t)strtoul(argv[i], NULL, 10);
        } else if (strcmp(argv[i], "--once") == 0) {
            run_once = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        } else {
            (void)fprintf(stderr, "ar_eventd: unrecognized option '%s'\n", argv[i]);
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    (void)sigaction(SIGINT, &sa, NULL);
    (void)sigaction(SIGTERM, &sa, NULL);

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    (void)sigaction(SIGPIPE, &sa, NULL);

    config.max_topics = max_topics;
    config.max_payload_size = max_payload;
    config.max_subscribers = max_subscribers;
    config.subscriber_queue_depth = queue_depth;

    if (ar_event_broker_create(&config, ar_eventd_authorize, NULL, &server.broker) != AR_EVENT_OK) {
        (void)fprintf(stderr, "ar_eventd: failed to create in-memory broker\n");
        return EXIT_FAILURE;
    }

    server.listen_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (server.listen_fd < 0) {
        (void)fprintf(stderr, "ar_eventd: failed to create AF_UNIX SOCK_SEQPACKET socket: %s\n",
                      strerror(errno));
        ar_event_broker_destroy(server.broker);
        return EXIT_FAILURE;
    }

    int flags = fcntl(server.listen_fd, F_GETFL, 0);
    if (flags >= 0) {
        (void)fcntl(server.listen_fd, F_SETFL, flags | O_NONBLOCK);
    }
    flags = fcntl(server.listen_fd, F_GETFD, 0);
    if (flags >= 0) {
        (void)fcntl(server.listen_fd, F_SETFD, flags | FD_CLOEXEC);
    }

    memset(&sun, 0, sizeof(sun));
    sun.sun_family = AF_UNIX;
    size_t path_len = strlen(socket_path);
    if (path_len >= sizeof(sun.sun_path)) {
        (void)fprintf(stderr, "ar_eventd: socket path too long\n");
        (void)close(server.listen_fd);
        ar_event_broker_destroy(server.broker);
        return EXIT_FAILURE;
    }
    memcpy(sun.sun_path, socket_path, path_len + 1U);
    memcpy(server.socket_path, socket_path, path_len + 1U);

    (void)unlink(socket_path);
    if (bind(server.listen_fd, (struct sockaddr *)&sun, (socklen_t)sizeof(sun)) != 0) {
        (void)fprintf(stderr, "ar_eventd: failed to bind socket '%s': %s\n",
                      socket_path, strerror(errno));
        (void)close(server.listen_fd);
        ar_event_broker_destroy(server.broker);
        return EXIT_FAILURE;
    }

    if (listen(server.listen_fd, 32) != 0) {
        (void)fprintf(stderr, "ar_eventd: failed to listen on socket '%s': %s\n",
                      socket_path, strerror(errno));
        (void)unlink(server.socket_path);
        (void)close(server.listen_fd);
        ar_event_broker_destroy(server.broker);
        return EXIT_FAILURE;
    }

    if (run_once) {
        /* Run a single non-blocking polling pass */
        struct pollfd pfd;
        pfd.fd = server.listen_fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        (void)poll(&pfd, 1, 0);
        g_keep_running = 0;
    }

    while (g_keep_running) {
        struct pollfd pfds[AR_EVENTD_MAX_CLIENTS + 1U];
        nfds_t nfds = 0;
        size_t idx;

        pfds[nfds].fd = server.listen_fd;
        pfds[nfds].events = POLLIN;
        pfds[nfds].revents = 0;
        nfds++;

        for (idx = 0U; idx < server.client_count; ++idx) {
            pfds[nfds].fd = server.clients[idx].fd;
            pfds[nfds].events = POLLIN;
            pfds[nfds].revents = 0;
            nfds++;
        }

        int poll_res = poll(pfds, nfds, 100);
        if (poll_res < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        if (pfds[0].revents & POLLIN) {
            int client_fd = accept(server.listen_fd, NULL, NULL);
            if (client_fd >= 0) {
                int cflags = fcntl(client_fd, F_GETFL, 0);
                if (cflags >= 0) {
                    (void)fcntl(client_fd, F_SETFL, cflags | O_NONBLOCK);
                }
                cflags = fcntl(client_fd, F_GETFD, 0);
                if (cflags >= 0) {
                    (void)fcntl(client_fd, F_SETFD, cflags | FD_CLOEXEC);
                }

                int passcred = 1;
                (void)setsockopt(client_fd, SOL_SOCKET, SO_PASSCRED, &passcred, (socklen_t)sizeof(passcred));

                if (server.client_count < AR_EVENTD_MAX_CLIENTS) {
                    ar_eventd_client_t *new_cli = &server.clients[server.client_count];
                    memset(new_cli, 0, sizeof(*new_cli));
                    new_cli->fd = client_fd;
                    if (get_peer_credentials(client_fd, &new_cli->identity) == 0) {
                        server.client_count++;
                    } else {
                        (void)close(client_fd);
                    }
                } else {
                    (void)close(client_fd);
                }
            }
        }

        for (idx = 0U; idx < server.client_count; ) {
            size_t p_idx = idx + 1U;
            if (pfds[p_idx].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                client_disconnect(&server, idx);
                continue;
            }
            if (pfds[p_idx].revents & POLLIN) {
                if (handle_client_datagram(&server, idx) != 0) {
                    continue;
                }
            }
            idx++;
        }

        if (run_once) {
            break;
        }
    }

    /* Clean up all clients */
    while (server.client_count > 0U) {
        client_disconnect(&server, 0U);
    }

    if (server.listen_fd >= 0) {
        (void)close(server.listen_fd);
        server.listen_fd = -1;
    }
    if (server.socket_path[0] != '\0') {
        (void)unlink(server.socket_path);
    }

    ar_event_broker_destroy(server.broker);
    server.broker = NULL;

    return EXIT_SUCCESS;
}
