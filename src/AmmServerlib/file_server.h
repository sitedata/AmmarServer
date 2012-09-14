#ifndef FILE_SERVER_H_INCLUDED
#define FILE_SERVER_H_INCLUDED

unsigned long SendAuthorizationHeader(int clientsock,char * message,char * verified_filename);

unsigned long SendFile
  (
    int clientsock, // The socket that will be used to send the data
    char * verified_filename_pending_copy, // The filename to be served on the socket above

    unsigned long start_at_byte,   // Optionally start with an offset ( resume download functionality )
    unsigned long end_at_byte,     // Optionally end at an offset ( resume download functionality )
    unsigned int force_error_code, // Instead of the file , serve an error code..!
    unsigned char header_only,     // Only serve header ( HEAD instead of GET )
    unsigned char keepalive,       // Keep alive functionality
    unsigned char gzip_supported,  // If gzip is supported try to use it!

    //char * webserver_root,
    char * templates_root // In case we fail to serve verified_filename_etc.. serve something from the templates..!
    );


unsigned long SendFileMemory
  (
    int clientsock, // The socket that will be used to send the data
    //char * path, // The filename to be served on the socket above
    char * mem, // The filename to be served on the socket above
    unsigned long mem_block
  );

#endif // FILE_SERVER_H_INCLUDED