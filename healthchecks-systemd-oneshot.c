/*
Helper for systemd oneshot services to ping Healthchecks.io
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdnoreturn.h>

#include <time.h>
#include <unistd.h>

#include <curl/curl.h>
#include <systemd/sd-journal.h>

extern int optind;

#define ATTEMPT_LIMIT (5)
#define JOURNAL_LIMIT (1000)

static bool journal = false;
static bool verbose = false;

static FILE* journal_file = NULL;
static long journal_size = 0;

static noreturn void help(const char* argv0)
{
  fprintf(stderr, "Usage: %s [-jvh] URL\n", argv0);
  exit(1);
}

static size_t discard_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
  (void)ptr;
  (void)userdata;
  return size * nmemb;
}

static bool load_journal(const char* invocation_id)
{
  char match_id[64] = {0};
  int match_id_len = snprintf(match_id, sizeof(match_id), "_SYSTEMD_INVOCATION_ID=%s", invocation_id);
  if (match_id_len <= 0 || match_id_len >= (int)sizeof(match_id))
  {
    if (verbose)
      fputs("Error: Failed to format match string\n", stderr);
    return false;
  }

  if ((journal_file = tmpfile()) == NULL)
  {
    if (verbose)
      fputs("Error: Failed to open temporary file for journal messages\n", stderr);
    return false;
  }

  sd_journal* j = NULL;
  if (sd_journal_open(&j, SD_JOURNAL_LOCAL_ONLY) != 0)
  {
    if (verbose)
      fputs("Error: Failed to open journal\n", stderr);
    fclose(journal_file);
    journal_file = NULL;
    return false;
  }

  if (verbose)
    fprintf(stderr, "Journal filter: %.*s\n", match_id_len, match_id);

  if (sd_journal_add_match(j, match_id, match_id_len) != 0)
  {
    if (verbose)
      fputs("Error: Failed to filter journal\n", stderr);
    fclose(journal_file);
    journal_file = NULL;
    sd_journal_close(j);
    return false;
  }

  const char* message;
  size_t message_len;
  uint64_t timestamp;
  time_t timestamp_sec;
  struct tm timestamp_tm;
  char timestamp_sec_str[64] = {0};
  unsigned limit = JOURNAL_LIMIT;
  SD_JOURNAL_FOREACH(j)
  {
    if (--limit == 0)
      break;
    sd_journal_restart_data(j);
    if (sd_journal_get_realtime_usec(j, &timestamp) < 0)
      continue;
    if (sd_journal_get_data(j, "MESSAGE", (const void**)&message, &message_len) < 0)
      continue;
    if (!(message = strchr(message, '=')))
      continue;
    timestamp_sec = timestamp / 1000000;
    memset(&timestamp_tm, 0, sizeof(timestamp_tm));
    gmtime_r(&timestamp_sec, &timestamp_tm);
    strftime(timestamp_sec_str, sizeof(timestamp_sec_str), "%FT%T", &timestamp_tm);
    fprintf(journal_file, "[%s.%06dZ] %s\n", timestamp_sec_str, (int)(timestamp % 1000000), message+1); // +1 to skip '='
  }
  sd_journal_close(j);
  fflush(journal_file);
  journal_size = ftell(journal_file);
  if (journal_size <= 0)
  {
    fclose(journal_file);
    journal_file = NULL;
    if (verbose)
      fputs("No journal data to upload\n", stderr);
    return false;
  }
  if (verbose)
    fprintf(stderr, "Journal data to upload is %ld bytes\n", journal_size);
  return true;
}


int main(int argc, char *argv[])
{
  int opt;
  while ((opt = getopt(argc, argv, "jvh")) != -1)
  {
    switch (opt)
    {
    case 'j': journal = true; break;
    case 'v': verbose = true; break;
    case 'h': help(argv[0]); break;
    default:  help(argv[0]); break;
    }
  }

  if (optind >= argc)
    help(argv[0]);

  const char* base_url = argv[optind];
  if (strncmp(base_url, "http", 4) != 0)
  {
    fputs("URL expected to start with 'http'\n", stderr);
    exit(1);
  }

  const char* service_result = getenv("SERVICE_RESULT");
  const char* exit_code = getenv("EXIT_CODE");
  const char* exit_status = getenv("EXIT_STATUS");
  const char* invocation_id = getenv("INVOCATION_ID");

  // Reformat the invocation ID into the UUID form for the Heathchecks runtime ID
  char rid[40] = {0};
  if (invocation_id != NULL && strlen(invocation_id) == 32)
  {
    snprintf(rid, sizeof(rid), "%.8s-%.4s-%.4s-%.4s-%.12s",
      invocation_id, invocation_id + 8, invocation_id + 12, invocation_id + 16, invocation_id + 20);
  }
  else
  {
    journal = false;  // Need the INVOCATION_ID to match the journal entries
  }

  char* url = NULL;
  int url_rc;
  if (service_result == NULL)
  {
    if (*rid == '\0')
      url_rc = asprintf(&url, "%s/start", base_url);
    else
      url_rc = asprintf(&url, "%s/start?rid=%s", base_url, rid);

    journal = false;  // Can't post the journal when starting
  }
  else if ((exit_code != NULL) && (exit_status != NULL) && (strcmp(exit_code, "exited") == 0) && (*exit_status != '\0'))
  {
    char* endptr = NULL;
    long status = strtol(exit_status, &endptr, 10);
    if (endptr == NULL || *endptr != '\0')
    {
      fputs("Failed to parse EXIT_STATUS as number\n", stderr);
      exit(1);
    }
    if (status < 0 || status > 255)
    {
      fputs("EXIT_STATUS value out of range\n", stderr);
      exit(1);
    }
    if (*rid == '\0')
      url_rc = asprintf(&url, "%s/%ld", base_url, status);
    else
      url_rc = asprintf(&url, "%s/%ld?rid=%s", base_url, status, rid);
  }
  else
  {
    if (*rid == '\0')
      url_rc = asprintf(&url, "%s/fail", base_url);
    else
      url_rc = asprintf(&url, "%s/fail?rid=%s", base_url, rid);
  }

  if (url == NULL || url_rc <= 0)
  {
    fputs("Failed to format URL\n", stderr);
    exit(1);
  }

  if (verbose)
  {
    fprintf(stderr, "URL: %s\n", url);
  }

  if (journal && invocation_id)
  {
    journal = load_journal(invocation_id);
  }


  CURLcode ret;
  struct curl_slist *hs = NULL;
  CURL *hnd = curl_easy_init();
  curl_easy_setopt(hnd, CURLOPT_URL, url);
  curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
  curl_easy_setopt(hnd, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(hnd, CURLOPT_USERAGENT, "healthchecks-systemd-oneshot/1.0");
  curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 10L);
  curl_easy_setopt(hnd, CURLOPT_VERBOSE, (long)verbose);
  if (!verbose)
  {
    curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, &discard_write_callback);
  }
  if (journal && journal_file && journal_size > 0)
  {
    curl_easy_setopt(hnd, CURLOPT_POST, 1L);
    curl_easy_setopt(hnd, CURLOPT_READDATA, journal_file);
    curl_easy_setopt(hnd, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)journal_size);
    hs = curl_slist_append(hs, "Content-Type: text/plain");
  }
  if (hs)
    curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, hs);

  for (unsigned attempt = 0; attempt < ATTEMPT_LIMIT; ++attempt)
  {
    if (attempt)
    {
      const struct timespec delay = {1u << (attempt - 1u), 0};
      if (verbose)
      {
        fprintf(stderr, "Error: %s (%u) retry in %ld seconds\n",
          curl_easy_strerror(ret), ret, delay.tv_sec);
      }
      nanosleep(&delay, NULL);
    }
    if (journal_file)
      fseek(journal_file, 0, SEEK_SET);

    ret = curl_easy_perform(hnd);
    if (ret == CURLE_OK ||
        ret == CURLE_UNSUPPORTED_PROTOCOL ||
        ret == CURLE_URL_MALFORMAT ||
        ret == CURLE_HTTP_RETURNED_ERROR)
    {
      break;
    }
  }

  curl_easy_cleanup(hnd);
  free(url);
  if (journal_file)
    fclose(journal_file);
  exit((int)ret);
}
