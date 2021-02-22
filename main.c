#define _GNU_SOURCE 1

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <fcntl.h>
#include <signal.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <uuid/uuid.h>

#ifndef SOCKET_PATH
#define SOCKET_PATH "/var/run/pastebin-server.sock"
#endif

#ifndef STORE_PATH
#define STORE_PATH "/var/www/paste"
#endif

#ifndef BASE_URL
#define BASE_URL "https://docs.ntzyz.cn/paste"
#endif

const char *default_socket_path = SOCKET_PATH;
const char *default_pasted_file_store_path = STORE_PATH;
const char *base_url = BASE_URL;

const int http_header_read_length_max = 8192;
const char *http_head_size_exceeded_response = "HTTP/1.1 413 Entity Too Large\r\n"
                                               "Content-Type: text/plain\r\n"
                                               "Content-Length: 23\r\n"
                                               "\r\n"
                                               "HTTP header too large.\n";
const char *http_header_content_length_key = "content-length:";

struct store_context
{
  int client_fd;
  const char *pasted_file_store_path;
};

static void *store_file(void *arg)
{
  const struct store_context *context = (const struct store_context *)arg;
  uuid_t uuid;
  char *output_path = (char *)malloc(strlen(context->pasted_file_store_path) + 56);
  char uuid_string[56] = {0};
  char buffer[http_header_read_length_max + 10];
  int file_fd = 0;
  int total_bytes_write = 0;
  int bytes_expected_to_read = 0;

  /* Generate file name */
  uuid_generate_random(uuid);
  uuid_unparse(uuid, uuid_string);

  sprintf(output_path, "%s/%s", context->pasted_file_store_path, uuid_string);

  /* Prepare to read some data */
  memset(buffer, 0, sizeof(buffer));
  int bytes_read = recv(context->client_fd, buffer, http_header_read_length_max, 0);

  if (bytes_read < 0)
  {
    perror("recv");
    goto error;
  }
  else if (bytes_read == 0)
  {
    goto error;
  }

  const char *body_begin_pointer = NULL;
  char *p = buffer;

  while (*p)
  {
    if (p[0] == '\r' && p[1] == '\n')
    {
      if (p[2] == '\r' && p[3] == '\n')
      {
        body_begin_pointer = p + 4;
        break;
      }
      else
      {
        p += 2;
      }
    }
    else
    {
      *p = tolower(*p);
      p++;
    }
  }

  if (body_begin_pointer == NULL)
  {
    /* We never meet the end of HTTP header in first http_header_read_length_max bytes
      Treat it as an error. */
    send(context->client_fd, http_head_size_exceeded_response, strlen(http_head_size_exceeded_response), 0);
    goto error;
  }

  const char *content_length_begin_pointer = strstr(buffer, http_header_content_length_key);
  if (content_length_begin_pointer == NULL)
  {
    const char *error_message = "Content-Length is required!";
    sprintf(buffer, "HTTP/1.1 400 Bad Request\r\n"
                    "Content-Type: text/plain\r\n"
                    "Content-Length: %ld"
                    "\r\n"
                    "%s\n",
            strlen(error_message) + 1, error_message);
    send(context->client_fd, buffer, strlen(buffer), 0);
    goto error;
  }

  p = (char *)content_length_begin_pointer + strlen(http_header_content_length_key);
  while (*p == ' ')
    p++;
  sscanf(p, "%d", &bytes_expected_to_read);

  /* Okay, here we are ready to write the request body to file. */
  if ((file_fd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0)
  {
    perror("open");
    goto error;
  }

  fchmod(file_fd, S_IRUSR | S_IRGRP | S_IROTH);

  if (bytes_expected_to_read == total_bytes_write)
  {
    goto end;
  }

  if (write(file_fd, body_begin_pointer, bytes_read - (body_begin_pointer - buffer)) < 0)
  {
    perror("write");
    goto error;
  }

  total_bytes_write += bytes_read - (body_begin_pointer - buffer);
  if (bytes_expected_to_read == total_bytes_write)
  {
    goto end;
  }

  for (;;)
  {
    bytes_read = recv(context->client_fd, buffer, sizeof(buffer), 0);

    if (bytes_read < 0)
    {
      perror("recv");
      goto error;
    }
    else if (bytes_read == 0)
    {
      break;
    }

    write(file_fd, buffer, bytes_read);
    total_bytes_write += bytes_read;

    if (bytes_expected_to_read == total_bytes_write)
    {
      goto end;
    }
  }

end:
  sprintf(buffer, "HTTP/1.1 200 OK\r\n"
                  "Content-Type: text/plain\r\n"
                  "Content-Length: %ld\r\n"
                  "\r\n"
                  "%s/%s\n",
          strlen(uuid_string) + strlen(base_url) + 2, base_url, uuid_string);
  send(context->client_fd, buffer, strlen(buffer), 0);

error:
  if (file_fd != 0)
  {
    close(file_fd);
  }

  free(output_path);
  free(arg);
  close(context->client_fd);

  return NULL;
}

int main()
{
  const char *socket_path = default_socket_path;
  struct sockaddr_un server_addr, client_addr;
  int server_fd;
  int client_fd;

  if ((server_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
  {
    perror("socket");
    exit(1);
  }

  server_addr.sun_family = AF_UNIX;
  strncpy(server_addr.sun_path, socket_path, sizeof(server_addr.sun_path));
  fchmod(server_fd, S_IRWXU | S_IRWXG | S_IRWXO);

  /* delete old path if necessary */
  unlink(socket_path);
  umask(0111);

  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    perror("bind");
    exit(1);
  }

  listen(server_fd, 10);
  puts("Server is up.");
  umask(0022);

  for (;;)
  {
    size_t client_len = sizeof(client_addr);
    pthread_t thread;
    struct store_context *context = NULL;

    if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_len)) < 0)
    {
      perror("accept");
      exit(1);
    }

    context = (struct store_context *)malloc(sizeof(struct store_context));
    context->client_fd = client_fd;
    context->pasted_file_store_path = default_pasted_file_store_path;

    pthread_create(&thread, NULL, store_file, context);
    pthread_detach(thread);
  }

  return 0;
}
