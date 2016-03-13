/**
 * Encoding:	utf8
 * File:		barbers.c
 * Compiled:	gcc 4.4 (gcc -std=c99 -Wall -Wextra -Werror -pedantic -lrt barbers.c)
 * Author:		Pavol Loffay xloffa00@fit.vutbr.cz
 * Project:		IOS project2-barbers, FIT VUTBR
 * Date			11.4.2011 
 * Use:			./barber Q GenC GenB N F
 * Q - pocet stoliciek v cakarni
 * GenC - nahodny cas v rozmedzi <0, GenC> v ms, cas medzi vytvaranim procesov zakaznikov
 * GenB - nahodny cas v rozmedzi <0, GenB> v ms, cas strihania jedneho zakaznika
 * N - celkovy pocet zakaznikov (refused + served), celkovy pocet kolko procesov vznikne a zanikne
 * F - nazov suboru, kde sa budu ukladat vystupne data, alebo - pre vystup na stdout
 */

//FILE, fopen, fclose, fprintf, setbuf
#include <stdio.h>
//EXIT_SUCCESS, exit(), strtol, srand
#include <stdlib.h>
//errno
#include <errno.h>
//strcmp
#include <string.h>
//LONG_MAX, LONG_MIN
#include <limits.h>
//generovanie casu, time()
#include <time.h>

//bool, true
#include <stdbool.h>

//SIGTERM, kill()
#include <signal.h>
//fork()-vytvory movy proces, usleep, getgid
#include <unistd.h>
//pid_t
#include <sys/types.h>
//waitpid()
#include <sys/wait.h>
//IPC_CREAT, IPC_PRIVATE
#include <sys/ipc.h>
//shmget()-vytvorenie zdielanej pameti,
#include <sys/shm.h>

//semafory, sem_t, sem_open, SEM_FAILED
#include <semaphore.h>
#include <sys/stat.h>
//O_CREAT
#include <fcntl.h> 

#define KILO 1000

//vyctovy typ sluziaci pre pristup k premennym v zdielanej pameti
enum shareMemItems
{
	//premenna counter sluzi na vypis cisla vypisanej akcie
	COUNTER = 0,
	//premenna volnych stoliciek
	SEATS,
	//premenna obsluzenych zakaznikov
	CUSTOMER_COUNTER,
	//premenna sa rovna = 3, identifikuje kolko premennych je v zidelanej
	//pameti
	SHAREMEM_END,
};

//vyctovy typ pre pristup k semaforom
enum semaphoreItems
{
	PRINTF = 0,
	ACCESS_SEATS,
	CUSTOMERS,
	BARBER,
	BARBER_FINISHED,
	CUSTOMER_READY,
	CUSTOMER_COUNTER_SEMAPHORE,
	CUSTOMER_END,
	END,
};

//nazvy pomenovanych semaforov
const char *semaphoreName[]= 
{
	"xloffa00.printf",
	"xloffa00.accessSeats",
	"xloffa00.customers",
	"xloffa00.barber",
	"xloffa00.barberFinished",
	"xloffa00.customerReady",
	"xloffa00.procesCounter",
	"xloffa00.customerEnd",
};

//struktura uchovava informacie o programe
typedef struct params
{
	//pocet stoliciek v holicstve
	int chairs;
	//nahodna doba medzi vytvaranim zakaznikov v <intervale> 
	int genC;
	//nahodna doba trvania ostrihania v <intervale>
	int genB;
	//pocet vygenerovanych zakaznikov 
	int customers;
	//ukazatel na subor ak sa nema zapisovat na stdout
	FILE *fw;
}TParams;

/**
 * Funkcie 
 */
void printError(char *mesg, FILE *fw);
int *getNumber(char *argv, int *number);
TParams getParams(int argc, char **argv);
void customerFunction(int customerNumber, FILE *fw, sem_t **semaphore, int *shareMem);
void barberFunction(int chairs,FILE *fw, int timeSleep, sem_t **semaphore, int *shareMem);


/**
 * funcia  vytlaci spravu a ukonci program s exit(1)
 * @param *megs ukazatel na spravu
 * @return exit(1)
 */
void printError(char *mesg, FILE *fw)
{
	//ak subor nie je zavrety, tak zavriet!
	if (fw != stdout && fw != NULL)
		fclose(fw);
	//vypis chyboveho hlasenia
	fprintf(stderr, "%s", mesg);
	//ukonci program 
	exit(1);
}

/**
 * funkcia skonvertuje retazec na cislo typu long  a kontroluje pretecenie
 * @param *argv ukazatel na retazec
 * @param *number ukazatel na cislo kde sa ma ulozit vysledok
 * @return cislo ak sa skonvertovalo vporiadku, NULL ak nastala chyba/pretecenie(9a..)
 */
int *getNumber(char *argv, int *number)
{
	char *endpr = NULL;
	*number = strtoul(argv, &endpr, 10);
	//ak za parametrom nebol znak 0 tak bolo zadanie nieco ako 9a 98a co je nespravne
	if (*endpr != '\0')
		return NULL;
	//kontrola pretecenia
	if ((errno == ERANGE && (*number == INT_MIN || *number == INT_MAX)) || errno != 0 || *number < 0)
		return NULL;
	
	return number;
}

/**
 * funkcia spracuje parametre prikazoveho riadku
 * @param argc pocet parametrov prik riadku aj s nazvom programu 
 * @param **argv pole ukazatelov na dane parametre prik. riadku
 * @return struktura params
 */
TParams getParams(int argc, char **argv)
{
	//inicializacia struktury
	TParams params = 
	{
		.chairs = 0,
		.genC = 0,
		.genB = 0,
		.customers = 0,
		.fw = stdout,
	};
	
	//ak sa zadali spravny pocet parametrov na prik riadku ./barbers chairs genB genC customers -/subor
	if (argc == 6)
	{
		if (strcmp(argv[5], "-") != 0)
		{
			//otvorenie subora
			params.fw = fopen(argv[5], "w");
			if (params.fw == NULL)
				printError("Nepodarilo sa otvorit subor na zapisovanie!\n", params.fw);
		}
		
		if (getNumber(argv[1], &params.chairs) == NULL)
			printError("Zadali ste nespravne parametre prik. riadku!\n", params.fw);
		
		if (getNumber(argv[2], &params.genC) == NULL)
			printError("Zadali ste nespravne parametre prik. riadku!\n", params.fw);
		
		if (getNumber(argv[3], &params.genB) == NULL)
			printError("Zadali ste nespravne parametre prik. riadku!\n", params.fw);
		
		if (getNumber(argv[4], &params.customers) == NULL)
			printError("Zadali ste nespravne parametre prik. riadku!\n", params.fw);
	}
	//zadal sa nespravny pocet parametrov na prik. riadku
	else
		printError("Zadali ste zle parametre prikazobeho riadku!\n", params.fw);
	
	return params;
}

/**
 * funkcia pre proces zakaznika
 * @param customerNumber cislo identifikujuce zakaznika
 * @param *fw ukazatel na otvoreny suvor alebo stdout
 * @param **semaphore semafory
 * @param *shareMem zdielana pamet
 */
void customerFunction(int customerNumber, FILE *fw, sem_t **semaphore, int *shareMem)
{
	//zakaznik sa vytvory
	sem_wait(semaphore[PRINTF]);
		setbuf(fw, NULL);
		fprintf(fw, "%d: customer %d: created\n", shareMem[COUNTER], customerNumber);
		shareMem[COUNTER] = shareMem[COUNTER] + 1;
	sem_post(semaphore[PRINTF]);
		
	//zakaznik ziska zamok do cakarne
	sem_wait(semaphore[ACCESS_SEATS]);
	//zakaznik vosiel do cakarne
		sem_wait(semaphore[PRINTF]);
			setbuf(fw, NULL);
			fprintf(fw, "%d: customer %d: enters\n", shareMem[COUNTER], customerNumber);
			shareMem[COUNTER] = shareMem[COUNTER] + 1;
		sem_post(semaphore[PRINTF]);
		
		//ak je cakaren prazdna
		if (shareMem[SEATS] > 0)
		{
			//zvnizi hodnotu volnych stoliciek v cakarni
			shareMem[SEATS] = shareMem[SEATS] - 1;
			
			//---upozorni holica na naveho zakaznika
			sem_post(semaphore[CUSTOMERS]);			
			
			//uvolni zamok cakarne
			sem_post(semaphore[ACCESS_SEATS]);
			//snazi sa ziskat zamok holica
			sem_wait(semaphore[BARBER]);
			
			
			sem_wait(semaphore[PRINTF]);
				setbuf(fw, NULL);
				fprintf(fw, "%d: customer %d: ready\n", shareMem[COUNTER], customerNumber);
				shareMem[COUNTER] = shareMem[COUNTER] + 1;
			sem_post(semaphore[PRINTF]);
			
			sem_post(semaphore[CUSTOMER_READY]);
			
			sem_wait(semaphore[BARBER_FINISHED]);
				sem_wait(semaphore[PRINTF]);
					setbuf(fw, NULL);
					fprintf(fw, "%d: customer %d: served\n", shareMem[COUNTER], customerNumber);
					shareMem[COUNTER] = shareMem[COUNTER] + 1;
				sem_post(semaphore[PRINTF]);
				
			sem_post(semaphore[CUSTOMER_END]);
		}
		else
		{
			//ak je cakaren plna
			//nie su volne stolicky zakaznik musi odyst
			sem_wait(semaphore[PRINTF]);
				setbuf(fw, NULL);
				fprintf(fw, "%d: customer %d: refused\n", shareMem[COUNTER], customerNumber);
				shareMem[COUNTER] = shareMem[COUNTER] + 1;
			sem_post(semaphore[PRINTF]);
			
			//znizi hodnotu obsluzenych zakaznikov
			sem_wait(semaphore[CUSTOMER_COUNTER_SEMAPHORE]);
				shareMem[CUSTOMER_COUNTER] = shareMem[CUSTOMER_COUNTER] - 1;
			sem_post(semaphore[CUSTOMER_COUNTER_SEMAPHORE]);
			
			sem_post(semaphore[ACCESS_SEATS]);
		}
}

/**
 * funkcia pre proces holica
 * @param chairs cislo udavajuce pocet stoliciek v cakarni
 * ak je stopliciek 0 tak ukonci cyklus
 * @param *fw ukazatel na otvoreny subor alebo stdout
 * @param timeSleep cislo ktore uspi proces holica -- strihanie
 * @param **semaphore semafory
 * @param *sareMem zdielana pamet
 */
void barberFunction(int chairs, FILE *fw, int timeSleep, sem_t **semaphore, int *shareMem)
{
	//nekonecny cyklus pre poroces holica, aby vzdy zkontroloval cakaren
	while(true)
	{
		sem_wait(semaphore[ACCESS_SEATS]);
		sem_wait(semaphore[CUSTOMER_COUNTER_SEMAPHORE]);
			if (shareMem[CUSTOMER_COUNTER] > 0)
			{
				sem_wait(semaphore[PRINTF]);
					setbuf(fw, NULL);
					fprintf(fw, "%d: barber: checks\n", shareMem[COUNTER]);
					shareMem[COUNTER] = shareMem[COUNTER] + 1;
				sem_post(semaphore[PRINTF]);
				sem_post(semaphore[CUSTOMER_COUNTER_SEMAPHORE]);
				sem_post(semaphore[ACCESS_SEATS]);
				
				if (chairs == 0)
					break;
			}
			else
			{
				sem_post(semaphore[CUSTOMER_COUNTER_SEMAPHORE]);
				sem_post(semaphore[ACCESS_SEATS]);
				break;
			}			
		
		//caka na najekaho zakaznika
		sem_wait(semaphore[CUSTOMERS]);

		//holic si ide po zakaznika
		sem_wait(semaphore[ACCESS_SEATS]);
		
			//vypisem ze je customer ready a uvolnim jednu stolicku z cakarne
			sem_wait(semaphore[PRINTF]);
					setbuf(fw, NULL);
					fprintf(fw, "%d: barber: ready\n", shareMem[COUNTER]);
					shareMem[COUNTER] = shareMem[COUNTER] + 1;
			sem_post(semaphore[PRINTF]);
			shareMem[SEATS] = shareMem[SEATS] + 1;
			
			//zamok holica moze ziskat niektory z dalsich procesov			
			sem_post(semaphore[BARBER]);
		//uvolnenie zamku cakarne
		sem_post(semaphore[ACCESS_SEATS]);
		
		sem_wait(semaphore[CUSTOMER_READY]);
		
		//simulacia strihania
		usleep((rand() % (timeSleep + 1)) * KILO);
		
		sem_wait(semaphore[PRINTF]);
				setbuf(fw, NULL);
				fprintf(fw, "%d: barber: finished\n", shareMem[COUNTER]);
				shareMem[COUNTER] = shareMem[COUNTER] + 1;
		sem_post(semaphore[PRINTF]);
		
		//znizi hodnotu obsluzenych zakaznikov
		sem_wait(semaphore[CUSTOMER_COUNTER_SEMAPHORE]);
			shareMem[CUSTOMER_COUNTER] = shareMem[CUSTOMER_COUNTER] - 1;
		sem_post(semaphore[CUSTOMER_COUNTER_SEMAPHORE]);
		
		sem_post(semaphore[BARBER_FINISHED]);
		//caka az pokial bude customer ready
		sem_wait(semaphore[CUSTOMER_END]);
	}
}

/**
 * Hlavny program 
 */
int main(int argc, char **argv)
{
	//nacitanie parametrov z prik. riadky od struktury params
	TParams params = getParams(argc, argv);
	//aby sa vzdy vygeneroval rozdielny cas 
	srand(time(NULL));

	//ukladam navratove hodnoty z volania fork()
	pid_t pid = 0;
	//ukladam pid procesov zakaznikov
	pid_t pidCustomer[params.customers];
	//ukladam pid procesu holica
	pid_t pidBarber;
	
	//cislo akcie A:
	int actionNumberS = 0;
	int *shareMem;

	//vygenerovanie jedinecneho kluca pre funkciu shmget
	key_t key = ftok(argv[0], 'b');
	if (key == -1)
		printError("(ftok) Nepodaril sa vygeneovat jedinecky kluc!\n", params.fw);
	actionNumberS = shmget(key,SHAREMEM_END * sizeof(int), IPC_CREAT | 0666);
	if (actionNumberS == -1)
		printError("(shmget) Nepodaril sa shmget!\n", params.fw);
	//spristupnenie zdielanej pamete
	//spopjenie datoveho priestoru procesu s zielanov pametov
	shareMem = (int *)shmat(actionNumberS, 0, 0);
	if (*shareMem == -1)
		printError("(shmat) Nepodarilo sa spristupenie zdielanej pameti!\n", params.fw);
	//inicializacia prvkov v zdielanej pameti
	shareMem[COUNTER] = 1;
	shareMem[SEATS] = params.chairs;
	shareMem[CUSTOMER_COUNTER] = params.customers;
	
	//SEMAFORY;
	sem_t *semaphore[END];
	//otvorenie + inicializacia 
	for (int i = 0; i < END; i++)
	{
		semaphore[i] = sem_open(semaphoreName[i],O_CREAT, S_IRUSR | S_IWUSR, 0);
		if (semaphore[i] == SEM_FAILED)
		{
			shmdt(shareMem);
			shmctl(actionNumberS, IPC_RMID, NULL);
			//ak i je vacsie ako 0, takze treba "uvolnit uz vytvorene semafory
			if (i > 0)
				for (int j = i - 1; j >= 0; j--)
				{
					sem_close(semaphore[j]);
					sem_unlink(semaphoreName[j]);
				}
			printError("(sem_open) Nepodaril sa otvorit semafor!\n", params.fw);
		}
	}
	//zvysim hodnotu semaforu na 1
	sem_post(semaphore[PRINTF]);
	//inicializujem cakaren na 1
	sem_post(semaphore[ACCESS_SEATS]);
	//inicializujem pristup k premenej procesCounter
	sem_post(semaphore[CUSTOMER_COUNTER_SEMAPHORE]);
	
	//vytvorenie procesu holica
	 pidBarber = fork();
	 if (pidBarber == 0)
	 {
		 //kod potomka = holica, fork sa podaril
		 //skocenie do funkcie holica
		 barberFunction(params.chairs, params.fw, params.genB, semaphore, shareMem);
		 //odstranenie semaforov
		for (int i = 0; i < END; i++)
		{
			sem_close(semaphore[i]);
			sem_unlink(semaphoreName[i]);
		}
		//odstranenie zdielanej pameti
		shmdt(shareMem);
		shmctl(actionNumberS, IPC_RMID, NULL); 
		//zavretie suboru
		if (params.fw != stdout)
			fclose(params.fw);
		//ukonci proces zakaznika po jeho uspesnom prebehnuti
		return EXIT_SUCCESS;
	 }
	 else
		 if (pidBarber == -1)
		 {
			 //nastala chyba kod rodica, nepodaril sa fork
			 //ukoncim semafory
			for (int i = 0; i < END; i++)
			{
				sem_close(semaphore[i]);
				sem_unlink(semaphoreName[i]);
			}
			//zavrem zdielanu pamet
			shmdt(shareMem);
			shmctl(actionNumberS, IPC_RMID, NULL);
			//zabijem ostatne procesy
			kill(getuid(), SIGKILL);
			printError("Doslo k chybe pri vytvarani procesu holica!\n",params.fw);	
		 }
		 else
			 //kod hlavneho procesu main
			 //kod pre rodica pid = PIDpotomka
			pidBarber = pid;  
		 
	//vytvarenie procesov zakaznikov
	for (int i = 0; i < params.customers; i++)
	{
		pid = fork();
		if (pid == 0)
		{
			//kod potomka
			//skocenie do funkcie potomka
			customerFunction(i + 1, params.fw, semaphore, shareMem);			
			//zavrem subor
			if (params.fw != stdout)
				fclose(params.fw);			
			//zavrem semafory
			for (int j = 0; j < END; j++)
			{
				sem_close(semaphore[j]);
				sem_unlink(semaphoreName[j]);
			}
			//odstranenie zdielanej pamete
			shmdt(shareMem);
			shmctl(actionNumberS, IPC_RMID, NULL);
			//ukonci proces zakaznika			
			return EXIT_SUCCESS;
		}
		else 
			if(pid < 0)
			{
				//nastala chyba kod rodica, nepodaril sa fork
				//ukoncim semafory
				for (int i = 0; i < END; i++)
				{
					sem_close(semaphore[i]);
					sem_unlink(semaphoreName[i]);
				}	
				//zavrem zdielanu pamet
				shmdt(shareMem);
				shmctl(actionNumberS, IPC_RMID, NULL);
				//zabijem ostatne procesy
				kill(getuid(), SIGKILL);
				printError("(fork) Doslo k chybe pri vytvarani procesu zakaznika!\n", params.fw);
			}
			else
			{
				//kod hlavneho procesu main
				//pid = PIDpotomka
				pidCustomer[i] = pid;
	//generovanie nahodneho casu + uspanie, aby sa procesy vytvarali v intervale 0 .. genC
				usleep((rand() % (params.genC + 1)) * KILO);
			}
	}
	
	//cakanie hlavneho procesu na ukoncenie potomkov
	for (int i = 0; i < params.customers; i++)
	{
		//ak sa nepodarilo cakanie na ukoncanie procesov
		if (waitpid(pidCustomer[i], NULL, 0) == -1)
		{
			//ukoncim semafory
			for (int i = 0; i < END; i++)
			{
				sem_close(semaphore[i]);
				sem_unlink(semaphoreName[i]);
			}
			//zavrem zdielanu pamet
			shmdt(shareMem);
			shmctl(actionNumberS, IPC_RMID, NULL);
			//zabijem ostatne procesy
			kill(getuid(), SIGKILL);
			printError("(waitpi) Pri behu potomka doslo k chybe!\n", params.fw);
		}
	}
	
	//cakam na ukoncenie procesu holica
	if (waitpid(pidBarber, NULL, 0) == -1)
	{
		//ukoncim semafory
		for (int i = 0; i < END; i++)
		{
			sem_close(semaphore[i]);
			sem_unlink(semaphoreName[i]);
		}
		//zavrem zdielanu pamet
		shmdt(shareMem);
		shmctl(actionNumberS, IPC_RMID, NULL);
		//zabijem ostatne procesy
		kill(getuid(), SIGKILL);
		printError("(waitpid) Pri behu holica doslo k chybe!\n", params.fw);
	}
	
	//zavrem semafory
	for (int i = 0; i < END; i++)
	{
		sem_close(semaphore[i]);
		sem_unlink(semaphoreName[i]);
	}
	//odstranenie zdielanej pamete
	shmdt(shareMem);
	shmctl(actionNumberS, IPC_RMID, NULL);
	//zavrem subor
	if (params.fw != stdout)
		fclose(params.fw);
	return EXIT_SUCCESS;
}