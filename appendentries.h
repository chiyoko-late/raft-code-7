#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#include <stddef.h>
#include <errno.h>
#include <fcntl.h>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <thread>
#include <mutex>
#include "my_sock.h"
// #include <array>

#define SERVER_ADDR "0.0.0.0"
#define STRING (10LL)
#define ALL_ACCEPTED_ENTRIES (10000L * 5000L)
#define ENTRY_NUM (10000L * 5000L)

// using namespace std;

// int STRING = 100;
// int ALL_ACCEPTED_ENTRIES = 10000;
// int ENTRY_NUM = 10000;

uint64_t c1,
    c2;
struct timespec ts1, ts2, ts3, ts4;
double t;
double tsum;

static uint64_t rdtscp()
{
    uint64_t rax;
    uint64_t rdx;
    uint32_t aux;
    asm volatile("rdtscp"
                 : "=a"(rax), "=d"(rdx), "=c"(aux)::);
    return (rdx << 32) | rax;
}

struct append_entry
{
    char entry[STRING];
};

struct AppendEntriesRPC_Argument
{
    int term;
    int leaderID;
    int prevLogIndex;
    int prevLogTerm;
    int leaderCommit;
    struct append_entry entries[ENTRY_NUM];
};

struct AppendEntriesRPC_Result
{
    int term;
    bool success;
};

struct LOG
{
    int term;
    int index;
    struct append_entry entries[ENTRY_NUM];
};

struct AllServer_PersistentState
{
    int currentTerm;
    int voteFor;
    LOG log;
};

struct AllServer_VolatileState
{
    int commitIndex;
    int LastAppliedIndex;
};

struct Leader_VolatileState
{
    std::vector<int> nextIndex = std::vector<int>(2);
    std::vector<int> matchIndex = std::vector<int>(2);
};

char *filename;
char *logfilename;

int fdo;
// // logfile初期化
void make_logfile(char *name)
{
    filename = name;
    fdo = open(filename, (O_CREAT | O_RDWR), 0644);
    // fdo = open(filename, (O_CREAT | O_APPEND), 0644);
    if (fdo == -1)
    {
        printf("file open error\n");
        exit(1);
    }
    printf("fdo: %d\n", fdo);
    return;
}

void read_prev(int prevLogIndex, int *read_index, int *read_term)
{
    int read_log[2];
    lseek(fdo, sizeof(struct LOG) * (prevLogIndex - 1), SEEK_SET);
    read(fdo, read_log, sizeof(int) * 2);
    *read_term = read_log[0];
    *read_index = read_log[1];
    return;
}

double write_log(
    struct LOG *log,
    struct AppendEntriesRPC_Argument *rpc)
{
    clock_gettime(CLOCK_MONOTONIC, &ts1);

    write(fdo, &log->term, sizeof(int));
    write(fdo, &log->index, sizeof(int));
    write(fdo, &rpc->entries, sizeof(append_entry) * ENTRY_NUM);
    fsync(fdo);

    clock_gettime(CLOCK_MONOTONIC, &ts2);
    t = ts2.tv_sec - ts1.tv_sec + (ts2.tv_nsec - ts1.tv_nsec) / 1e9;
    // printf("%.4f\n", t);
    return t;
}

void read_log()
{
    struct LOG *r_log = new LOG;

    lseek(fdo, 0, SEEK_SET);
    for (int i = 0; i < ALL_ACCEPTED_ENTRIES / ENTRY_NUM; i++)
    {
        read(fdo, r_log, sizeof(struct LOG));
        printf("%d \n", r_log->term);
        printf("%d \n", r_log->index);
        for (int num = 0; num < ENTRY_NUM; num++)
        {
            printf("%d : %s\n", num, r_log->entries[num].entry);
        }
    }
    return;
}

// void output_AERPC_A(struct AppendEntriesRPC_Argument *p)
// {
//     printf("---appendEntriesRPC---\n");
//     printf("term: %d\n", p->term);
//     for (int i = 1; i < ONCE_SEND_ENTRIES; i++)
//     {
// std::string output_string(p->entries[i - 1].entry.begin(), p->entries[i - 1].entry.end());
//         printf("entry: %s\n", output_string.c_str());
//         // cout << "entry:" << p->entries[i - 1].entry << endl;
//     }
//     printf("prevLogIndex: %d-%d\n", p->prevLogIndex[0], p->prevLogIndex[ONCE_SEND_ENTRIES - 1]);
//     printf("prevLogTerm: %d-%d\n", p->prevLogTerm[0], p->prevLogIndex[ONCE_SEND_ENTRIES - 1]);
//     printf("LeaderCommitIndex: %d\n", p->leaderCommit);
//     printf("----------------------\n");
//     return;
// }

// void output_AERPC_R(struct AppendEntriesRPC_Result *p)
// {
//     printf("**AERPC_R**\n");
//     printf("term: %d\n", p->term);
//     printf("bool: %d\n", p->success);
//     printf("***********\n");
// }
