

#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>

#define block_size 1024
//102.400

typedef struct superbloque {
	char datablocks[block_size*100];		//número de bloques totales
	char data_bitmap[105];      			//array de bloques de datos disponibles
	char inode_bitmap[105];   				//array de inodos disponibles
} superbloque;

typedef struct inode {
	int datablocks[16];				//bloque de datos que apunta el inodo
	int number;
	int blocks;
	int size;						//tamaño del archivo o directorio
} inode;

typedef struct filetype {
	int valid;
	char path[100];
	char name[100];					//nombre
	inode *inum;					//número de inodo asignado
	struct filetype ** children;
	int num_children;
	int num_links;
	struct filetype * parent;
	char type[20];					//extension del archivo
	mode_t permissions;				// permisos
	uid_t user_id;					// userid
	gid_t group_id;					// groupid
	time_t a_time;					// tiempo de accesos
	time_t m_time;					// fecha de modificación
	time_t c_time;					// hora del estado
	time_t b_time;					// hora de la creación
	off_t size;						// tamaño del nodo
	
	int datablocks[16];
	int number;						//id
	int blocks;						//Numero de bloques ocupados

} filetype;

void initialize_superbloque();
void initialize_root_directory();
filetype *filetype_from_path(char *path);
void inodos_libres();
void bloques_libres();
int encontrar_libre_inodo();
int encontrar_libre_db();
void add_child(filetype *parent, filetype *child);
static int mymkdir(const char *path, mode_t mode);
