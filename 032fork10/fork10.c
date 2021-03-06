#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <semaphore.h>


#define NUM_PROCESSES 10

// sem_wait e simili possono essere interrotti da un segnale; il comportamento corretto è verificare questo caso
#define CHECK_EINTR

void * create_anon_mmap(size_t size) {

	// MAP_SHARED: condivisibile tra processi
	// PROT_READ | PROT_WRITE: posso leggere e scrivere nello spazio di memoria
	void * memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	if (memory == MAP_FAILED) {
		perror("mmap");

		return NULL;
	}

	return memory;

}

sem_t * sem; // contatore usato come "countdown" fino a 0



void add_counter(sem_t * semaphore, int * var, int val) {

	int res;

#ifdef CHECK_EINTR
	// vedere esempio 16sem_signal
	// vedere http://man7.org/linux/man-pages/man7/signal.7.html
	// mentre stiamo aspettando che il semaforo vanga sbloccato, la chiamata a sem_wait potrebbe essere interrotta
	// da un segnale il cui handler è stato impostato senza flag SA_RESTART
	while ((res = sem_wait(semaphore)) == -1 && errno == EINTR)
		continue;

	if (res == -1) {
		perror("sem_wait");
		exit(EXIT_FAILURE);
	}
#else

	if (sem_wait(semaphore) == -1) {
		perror("sem_wait");
		exit(EXIT_FAILURE);
	}
#endif

	*var += val;

	if (sem_post(semaphore) == -1) {
		perror("sem_post");
		exit(EXIT_FAILURE);
	}

}



int * process_counter;
sem_t * process_counter_sem;

int main(int argc, char * argv[]) {

	// questo è il semaforo che fa funzionare correttamente la concorrenza tra processi
	sem = create_anon_mmap(sizeof(sem_t));

	////////// process_counter è in più, il "meccanismo" funziona grazie al solo semaforo sem
	process_counter = create_anon_mmap(sizeof(int));
	process_counter_sem = create_anon_mmap(sizeof(sem_t));

	*process_counter = 1; // conto il processo corrente
	/////////


	if (sem_init(sem, 1, NUM_PROCESSES -1) == -1) {
		perror("sem_init");
		exit(EXIT_FAILURE);
	}

	// il valore iniziale di process_counter_sem è 1 (perchè conto il primo processo)
	if (sem_init(process_counter_sem, 1, 1) == -1) {
		perror("sem_init");
		exit(EXIT_FAILURE);
	}

	printf("primo processo: pid=%d\n", getpid());

	int s;
	int sem_val;

	int child_process_pid;

	int boss = 1;

	while (1) {

		sem_getvalue(sem, &sem_val);
		// dopo aver letto il valore del semaforo e prima del printf...
		// il valore del semaforo potrebbe essere già cambiato
		printf("prima di sem_trywait1 [%d] sem_val=%d\n", getpid(), sem_val);

		// sem_trywait decrementa il semaforo, se semaforo è > 0; altrimenti restituisce errore
		s = sem_trywait(sem); // restituisce 0 se il semaforo è stato preso dal processo corrente
		// restituisce -1 ed errno == EAGAIN se il semaforo vale zero

		if (s == -1 && errno == EAGAIN) { // il semaforo vale zero, il processo può terminare

			printf("break1[%d]- sem_val vale zero\n", getpid());

			break; // usciamo da while(1)
		}

		switch(child_process_pid = fork()) {
		case 0: // child process

			boss = 0;

			// incremento il contatore condiviso, protetto da semaforo "lock"
			add_counter(process_counter_sem, process_counter, 1);

			continue; // rimanda al while(1), così il nuovo processo potrà fare al più 2 fork
		default:
			printf("fork[%d] new child process pid=%d, contatore_processi=%d\n", getpid(), child_process_pid, *process_counter);
		}

		sem_getvalue(sem, &sem_val);
		printf("prima di sem_trywait2 [%d] sem_val=%d\n", getpid(), sem_val);

		s = sem_trywait(sem); // restituisce 0 se il semaforo è stato preso dal processo corrente
		// restituisce -1 ed errno == EAGAIN se il semaforo vale zero

		if (s == -1 && errno == EAGAIN) { // il semaforo vale zero, il processo può terminare
			printf("break2[%d]- sem_val vale zero\n", getpid());

			break; // usciamo da while(1)
		}

		switch(child_process_pid = fork()) {
		case 0: // child process

			boss = 0;

			// incremento il contatore condiviso, protetto da semaforo "lock"
			add_counter(process_counter_sem, process_counter, 1);

			continue;
		default:
			printf("fork[%d] new child process pid=%d, contatore_processi=%d\n", getpid(), child_process_pid, *process_counter);
		}

		break; // abbiamo lanciato due processi figli, usciamo da while(1)

	} // while(1)

	sem_getvalue(sem, &sem_val);
	printf("prima di wait [%d], sem_val=%d\n", getpid(), sem_val);

	while (wait(NULL) != -1) // aspettiamo la conclusione degli eventuali processi figli (al più 2)
		;

	sem_getvalue(sem, &sem_val);
	printf("bye! [%d] sem_val=%d  contatore_processi=%d\n", getpid(), sem_val, *process_counter);

	if (boss == 1 && sem_destroy(sem) == -1) { // distruggiamo il semaforo
		perror("sem_destroy");
	}

	munmap(sem, sizeof(sem_t)); // liberiamo la memoria condivisa tra i processi usata per il semaforo

	///
	if (boss == 1 && sem_destroy(process_counter_sem) == -1) { // distruggiamo il semaforo
		perror("sem_destroy");
	}

	munmap(process_counter_sem, sizeof(sem_t));

	munmap(process_counter, sizeof(int));
	///

	return 0;
}

