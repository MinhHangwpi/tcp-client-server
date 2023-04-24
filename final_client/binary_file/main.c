#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define BUFFER_SIZE 16384

void read_image_file(const char* input_filename, const char* output_filename) {
    int input_fd = open(input_filename, O_RDONLY);
    if (input_fd == -1) {
        printf("Error: could not open input file\n");
        exit(1);
    }

    int output_fd = open(output_filename, O_CREAT | O_WRONLY, 0644);
    if (output_fd == -1) {
        printf("Error: could not open output file\n");
        exit(1);
    }

    off_t file_size = lseek(input_fd, 0, SEEK_END);
    lseek(input_fd, 0, SEEK_SET);

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    ssize_t total_bytes_written = 0;

    while ((bytes_read = read(input_fd, buffer, BUFFER_SIZE)) > 0) {
        ssize_t bytes_written = write(output_fd, buffer, bytes_read);
        if (bytes_written == -1) {
            printf("Error: could not write to output file\n");
            exit(1);
        }
        total_bytes_written += bytes_written;
    }

    if (bytes_read == -1) {
        printf("Error: could not read from input file\n");
        exit(1);
    }

    close(input_fd);
    close(output_fd);

    if (total_bytes_written != file_size) {
        printf("Warning: number of bytes written does not match file size\n");
    }
}

int main(){
    char * inputFile = "qr_image.png";
    char * outputFile = "new_qr_image.png";

    read_image_file(inputFile, outputFile);

    return 0;
}