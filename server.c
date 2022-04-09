/* This is a simple TCP server that listens on port 1234 and provides lists
 * of files to clients, using a protocol defined in file_server.proto.
 *
 * It directly deserializes and serializes messages from network, minimizing
 * memory use.
 *
 * For flexibility, this example is implemented using posix api.
 * In a real embedded system you would typically use some other kind of
 * a communication and filesystem layer.
 */

#include <dirent.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pb_decode.h>
#include <pb_encode.h>

#include "common.h"
#include "siopayload.pb.h"
#include <pb_decode.h>
#include <pb_common.h>
#define HOST_IP "0.0.0.0"
#define HOST_PORT 6700

typedef struct _mydata_t 
{ 
size_t len; 
uint8_t *buf; 
} mydata_t; 

bool encode_string(pb_ostream_t *stream, const pb_field_t *field,
                   void *const *arg) {
  const char *str = "mystring";

  if (!pb_encode_tag_for_field(stream, field))
    return false;

  return pb_encode_string(stream, (const uint8_t *)str, strlen(str));
}

bool encode_bytes(pb_ostream_t *stream, const pb_field_t *field,
                   void *const *arg) {
    mydata_t *data = *arg;

  if (!pb_encode_tag_for_field(stream, field))
    return false;

  return pb_encode_string(stream, data->buf, data->len);
}

bool DataTransfer_callback(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    
    uint8_t buffer[1024];
    
    int strlen = stream->bytes_left;
    mydata_t data = {strlen, buffer};
    if (strlen > sizeof(buffer) - 1)
      return false;

    if (!pb_read(stream, buffer, strlen))
        return false;
    buffer[strlen] = '\0';
    /*printf("size was %d and bytes were %s\n", strlen, data.buf);*/
    printf("%s\n", data.buf);
    return true;

}

/* The callback below is a message-level callback which is called before each
 * submessage is decoded. It is used to set the pb_callback_t callbacks inside
 * the submessage. The reason we need this is that different submessages share
 * storage inside oneof union, and before we know the message type we can't set
 * the callbacks without overwriting each other.
 */
bool msg_callback(pb_istream_t *stream, const pb_field_t *field, void **arg) {
  /* Print the prefix field before the submessages.
   * This also demonstrates how to access the top level message fields
   * from callbacks.
   */

  if (field->tag == SIOPayload_data_transfer_tag) {
      DataTransfer *msg = field->pData;
    /*printf("bytes message\n");*/
      msg->data.funcs.decode = DataTransfer_callback;
  } else if (field->tag == SIOPayload_flow_control_tag) {
    printf("Flow Control:\n");
  }

  return true;
}



void handle_connection(int connfd, int argc, char** argv) {


    while(1) {
        int count = 0;
        ioctl(connfd, FIONREAD, &count);
        if (count > 0) {

          SIOPayload msg = SIOPayload_init_zero;
          msg.cb_type.funcs.decode = msg_callback;
          pb_istream_t stream = pb_istream_from_socket(connfd);
          if (!pb_decode_delimited(&stream, SIOPayload_fields, &msg)) {
            fprintf(stderr, "Decoding failed: %s\n", PB_GET_ERROR(&stream));
          }
          
          if (msg.which_type == SIOPayload_flow_control_tag ) {
            printf("dxr: %d, xts: %d\n", msg.type.flow_control.dxr, msg.type.flow_control.xts);
            SIOPayload payload = SIOPayload_init_zero;
            payload.which_type = SIOPayload_flow_control_tag;
            payload.type.flow_control.dxr = 0;
            payload.type.flow_control.xts = 0;
            pb_ostream_t output = pb_ostream_from_socket(connfd);
            if (!pb_encode_delimited(&output, SIOPayload_fields, &payload)) {
            printf("Encoding failed: %s\n", PB_GET_ERROR(&output));
            }

          } 

        }
    }
}



int main(int argc, char **argv) {

  int listenfd, connfd;
  struct sockaddr_in servaddr;
  int reuse = 1;

  /* Listen on localhost:1234 for TCP connections */
  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  inet_pton(AF_INET, HOST_IP, &servaddr.sin_addr); 
  
  servaddr.sin_port = htons(HOST_PORT);
  if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0) {
    perror("bind");
    return 1;
  }

  if (listen(listenfd, 5) != 0) {
    perror("listen");
    return 1;
  }

  for (;;) {
    /* Wait for a client */
    connfd = accept(listenfd, NULL, NULL);

    if (connfd < 0) {
      perror("accept");
      return 1;
    }

    printf("Got connection.\n");

    handle_connection(connfd, argc, argv);

    /* printf("Closing connection.\n"); */

    /* close(connfd); */
  }
  return 0;
}

