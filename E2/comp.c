#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include "compress.h"
#include "chunk_archive.h"
#include "queue.h"
#include "options.h"

#define CHUNK_SIZE (1024*1024)
#define QUEUE_SIZE 20

#define COMPRESS 1
#define DECOMPRESS 0

typedef struct {
	int  chunksTotales;
	int *chunksProcesados;
	pthread_mutex_t *mutexChunks;
	queue	in;
	queue	out;
} ArgsCompresor;

// take chunks from queue in, run them through process (compress or decompress), send them to queue out
void worker(queue in, queue out, chunk (*process)(chunk)) {
    chunk ch, res;
    while(q_elements(in)>0) {
        ch = q_remove(in);

        res = process(ch);
        free_chunk(ch);

        q_insert(out, res);
    }
}

void *workerCompresor(void *ptr) {
	ArgsCompresor *args = ptr;
    chunk ch,res;
    while(1){

		pthread_mutex_lock(args->mutexChunks);

		if(*args->chunksProcesados == args->chunksTotales){
			pthread_mutex_unlock(args->mutexChunks);
			break;
		}

		(*(args->chunksProcesados))++;
		pthread_mutex_unlock(args->mutexChunks);
		ch = q_remove(args->in);
		res = zcompress(ch);
		free_chunk(ch);
		q_insert(args->out, res);

	}

	return NULL;
}

// Compress file taking chunks of opt.size from the input file,
// inserting them into the in queue, running them using a worker,
// and sending the output from the out queue into the archive file
void comp(struct options opt) {
    int fd, chunks, i, offset, chunksProcesados = 0;
    struct stat st;
    char comp_file[256];
    archive ar;
    queue in, out;
    chunk ch;
    pthread_mutex_t	 mutexChunksProcesados;
    ArgsCompresor *argsComp;
    pthread_t *IdComp;

    if((fd=open(opt.file, O_RDONLY))==-1) {
        printf("Cannot open %s\n", opt.file);
        exit(0);
    }

    fstat(fd, &st);
    chunks = st.st_size/opt.size+(st.st_size % opt.size ? 1:0);

    if(opt.out_file) {
        strncpy(comp_file,opt.out_file,255);
    } else {
        strncpy(comp_file, opt.file, 255);
        strncat(comp_file, ".ch", 255);
    }

    ar = create_archive_file(comp_file);

    in  = q_create(opt.queue_size);
    out = q_create(opt.queue_size);

    if(pthread_mutex_init(&mutexChunksProcesados,NULL)){
      printf("No se pudo inicializar el mutex ChunksProcesados\n");
      exit(1);
    }

    argsComp = malloc(opt.num_threads*sizeof(ArgsCompresor));
    IdComp = malloc(opt.num_threads*sizeof(pthread_t));

    if (argsComp == NULL || IdComp==NULL) {
      printf("Not enough memory\n");
      exit(1);
    }

    for(i=0; i<opt.num_threads; i++){

  		argsComp[i].chunksTotales = chunks;
  		argsComp[i].chunksProcesados = &chunksProcesados;
  		argsComp[i].mutexChunks = &mutexChunksProcesados;
  		argsComp[i].in = in;
  		argsComp[i].out = out;
  		pthread_create(&IdComp[i],NULL,workerCompresor,&argsComp[i]);

    }



    // read input file and send chunks to the in queue
    for(i=0; i<chunks; i++) {
        ch = alloc_chunk(opt.size);

        offset=lseek(fd, 0, SEEK_CUR);

        ch->size   = read(fd, ch->data, opt.size);
        ch->num    = i;
        ch->offset = offset;

        q_insert(in, ch);
    }

    for(i=0; i<opt.num_threads; i++){
      pthread_join(IdComp[i],NULL);
    }

    // send chunks to the output archive file
    for(i=0; i<chunks; i++) {
        ch = q_remove(out);

        add_chunk(ar, ch);
        free_chunk(ch);
    }

    free(argsComp);
    free(IdComp);
    close_archive_file(ar);
    close(fd);
    pthread_mutex_destroy(&mutexChunksProcesados);
    q_destroy(in);
    q_destroy(out);
    pthread_exit(NULL);
}


// Decompress file taking chunks of opt.size from the input file,
// inserting them into the in queue, running them using a worker,
// and sending the output from the out queue into the decompressed file

void decomp(struct options opt) {
    int fd, i;
    struct stat st;
    char uncomp_file[256];
    archive ar;
    queue in, out;
    chunk ch;

    if((ar=open_archive_file(opt.file))==NULL) {
        printf("Cannot open archive file\n");
        exit(0);
    };

    if(opt.out_file) {
        strncpy(uncomp_file, opt.out_file, 255);
    } else {
        strncpy(uncomp_file, opt.file, strlen(opt.file) -3);
        uncomp_file[strlen(opt.file)-3] = '\0';
    }

    if((fd=open(uncomp_file, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH))== -1) {
        printf("Cannot create %s: %s\n", uncomp_file, strerror(errno));
        exit(0);
    }

    in  = q_create(opt.queue_size);
    out = q_create(opt.queue_size);

    // read chunks with compressed data
    for(i=0; i<chunks(ar); i++) {
        ch = get_chunk(ar, i);
        q_insert(in, ch);
    }

    // decompress from in to out
    worker(in, out, zdecompress);

    // write chunks from output to decompressed file
    for(i=0; i<chunks(ar); i++) {
        ch=q_remove(out);
        lseek(fd, ch->offset, SEEK_SET);
        write(fd, ch->data, ch->size);
        free_chunk(ch);
    }

    close_archive_file(ar);
    close(fd);
    q_destroy(in);
    q_destroy(out);
}

int main(int argc, char *argv[]) {
    struct options opt;

    opt.compress    = COMPRESS;
    opt.num_threads = 3;
    opt.size        = CHUNK_SIZE;
    opt.queue_size  = QUEUE_SIZE;
    opt.out_file    = NULL;

    read_options(argc, argv, &opt);

    if(opt.compress == COMPRESS) comp(opt);
    else decomp(opt);
}
