#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "queue.c"


#define MAX_FABRICAS 4
#define MAX_TIENDAS 3
#define MAX_CAMION 2
#define MAX_SUMINISTROS 10
#define MS 1000

typedef struct fabrica {
  int num_fabrica;
  int *num_suministros_creados;
  pthread_mutex_t *mutex;
  queue *camion;
} fabrica;

typedef struct tienda {
  int num_tienda;
  int *num_suministros_consumidos;
  pthread_mutex_t *mutex;
  queue *camion;
} tienda;

typedef struct suministros {
  int id;
} suministros;

void *workerFabrica(void *ptr) {
  fabrica *argsFabrica=ptr;
  suministros *suministro;
  printf("Abriendo Fabrica numero %d\n", argsFabrica->num_fabrica);

  while(1) {
    pthread_mutex_lock(argsFabrica->mutex);

    if (*(argsFabrica->num_suministros_creados)<MAX_SUMINISTROS){

      suministro = malloc (sizeof(suministros));
      suministro->id= *argsFabrica->num_suministros_creados;
      (*(argsFabrica->num_suministros_creados))++;
      pthread_mutex_unlock(argsFabrica->mutex);

    } else {

      pthread_mutex_unlock(argsFabrica->mutex);
      break;

    }

    printf ("La fabrica %d crea el suministro %d\n",argsFabrica->num_fabrica,
    suministro->id);
    q_insert(*argsFabrica->camion,suministro);
  }
  printf("La fabrica %d ha cerrado\n", argsFabrica->num_fabrica);
  return NULL;
}

void *workerTienda(void *ptr) {

	tienda *argsTienda = ptr;
	suministros *suministro;
  printf("Abriendo Tienda numero %d\n", argsTienda->num_tienda);

	while(1) {
    pthread_mutex_lock(argsTienda->mutex);
    if (*(argsTienda->num_suministros_consumidos)<MAX_SUMINISTROS) {
      (*(argsTienda->num_suministros_consumidos))++;
      pthread_mutex_unlock(argsTienda->mutex);

    } else {
      pthread_mutex_unlock(argsTienda->mutex);
      break;
    }
    suministro = q_remove(*(argsTienda->camion));
    printf("Suministro %d consumido por tienda numero %d\n", suministro->id,
    argsTienda->num_tienda);
	}
  printf("La tienda %d ha cerrado\n", argsTienda->num_tienda);
	return NULL;
}

int main (int argc, char **argv)
{

  pthread_t fabricas[MAX_FABRICAS];
  pthread_t tiendas[MAX_TIENDAS];
  pthread_mutex_t mutexFabrica;
  pthread_mutex_t mutexTienda;
  fabrica argsFabrica[MAX_FABRICAS];
  tienda argsTienda[MAX_TIENDAS];
  queue camion;
  int suministros_creados=0;
  int suministros_consumidos=0;
  int i;

  camion = q_create(MAX_CAMION);
  if(pthread_mutex_init(&mutexFabrica,NULL) || pthread_mutex_init(&mutexTienda,NULL)){
    printf("No se puede inicializar alguno de los mutex\n");
    exit(1);
  }

  for (i = 0; i<MAX_FABRICAS; i++) {
    argsFabrica[i].num_fabrica = i;
    argsFabrica[i].mutex = &mutexFabrica;
    argsFabrica[i].camion = &camion;
    argsFabrica[i].num_suministros_creados=&suministros_creados;
    if ( 0 != pthread_create(&fabricas[i],NULL,workerFabrica,&argsFabrica[i])) {
      printf("No se ha podido crear el hilo #%d", i);
      exit(1);
    }
  }

  for (i = 0; i<MAX_TIENDAS; i++) {
    argsTienda[i].num_tienda = i;
    argsTienda[i].mutex = &mutexTienda;
    argsTienda[i].camion = &camion;
    argsTienda[i].num_suministros_consumidos=&suministros_consumidos;
    if ( 0 != pthread_create(&tiendas[i],NULL,workerTienda,&argsTienda[i])) {
      printf("No se ha podido crear el hilo #%d", i);
      exit(1);
    }

  }

  for (i = 0; i < MAX_FABRICAS; i++)
    pthread_join(fabricas[i], NULL);

  printf("Fabricas cerradas\n");

  for (i = 0; i < MAX_TIENDAS; i++)
  	pthread_join(tiendas[i], NULL);

  printf("Tiendas cerradas\n");

  q_destroy(camion);

  pthread_exit(NULL);
}
