#include "ext_2.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <disk image>\n", argv[0]);
        return 1;
    }

    ext2_context ctx;
    if (ext2_init(&ctx, argv[1]) != 0) {
        return 1;
    }

    ext2_get_superblock_info(&ctx);  // Print superblock information
    ext2_list_directory(&ctx, 2);    // List root directory (inode 2)
    ext2_get_inode_info(&ctx, 2);    // Get inode info for root directory

// Test block map retrieval
    int blocks[10];
    int block_count = ext2_get_block_map(&ctx, 2, blocks, 10);
    if (block_count > 0) {
        printf("Block Map for Inode 2: ");
        for (int i = 0; i < block_count; i++) {
            printf("%d ", blocks[i]);
        }
        printf("\n");
    } else {
        printf("Failed to retrieve block map.\n");
    }

    // Test file search
    char json_buffer[1024];
    if (ext2_search_file(&ctx, "testfile.txt", json_buffer, sizeof(json_buffer)) == 0) {
        printf("Search Results: %s\n", json_buffer);
    } else {
        printf("File not found.\n");
    }


    ext2_close(&ctx);
    return 0;
}

