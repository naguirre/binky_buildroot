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

typedef struct _HttpData {
    char *ptr;
    size_t len;
} HttpData;

typedef struct _Playlist {
    const char *name;
    unsigned int id;
}Playlist;

static Playlist _playlists[64];
static unsigned int _nb_playlists = 0;
static size_t read_callback(void *dest, size_t size, size_t nmemb, void *userp)
{
    HttpData *s = userp;
    size_t new_len = s->len + size*nmemb;
    s->ptr = realloc(s->ptr, new_len+1);
    if (s->ptr == NULL)
    {
        printf("realloc() failed\n");
        return 0;
    }
    memcpy(s->ptr+s->len, dest, size*nmemb);
    s->ptr[new_len] = '\0';
    s->len = new_len;

    return size*nmemb;
}

static cJSON *_list_playlists(void)
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
    cJSON_AddItemToArray(params, cJSON_CreateString("-"));
    cJSON *cmd = cJSON_CreateArray();
    cJSON_AddItemToArray(cmd, cJSON_CreateString("playlists"));
    cJSON_AddItemToArray(cmd, cJSON_CreateNumber(0));
    cJSON_AddItemToArray(cmd, cJSON_CreateNumber(999));
    cJSON_AddItemToArray(params, cmd);
    cJSON_AddItemToObject(root, "params", params);

    return root;
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

static cJSON * _basic_action(const char *action)
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
    cJSON_AddItemToArray(params, cJSON_CreateString(_mac_addr));
    cJSON *cmd = cJSON_CreateArray();
    cJSON_AddItemToArray(cmd, cJSON_CreateString(action));
    cJSON_AddItemToArray(cmd, cJSON_CreateString("useContextMenu:1"));
    cJSON_AddItemToArray(params, cmd);
    cJSON_AddItemToObject(root, "params", params);

    return root;
}


static char * _http_post(const char *url, const char *post_data)
{
    char *retdata = NULL;
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

    HttpData http_data;

    http_data.ptr = malloc(1);
    http_data.ptr[0] = '\0';
    http_data.len = 0;

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
    //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, read_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &http_data);

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

    retdata = strdup(http_data.ptr);;

cleanup:
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return retdata;

}

static void print_usage(const char *progname)
{
    printf("usage: %s [-v]\n", progname);
    printf("  -v\t verbose display\n");
}

void _reload_playlists(void)
{
    cJSON *json;
    json = _list_playlists();
    char *req = cJSON_PrintUnformatted(json);
    char *resp = _http_post("http://192.168.1.12:9000/jsonrpc.js", req);
    cJSON * root = cJSON_Parse(resp);
    cJSON * result = cJSON_GetObjectItemCaseSensitive(root,"result");

    cJSON *array = cJSON_GetObjectItemCaseSensitive(result, "playlists_loop");
    _nb_playlists = cJSON_GetArraySize(array);
    for (int i = 0; i < _nb_playlists; i++)
    {
        cJSON * playlist = cJSON_GetArrayItem(array, i);
        cJSON * name = cJSON_GetObjectItemCaseSensitive(playlist, "playlist");
        cJSON *id = cJSON_GetObjectItemCaseSensitive(playlist, "id");

        if (cJSON_IsNumber(id) && cJSON_IsString(name))
        {
            _playlists[i].id = id->valueint;
            _playlists[i].name = strdup(name->valuestring);
        }
        printf("Add %d %s to playlists\n", _playlists[i].id, _playlists[i].name);
    }
    cJSON_Delete(root);
    cJSON_Delete(json);
    free(resp);
    free(req);
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
    _mac_addr = strdup(argv[1]);

    const uint8_t uiPollNr = 20;
    const uint8_t uiPeriod = 2;

    const nfc_modulation nmMifare = {
        .nmt = NMT_ISO14443A,
        .nbr = NBR_106,
    };
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

    _reload_playlists();

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
            char found = 0;
            for (int i  = 0; i < _nb_playlists; i++)
            {
                if (strstr(_playlists[i].name, serial))
                {
                    cJSON *json;
                    json = _load_playlist(_playlists[i].id);
                    char *tmp = cJSON_PrintUnformatted(json);
                    printf("Playing id %d : %s\n", _playlists[i].id, _playlists[i].name);
                    _http_post("http://192.168.1.12:9000/jsonrpc.js", tmp);
                    cJSON_Delete(json);
                    free(tmp);

                    /* Playthe player when the card is inserted*/
                    json = _basic_action("play");
                    tmp = cJSON_PrintUnformatted(json);
                    _http_post("http://192.168.1.12:9000/jsonrpc.js", tmp);
                    cJSON_Delete(json);
                    free(tmp);

                    found = 1;
                    break;
                }
            }

            if (!found)
            {
                printf("%s not found in playlists : Rename a playlists like this : \"Playlist Name - %s\"\n", serial, serial);
                // Reload plaulists just in case
                _reload_playlists();
            }

            while (0 == nfc_initiator_target_is_present(pnd, NULL))
            {
                // Do nothing
            }
            printf("Card removed, stop playing\n");
            /* Stop the player when the card is removed */
            cJSON *json = _basic_action("stop");
            char *tmp = cJSON_PrintUnformatted(json);
            _http_post("http://192.168.1.12:9000/jsonrpc.js", tmp);
            cJSON_Delete(json);
            free(tmp);

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
