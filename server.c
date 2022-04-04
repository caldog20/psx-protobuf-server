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
#include <sys/ioctl.h>
#include "siopayload.pb.h"
#include "common.h"

#define HOST_IP "0.0.0.0"
#define HOST_PORT 3333

/* This callback function will be called during the encoding.
 * It will write out any number of FileInfo entries, without consuming unnecessary memory.
 * This is accomplished by fetching the filenames one at a time and encoding them
 * immediately.
 */
// bool ListFilesResponse_callback(pb_istream_t *istream, pb_ostream_t *ostream, const pb_field_iter_t *field)
// {
//     PB_UNUSED(istream);
//     if (ostream != NULL && field->tag == ListFilesResponse_file_tag)
//     {
//         DIR *dir = *(DIR**)field->pData;
//         struct dirent *file;
//         FileInfo fileinfo = {};

//         while ((file = readdir(dir)) != NULL)
//         {
//             fileinfo.inode = file->d_ino;
//             strncpy(fileinfo.name, file->d_name, sizeof(fileinfo.name));
//             fileinfo.name[sizeof(fileinfo.name) - 1] = '\0';

//             /* This encodes the header for the field, based on the constant info
//             * from pb_field_t. */
//             if (!pb_encode_tag_for_field(ostream, field))
//                 return false;

//             /* This encodes the data for the field, based on our FileInfo structure. */
//             if (!pb_encode_submessage(ostream, FileInfo_fields, &fileinfo))
//                 return false;
//         }

//         /* Because the main program uses pb_encode_delimited(), this callback will be
//         * called twice. Rewind the directory for the next call. */
//         rewinddir(dir);
//     }

//     return true;
// }

/* Handle one arriving client connection.
 * Clients are expected to send a ListFilesRequest, terminated by a '0'.
 * Server will respond with a ListFilesResponse message.
 */
void handle_connection(int connfd)
{
    int pin1, pin2;
    while(1) {
        int count = 0;
        ioctl(connfd, FIONREAD, &count);
        // If we received a message, send one back to toggle PSX lines
            if (count > 0){
            /* Decode the message from the client and open the requested directory. */
                SIOPayload payload = {};
                pb_istream_t input = pb_istream_from_socket(connfd);
                
                if (!pb_decode_delimited(&input, SIOPayload_fields, &payload))
                {
                    printf("Decode failed: %s\n", PB_GET_ERROR(&input));
                    return;
                }
                
                /* Print the data contained in the message. */
                printf("SIO Payload:\n");
                printf("which_type: %d\n", payload.which_type);
                switch(payload.which_type)
                {
                    case SIOPayload_data_transfer_tag:
                        printf("Data Transfer Message\n");
                        // printf("  data_send.length: %d\n", payload.type.data_transfer);
                        break;

                    case SIOPayload_flow_control_tag:
                        printf("Flow Control Message\n");
                        printf("psx_rts: %d\n", payload.type.flow_control.dxr);
                        printf("psx_dtr: %d\n\n", payload.type.flow_control.xts);
                        pin1 = payload.type.flow_control.dxr;
                        pin2 = payload.type.flow_control.xts;
                        break;
                }
            {
                printf("Sending a response\n");
                FlowControl ftcl = FlowControl_init_zero;
                ftcl.dxr = pin1;
                ftcl.xts = pin2;
                SIOPayload payload = SIOPayload_init_zero;
                pb_ostream_t output = pb_ostream_from_socket(connfd);
                payload.type.flow_control = ftcl;
                payload.which_type = SIOPayload_flow_control_tag;
                printf("Encoding\n");
                if (!pb_encode_delimited(&output, SIOPayload_fields, &payload)) {
                    printf("Encoding failed: %s\n", PB_GET_ERROR(&output));
                    break;
                }
                printf("Sent\n");
            }
        }



}
    // /* List the files in the directory and transmit the response to client */
    // {
    //     ListFilesResponse response = {};
    //     pb_ostream_t output = pb_ostream_from_socket(connfd);
        
    //     if (directory == NULL)
    //     {
    //         perror("opendir");
            
    //         /* Directory was not found, transmit error status */
    //         response.has_path_error = true;
    //         response.path_error = true;
    //     }
    //     else
    //     {
    //         /* Directory was found, transmit filenames */
    //         response.has_path_error = false;
    //         response.file = directory;
    //     }
        
    //     if (!pb_encode_delimited(&output, ListFilesResponse_fields, &response))
    //     {
    //         printf("Encoding failed: %s\n", PB_GET_ERROR(&output));
    //     }
    // }
}
int main(int argc, char **argv)
{
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
    if (bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0)
    {
        perror("bind");
        return 1;
    }
    
    if (listen(listenfd, 5) != 0)
    {
        perror("listen");
        return 1;
    }
    
    for(;;)
    {
        /* Wait for a client */
        connfd = accept(listenfd, NULL, NULL);
        
        if (connfd < 0)
        {
            perror("accept");
            return 1;
        }
        
        printf("Got connection.\n");
        
        handle_connection(connfd);
        
        printf("Closing connection.\n");
        
        // close(connfd);
    }
    
    return 0;
}
