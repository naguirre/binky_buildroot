#include <err.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

#include <cjson/cJSON.h>

#include <nfc/nfc.h>
#include <nfc/nfc-types.h>

static nfc_device *pnd = NULL;
static nfc_context *context;
const char _last_serial[32] = {0};
struct timeval _last_time_detection = {0};

char *_mac_addr = NULL;

static void stop_polling(int sig)
{
    (void) sig;
    if (pnd != NULL)
    {
        nfc_abort_command(pnd);
    }
    curl_global_cleanup();
    nfc_exit(context);
    exit(EXIT_FAILURE);
}


static cJSON * _load_playlist(unsigned int playlist_id)
{
    const char tmp[32];
    cJSON *root = cJSON_CreateObject();
    if(!root)
    {
        fprintf(stderr, "Error: cJSON_CreateObject failed.\n");
        return NULL;
    }

    cJSON_AddNumberToObject(root, "id", 1);
    cJSON_AddStringToObject(root, "method", "slim.request");
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(_mac_addr));//b8:27:eb:23:9c:90"));
    cJSON *cmd = cJSON_CreateArray();
    cJSON_AddItemToArray(cmd, cJSON_CreateString("playlistcontrol"));
    cJSON_AddItemToArray(cmd, cJSON_CreateString("play_index:0"));
    cJSON_AddItemToArray(cmd, cJSON_CreateString("cmd:load"));
    cJSON_AddItemToArray(cmd, cJSON_CreateString("menu:1"));
    snprintf(tmp, sizeof(tmp), "playlist_id:%d", playlist_id);
    cJSON_AddItemToArray(cmd, cJSON_CreateString(tmp));
    cJSON_AddItemToArray(cmd, cJSON_CreateString("useContextMenu:1"));
    cJSON_AddItemToArray(params, cmd);
    cJSON_AddItemToObject(root, "params", params);

    return root;
}

static int _http_post(const char *url, const char *post_data)
{
    int retcode = 0;
    CURL *curl = NULL;
    CURLcode res = CURLE_FAILED_INIT;
    char errbuf[CURL_ERROR_SIZE] = { 0, };
    struct curl_slist *headers = NULL;
    char agent[1024] = { 0, };

    curl = curl_easy_init();
    if(!curl)
    {
        fprintf(stderr, "Error: curl_easy_init failed.\n");
        goto cleanup;
    }
    snprintf(agent, sizeof agent, "libcurl/%s",
             curl_version_info(CURLVERSION_NOW)->version);
    agent[sizeof agent - 1] = 0;
    curl_easy_setopt(curl, CURLOPT_USERAGENT, agent);

    headers = curl_slist_append(headers, "Expect:");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, -1L);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

    res = curl_easy_perform(curl);
    if(res != CURLE_OK)
    {
        size_t len = strlen(errbuf);
        fprintf(stderr, "\nlibcurl: (%d) ", res);
        if(len)
            fprintf(stderr, "%s%s", errbuf, ((errbuf[len - 1] != '\n') ? "\n" : ""));
        fprintf(stderr, "%s\n\n", curl_easy_strerror(res));
        goto cleanup;
    }

    retcode = 1;

cleanup:
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return retcode;

}

static void print_usage(const char *progname)
{
    printf("usage: %s [-v]\n", progname);
    printf("  -v\t verbose display\n");
}

int main(int argc, const char *argv[])
{
    bool verbose = false;

    signal(SIGINT, stop_polling);


    if(curl_global_init(CURL_GLOBAL_ALL))
    {
        fprintf(stderr, "Fatal: The initialization of libcurl has failed.\n");
        return EXIT_FAILURE;
    }

    // Display libnfc version
    const char *acLibnfcVersion = nfc_version();

    /* printf("%s uses libnfc %s\n", argv[0], acLibnfcVersion); */
    /* if (argc != 1) */
    /* { */
    /*     if ((argc == 2) && (0 == strcmp("-v", argv[1]))) */
    /*     { */
    /*         verbose = true; */
    /*     } */
    /*     else */
    /*     { */
    /*         print_usage(argv[0]); */
    /*         exit(EXIT_FAILURE); */
    /*     } */
    /* } */
    _mac_addr = strdup(argv[1]);

    const uint8_t uiPollNr = 20;
    const uint8_t uiPeriod = 2;

    const nfc_modulation nmMifare = {
        .nmt = NMT_ISO14443A,
        .nbr = NBR_106,
    };


    /* const nfc_modulation nmModulations[1] = { */
    /*     { .nmt = NMT_ISO14443A, .nbr = NBR_106 }, */
    /* }; */
    /* const size_t szModulations = 1; */

    nfc_target nt;
    int res = 0;

    nfc_init(&context);
    if (context == NULL) {
        printf("Unable to init libnfc (malloc)");
        exit(EXIT_FAILURE);
    }

    pnd = nfc_open(context, NULL);

    if (pnd == NULL)
    {
        printf("%s", "Unable to open NFC device.");
        nfc_exit(context);
        exit(EXIT_FAILURE);
    }

    if (nfc_initiator_init(pnd) < 0)
    {
        nfc_perror(pnd, "nfc_initiator_init");
        nfc_close(pnd);
        nfc_exit(context);
        exit(EXIT_FAILURE);
    }

    printf("NFC reader: %s opened\n", nfc_device_get_name(pnd));



    while(1)
    {
        if ((res = nfc_initiator_select_passive_target(pnd, nmMifare, NULL, 0, &nt)) < 0)
        {
            nfc_perror(pnd, "nfc_initiator_select_passive_target");
            continue;
        }

        if (res > 0)
        {
            const char serial[32] = {0};
            for (int i = 0; i < nt.nti.nai.szUidLen; i++)
            {
                snprintf(serial+i*2, sizeof(serial), "%02x", nt.nti.nai.abtUid[i]);
            }

            printf("New serial detected : %s\n", serial);
            printf("Waiting for card removing...");
            fflush(stdout);

            cJSON *json;
            json = _load_playlist(3071);
            char *tmp = cJSON_PrintUnformatted(json);
            printf("Json : %s\n", tmp);
            _http_post("http://192.168.1.12:9000/jsonrpc.js", tmp);
            cJSON_Delete(json);
            free(tmp);

            while (0 == nfc_initiator_target_is_present(pnd, NULL))
            {
                // do nothing
            }
            nfc_perror(pnd, "nfc_initiator_target_is_present");
            printf("done.\n");
        }
        else
        {
            printf("No target found.\n");
        }
    }

    nfc_close(pnd);
    nfc_exit(context);
    exit(EXIT_SUCCESS);
}
