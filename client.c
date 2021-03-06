#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <pb_encode.h>
#include <pb_decode.h>

#include "siopayload.pb.h"
#include "common.h"

bool sendFC(int fd, char* arg)
{
    /* Construct and send the request to server */
    {
        SIOPayload payload = SIOPayload_init_zero;
        FlowControl message = FlowControl_init_zero;
        pb_ostream_t output = pb_ostream_from_socket(fd);
        
        /* In our protocol, path is optional. If it is not given,
         * the server will list the root directory. */
        if (strcmp(arg, "ON")==0) {
            message.dxr = true;
            message.xts = true;
        } else {
            message.dxr = true;
            message.xts = false;    
        }
        
        payload.type.flow_control = message;
        payload.which_type = SIOPayload_flow_control_tag;

        fprintf(stdout, "Sending RTS: %d, DTR: %d\n", message.xts, message.dxr);
        /* Encode the request. It is written to the socket immediately
         * through our custom stream. */
        if (!pb_encode_delimited(&output, SIOPayload_fields, &payload))
        {
            fprintf(stderr, "Encoding failed: %s\n", PB_GET_ERROR(&output));
            return false;
        }
    }
    fprintf(stdout, "Encoding sucessful\n");
    /* Read back the response from server */
    // {
    //     ControlReceive message = {};
    //     pb_istream_t input = pb_istream_from_socket(fd);
    //     fprintf(stdout, "Trying to decode...\n");
    //     /*
    //     if (!pb_decode(&input, TestMessage_fields, &message))
    //     {
    //         fprintf(stderr, "Decode failed: %s\n", PB_GET_ERROR(&input));
    //         return false;
    //     }
    //     */
    //     fprintf(stdout, "decoded...\n");
    //     pb_decode_delimited(&input, ControlReceive_fields, &message);
    //     /* If the message from server decodes properly, but directory was
    //      * not found on server side, we get path_error == true. */
    //     fprintf(stdout, "Received CTS: %d, DSR: %d\n", message.cts, message.dsr);
    // }
    
    return true;
}

int main(int argc, char **argv)
{
    int sockfd;
    struct sockaddr_in servaddr;
    char *arg = NULL;
    
    if (argc > 1)
        arg = argv[1];
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    /* Connect to server running on localhost:1234 */
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, "10.170.241.162", &servaddr.sin_addr);
    servaddr.sin_port = htons(3333);
    
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
    {
        perror("connect");
        return 1;
    }
    if (arg == NULL) {
        arg = "OFF\n";
    }
    /* Send the directory listing request */
    if (!sendFC(sockfd, arg))
        return 2;
    
    /* Close connection */
    close(sockfd);
    
    return 0;
}
