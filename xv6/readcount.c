#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int
main(int argc, char *argv[])
{
  int initial_count, final_count;
  char buffer[100];
  int fd;
  
  // Get initial count
  initial_count = getreadcount();
  printf("Initial read count: %d bytes\n", initial_count);
  
  // Read from a file
  fd = open("README", O_RDONLY);
  if(fd < 0) {
    printf("Error: Cannot open README file\n");
    exit(1);
  }
  
  // Read 100 bytes
  if(read(fd, buffer, 100) != 100) {
    printf("Error: Cannot read 100 bytes\n");
    close(fd);
    exit(1);
  }
  close(fd);
  
  // Get final count
  final_count = getreadcount();
  printf("Final read count: %d bytes\n", final_count);
  printf("Bytes read in this operation: %d bytes\n", final_count - initial_count);
  
  // Verification
  if((final_count - initial_count) == 100) {
    printf("SUCCESS: System call correctly counted 100 bytes\n");
  } else {
    printf("ERROR: Expected 100 bytes, got %d bytes\n", final_count - initial_count);
  }
  
  exit(0);
}