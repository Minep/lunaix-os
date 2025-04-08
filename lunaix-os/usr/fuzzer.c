#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <lunaix/syscall.h>
#include <lunaix/types.h>

#define NUM_ITERATIONS 1000000
#define RANDOM_DEVICE "/dev/rand" // Use urandom (non-blocking)
#define NUM_ARGS 6

// --- Minimal write_string helper ---
// Calculates length manually and uses write syscall.
// Returns number of bytes written or -1 on error.
ssize_t write_string(int fd, const char *s) {
    if (!s) return 0;
    size_t len = 0;
    while (s[len] != '\0') {
        len++;
    }
    return write(fd, s, len);
}

unsigned char random_bytes[NUM_ARGS * sizeof(unsigned long)];
unsigned long args[NUM_ARGS];
int main() {
    int fd_rand;

    // Buffer to hold random bytes for all arguments at once

    ssize_t bytes_read;
    long syscall_result; // To store the result

    // 1. Open the random device
    fd_rand = open(RANDOM_DEVICE, O_RDONLY);
    if (fd_rand == -1) {
        // Write error to stderr (file descriptor 2)
        write_string(2, "Error: Cannot open ");
        write_string(2, RANDOM_DEVICE);
        write_string(2, "\n");
        return 1; // Exit with error
    }

    write_string(1, "Starting simple syscall fuzzer (using dummy function)...\n");

    // 2. Fuzzing Loop
    for (unsigned long i = 0; i < NUM_ITERATIONS; ++i) {

        // 3. Read random bytes for arguments
        bytes_read = read(fd_rand, random_bytes, sizeof(random_bytes));
        if (bytes_read < 0) {
             write_string(2, "Error: Failed reading from random device\n");
             break; // Exit loop on read error
        }
        if ((size_t)bytes_read != sizeof(random_bytes)) {
            write_string(2, "Error: Insufficient random bytes read\n");
            // This is unlikely with urandom but check anyway
            break; // Exit loop
        }

        // 4. Construct arguments from random bytes
        // Assumes system is little-endian (most common desktops/servers)
        for (int j = 0; j < NUM_ARGS; ++j) {
            args[j] = 0;
            // Reconstruct one unsigned long from its bytes
            for (unsigned int b = 0; b < sizeof(unsigned long); ++b) {
                // Least significant byte first
                args[j] |= ((unsigned long)random_bytes[j * sizeof(unsigned long) + b]) << (b * 8);
            }
        }

        // 5. Call the hypothetical syscall function
        // !!! THIS IS THE POTENTIALLY DANGEROUS STEP !!!
        args[0] = (args[0] % 0x201);
        printf("do_lunaix_syscall(%d, %x, %x, %x, %x, %x);\n", 
            args[0], args[1], args[2], args[3], args[4], args[5]);
        syscall_result = do_lunaix_syscall(args[0], args[1], args[2], args[3], args[4], args[5]);

        // 6. Optional: Check result or print progress
        // In a real fuzzer, you might check 'syscall_result' for specific error codes,
        // or monitor system logs/dmesg for crashes.
        // For this simple version, we just continue.

        // Print progress indicator occasionally to show it's running
        if ((i + 1) % 100000 == 0) { // Every 100k iterations
            write_string(1, ".");
        }
    }

    write_string(1, "\nFuzzer finished ");
    // We could print the final iteration count here if needed
    write_string(1, "iterations.\n");

    // 7. Clean up
    close(fd_rand);

    return 0; // Exit successfully
}