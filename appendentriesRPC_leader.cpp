/* ./leader log 127.0.0.1 127.0.0.1 127.0.0.1 127.0.0.1 */

#include "appendentries.h"
#include "debug.h"

std::mutex mutex;
int replicatelog_num;
int sock[3];

struct AppendEntriesRPC_Argument *AERPC_A = new struct AppendEntriesRPC_Argument;
struct AllServer_PersistentState *AS_PS = new struct AllServer_PersistentState;
struct AllServer_VolatileState *AS_VS = new struct AllServer_VolatileState;
struct Leader_VolatileState *L_VS = new struct Leader_VolatileState;

void AppendEntriesRPC(int i)
{

    /* AERPC_Aの設定 */
    AERPC_A->term = AS_PS->currentTerm;
    AERPC_A->prevLogIndex = AS_PS->log.index - 1;
    AERPC_A->prevLogTerm = AS_PS->log.term;
    AERPC_A->leaderCommit = AS_VS->commitIndex;

    my_send(sock[i + 1], AERPC_A, sizeof(struct AppendEntriesRPC_Argument));
    // printf("server%d finish sending\n\n", i);
    struct AppendEntriesRPC_Result *AERPC_R = new struct AppendEntriesRPC_Result;
    my_recv(sock[i + 1], AERPC_R, sizeof(struct AppendEntriesRPC_Result));

    // output_AERPC_R(AERPC_R);
    // printf("recv result from server%d \n", i);

    // • If successful: update nextIndex and matchIndex for follower.
    if (AERPC_R->success == 1)
    {
        L_VS->nextIndex[i] += 1;
        L_VS->matchIndex[i] += 1;

        // printf("Success : server%d\n", i);
        mutex.lock();
        replicatelog_num++;
        // printf("Now, replicatelog_num = %d\n", replicatelog_num);
        mutex.unlock();
    }
    // • If AppendEntries fails because of log inconsistency: decrement nextIndex and retry.
    else
    {
        printf("failure0\n");
        // L_VS->nextIndex[i] -= (ONCE_SEND_ENTRIES - 1);
        // AERPC_A->prevLogIndex -= (ONCE_SEND_ENTRIES - 1);
        // AppendEntriesRPC(connectserver_num, sock, AERPC_A, AERPC_R, L_VS, AS_VS, AS_PS);
        printf("failure1\n");
        exit(1);
    }
    // return 0;
}

void worker(int &sock_client, int &connectserver_num)
{
    char *str = new char[STRING];
    printf("entry -> ");
    scanf("%s", str);

    for (int i = 0; i < ENTRY_NUM; i++)
    {
        strcpy(AERPC_A->entries[i].entry, str);
        // printf("%s", AERPC_A->entries[i].entry);
    }

    for (int i = 0; i < (ALL_ACCEPTED_ENTRIES / ENTRY_NUM); i++)
    {
        replicatelog_num = 0;

        clock_gettime(CLOCK_MONOTONIC, &ts3);

        AS_PS->log.term = AS_PS->currentTerm;
        AS_PS->log.index = AS_PS->log.index + 1;

        tsum += write_log(&AS_PS->log, AERPC_A);
        // read_log();

        /* AS_VSの更新 */
        AS_VS->LastAppliedIndex += 1;

        std::thread threads0(AppendEntriesRPC, 0);
        std::thread threads1(AppendEntriesRPC, 1);

        threads0.join();
        threads1.join();

        int result = 0;
        // printf("replicatelog_num :%d\n", replicatelog_num);
        if (replicatelog_num + 1 > (connectserver_num + 1) / 2)
        {
            // printf("majority of servers replicated\n");
            AS_VS->commitIndex += 1;
            result = 1;
        }
        clock_gettime(CLOCK_MONOTONIC, &ts4);
        t = ts4.tv_sec - ts3.tv_sec + (ts4.tv_nsec - ts3.tv_nsec) / 1e9;
        // printf("time\n");
        printf("%.4f\n", t);
        // my_send(sock_client, &result, sizeof(int) * 1);
        // printf("return commit\n");
    }

    printf("write time\n");
    printf("%.4f\n", tsum);
    // fprintf(timerec, "%.4f\n", t);
}

int main(int argc, char *argv[])
{
    printf("made logfile\n");
    make_logfile(argv[1]);

    // 時間記録用ファイル
    // FILE *timerec;
    // timerec = fopen("timerecord.txt", "w+");
    // if (timerec == NULL)
    // {
    //     printf("cannot open file\n");
    //     exit(1);
    // }

    AS_PS->currentTerm = 1;
    AS_PS->voteFor = 0;
    AS_PS->log.index = 0;
    AS_PS->log.term = 0;

    AS_VS->commitIndex = 0;
    AS_VS->LastAppliedIndex = 0;
    for (int i = 0; i < 2; i++)
    {
        L_VS->nextIndex[i] = 1;
        L_VS->matchIndex[i] = 0;
    }

    int port[3];
    port[0] = 1111;
    port[1] = 1234;
    port[2] = 2345;

    char *ip[2];
    ip[0] = argv[2];
    ip[1] = argv[3];

    // ーーー3threadでしたとき
    // [follower] thread1,2 : port1,2 : ip0,1
    // for (int i = 0; i < 2; i++)
    // {
    //     threads[i + 1] = std::thread(connect_follower(port[i + 1], ip[i], i));
    // }
    // // client : thread0 : port0
    // threads[0] = std::thread(connect_client(port[0], ip[0]));
    // ーーーーーーーーーー

    // ソケット作成
    struct sockaddr_in addr[3];
    for (int i = 1; i < 3; i++)
    {
        sock[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (sock[i] < 0)
        {
            perror("socket error ");
            exit(0);
        }
        memset(&addr[i], 0, sizeof(struct sockaddr_in));
    }

    // follower
    for (int i = 1; i < 3; i++)
    {
        addr[i].sin_family = AF_INET;
        addr[i].sin_port = htons(port[i]);
        addr[i].sin_addr.s_addr = inet_addr(ip[i - 1]);
        const size_t addr_size = sizeof(addr);
    }

    int opt = 1;
    // ポートが解放されない場合, SO_REUSEADDRを使う
    for (int i = 1; i < 3; i++)
    {
        if (setsockopt(sock[i], SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt)) == -1)
        {
            perror("setsockopt error ");
            close(sock[i]);
            exit(0);
        }
    }

    // /* followerとconnect */
    int connectserver_num = 0;
    printf("Start connect...\n");
    for (int i = 1; i < 3; i++)
    {

        int k = 0;
        connect(sock[i], (struct sockaddr *)&addr[i], sizeof(struct sockaddr_in));
        my_recv(sock[i], &k, sizeof(int) * 1);
        if (k == 1)
        {
            connectserver_num += k;
        }
    }
    printf("Finish connect with %dservers!\n", connectserver_num);

    AERPC_A->term = 1;
    AERPC_A->leaderID = 1;
    AERPC_A->prevLogIndex = 0;
    AERPC_A->prevLogTerm = 0;
    AERPC_A->leaderCommit = 0;

    int last_id = 0;
    int sock_client = 0;
    char *buffer = (char *)malloc(32 * 1024 * 1024);

    std::thread t1(worker, std::ref(sock_client), std::ref(connectserver_num));
    t1.join();

    printf("全部複製した\n");
    return 0;
}
